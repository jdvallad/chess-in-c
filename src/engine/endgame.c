#include "engine/endgame.h"

#include "core/attacks.h"
#include "core/board.h"
#include "engine/engine_internal.h"

#include <stdlib.h>
#include <string.h>

// =========================================================================
// Exact Retrograde-Solved KPK Bitbase (196,608 states = 24 pawn sq * 64 WK * 64 BK * 2 stm)
// Pawn normalized to files a..d (0..3), ranks 2..7 (1..6) -> 24 pawn indices.
// =========================================================================

#define KPK_UNKNOWN 0u
#define KPK_INVALID 1u
#define KPK_DRAW    2u
#define KPK_WIN     3u

#define KPK_TOTAL_STATES (24 * 64 * 64 * 2)

static u8 g_kpk_table[KPK_TOTAL_STATES];
static bool g_kpk_ready = false;

static inline int kpk_index(int stm, int bk, int wk, int psq) {
    // psq: rank 1..6, file 0..3 -> p_idx = (rank - 1) * 4 + file (0..23)
    int p_idx = ((psq >> 3) - 1) * 4 + (psq & 7);
    return (((p_idx * 64 + wk) * 64 + bk) * 2) + stm;
}

static inline bool kings_adjacent(int sq1, int sq2) {
    int df = abs((sq1 & 7) - (sq2 & 7));
    int dr = abs((sq1 >> 3) - (sq2 >> 3));
    return df <= 1 && dr <= 1;
}

static inline bool pawn_attacks_sq(int psq, int target) {
    return (g_pawn_attacks[0][psq] & (1ull << target)) != 0ull;
}

void endgame_init(void) {
    if (g_kpk_ready) {
        return;
    }
    attacks_init();
    memset(g_kpk_table, KPK_UNKNOWN, sizeof(g_kpk_table));

    // Step 1: Mark invalid and immediate terminal positions
    for (int pr = 1; pr <= 6; pr++) {
        for (int pf = 0; pf < 4; pf++) {
            int psq = pr * 8 + pf;
            for (int wk = 0; wk < 64; wk++) {
                for (int bk = 0; bk < 64; bk++) {
                    for (int stm = 0; stm < 2; stm++) {
                        int idx = kpk_index(stm, bk, wk, psq);
                        if (wk == bk || wk == psq || bk == psq || kings_adjacent(wk, bk)) {
                            g_kpk_table[idx] = KPK_INVALID;
                            continue;
                        }
                        // If White to move and Black king is already in check from pawn -> illegal
                        if (stm == 0 && pawn_attacks_sq(psq, bk)) {
                            g_kpk_table[idx] = KPK_INVALID;
                            continue;
                        }
                        if (stm == 0) {
                            // White to move: immediate win if pawn on rank 7 can promote safely
                            if (pr == 6) {
                                int promo_sq = psq + 8;
                                if (promo_sq != wk && promo_sq != bk
                                    && (!kings_adjacent(promo_sq, bk)
                                        || kings_adjacent(promo_sq, wk))) {
                                    g_kpk_table[idx] = KPK_WIN;
                                }
                            }
                        } else {
                            // Black to move: stalemate OR Black can safely capture the pawn -> DRAW
                            u64 bmoves = g_king_attacks[bk] & ~(g_king_attacks[wk] | (1ull << wk))
                                         & ~g_pawn_attacks[0][psq];
                            if (bmoves == 0ull) {
                                // Note: if BK is in check from pawn and has no moves, pawn can be
                                // captured if undefended (included in bmoves!), otherwise checkmate!
                                g_kpk_table[idx] = pawn_attacks_sq(psq, bk) ? KPK_WIN : KPK_DRAW;
                            } else if (bmoves & (1ull << psq)) {
                                // Black captures pawn cleanly -> K vs K draw!
                                g_kpk_table[idx] = KPK_DRAW;
                            }
                        }
                    }
                }
            }
        }
    }

    // Step 2: Retrograde fixed-point classification until convergence
    bool changed = true;
    while (changed) {
        changed = false;
        for (int pr = 6; pr >= 1; pr--) {
            for (int pf = 0; pf < 4; pf++) {
                int psq = pr * 8 + pf;
                for (int wk = 0; wk < 64; wk++) {
                    for (int bk = 0; bk < 64; bk++) {
                        for (int stm = 0; stm < 2; stm++) {
                            int idx = kpk_index(stm, bk, wk, psq);
                            if (g_kpk_table[idx] != KPK_UNKNOWN) {
                                continue;
                            }

                            if (stm == 0) {
                                // White to move: WIN if any move -> WIN; DRAW if all moves -> DRAW
                                bool all_draw = true;
                                bool found_win = false;

                                // 1. White king moves
                                u64 wmoves = g_king_attacks[wk]
                                             & ~(g_king_attacks[bk] | (1ull << bk))
                                             & ~(1ull << psq);
                                while (wmoves) {
                                    int nwk = __builtin_ctzll(wmoves);
                                    wmoves &= wmoves - 1ull;
                                    u8 child = g_kpk_table[kpk_index(1, bk, nwk, psq)];
                                    if (child == KPK_WIN) {
                                        found_win = true;
                                        break;
                                    }
                                    if (child != KPK_DRAW) {
                                        all_draw = false;
                                    }
                                }

                                // 2. Pawn single push (and double push from rank 2)
                                if (!found_win && pr < 6) {
                                    int np = psq + 8;
                                    if (np != wk && np != bk) {
                                        u8 child = g_kpk_table[kpk_index(1, bk, wk, np)];
                                        if (child == KPK_WIN) {
                                            found_win = true;
                                        } else if (child != KPK_DRAW) {
                                            all_draw = false;
                                        }
                                        if (!found_win && pr == 1) {
                                            int np2 = psq + 16;
                                            if (np2 != wk && np2 != bk) {
                                                u8 c2 = g_kpk_table[kpk_index(1, bk, wk, np2)];
                                                if (c2 == KPK_WIN) {
                                                    found_win = true;
                                                } else if (c2 != KPK_DRAW) {
                                                    all_draw = false;
                                                }
                                            }
                                        }
                                    }
                                }

                                if (found_win) {
                                    g_kpk_table[idx] = KPK_WIN;
                                    changed = true;
                                } else if (all_draw) {
                                    g_kpk_table[idx] = KPK_DRAW;
                                    changed = true;
                                }
                            } else {
                                // Black to move: DRAW if any move -> DRAW; WIN if all moves -> WIN
                                bool all_win = true;
                                bool found_draw = false;

                                u64 bmoves = g_king_attacks[bk]
                                             & ~(g_king_attacks[wk] | (1ull << wk))
                                             & ~g_pawn_attacks[0][psq];
                                while (bmoves) {
                                    int nbk = __builtin_ctzll(bmoves);
                                    bmoves &= bmoves - 1ull;
                                    u8 child = (nbk == psq)
                                                   ? KPK_DRAW
                                                   : g_kpk_table[kpk_index(0, nbk, wk, psq)];
                                    if (child == KPK_DRAW) {
                                        found_draw = true;
                                        break;
                                    }
                                    if (child != KPK_WIN) {
                                        all_win = false;
                                    }
                                }

                                if (found_draw) {
                                    g_kpk_table[idx] = KPK_DRAW;
                                    changed = true;
                                } else if (all_win) {
                                    g_kpk_table[idx] = KPK_WIN;
                                    changed = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Any remaining unknown states are perpetual/cycle draws
    for (int i = 0; i < KPK_TOTAL_STATES; i++) {
        if (g_kpk_table[i] == KPK_UNKNOWN) {
            g_kpk_table[i] = KPK_DRAW;
        }
    }
    g_kpk_ready = true;
}

bool endgame_probe_kpk(const chess *b, bool *pawn_holder_wins) {
    u64 occ = b->friendly_pieces | b->enemy_pieces;
    if (__builtin_popcountll(occ) != 3) {
        return false;
    }
    u64 pawns = b->pawns & PAWN_MASK;
    if (pawns == 0ull) {
        return false;
    }

    bool friendly_has_pawn = (b->friendly_pieces & pawns) != 0ull;
    int f_ksq = __builtin_ctzll(b->friendly_pieces & b->kings);
    int e_ksq = __builtin_ctzll(b->enemy_pieces & b->kings);
    int psq = __builtin_ctzll(pawns);

    int wk, bk, stm;
    if (friendly_has_pawn) {
        wk = f_ksq;
        bk = e_ksq;
        stm = 0; // pawn holder to move
    } else {
        // Flip vertically so enemy pawn becomes upward-moving White pawn
        wk = e_ksq ^ 56;
        bk = f_ksq ^ 56;
        psq ^= 56;
        stm = 1; // defender to move
    }

    // Mirror horizontally if pawn is on files e..h (4..7)
    if ((psq & 7) >= 4) {
        wk ^= 7;
        bk ^= 7;
        psq ^= 7;
    }

    int pr = psq >> 3;
    if (pr < 1 || pr > 6) {
        return false;
    }
    u8 res = g_kpk_table[kpk_index(stm, bk, wk, psq)];
    *pawn_holder_wins = (res == KPK_WIN);
    return true;
}

// =========================================================================
// Specialized Endgame Recognizers
// =========================================================================

// Corner distance for KBN vs K: drives losing king to corners matching bishop color.
static int kbnk_corner_dist(int sq, bool bishop_on_light) {
    int f = sq & 7;
    int r = sq >> 3;
    // Light corners: a8 (f=0, r=7) and h1 (f=7, r=0) -> min(|f-0|+|r-7|, |f-7|+|r-0|)
    // Dark corners:  a1 (f=0, r=0) and h8 (f=7, r=7) -> min(f+r, (7-f)+(7-r))
    if (bishop_on_light) {
        int d1 = f + (7 - r);
        int d2 = (7 - f) + r;
        return (d1 < d2) ? d1 : d2;
    }
    int d1 = f + r;
    int d2 = (7 - f) + (7 - r);
    return (d1 < d2) ? d1 : d2;
}

bool endgame_eval(const chess *b, int *score_out) {
    u64 friendly = b->friendly_pieces;
    u64 enemy = b->enemy_pieces;
    u64 occ = friendly | enemy;
    int total_pieces = __builtin_popcountll(occ);

    // 1. Exact KPK Bitbase (3 pieces, 1 pawn)
    if (total_pieces == 3) {
        bool wins = false;
        if (endgame_probe_kpk(b, &wins)) {
            if (!wins) {
                *score_out = 0;
                return true;
            }
            u64 pawns = b->pawns & PAWN_MASK;
            bool f_has = (friendly & pawns) != 0ull;
            int psq = __builtin_ctzll(pawns);
            int f_ksq = __builtin_ctzll(friendly & b->kings);
            int e_ksq = __builtin_ctzll(enemy & b->kings);
            int rel_rank = f_has ? (psq >> 3) : (7 - (psq >> 3));
            int win_k = f_has ? f_ksq : e_ksq;
            int promo_sq = f_has ? (56 + (psq & 7)) : (psq & 7);
            int dist_k = abs((win_k & 7) - (promo_sq & 7)) + abs((win_k >> 3) - (promo_sq >> 3));
            int sc = 550 + rel_rank * 45 + (14 - dist_k) * 8;
            *score_out = f_has ? sc : -sc;
            return true;
        }
    }

    // Only check remaining special endgames when total pieces <= 6 and no queens/rooks
    if (total_pieces <= 6 && b->orthogonal_pieces == 0ull) {
        u64 true_pawns = b->pawns & PAWN_MASK;
        u64 f_pawns = friendly & true_pawns;
        u64 e_pawns = enemy & true_pawns;
        u64 f_bishops = friendly & b->diagonal_pieces;
        u64 e_bishops = enemy & b->diagonal_pieces;
        u64 f_knights = friendly & ~(true_pawns | b->diagonal_pieces | b->kings);
        u64 e_knights = enemy & ~(true_pawns | b->diagonal_pieces | b->kings);
        int f_ksq = __builtin_ctzll(friendly & b->kings);
        int e_ksq = __builtin_ctzll(enemy & b->kings);

        // 2. K+N+N vs K (bare king, zero pawns): forced checkmate is impossible -> draw!
        if (total_pieces == 4 && true_pawns == 0ull && b->diagonal_pieces == 0ull) {
            if ((__builtin_popcountll(f_knights) == 2 && e_knights == 0ull)
                || (__builtin_popcountll(e_knights) == 2 && f_knights == 0ull)) {
                *score_out = 0;
                return true;
            }
        }

        // 3. K+B+N vs K: drive losing king specifically to the bishop's corner color!
        if (total_pieces == 4 && true_pawns == 0ull) {
            if (f_bishops && f_knights && e_bishops == 0ull && e_knights == 0ull) {
                bool light = (f_bishops & LIGHT_SQUARES) != 0ull;
                int cdist = kbnk_corner_dist(e_ksq, light);
                int kdist = abs((f_ksq & 7) - (e_ksq & 7)) + abs((f_ksq >> 3) - (e_ksq >> 3));
                *score_out = 650 + (7 - cdist) * 35 + (14 - kdist) * 10;
                return true;
            }
            if (e_bishops && e_knights && f_bishops == 0ull && f_knights == 0ull) {
                bool light = (e_bishops & LIGHT_SQUARES) != 0ull;
                int cdist = kbnk_corner_dist(f_ksq, light);
                int kdist = abs((f_ksq & 7) - (e_ksq & 7)) + abs((f_ksq >> 3) - (e_ksq >> 3));
                *score_out = -(650 + (7 - cdist) * 35 + (14 - kdist) * 10);
                return true;
            }
        }

        // 4. Wrong-Color Bishop + Rook-Pawn(s) vs Bare King fortress draw!
        if (f_knights == 0ull && e_knights == 0ull) {
            if (f_bishops && e_bishops == 0ull && e_pawns == 0ull && f_pawns != 0ull) {
                bool all_a = (f_pawns & ~COL_1) == 0ull;
                bool all_h = (f_pawns & ~COL_8) == 0ull;
                if (all_a || all_h) {
                    int corner = all_a ? 56 : 63; // a8 (light) or h8 (dark)
                    bool corner_light = all_a;
                    bool b_light = (f_bishops & LIGHT_SQUARES) != 0ull;
                    if (b_light != corner_light
                        && (e_ksq == corner || (g_king_attacks[corner] & (1ull << e_ksq)))) {
                        *score_out = 0;
                        return true;
                    }
                }
            }
            if (e_bishops && f_bishops == 0ull && f_pawns == 0ull && e_pawns != 0ull) {
                bool all_a = (e_pawns & ~COL_1) == 0ull;
                bool all_h = (e_pawns & ~COL_8) == 0ull;
                if (all_a || all_h) {
                    int corner = all_a ? 0 : 7; // a1 (dark) or h1 (light)
                    bool corner_light = all_h;
                    bool b_light = (e_bishops & LIGHT_SQUARES) != 0ull;
                    if (b_light != corner_light
                        && (f_ksq == corner || (g_king_attacks[corner] & (1ull << f_ksq)))) {
                        *score_out = 0;
                        return true;
                    }
                }
            }
        }
    }

    // 5. Bare-King Mop-Up (KRK, KQK, KBBK, KRRK, etc.): deterministic cornering even in Pure NNUE
    bool e_bare = (enemy & ~b->kings) == 0ull;
    bool f_bare = (friendly & ~b->kings) == 0ull;
    if (e_bare != f_bare) {
        u64 win_side = e_bare ? friendly : enemy;
        u64 win_ortho = win_side & b->orthogonal_pieces;
        u64 win_diag = win_side & b->diagonal_pieces;
        u64 win_q = win_ortho & win_diag;
        u64 win_r = win_ortho & ~win_diag;
        u64 win_b = win_diag & ~win_ortho;
        if (win_ortho != 0ull || __builtin_popcountll(win_b) >= 2) {
            u64 win_p = win_side & (b->pawns & PAWN_MASK);
            u64 win_n = win_side & ~(win_p | win_ortho | win_diag | b->kings);
            int mat = __builtin_popcountll(win_q) * 900 + __builtin_popcountll(win_r) * 500
                      + __builtin_popcountll(win_b) * 330 + __builtin_popcountll(win_n) * 320
                      + __builtin_popcountll(win_p) * 100;
            int f_ksq = __builtin_ctzll(friendly & b->kings);
            int e_ksq = __builtin_ctzll(enemy & b->kings);
            int lose_k = e_bare ? e_ksq : f_ksq;
            int lf = lose_k & 7;
            int lr = lose_k >> 3;
            int df_c = (lf < 3) ? (3 - lf) : ((lf > 4) ? (lf - 4) : 0);
            int dr_c = (lr < 3) ? (3 - lr) : ((lr > 4) ? (lr - 4) : 0);
            int center_dist = df_c + dr_c;
            int kdist = abs((f_ksq & 7) - (e_ksq & 7)) + abs((f_ksq >> 3) - (e_ksq >> 3));
            int sc = mat + 100 + center_dist * 20 + (14 - kdist) * 8;
            *score_out = e_bare ? sc : -sc;
            return true;
        }
    }

    return false;
}
