#include "core/movegen.h"

#include "core/attacks.h"
#include "core/board.h"
#include "core/primitives.h"

static inline bool attacked_with_occ(const chess *game, int sq, u64 occ) {
    u64 enemy = game->enemy_pieces;
    u64 true_pawns = game->pawns & PAWN_MASK;

    if (enemy & true_pawns & g_pawn_attacks[0][sq]) {
        return true;
    }
    u64 enemy_knights = enemy & (~true_pawns) & (~game->orthogonal_pieces)
                        & (~game->diagonal_pieces) & (~game->kings);
    if (enemy_knights & g_knight_attacks[sq]) {
        return true;
    }
    if (enemy & game->kings & g_king_attacks[sq]) {
        return true;
    }
    if ((enemy & game->diagonal_pieces) & bishop_attacks_bb(sq, occ)) {
        return true;
    }
    if ((enemy & game->orthogonal_pieces) & rook_attacks_bb(sq, occ)) {
        return true;
    }
    return false;
}

static inline u64 attackers_to_king(const chess *game, int king_sq, u64 occ) {
    u64 enemy = game->enemy_pieces;
    u64 true_pawns = game->pawns & PAWN_MASK;
    u64 enemy_knights = enemy & (~true_pawns) & (~game->orthogonal_pieces)
                        & (~game->diagonal_pieces) & (~game->kings);

    u64 atk = (enemy & true_pawns & g_pawn_attacks[0][king_sq]);
    atk |= (enemy_knights & g_knight_attacks[king_sq]);
    atk |= ((enemy & game->diagonal_pieces) & bishop_attacks_bb(king_sq, occ));
    atk |= ((enemy & game->orthogonal_pieces) & rook_attacks_bb(king_sq, occ));
    return atk;
}

static inline u64 compute_pinned(const chess *game, int king_sq, u64 occ) {
    u64 friendly = game->friendly_pieces;
    u64 enemy = game->enemy_pieces;
    u64 pinned = 0ull;

    u64 e_orth = enemy & game->orthogonal_pieces;
    if (e_orth) {
        u64 r_direct = rook_attacks_bb(king_sq, occ);
        u64 r_blockers = r_direct & friendly;
        u64 r_pinners = (rook_attacks_bb(king_sq, occ ^ r_blockers) ^ r_direct) & e_orth;
        while (r_pinners) {
            int p = __builtin_ctzll(r_pinners);
            r_pinners &= r_pinners - 1ull;
            pinned |= g_between_bb[king_sq][p] & friendly;
        }
    }

    u64 e_diag = enemy & game->diagonal_pieces;
    if (e_diag) {
        u64 b_direct = bishop_attacks_bb(king_sq, occ);
        u64 b_blockers = b_direct & friendly;
        u64 b_pinners = (bishop_attacks_bb(king_sq, occ ^ b_blockers) ^ b_direct) & e_diag;
        while (b_pinners) {
            int p = __builtin_ctzll(b_pinners);
            b_pinners &= b_pinners - 1ull;
            pinned |= g_between_bb[king_sq][p] & friendly;
        }
    }

    return pinned;
}

static inline u64 fast_encode(int src, int dst, u8 sp, u8 epc, u8 pr, u8 fdata, u8 edata) {
    return ((u64) src) | (((u64) dst) << 6) | (((u64) pr) << 15) | (((u64) sp) << 21)
           | (((u64) epc) << 27) | (((u64) fdata) << 39) | (((u64) edata) << 47);
}

static inline int emit_pawn_fast(
    const chess *game, u64 enemy, u64 *moves, int count, int src, int dst, u8 fdata, u8 edata) {
    u8 epc = (enemy & (1ull << dst)) ? get_piece_at_offset(game, dst) : NONE;
    if (__builtin_expect(dst >= 56, 0)) {
        moves[count++] = fast_encode(src, dst, FRIENDLY_PAWN, epc, FRIENDLY_QUEEN, fdata, edata);
        moves[count++] = fast_encode(src, dst, FRIENDLY_PAWN, epc, FRIENDLY_ROOK, fdata, edata);
        moves[count++] = fast_encode(src, dst, FRIENDLY_PAWN, epc, FRIENDLY_BISHOP, fdata, edata);
        moves[count++] = fast_encode(src, dst, FRIENDLY_PAWN, epc, FRIENDLY_KNIGHT, fdata, edata);
    } else {
        moves[count++] = fast_encode(src, dst, FRIENDLY_PAWN, epc, FRIENDLY_PAWN, fdata, edata);
    }
    return count;
}

static inline int emit_pawn_move(
    const chess *game, u64 *moves, int count, int start_index, int end_index, bool tactical_only) {
    if (end_index >= 56) {
        moves[count++]
            = build_move(game, start_index, end_index, false, false, false, FRIENDLY_QUEEN);
        moves[count++]
            = build_move(game, start_index, end_index, false, false, false, FRIENDLY_ROOK);
        moves[count++]
            = build_move(game, start_index, end_index, false, false, false, FRIENDLY_BISHOP);
        moves[count++]
            = build_move(game, start_index, end_index, false, false, false, FRIENDLY_KNIGHT);
    } else if (!tactical_only) {
        moves[count++]
            = build_move(game, start_index, end_index, false, false, false, FRIENDLY_PAWN);
    }
    return count;
}

// mode: 0 = all legal moves, 1 = tactical (captures + promos), 2 = quiet (non-captures, non-promos)
static int generate_legal_internal(chess *game, u64 *moves, int mode) {
    attacks_init();

    u64 friendly = game->friendly_pieces;
    u64 enemy = game->enemy_pieces;
    u64 occ = friendly | enemy;
    u64 empty = ~occ;
    u64 f_king_bb = friendly & game->kings;

    if (__builtin_expect(f_king_bb == 0ull, 0)) {
        moves[0] = NULL_MOVE;
        return 0;
    }
    int king_sq = __builtin_ctzll(f_king_bb);

    u64 checkers = attackers_to_king(game, king_sq, occ);
    if (checkers != 0ull) {
        game->pawns |= (FRIENDLY & IN_CHECK);
    } else {
        game->pawns &= ~(FRIENDLY & IN_CHECK);
    }

    u8 fdata = (u8) (game->pawns & 0xffull);
    u8 edata = (u8) ((game->pawns >> 56) & 0xffull);

    int count = 0;
    u64 occ_no_king = occ ^ f_king_bb;

    // 1. King moves (always legal if destination is not attacked with king removed from occ)
    u64 king_filter = (mode == 1) ? enemy : ((mode == 2) ? empty : (~friendly));
    u64 king_dests = g_king_attacks[king_sq] & king_filter;
    while (king_dests) {
        int dst = __builtin_ctzll(king_dests);
        king_dests &= king_dests - 1ull;
        if (!attacked_with_occ(game, dst, occ_no_king)) {
            u8 epc = (enemy & (1ull << dst)) ? get_piece_at_offset(game, dst) : NONE;
            moves[count++]
                = fast_encode(king_sq, dst, FRIENDLY_KING, epc, FRIENDLY_KING, fdata, edata);
        }
    }

    int num_checkers = __builtin_popcountll(checkers);
    if (__builtin_expect(num_checkers >= 2, 0)) {
        moves[count] = NULL_MOVE;
        return count;
    }

    u64 check_mask = ~0ull;
    if (num_checkers == 1) {
        int checker_sq = __builtin_ctzll(checkers);
        check_mask = checkers | g_between_bb[king_sq][checker_sq];
    }

    u64 pinned = compute_pinned(game, king_sq, occ);
    u64 true_pawns = game->pawns & PAWN_MASK;
    u64 f_pawns = friendly & true_pawns;

    // 2. Pawn moves (single push, double push, left/right captures, en passant)
    u64 single_pushes = (empty & (f_pawns << 8)) & check_mask;
    if (mode == 1) {
        single_pushes &= ROW_8;
    } else if (mode == 2) {
        single_pushes &= ~ROW_8;
    }
    while (single_pushes) {
        int dst = __builtin_ctzll(single_pushes);
        single_pushes &= single_pushes - 1ull;
        int src = dst - 8;
        if ((pinned & (1ull << src)) && !(g_line_bb[king_sq][src] & (1ull << dst))) {
            continue;
        }
        count = emit_pawn_fast(game, 0ull, moves, count, src, dst, fdata, edata);
    }

    if (mode != 1) {
        u64 step1 = empty & ((f_pawns & ROW_2) << 8);
        u64 double_pushes = (empty & (step1 << 8)) & check_mask;
        while (double_pushes) {
            int dst = __builtin_ctzll(double_pushes);
            double_pushes &= double_pushes - 1ull;
            int src = dst - 16;
            if ((pinned & (1ull << src)) && !(g_line_bb[king_sq][src] & (1ull << dst))) {
                continue;
            }
            moves[count++]
                = fast_encode(src, dst, FRIENDLY_PAWN, NONE, FRIENDLY_PAWN, fdata, edata);
        }
    }

    if (mode != 2) {
        // Left pawn captures (dx = -1, dy = +1 => src = dst - 7)
        u64 left_caps = (enemy & ((f_pawns & ~COL_1) << 7)) & check_mask;
        while (left_caps) {
            int dst = __builtin_ctzll(left_caps);
            left_caps &= left_caps - 1ull;
            int src = dst - 7;
            if ((pinned & (1ull << src)) && !(g_line_bb[king_sq][src] & (1ull << dst))) {
                continue;
            }
            count = emit_pawn_fast(game, enemy, moves, count, src, dst, fdata, edata);
        }

        // Right pawn captures (dx = +1, dy = +1 => src = dst - 9)
        u64 right_caps = (enemy & ((f_pawns & ~COL_8) << 9)) & check_mask;
        while (right_caps) {
            int dst = __builtin_ctzll(right_caps);
            right_caps &= right_caps - 1ull;
            int src = dst - 9;
            if ((pinned & (1ull << src)) && !(g_line_bb[king_sq][src] & (1ull << dst))) {
                continue;
            }
            count = emit_pawn_fast(game, enemy, moves, count, src, dst, fdata, edata);
        }

        // En passant capture (validated via make/unmake for rare horizontal discovered pins)
        if (__builtin_expect(has_en_passant(game), 0)) {
            int ep_target = get_en_passant_offset(game);
            int ep_file = ep_target - 40;
            if (ep_file < 7 && ((1ull << (32 + ep_file + 1)) & f_pawns)) {
                u64 m = build_move(
                    game, 32 + ep_file + 1, ep_target, true, false, false, FRIENDLY_PAWN);
                if (is_legal_move(game, m)) {
                    moves[count++] = m;
                }
            }
            if (ep_file > 0 && ((1ull << (32 + ep_file - 1)) & f_pawns)) {
                u64 m = build_move(
                    game, 32 + ep_file - 1, ep_target, true, false, false, FRIENDLY_PAWN);
                if (is_legal_move(game, m)) {
                    moves[count++] = m;
                }
            }
        }
    }

    u64 target_mask = king_filter & check_mask;

    // 3. Knights (pinned knights can NEVER move!)
    u64 knights = friendly & (~true_pawns) & (~game->orthogonal_pieces) & (~game->diagonal_pieces)
                  & (~game->kings) & (~pinned);
    while (knights) {
        int src = __builtin_ctzll(knights);
        knights &= knights - 1ull;
        u64 dests = g_knight_attacks[src] & target_mask;
        while (dests) {
            int dst = __builtin_ctzll(dests);
            dests &= dests - 1ull;
            u8 epc = (enemy & (1ull << dst)) ? get_piece_at_offset(game, dst) : NONE;
            moves[count++]
                = fast_encode(src, dst, FRIENDLY_KNIGHT, epc, FRIENDLY_KNIGHT, fdata, edata);
        }
    }

    // 4. Bishops (diagonal only)
    u64 f_diag = friendly & game->diagonal_pieces;
    u64 f_orth = friendly & game->orthogonal_pieces;
    u64 bishops = f_diag & ~f_orth;
    while (bishops) {
        int src = __builtin_ctzll(bishops);
        bishops &= bishops - 1ull;
        u64 dests = bishop_attacks_bb(src, occ) & target_mask;
        if (pinned & (1ull << src)) {
            dests &= g_line_bb[king_sq][src];
        }
        while (dests) {
            int dst = __builtin_ctzll(dests);
            dests &= dests - 1ull;
            u8 epc = (enemy & (1ull << dst)) ? get_piece_at_offset(game, dst) : NONE;
            moves[count++]
                = fast_encode(src, dst, FRIENDLY_BISHOP, epc, FRIENDLY_BISHOP, fdata, edata);
        }
    }

    // 5. Rooks (orthogonal only)
    u64 rooks = f_orth & ~f_diag;
    while (rooks) {
        int src = __builtin_ctzll(rooks);
        rooks &= rooks - 1ull;
        u64 dests = rook_attacks_bb(src, occ) & target_mask;
        if (pinned & (1ull << src)) {
            dests &= g_line_bb[king_sq][src];
        }
        while (dests) {
            int dst = __builtin_ctzll(dests);
            dests &= dests - 1ull;
            u8 epc = (enemy & (1ull << dst)) ? get_piece_at_offset(game, dst) : NONE;
            moves[count++] = fast_encode(src, dst, FRIENDLY_ROOK, epc, FRIENDLY_ROOK, fdata, edata);
        }
    }

    // 6. Queens (both diagonal & orthogonal in one pass)
    u64 queens = f_diag & f_orth;
    while (queens) {
        int src = __builtin_ctzll(queens);
        queens &= queens - 1ull;
        u64 dests = (bishop_attacks_bb(src, occ) | rook_attacks_bb(src, occ)) & target_mask;
        if (pinned & (1ull << src)) {
            dests &= g_line_bb[king_sq][src];
        }
        while (dests) {
            int dst = __builtin_ctzll(dests);
            dests &= dests - 1ull;
            u8 epc = (enemy & (1ull << dst)) ? get_piece_at_offset(game, dst) : NONE;
            moves[count++]
                = fast_encode(src, dst, FRIENDLY_QUEEN, epc, FRIENDLY_QUEEN, fdata, edata);
        }
    }

    // 7. Castling (quiet only, never in check)
    if (mode != 1 && num_checkers == 0 && king_sq == 4) {
        if ((game->pawns & FRIENDLY & QUEENSIDE) && (get_piece_at_offset(game, 0) == FRIENDLY_ROOK)
            && !(occ & ((1ull << 1) | (1ull << 2) | (1ull << 3)))
            && !attacked_with_occ(game, 3, occ) && !attacked_with_occ(game, 2, occ)) {
            moves[count++] = build_move(game, 4, 2, false, false, true, FRIENDLY_KING);
        }
        if ((game->pawns & FRIENDLY & KINGSIDE) && (get_piece_at_offset(game, 7) == FRIENDLY_ROOK)
            && !(occ & ((1ull << 5) | (1ull << 6))) && !attacked_with_occ(game, 5, occ)
            && !attacked_with_occ(game, 6, occ)) {
            moves[count++] = build_move(game, 4, 6, false, true, false, FRIENDLY_KING);
        }
    }

    moves[count] = NULL_MOVE;
    return count;
}

int set_legal_moves(chess *game, u64 *legal_moves) {
    return generate_legal_internal(game, legal_moves, 0);
}

int set_tactical_moves(chess *game, u64 *tactical_moves) {
    return generate_legal_internal(game, tactical_moves, 1);
}

int set_quiet_moves(chess *game, u64 *quiet_moves) {
    return generate_legal_internal(game, quiet_moves, 2);
}

// Fast O(1) validation of a packed 16-bit move tag without generating any other moves!
// Tag layout (matching engine_internal.h::pack_tt_move):
//   bits 0..5   = src (0..63)
//   bits 6..11  = dst (0..63)
//   bits 12..14 = promo piece_type_idx (0=P, 1=N, 2=B, 3=R, 4=Q, 5=K)
u64 validate_packed_move(chess *game, u16 tag) {
    if (tag == 0u) {
        return NULL_MOVE;
    }
    int src = (int) (tag & 63u);
    int dst = (int) ((tag >> 6) & 63u);
    int pt_out = (int) ((tag >> 12) & 7u);
    if (src == dst || pt_out > 5) {
        return NULL_MOVE;
    }

    u64 src_bb = 1ull << src;
    u64 dst_bb = 1ull << dst;
    u64 friendly = game->friendly_pieces;
    if (!(friendly & src_bb) || (friendly & dst_bb)) {
        return NULL_MOVE;
    }

    static const u8 pt_to_friendly[6] = { FRIENDLY_PAWN, FRIENDLY_KNIGHT, FRIENDLY_BISHOP,
        FRIENDLY_ROOK, FRIENDLY_QUEEN, FRIENDLY_KING };
    u8 sp = get_piece_at_offset(game, src);
    u8 pr = pt_to_friendly[pt_out];
    u64 enemy = game->enemy_pieces;
    u64 occ = friendly | enemy;
    bool is_ep = false;
    bool castle_k = false;
    bool castle_q = false;

    if (sp == FRIENDLY_PAWN) {
        if (dst >= 56) {
            if (pr != FRIENDLY_QUEEN && pr != FRIENDLY_ROOK && pr != FRIENDLY_BISHOP
                && pr != FRIENDLY_KNIGHT) {
                return NULL_MOVE;
            }
        } else if (pr != FRIENDLY_PAWN) {
            return NULL_MOVE;
        }
        int diff = dst - src;
        if (diff == 8) {
            if (occ & dst_bb) {
                return NULL_MOVE;
            }
        } else if (diff == 16) {
            if (src < 8 || src >= 16 || (occ & (dst_bb | (1ull << (src + 8))))) {
                return NULL_MOVE;
            }
        } else if ((diff == 7 && (src & 7) > 0) || (diff == 9 && (src & 7) < 7)) {
            if (!(enemy & dst_bb)) {
                if (has_en_passant(game) && dst == get_en_passant_offset(game) && src >= 32
                    && src < 40) {
                    is_ep = true;
                } else {
                    return NULL_MOVE;
                }
            }
        } else {
            return NULL_MOVE;
        }
    } else {
        if (pr != sp) {
            return NULL_MOVE;
        }
        if (sp == FRIENDLY_KNIGHT) {
            if (!(g_knight_attacks[src] & dst_bb)) {
                return NULL_MOVE;
            }
        } else if (sp == FRIENDLY_BISHOP) {
            if (!(bishop_attacks_bb(src, occ) & dst_bb)) {
                return NULL_MOVE;
            }
        } else if (sp == FRIENDLY_ROOK) {
            if (!(rook_attacks_bb(src, occ) & dst_bb)) {
                return NULL_MOVE;
            }
        } else if (sp == FRIENDLY_QUEEN) {
            if (!((bishop_attacks_bb(src, occ) | rook_attacks_bb(src, occ)) & dst_bb)) {
                return NULL_MOVE;
            }
        } else if (sp == FRIENDLY_KING) {
            if (g_king_attacks[src] & dst_bb) {
                // normal king step
            } else if (src == 4 && dst == 6 && (game->pawns & FRIENDLY & KINGSIDE)
                       && get_piece_at_offset(game, 7) == FRIENDLY_ROOK
                       && !(occ & ((1ull << 5) | (1ull << 6)))) {
                castle_k = true;
            } else if (src == 4 && dst == 2 && (game->pawns & FRIENDLY & QUEENSIDE)
                       && get_piece_at_offset(game, 0) == FRIENDLY_ROOK
                       && !(occ & ((1ull << 1) | (1ull << 2) | (1ull << 3)))) {
                castle_q = true;
            } else {
                return NULL_MOVE;
            }
        } else {
            return NULL_MOVE;
        }
    }

    u64 m;
    if (is_ep || castle_k || castle_q) {
        m = build_move(game, src, dst, is_ep, castle_k, castle_q, pr);
    } else {
        u8 fdata = (u8) (game->pawns & 0xffull);
        u8 edata = (u8) ((game->pawns >> 56) & 0xffull);
        u8 epc = (enemy & dst_bb) ? get_piece_at_offset(game, dst) : NONE;
        m = fast_encode(src, dst, sp, epc, pr, fdata, edata);
    }
    return is_legal_move(game, m) ? m : NULL_MOVE;
}

// ============================================================================
// Public Pseudo-Legal / Legacy Helper Functions (Magic-Accelerated)
// ============================================================================

static int add_pawn_move_with_promotions(
    const chess *game, u64 *legal_moves, int count, int start_index, int end_index) {
    count = emit_pawn_move(game, legal_moves, count, start_index, end_index, false);
    legal_moves[count] = NULL_MOVE;
    return count;
}

int set_pseudo_legal_moves(const chess *game, u64 *legal_moves) {
    attacks_init();
    int count = 0;
    legal_moves[0] = NULL_MOVE;
    count = add_single_pawn_pushes(game, legal_moves, count);
    count = add_double_pawn_pushes(game, legal_moves, count);
    count = add_right_pawn_captures(game, legal_moves, count);
    count = add_left_pawn_captures(game, legal_moves, count);
    count = add_en_passant_capture(game, legal_moves, count);
    count = add_knight_moves(game, legal_moves, count);
    count = add_diagonal_moves(game, legal_moves, count);
    count = add_orthogonal_moves(game, legal_moves, count);
    count = add_king_moves(game, legal_moves, count);
    count = add_castling_moves(game, legal_moves, count);
    return count;
}

bool is_legal_move(chess *game, u64 focus_move) {
    attacks_init();
    if (is_castle_king_move(focus_move)) {
        if (attacked_offset(game, 4) || attacked_offset(game, 5) || attacked_offset(game, 6)) {
            return false;
        }
    } else if (is_castle_queen_move(focus_move)) {
        if (attacked_offset(game, 4) || attacked_offset(game, 3) || attacked_offset(game, 2)) {
            return false;
        }
    }

    chess_make_move(game, focus_move);
    int moved_king_offset = get_next_offset(game->kings & game->enemy_pieces, -1);
    bool legal = (moved_king_offset < 64) && !attacking_offset(game, moved_king_offset);
    chess_unmake_move(game, focus_move);
    return legal;
}

int trim_legal_moves(chess *game, u64 *legal_moves) {
    int index = 0;
    int legal_index = 0;
    while (legal_moves[index] != NULL_MOVE) {
        if (is_legal_move(game, legal_moves[index])) {
            legal_moves[legal_index++] = legal_moves[index];
        }
        index++;
    }
    legal_moves[legal_index] = NULL_MOVE;
    return legal_index;
}

int add_en_passant_capture(const chess *game, u64 *legal_moves, int legal_moves_count) {
    if (!has_en_passant(game)) {
        return legal_moves_count;
    }
    int ep_target = get_en_passant_offset(game);
    int ep_file = ep_target - 40;
    u64 friendly_pawns = game->friendly_pieces & (game->pawns & PAWN_MASK);
    if (ep_file < 7 && ((1ull << (32 + ep_file + 1)) & friendly_pawns)) {
        legal_moves[legal_moves_count++]
            = build_move(game, 32 + ep_file + 1, ep_target, true, false, false, FRIENDLY_PAWN);
        legal_moves[legal_moves_count] = NULL_MOVE;
    }
    if (ep_file > 0 && ((1ull << (32 + ep_file - 1)) & friendly_pawns)) {
        legal_moves[legal_moves_count++]
            = build_move(game, 32 + ep_file - 1, ep_target, true, false, false, FRIENDLY_PAWN);
        legal_moves[legal_moves_count] = NULL_MOVE;
    }
    return legal_moves_count;
}

int add_castling_moves(const chess *game, u64 *legal_moves, int legal_moves_count) {
    u64 occupied = game->friendly_pieces | game->enemy_pieces;
    if ((game->pawns & FRIENDLY & QUEENSIDE) && (get_piece_at_offset(game, 4) == FRIENDLY_KING)
        && (get_piece_at_offset(game, 0) == FRIENDLY_ROOK)
        && !(occupied & ((1ull << 1) | (1ull << 2) | (1ull << 3)))) {
        legal_moves[legal_moves_count++]
            = build_move(game, 4, 2, false, false, true, FRIENDLY_KING);
        legal_moves[legal_moves_count] = NULL_MOVE;
    }
    if ((game->pawns & FRIENDLY & KINGSIDE) && (get_piece_at_offset(game, 4) == FRIENDLY_KING)
        && (get_piece_at_offset(game, 7) == FRIENDLY_ROOK)
        && !(occupied & ((1ull << 5) | (1ull << 6)))) {
        legal_moves[legal_moves_count++]
            = build_move(game, 4, 6, false, true, false, FRIENDLY_KING);
        legal_moves[legal_moves_count] = NULL_MOVE;
    }
    return legal_moves_count;
}

int add_double_pawn_pushes(const chess *game, u64 *legal_moves, int legal_moves_count) {
    u64 empty = ~(game->friendly_pieces | game->enemy_pieces);
    u64 pawns = game->friendly_pieces & (game->pawns & ROW_2);
    u64 step1 = empty & (pawns << 8);
    u64 step2 = empty & (step1 << 8);
    for (int end_index = -1; (end_index = get_next_offset(step2, end_index)) < 64;) {
        legal_moves[legal_moves_count++]
            = build_move(game, end_index - 16, end_index, false, false, false, FRIENDLY_PAWN);
        legal_moves[legal_moves_count] = NULL_MOVE;
    }
    return legal_moves_count;
}

int add_single_pawn_pushes(const chess *game, u64 *legal_moves, int legal_moves_count) {
    u64 empty = ~(game->friendly_pieces | game->enemy_pieces);
    u64 pawns = game->friendly_pieces & (game->pawns & PAWN_MASK);
    u64 output_squares = empty & (pawns << 8);
    for (int end_index = -1; (end_index = get_next_offset(output_squares, end_index)) < 64;) {
        legal_moves_count = add_pawn_move_with_promotions(
            game, legal_moves, legal_moves_count, end_index - 8, end_index);
    }
    return legal_moves_count;
}

int add_left_pawn_captures(const chess *game, u64 *legal_moves, int legal_moves_count) {
    u64 pawns = game->friendly_pieces & (game->pawns & PAWN_MASK);
    u64 output_squares = game->enemy_pieces & compass(pawns, -1, 1);
    for (int end_index = -1; (end_index = get_next_offset(output_squares, end_index)) < 64;) {
        legal_moves_count = add_pawn_move_with_promotions(
            game, legal_moves, legal_moves_count, end_index - 7, end_index);
    }
    return legal_moves_count;
}

int add_right_pawn_captures(const chess *game, u64 *legal_moves, int legal_moves_count) {
    u64 pawns = game->friendly_pieces & (game->pawns & PAWN_MASK);
    u64 output_squares = game->enemy_pieces & compass(pawns, 1, 1);
    for (int end_index = -1; (end_index = get_next_offset(output_squares, end_index)) < 64;) {
        legal_moves_count = add_pawn_move_with_promotions(
            game, legal_moves, legal_moves_count, end_index - 9, end_index);
    }
    return legal_moves_count;
}

u64 get_king_destination_bitboard(const chess *game, int king_index) {
    attacks_init();
    return g_king_attacks[king_index] & (~game->friendly_pieces);
}

int add_king_moves(const chess *game, u64 *legal_moves, int legal_moves_count) {
    int king_index = get_next_offset(game->friendly_pieces & game->kings, -1);
    if (king_index >= 64) {
        return legal_moves_count;
    }
    u64 output_squares = get_king_destination_bitboard(game, king_index);
    for (int end_index = -1; (end_index = get_next_offset(output_squares, end_index)) < 64;) {
        legal_moves[legal_moves_count++]
            = build_move(game, king_index, end_index, false, false, false, FRIENDLY_KING);
        legal_moves[legal_moves_count] = NULL_MOVE;
    }
    return legal_moves_count;
}

u64 get_diagonal_destination_bitboard(const chess *game, int start_index) {
    attacks_init();
    u64 occupied = game->friendly_pieces | game->enemy_pieces;
    return bishop_attacks_bb(start_index, occupied) & (~game->friendly_pieces);
}

int add_diagonal_moves(const chess *game, u64 *legal_moves, int legal_moves_count) {
    u64 diagonals = game->friendly_pieces & game->diagonal_pieces;
    for (int start_index = -1; (start_index = get_next_offset(diagonals, start_index)) < 64;) {
        u64 output_squares = get_diagonal_destination_bitboard(game, start_index);
        for (int end_index = -1; (end_index = get_next_offset(output_squares, end_index)) < 64;) {
            legal_moves[legal_moves_count++]
                = build_move(game, start_index, end_index, false, false, false, NONE);
            legal_moves[legal_moves_count] = NULL_MOVE;
        }
    }
    return legal_moves_count;
}

u64 get_orthogonal_destination_bitboard(const chess *game, int start_index) {
    attacks_init();
    u64 occupied = game->friendly_pieces | game->enemy_pieces;
    return rook_attacks_bb(start_index, occupied) & (~game->friendly_pieces);
}

int add_orthogonal_moves(const chess *game, u64 *legal_moves, int legal_moves_count) {
    u64 orthogonals = game->friendly_pieces & game->orthogonal_pieces;
    for (int start_index = -1; (start_index = get_next_offset(orthogonals, start_index)) < 64;) {
        u64 output_squares = get_orthogonal_destination_bitboard(game, start_index);
        for (int end_index = -1; (end_index = get_next_offset(output_squares, end_index)) < 64;) {
            legal_moves[legal_moves_count++]
                = build_move(game, start_index, end_index, false, false, false, NONE);
            legal_moves[legal_moves_count] = NULL_MOVE;
        }
    }
    return legal_moves_count;
}

u64 get_knight_destination_bitboard(const chess *game, int start_index) {
    attacks_init();
    return g_knight_attacks[start_index] & (~game->friendly_pieces);
}

int add_knight_moves(const chess *game, u64 *legal_moves, int legal_moves_count) {
    u64 knights = game->friendly_pieces & (~(game->pawns & PAWN_MASK)) & (~game->orthogonal_pieces)
                  & (~game->diagonal_pieces) & (~game->kings);
    for (int start_index = -1; (start_index = get_next_offset(knights, start_index)) < 64;) {
        u64 output_squares = get_knight_destination_bitboard(game, start_index);
        for (int end_index = -1; (end_index = get_next_offset(output_squares, end_index)) < 64;) {
            legal_moves[legal_moves_count++]
                = build_move(game, start_index, end_index, false, false, false, FRIENDLY_KNIGHT);
            legal_moves[legal_moves_count] = NULL_MOVE;
        }
    }
    return legal_moves_count;
}

bool in_set(const u64 *legal_moves, u64 focus_move) {
    for (int i = 0; legal_moves[i] != NULL_MOVE; i++) {
        if (legal_moves[i] == focus_move) {
            return true;
        }
    }
    return false;
}

bool attacked_offset(const chess *game, int focus_offset) {
    attacks_init();
    return attacked_with_occ(game, focus_offset, game->friendly_pieces | game->enemy_pieces);
}

bool attacking_offset(const chess *game, int focus_offset) {
    attacks_init();
    u64 friendly = game->friendly_pieces;
    u64 occ = friendly | game->enemy_pieces;
    u64 true_pawns = game->pawns & PAWN_MASK;

    if (friendly & true_pawns & g_pawn_attacks[1][focus_offset]) {
        return true;
    }
    u64 friendly_knights = friendly & (~true_pawns) & (~game->orthogonal_pieces)
                           & (~game->diagonal_pieces) & (~game->kings);
    if (friendly_knights & g_knight_attacks[focus_offset]) {
        return true;
    }
    if (friendly & game->kings & g_king_attacks[focus_offset]) {
        return true;
    }
    if ((friendly & game->diagonal_pieces) & bishop_attacks_bb(focus_offset, occ)) {
        return true;
    }
    if ((friendly & game->orthogonal_pieces) & rook_attacks_bb(focus_offset, occ)) {
        return true;
    }
    return false;
}

u64 get_attacking_board(const chess *game) {
    attacks_init();
    u64 output = 0ull;
    for (int i = 0; i < 64; i++) {
        if (attacking_offset(game, i)) {
            output |= 1ull << i;
        }
    }
    return output;
}

u64 get_attacked_board(const chess *game) {
    attacks_init();
    u64 output = 0ull;
    for (int i = 0; i < 64; i++) {
        if (attacked_offset(game, i)) {
            output |= 1ull << i;
        }
    }
    return output;
}

bool in_check(const chess *game) {
    return (game->pawns & FRIENDLY & IN_CHECK) != 0;
}
