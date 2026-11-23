#include "engine/see.h"

#include "core/attacks.h"
#include "core/board.h"
#include "engine/engine_internal.h"

int engine_see(const chess *b, u64 move) {
    engine_global_init();
    int from = get_start_offset(move);
    int to = get_end_offset(move);
    bool is_ep = is_en_passant_move(move);
    u8 sp = get_start_piece(move);
    u8 promo = get_promotion_piece(move);
    u8 cap = get_end_piece(move);

    int sp_pt = piece_type_idx(sp);
    int pr_pt = piece_type_idx(promo);
    if (sp_pt < 0 || pr_pt < 0) {
        return 0;
    }

    int cap_val = is_ep ? g_see_value[0] : (cap != NONE ? g_see_value[piece_type_idx(cap)] : 0);
    int promo_bonus = (pr_pt != sp_pt) ? (g_see_value[pr_pt] - g_see_value[0]) : 0;

    int gain[32];
    gain[0] = cap_val + promo_bonus;

    u64 occ = (b->friendly_pieces | b->enemy_pieces) ^ (1ull << from);
    if (is_ep) {
        occ &= ~(1ull << (to - 8));
    }

    u64 pawns_bb = b->pawns & PAWN_MASK;
    u64 orth_bb = b->orthogonal_pieces;
    u64 diag_bb = b->diagonal_pieces;
    u64 kings_bb = b->kings;
    u64 knights_bb
        = (b->friendly_pieces | b->enemy_pieces) & ~(pawns_bb | orth_bb | diag_bb | kings_bb);

    u64 side_bb[2] = { b->friendly_pieces, b->enemy_pieces };
    u64 f_pawns = side_bb[0] & pawns_bb;
    u64 e_pawns = side_bb[1] & pawns_bb;
    u64 bishops_only = diag_bb & ~orth_bb;
    u64 rooks_only = orth_bb & ~diag_bb;
    u64 queens_only = orth_bb & diag_bb;

    int cur_val = g_see_value[pr_pt];
    int side = 1;
    int d = 1;

    while (d < 32) {
        u64 stm = side_bb[side] & occ;
        u64 att = 0ull;
        int att_val = 0;

        u64 p_att = (side == 0) ? (g_pawn_attacks[1][to] & f_pawns & occ)
                                : (g_pawn_attacks[0][to] & e_pawns & occ);
        if (p_att) {
            att = p_att;
            att_val = g_see_value[0];
        } else if ((att = g_knight_attacks[to] & knights_bb & stm) != 0ull) {
            att_val = g_see_value[1];
        } else {
            u64 diag_att = bishop_attacks_bb(to, occ);
            if ((att = diag_att & bishops_only & stm) != 0ull) {
                att_val = g_see_value[2];
            } else {
                u64 orth_att = rook_attacks_bb(to, occ);
                if ((att = orth_att & rooks_only & stm) != 0ull) {
                    att_val = g_see_value[3];
                } else if ((att = (diag_att | orth_att) & queens_only & stm) != 0ull) {
                    att_val = g_see_value[4];
                } else if ((att = g_king_attacks[to] & kings_bb & stm) != 0ull) {
                    att_val = g_see_value[5];
                } else {
                    break;
                }
            }
        }

        u64 lsb = att & (~att + 1ull);
        occ ^= lsb;
        gain[d] = cur_val - gain[d - 1];
        cur_val = att_val;
        side ^= 1;
        d++;
        if (att_val == g_see_value[5]) {
            u64 opp = side_bb[side] & occ;
            u64 opp_p = (side == 0) ? (g_pawn_attacks[1][to] & f_pawns & occ)
                                    : (g_pawn_attacks[0][to] & e_pawns & occ);
            bool opp_attacks = (opp_p != 0ull)
                               || ((g_knight_attacks[to] & knights_bb & opp) != 0ull)
                               || ((g_king_attacks[to] & kings_bb & opp) != 0ull)
                               || ((bishop_attacks_bb(to, occ) & diag_bb & opp) != 0ull)
                               || ((rook_attacks_bb(to, occ) & orth_bb & opp) != 0ull);
            if (opp_attacks && d < 32) {
                gain[d] = g_see_value[5] - gain[d - 1];
                d++;
            }
            break;
        }
    }

    while (--d > 0) {
        int neg_prev = -gain[d - 1];
        gain[d - 1] = -(neg_prev > gain[d] ? neg_prev : gain[d]);
    }
    return gain[0];
}
