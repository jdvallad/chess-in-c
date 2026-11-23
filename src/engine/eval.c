#include "engine/eval.h"

#include "core/attacks.h"
#include "core/board.h"
#include "engine/endgame.h"
#include "engine/engine_internal.h"
#include "engine/nnue.h"

#include <stdlib.h>

// Shared PeSTO material values, file masks, and piece-type lookup table
const int g_mg_value[6] = { 82, 337, 365, 477, 1025, 0 };
const int g_eg_value[6] = { 94, 281, 297, 512, 936, 0 };
const int g_see_value[6] = { 100, 320, 330, 500, 900, 20000 };

const u64 g_file_masks[8] = { COL_1, COL_2, COL_3, COL_4, COL_5, COL_6, COL_7, COL_8 };
const u64 g_adj_file_masks[8] = {
    COL_2,
    COL_1 | COL_3,
    COL_2 | COL_4,
    COL_3 | COL_5,
    COL_4 | COL_6,
    COL_5 | COL_7,
    COL_6 | COL_8,
    COL_7,
};

// Tunable positional evaluation weights (Regularized WDL Texel-tuned on 74,988 self-play positions)
eval_params g_eval_params = {
    40, 46, // bishop_pair mg/eg
    5, 3, // knight_mob mg/eg
    6, 8, // bishop_mob mg/eg
    3, 4, // rook_mob mg/eg
    1, 2, // queen_mob mg/eg
    26, 20, // knight_outpost mg/eg
    14, 13, // rook_open mg/eg
    22, 12, // rook_semi mg/eg
    15, 34, // rook_7th mg/eg
    6, 7, // rook_conn mg/eg
    46, 30, // pawn_threat mg/eg
    16, 34, // doubled mg/eg
    8, 12, // isolated mg/eg
    5, 16, // backward mg/eg
    8, 7, // shield_r2 / shield_r3 mg
    11, 7, // king_open / king_semi mg
    4, // king_att_num (x/10)
    75, // passer_scale_pct
    10, // tempo
};

const int8_t g_pt_lookup[64] = {
    -1,
    -1,
    1,
    0,
    1,
    0,
    -1,
    -1,
    -1,
    -1,
    3,
    -1,
    3,
    -1,
    -1,
    -1,
    -1,
    -1,
    2,
    -1,
    2,
    -1,
    -1,
    -1,
    -1,
    -1,
    4,
    -1,
    4,
    -1,
    -1,
    -1,
    -1,
    -1,
    5,
    -1,
    5,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
};

// PeSTO piece-square tables (top-down: index 0 = a8, index 56 = a1)
static const int mg_pawn_table[64] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    98,
    134,
    61,
    95,
    68,
    126,
    34,
    -11,
    -6,
    7,
    26,
    31,
    65,
    56,
    25,
    -20,
    -14,
    13,
    6,
    21,
    23,
    12,
    17,
    -23,
    -27,
    -2,
    -5,
    12,
    17,
    6,
    10,
    -25,
    -26,
    -4,
    -4,
    -10,
    3,
    3,
    33,
    -12,
    -35,
    -1,
    -20,
    -23,
    -15,
    24,
    38,
    -22,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static const int eg_pawn_table[64] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    178,
    173,
    158,
    134,
    147,
    132,
    165,
    187,
    94,
    100,
    85,
    67,
    56,
    53,
    82,
    84,
    32,
    24,
    13,
    5,
    -2,
    4,
    17,
    17,
    13,
    9,
    -3,
    -7,
    -7,
    -8,
    3,
    -1,
    4,
    7,
    -6,
    1,
    0,
    -5,
    -1,
    -8,
    13,
    8,
    8,
    10,
    13,
    0,
    2,
    -7,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static const int mg_knight_table[64] = {
    -167,
    -89,
    -34,
    -49,
    61,
    -97,
    -15,
    -107,
    -73,
    -41,
    72,
    36,
    23,
    62,
    7,
    -17,
    -47,
    60,
    37,
    65,
    84,
    129,
    73,
    44,
    -9,
    17,
    19,
    53,
    37,
    69,
    18,
    22,
    -13,
    4,
    16,
    13,
    28,
    19,
    21,
    -8,
    -23,
    -9,
    12,
    10,
    19,
    17,
    25,
    -16,
    -29,
    -53,
    -12,
    -3,
    -1,
    18,
    -14,
    -19,
    -105,
    -21,
    -58,
    -33,
    -17,
    -28,
    -19,
    -23,
};

static const int eg_knight_table[64] = {
    -58,
    -38,
    -13,
    -28,
    -31,
    -27,
    -63,
    -99,
    -25,
    -8,
    -25,
    -2,
    -9,
    -25,
    -24,
    -52,
    -24,
    -20,
    10,
    9,
    -1,
    -9,
    -19,
    -41,
    -17,
    3,
    22,
    22,
    22,
    11,
    8,
    -18,
    -18,
    -6,
    16,
    25,
    16,
    17,
    4,
    -18,
    -23,
    -3,
    -1,
    15,
    10,
    -3,
    -20,
    -22,
    -42,
    -20,
    -10,
    -5,
    -2,
    -20,
    -23,
    -44,
    -29,
    -51,
    -23,
    -15,
    -22,
    -18,
    -50,
    -64,
};

static const int mg_bishop_table[64] = {
    -29,
    4,
    -82,
    -37,
    -25,
    -42,
    7,
    -8,
    -26,
    16,
    -18,
    -13,
    30,
    59,
    18,
    -47,
    -16,
    37,
    43,
    40,
    35,
    50,
    37,
    -2,
    -4,
    5,
    19,
    50,
    37,
    37,
    7,
    -2,
    -6,
    13,
    13,
    26,
    34,
    12,
    10,
    4,
    0,
    15,
    15,
    15,
    14,
    27,
    18,
    10,
    4,
    15,
    16,
    0,
    7,
    21,
    33,
    1,
    -33,
    -3,
    -14,
    -21,
    -13,
    -12,
    -39,
    -21,
};

static const int eg_bishop_table[64] = {
    -14,
    -21,
    -11,
    -8,
    -7,
    -9,
    -17,
    -24,
    -8,
    -4,
    7,
    -12,
    -3,
    -13,
    -4,
    -14,
    2,
    -8,
    0,
    -1,
    -2,
    6,
    0,
    4,
    -3,
    9,
    12,
    9,
    14,
    10,
    3,
    2,
    -6,
    3,
    13,
    19,
    7,
    10,
    -3,
    -9,
    -12,
    -3,
    8,
    10,
    13,
    3,
    -7,
    -15,
    -14,
    -18,
    -7,
    -1,
    4,
    -9,
    -15,
    -27,
    -23,
    -9,
    -23,
    -5,
    -9,
    -16,
    -5,
    -17,
};

static const int mg_rook_table[64] = {
    32,
    42,
    32,
    51,
    63,
    9,
    31,
    43,
    27,
    32,
    58,
    62,
    80,
    67,
    26,
    44,
    -5,
    19,
    26,
    36,
    17,
    45,
    61,
    16,
    -24,
    -11,
    7,
    26,
    24,
    35,
    -8,
    -20,
    -36,
    -26,
    -12,
    -1,
    9,
    -7,
    6,
    -23,
    -45,
    -25,
    -16,
    -17,
    3,
    0,
    -5,
    -33,
    -44,
    -16,
    -20,
    -9,
    -1,
    11,
    -6,
    -71,
    -19,
    -13,
    1,
    17,
    16,
    7,
    -37,
    -26,
};

static const int eg_rook_table[64] = {
    13,
    10,
    18,
    15,
    12,
    12,
    8,
    5,
    11,
    13,
    13,
    11,
    -3,
    3,
    8,
    3,
    7,
    7,
    7,
    5,
    4,
    -3,
    -5,
    -3,
    4,
    3,
    13,
    1,
    2,
    1,
    -1,
    2,
    3,
    5,
    8,
    4,
    -5,
    -6,
    -8,
    -11,
    -4,
    0,
    -5,
    -1,
    -7,
    -12,
    -8,
    -16,
    -6,
    -6,
    0,
    2,
    -9,
    -9,
    -11,
    -3,
    -9,
    2,
    3,
    -1,
    -5,
    -13,
    4,
    -20,
};

static const int mg_queen_table[64] = {
    -28,
    0,
    29,
    12,
    59,
    44,
    43,
    45,
    -24,
    -39,
    -5,
    1,
    -16,
    57,
    28,
    54,
    -13,
    -17,
    7,
    8,
    29,
    56,
    47,
    57,
    -27,
    -27,
    -16,
    -16,
    -1,
    17,
    -2,
    1,
    -9,
    -26,
    -9,
    -10,
    -2,
    -4,
    3,
    -3,
    -14,
    2,
    -11,
    -2,
    -5,
    2,
    14,
    5,
    -35,
    -8,
    11,
    2,
    8,
    15,
    -3,
    1,
    -1,
    -18,
    -9,
    10,
    -15,
    -25,
    -31,
    -50,
};

static const int eg_queen_table[64] = {
    -9,
    22,
    22,
    27,
    27,
    19,
    10,
    20,
    -17,
    20,
    32,
    41,
    58,
    25,
    30,
    0,
    -20,
    6,
    9,
    49,
    47,
    35,
    19,
    9,
    3,
    22,
    24,
    45,
    57,
    40,
    57,
    36,
    -18,
    28,
    19,
    47,
    31,
    34,
    39,
    23,
    -16,
    -27,
    15,
    6,
    9,
    17,
    10,
    5,
    -22,
    -23,
    -30,
    -16,
    -16,
    -23,
    -36,
    -32,
    -33,
    -28,
    -22,
    -43,
    -5,
    -32,
    -20,
    -41,
};

static const int mg_king_table[64] = {
    -65,
    23,
    16,
    -15,
    -56,
    -34,
    2,
    13,
    29,
    -1,
    -20,
    -7,
    -8,
    -4,
    -38,
    -29,
    -9,
    24,
    2,
    -16,
    -20,
    6,
    22,
    -22,
    -17,
    -20,
    -12,
    -27,
    -30,
    -25,
    -14,
    -36,
    -49,
    -1,
    -27,
    -39,
    -46,
    -44,
    -33,
    -51,
    -14,
    -14,
    -22,
    -46,
    -44,
    -30,
    -15,
    -27,
    1,
    7,
    -8,
    -64,
    -43,
    -16,
    9,
    8,
    -15,
    36,
    12,
    -54,
    8,
    -28,
    24,
    14,
};

static const int eg_king_table[64] = {
    -74,
    -35,
    -18,
    -18,
    -11,
    15,
    4,
    -17,
    -12,
    17,
    14,
    17,
    17,
    38,
    23,
    11,
    10,
    17,
    23,
    15,
    20,
    45,
    44,
    13,
    -8,
    22,
    24,
    27,
    26,
    33,
    26,
    3,
    -18,
    -4,
    21,
    24,
    27,
    23,
    9,
    -11,
    -19,
    -3,
    11,
    21,
    23,
    16,
    7,
    -9,
    -27,
    -11,
    4,
    13,
    14,
    4,
    -5,
    -17,
    -53,
    -34,
    -21,
    -11,
    -28,
    -14,
    -24,
    -43,
};

static const int g_center_manhattan[64] = {
    6,
    5,
    4,
    3,
    3,
    4,
    5,
    6,
    5,
    4,
    3,
    2,
    2,
    3,
    4,
    5,
    4,
    3,
    2,
    1,
    1,
    2,
    3,
    4,
    3,
    2,
    1,
    0,
    0,
    1,
    2,
    3,
    3,
    2,
    1,
    0,
    0,
    1,
    2,
    3,
    4,
    3,
    2,
    1,
    1,
    2,
    3,
    4,
    5,
    4,
    3,
    2,
    2,
    3,
    4,
    5,
    6,
    5,
    4,
    3,
    3,
    4,
    5,
    6,
};

static inline int chebyshev_dist(int sq1, int sq2) {
    int df = abs((sq1 & 7) - (sq2 & 7));
    int dr = abs((sq1 >> 3) - (sq2 >> 3));
    return (df > dr) ? df : dr;
}

static void eval_pawn_structure(search_worker *w, u64 f_pawns, u64 e_pawns, int *mg_delta,
    int *eg_delta, u64 *f_passed_out, u64 *e_passed_out) {
    if ((f_pawns | e_pawns) == 0ull) {
        *mg_delta = 0;
        *eg_delta = 0;
        *f_passed_out = 0ull;
        *e_passed_out = 0ull;
        return;
    }

    pawn_tt_entry *pte = NULL;
    if (w) {
        u64 pkey = (f_pawns * 0x9e3779b97f4a7c15ull) ^ (e_pawns * 0xbf58476d1ce4e5b9ull);
        pte = &w->pawn_tt[(pkey >> 32) & PAWN_TT_MASK];
        if (pte->f_pawns == f_pawns && pte->e_pawns == e_pawns) {
            *mg_delta = pte->mg;
            *eg_delta = pte->eg;
            *f_passed_out = pte->f_passed;
            *e_passed_out = pte->e_passed;
            return;
        }
    }

    static const int passed_eg[8] = { 0, 8, 14, 26, 48, 86, 140, 0 };
    static const int passed_mg[8] = { 0, 4, 8, 14, 28, 50, 82, 0 };

    int mg = 0;
    int eg = 0;
    u64 f_passed = 0ull;
    u64 e_passed = 0ull;

    u64 f_att = ((f_pawns & ~COL_1) << 7) | ((f_pawns & ~COL_8) << 9);
    u64 e_att = ((e_pawns & ~COL_8) >> 7) | ((e_pawns & ~COL_1) >> 9);

    const eval_params *P = &g_eval_params;
    for (int f = 0; f < 8; f++) {
        int fc = __builtin_popcountll(f_pawns & g_file_masks[f]);
        if (fc > 1) {
            mg -= P->doubled_mg * (fc - 1);
            eg -= P->doubled_eg * (fc - 1);
        }
        int ec = __builtin_popcountll(e_pawns & g_file_masks[f]);
        if (ec > 1) {
            mg += P->doubled_mg * (ec - 1);
            eg += P->doubled_eg * (ec - 1);
        }
    }

    u64 bb = f_pawns;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        int f = sq & 7;
        int r = sq >> 3;
        u64 bit = 1ull << sq;
        u64 adj = g_adj_file_masks[f];

        if ((f_pawns & adj) == 0ull) {
            mg -= P->isolated_mg;
            eg -= P->isolated_eg;
        } else {
            if (f_att & bit) {
                mg += 10 + r;
                eg += 8 + r;
            } else if (f_pawns & adj & (0xffull << (8 * r))) {
                mg += 7;
                eg += 6;
            } else {
                u64 behind = (1ull << (8 * (r + 1))) - 1ull;
                if ((f_pawns & adj & behind) == 0ull && (e_att & (bit << 8))) {
                    mg -= P->backward_mg;
                    eg -= P->backward_eg;
                }
            }
        }

        u64 span = g_file_masks[f] | adj;
        u64 ahead = (r < 7) ? (~0ull << (8 * (r + 1))) : 0ull;
        if ((e_pawns & span & ahead) == 0ull) {
            f_passed |= bit;
            mg += (passed_mg[r] * P->passer_scale_pct) / 100;
            eg += (passed_eg[r] * P->passer_scale_pct) / 100;
            if (f_att & bit) {
                mg += 8;
                eg += 16;
            }
        }
    }

    bb = e_pawns;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        int f = sq & 7;
        int r = sq >> 3;
        int rel_r = 7 - r;
        u64 bit = 1ull << sq;
        u64 adj = g_adj_file_masks[f];

        if ((e_pawns & adj) == 0ull) {
            mg += P->isolated_mg;
            eg += P->isolated_eg;
        } else {
            if (e_att & bit) {
                mg -= 10 + rel_r;
                eg -= 8 + rel_r;
            } else if (e_pawns & adj & (0xffull << (8 * r))) {
                mg -= 7;
                eg -= 6;
            } else {
                u64 behind = ~((1ull << (8 * r)) - 1ull);
                if ((e_pawns & adj & behind) == 0ull && (f_att & (bit >> 8))) {
                    mg += P->backward_mg;
                    eg += P->backward_eg;
                }
            }
        }

        u64 span = g_file_masks[f] | adj;
        u64 ahead = (r > 0) ? ((1ull << (8 * r)) - 1ull) : 0ull;
        if ((f_pawns & span & ahead) == 0ull) {
            e_passed |= bit;
            mg -= (passed_mg[rel_r] * P->passer_scale_pct) / 100;
            eg -= (passed_eg[rel_r] * P->passer_scale_pct) / 100;
            if (e_att & bit) {
                mg -= 8;
                eg -= 16;
            }
        }
    }

    if (pte) {
        pte->f_pawns = f_pawns;
        pte->e_pawns = e_pawns;
        pte->f_passed = f_passed;
        pte->e_passed = e_passed;
        pte->mg = (int16_t) mg;
        pte->eg = (int16_t) eg;
    }
    *mg_delta = mg;
    *eg_delta = eg;
    *f_passed_out = f_passed;
    *e_passed_out = e_passed;
}

int eval_board_classical(search_worker *w, const chess *b) {
    const eval_params *P = &g_eval_params;
    if (is_insufficient_material(b)) {
        return 0;
    }
    int eg_special = 0;
    if (endgame_eval(b, &eg_special)) {
        return eg_special;
    }

    u64 friendly = b->friendly_pieces;
    u64 enemy = b->enemy_pieces;
    u64 occ = friendly | enemy;
    u64 true_pawns = b->pawns & PAWN_MASK;
    u64 orth = b->orthogonal_pieces;
    u64 diag = b->diagonal_pieces;
    u64 kings = b->kings;

    u64 f_pawns = friendly & true_pawns;
    u64 e_pawns = enemy & true_pawns;
    u64 all_pawns = f_pawns | e_pawns;

    u64 f_knights = friendly & ~(true_pawns | orth | diag | kings);
    u64 e_knights = enemy & ~(true_pawns | orth | diag | kings);
    u64 f_bishops = friendly & diag & ~orth;
    u64 e_bishops = enemy & diag & ~orth;
    u64 f_rooks = friendly & orth & ~diag;
    u64 e_rooks = enemy & orth & ~diag;
    u64 f_queens = friendly & diag & orth;
    u64 e_queens = enemy & diag & orth;
    u64 f_king_bb = friendly & kings;
    u64 e_king_bb = enemy & kings;

    int f_king_sq = f_king_bb ? __builtin_ctzll(f_king_bb) : 4;
    int e_king_sq = e_king_bb ? __builtin_ctzll(e_king_bb) : 60;
    u64 f_king_zone = g_king_attacks[f_king_sq] | (1ull << f_king_sq);
    u64 e_king_zone = g_king_attacks[e_king_sq] | (1ull << e_king_sq);

    u64 f_pawn_att = ((f_pawns & ~COL_1) << 7) | ((f_pawns & ~COL_8) << 9);
    u64 e_pawn_att = ((e_pawns & ~COL_8) >> 7) | ((e_pawns & ~COL_1) >> 9);

    int mg_f = 0, mg_e = 0;
    int eg_f = 0, eg_e = 0;

    int n_f_n = __builtin_popcountll(f_knights);
    int n_e_n = __builtin_popcountll(e_knights);
    int n_f_b = __builtin_popcountll(f_bishops);
    int n_e_b = __builtin_popcountll(e_bishops);
    int n_f_r = __builtin_popcountll(f_rooks);
    int n_e_r = __builtin_popcountll(e_rooks);
    int n_f_q = __builtin_popcountll(f_queens);
    int n_e_q = __builtin_popcountll(e_queens);
    int n_f_p = __builtin_popcountll(f_pawns);
    int n_e_p = __builtin_popcountll(e_pawns);

    int phase = (n_f_n + n_e_n + n_f_b + n_e_b) + 2 * (n_f_r + n_e_r) + 4 * (n_f_q + n_e_q);

    mg_f += n_f_p * g_mg_value[0] + n_f_n * g_mg_value[1] + n_f_b * g_mg_value[2]
            + n_f_r * g_mg_value[3] + n_f_q * g_mg_value[4];
    mg_e += n_e_p * g_mg_value[0] + n_e_n * g_mg_value[1] + n_e_b * g_mg_value[2]
            + n_e_r * g_mg_value[3] + n_e_q * g_mg_value[4];
    int mat_f = n_f_p * g_eg_value[0] + n_f_n * g_eg_value[1] + n_f_b * g_eg_value[2]
                + n_f_r * g_eg_value[3] + n_f_q * g_eg_value[4];
    int mat_e = n_e_p * g_eg_value[0] + n_e_n * g_eg_value[1] + n_e_b * g_eg_value[2]
                + n_e_r * g_eg_value[3] + n_e_q * g_eg_value[4];
    eg_f += mat_f;
    eg_e += mat_e;

    mg_f += mg_king_table[f_king_sq ^ 56];
    eg_f += eg_king_table[f_king_sq ^ 56];
    mg_e += mg_king_table[e_king_sq];
    eg_e += eg_king_table[e_king_sq];

    u64 bb = f_pawns;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        mg_f += mg_pawn_table[sq ^ 56];
        eg_f += eg_pawn_table[sq ^ 56];
    }
    bb = e_pawns;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        mg_e += mg_pawn_table[sq];
        eg_e += eg_pawn_table[sq];
    }

    int att_on_f_king = 0, att_cnt_f = 0;
    int att_on_e_king = 0, att_cnt_e = 0;

    u64 e_king_diag_chk = 0ull, e_king_orth_chk = 0ull, e_king_n_chk = 0ull;
    u64 f_king_diag_chk = 0ull, f_king_orth_chk = 0ull, f_king_n_chk = 0ull;
    if (phase > 6) {
        if (f_bishops | f_queens) {
            e_king_diag_chk = bishop_attacks_bb(e_king_sq, occ) & ~friendly & ~e_pawn_att;
        }
        if (f_rooks | f_queens) {
            e_king_orth_chk = rook_attacks_bb(e_king_sq, occ) & ~friendly & ~e_pawn_att;
        }
        if (f_knights) {
            e_king_n_chk = g_knight_attacks[e_king_sq] & ~friendly & ~e_pawn_att;
        }
        if (e_bishops | e_queens) {
            f_king_diag_chk = bishop_attacks_bb(f_king_sq, occ) & ~enemy & ~f_pawn_att;
        }
        if (e_rooks | e_queens) {
            f_king_orth_chk = rook_attacks_bb(f_king_sq, occ) & ~enemy & ~f_pawn_att;
        }
        if (e_knights) {
            f_king_n_chk = g_knight_attacks[f_king_sq] & ~enemy & ~f_pawn_att;
        }
    }

    // Knights
    bb = f_knights;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        mg_f += mg_knight_table[sq ^ 56];
        eg_f += eg_knight_table[sq ^ 56];
        u64 att = g_knight_attacks[sq];
        int mob = __builtin_popcountll(att & ~friendly & ~e_pawn_att) - 4;
        mg_f += mob * P->knight_mob_mg;
        eg_f += mob * P->knight_mob_eg;
        int r = sq >> 3;
        if (r >= 3 && r <= 5 && (f_pawn_att & (1ull << sq))) {
            u64 ahead = ~0ull << (8 * (r + 1));
            if ((e_pawns & g_adj_file_masks[sq & 7] & ahead) == 0ull) {
                mg_f += P->knight_outpost_mg;
                eg_f += P->knight_outpost_eg;
            }
        }
        if (att & e_king_zone) {
            att_on_e_king += 2;
            att_cnt_e++;
        }
        if (att & e_king_n_chk) {
            att_on_e_king += 2;
        }
    }
    bb = e_knights;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        mg_e += mg_knight_table[sq];
        eg_e += eg_knight_table[sq];
        u64 att = g_knight_attacks[sq];
        int mob = __builtin_popcountll(att & ~enemy & ~f_pawn_att) - 4;
        mg_e += mob * P->knight_mob_mg;
        eg_e += mob * P->knight_mob_eg;
        int r = sq >> 3;
        if (r >= 2 && r <= 4 && (e_pawn_att & (1ull << sq))) {
            u64 ahead = (1ull << (8 * r)) - 1ull;
            if ((f_pawns & g_adj_file_masks[sq & 7] & ahead) == 0ull) {
                mg_e += P->knight_outpost_mg;
                eg_e += P->knight_outpost_eg;
            }
        }
        if (att & f_king_zone) {
            att_on_f_king += 2;
            att_cnt_f++;
        }
        if (att & f_king_n_chk) {
            att_on_f_king += 2;
        }
    }

    // Bishops
    bb = f_bishops;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        mg_f += mg_bishop_table[sq ^ 56];
        eg_f += eg_bishop_table[sq ^ 56];
        u64 att = bishop_attacks_bb(sq, occ);
        int mob = __builtin_popcountll(att & ~friendly & ~e_pawn_att) - 6;
        mg_f += mob * P->bishop_mob_mg;
        eg_f += mob * P->bishop_mob_eg;
        if (att & e_king_zone) {
            att_on_e_king += 2;
            att_cnt_e++;
        }
        if (att & e_king_diag_chk) {
            att_on_e_king += 2;
        }
    }
    bb = e_bishops;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        mg_e += mg_bishop_table[sq];
        eg_e += eg_bishop_table[sq];
        u64 att = bishop_attacks_bb(sq, occ);
        int mob = __builtin_popcountll(att & ~enemy & ~f_pawn_att) - 6;
        mg_e += mob * P->bishop_mob_mg;
        eg_e += mob * P->bishop_mob_eg;
        if (att & f_king_zone) {
            att_on_f_king += 2;
            att_cnt_f++;
        }
        if (att & f_king_diag_chk) {
            att_on_f_king += 2;
        }
    }

    // Rooks
    bb = f_rooks;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        mg_f += mg_rook_table[sq ^ 56];
        eg_f += eg_rook_table[sq ^ 56];
        u64 att = rook_attacks_bb(sq, occ);
        int mob = __builtin_popcountll(att & ~friendly) - 7;
        mg_f += mob * P->rook_mob_mg;
        eg_f += mob * P->rook_mob_eg;
        u64 fmask = g_file_masks[sq & 7];
        if ((all_pawns & fmask) == 0ull) {
            mg_f += P->rook_open_mg;
            eg_f += P->rook_open_eg;
        } else if ((f_pawns & fmask) == 0ull) {
            mg_f += P->rook_semi_mg;
            eg_f += P->rook_semi_eg;
        }
        if ((sq >> 3) == 6) {
            mg_f += P->rook_7th_mg;
            eg_f += P->rook_7th_eg;
        }
        if (att & f_rooks) {
            mg_f += P->rook_conn_mg;
            eg_f += P->rook_conn_eg;
        }
        if (att & e_king_zone) {
            att_on_e_king += 3;
            att_cnt_e++;
        }
        if (att & e_king_orth_chk) {
            att_on_e_king += 2;
        }
    }
    bb = e_rooks;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        mg_e += mg_rook_table[sq];
        eg_e += eg_rook_table[sq];
        u64 att = rook_attacks_bb(sq, occ);
        int mob = __builtin_popcountll(att & ~enemy) - 7;
        mg_e += mob * P->rook_mob_mg;
        eg_e += mob * P->rook_mob_eg;
        u64 fmask = g_file_masks[sq & 7];
        if ((all_pawns & fmask) == 0ull) {
            mg_e += P->rook_open_mg;
            eg_e += P->rook_open_eg;
        } else if ((e_pawns & fmask) == 0ull) {
            mg_e += P->rook_semi_mg;
            eg_e += P->rook_semi_eg;
        }
        if ((sq >> 3) == 1) {
            mg_e += P->rook_7th_mg;
            eg_e += P->rook_7th_eg;
        }
        if (att & e_rooks) {
            mg_e += P->rook_conn_mg;
            eg_e += P->rook_conn_eg;
        }
        if (att & f_king_zone) {
            att_on_f_king += 3;
            att_cnt_f++;
        }
        if (att & f_king_orth_chk) {
            att_on_f_king += 2;
        }
    }

    // Queens
    bb = f_queens;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        mg_f += mg_queen_table[sq ^ 56];
        eg_f += eg_queen_table[sq ^ 56];
        u64 att = queen_attacks_bb(sq, occ);
        int mob = __builtin_popcountll(att & ~friendly & ~e_pawn_att) - 14;
        mg_f += mob * P->queen_mob_mg;
        eg_f += mob * P->queen_mob_eg;
        if (att & e_king_zone) {
            att_on_e_king += 5;
            att_cnt_e++;
        }
        if (att & (e_king_diag_chk | e_king_orth_chk)) {
            att_on_e_king += 3;
        }
    }
    bb = e_queens;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        mg_e += mg_queen_table[sq];
        eg_e += eg_queen_table[sq];
        u64 att = queen_attacks_bb(sq, occ);
        int mob = __builtin_popcountll(att & ~enemy & ~f_pawn_att) - 14;
        mg_e += mob * P->queen_mob_mg;
        eg_e += mob * P->queen_mob_eg;
        if (att & f_king_zone) {
            att_on_f_king += 5;
            att_cnt_f++;
        }
        if (att & (f_king_diag_chk | f_king_orth_chk)) {
            att_on_f_king += 3;
        }
    }

    // Bishop pair
    if (n_f_b >= 2) {
        mg_f += P->bishop_pair_mg;
        eg_f += P->bishop_pair_eg;
    }
    if (n_e_b >= 2) {
        mg_e += P->bishop_pair_mg;
        eg_e += P->bishop_pair_eg;
    }

    // Pawn threats on enemy non-pawn pieces
    int f_pthreats = __builtin_popcountll(f_pawn_att & (enemy & ~e_pawns & ~kings));
    int e_pthreats = __builtin_popcountll(e_pawn_att & (friendly & ~f_pawns & ~kings));
    mg_f += f_pthreats * P->pawn_threat_mg;
    eg_f += f_pthreats * P->pawn_threat_eg;
    mg_e += e_pthreats * P->pawn_threat_mg;
    eg_e += e_pthreats * P->pawn_threat_eg;

    // Pawn structure + cached passed pawn bitboards
    int pawn_mg = 0, pawn_eg = 0;
    u64 f_passed = 0ull, e_passed = 0ull;
    eval_pawn_structure(w, f_pawns, e_pawns, &pawn_mg, &pawn_eg, &f_passed, &e_passed);

    // Dynamic passed pawn evaluation
    static const int conn_pass_eg[8] = { 0, 0, 0, 14, 38, 90, 175, 0 };
    bb = f_passed;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        int r = sq >> 3;
        int f = sq & 7;
        if (r >= 3) {
            int wgt = r - 2;
            int stop_sq = sq + 8;
            eg_f
                += (chebyshev_dist(e_king_sq, stop_sq) * 5 - chebyshev_dist(f_king_sq, stop_sq) * 2)
                   * wgt;
            if (!(occ & (1ull << stop_sq))) {
                eg_f += (e_pawn_att & (1ull << stop_sq)) ? (4 * wgt) : (10 * wgt);
            }
            u64 conn_ranks = (0xffull << (8 * r)) | (0xffull << (8 * (r - 1)));
            if (f_passed & g_adj_file_masks[f] & conn_ranks) {
                mg_f += conn_pass_eg[r] / 2;
                eg_f += conn_pass_eg[r];
            }
            u64 behind_file = g_file_masks[f] & ((1ull << sq) - 1ull);
            if (f_rooks & behind_file) {
                eg_f += 16;
            } else if (e_rooks & behind_file) {
                eg_f -= 14;
            }
        }
    }
    bb = e_passed;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        int r = sq >> 3;
        int f = sq & 7;
        int rel_r = 7 - r;
        if (rel_r >= 3) {
            int wgt = rel_r - 2;
            int stop_sq = sq - 8;
            eg_e
                += (chebyshev_dist(f_king_sq, stop_sq) * 5 - chebyshev_dist(e_king_sq, stop_sq) * 2)
                   * wgt;
            if (!(occ & (1ull << stop_sq))) {
                eg_e += (f_pawn_att & (1ull << stop_sq)) ? (4 * wgt) : (10 * wgt);
            }
            u64 conn_ranks = (0xffull << (8 * r)) | (0xffull << (8 * (r + 1)));
            if (e_passed & g_adj_file_masks[f] & conn_ranks) {
                mg_e += conn_pass_eg[rel_r] / 2;
                eg_e += conn_pass_eg[rel_r];
            }
            u64 behind_file = g_file_masks[f] & (~0ull << (sq + 1));
            if (e_rooks & behind_file) {
                eg_e += 16;
            } else if (f_rooks & behind_file) {
                eg_e -= 14;
            }
        }
    }

    // Opposite-wing pawn storm
    if (phase >= 12 && abs((f_king_sq & 7) - (e_king_sq & 7)) >= 3) {
        int ekf = e_king_sq & 7;
        u64 ek_files = g_file_masks[ekf] | g_adj_file_masks[ekf];
        u64 f_storm = f_pawns & ek_files;
        mg_f += __builtin_popcountll(f_storm & ROW_4) * 6
                + __builtin_popcountll(f_storm & ROW_5) * 14
                + __builtin_popcountll(f_storm & ROW_6) * 24;

        int fkf = f_king_sq & 7;
        u64 fk_files = g_file_masks[fkf] | g_adj_file_masks[fkf];
        u64 e_storm = e_pawns & fk_files;
        mg_e += __builtin_popcountll(e_storm & ROW_5) * 6
                + __builtin_popcountll(e_storm & ROW_4) * 14
                + __builtin_popcountll(e_storm & ROW_3) * 24;
    }

    // Middlegame King Safety: pawn shield + open files near king + king-zone pressure
    if ((f_king_sq >> 3) <= 1) {
        int kf = f_king_sq & 7;
        int f_lo = (kf > 0) ? (kf - 1) : 0;
        int f_hi = (kf < 7) ? (kf + 1) : 7;
        for (int f = f_lo; f <= f_hi; f++) {
            u64 fm = g_file_masks[f];
            if (f_pawns & (1ull << (8 + f))) {
                mg_f += P->shield_r2_mg;
            } else if (f_pawns & (1ull << (16 + f))) {
                mg_f += P->shield_r3_mg;
            } else if ((all_pawns & fm) == 0ull) {
                mg_f -= P->king_open_mg;
            } else if ((f_pawns & fm) == 0ull) {
                mg_f -= P->king_semi_mg;
            }
        }
    }
    if ((e_king_sq >> 3) >= 6) {
        int kf = e_king_sq & 7;
        int f_lo = (kf > 0) ? (kf - 1) : 0;
        int f_hi = (kf < 7) ? (kf + 1) : 7;
        for (int f = f_lo; f <= f_hi; f++) {
            u64 fm = g_file_masks[f];
            if (e_pawns & (1ull << (48 + f))) {
                mg_e += P->shield_r2_mg;
            } else if (e_pawns & (1ull << (40 + f))) {
                mg_e += P->shield_r3_mg;
            } else if ((all_pawns & fm) == 0ull) {
                mg_e -= P->king_open_mg;
            } else if ((e_pawns & fm) == 0ull) {
                mg_e -= P->king_semi_mg;
            }
        }
    }

    if (att_cnt_f >= 2) {
        int pen = (att_on_f_king * att_on_f_king * P->king_att_num) / 10;
        if (pen > 300) {
            pen = 300;
        }
        mg_f -= pen;
    }
    if (att_cnt_e >= 2) {
        int pen = (att_on_e_king * att_on_e_king * P->king_att_num) / 10;
        if (pen > 300) {
            pen = 300;
        }
        mg_e -= pen;
    }

    int mg_phase = (phase > 24) ? 24 : phase;
    int eg_phase = 24 - mg_phase;

    // Endgame mop-up
    if (eg_phase >= 18) {
        int dist
            = abs((f_king_sq & 7) - (e_king_sq & 7)) + abs((f_king_sq >> 3) - (e_king_sq >> 3));
        if (mat_f - mat_e >= 250 && e_pawns == 0ull) {
            eg_f += g_center_manhattan[e_king_sq] * 12 + (14 - dist) * 5;
        } else if (mat_e - mat_f >= 250 && f_pawns == 0ull) {
            eg_e += g_center_manhattan[f_king_sq] * 12 + (14 - dist) * 5;
        }
    }

    int mg_score = (mg_f - mg_e) + pawn_mg;
    int eg_score = (eg_f - eg_e) + pawn_eg;

    // Endgame drawishness scaling
    if (n_f_q == 0 && n_e_q == 0 && n_f_r == 0 && n_e_r == 0 && n_f_n == 0 && n_e_n == 0
        && n_f_b == 1 && n_e_b == 1) {
        bool f_light = (f_bishops & LIGHT_SQUARES) != 0ull;
        bool e_light = (e_bishops & LIGHT_SQUARES) != 0ull;
        if (f_light != e_light) {
            eg_score = (eg_score * 55) / 100;
        }
    }
    if (eg_score > 0 && n_f_p == 0 && (mat_f - mat_e) < 400) {
        eg_score /= 4;
    } else if (eg_score < 0 && n_e_p == 0 && (mat_e - mat_f) < 400) {
        eg_score /= 4;
    }

    return ((mg_score * mg_phase) + (eg_score * eg_phase)) / 24 + P->tempo;
}

int eval_board_ply(search_worker *w, const chess *b, int ply) {
    if (is_insufficient_material(b)) {
        return 0;
    }
    int eg_special = 0;
    if (endgame_eval(b, &eg_special)) {
        return eg_special;
    }
    nnue_mode mode = nnue_get_mode();
    if (mode != NNUE_OFF && nnue_is_loaded()) {
        int n_ev;
        if (w && ply >= 0 && ply < MAX_PLY + 4
            && (w->acc_stack[ply].valid || w->acc_stack[ply].dirty)) {
            if (!w->acc_stack[ply].valid) {
                nnue_acc_materialize(w->acc_stack, w->finny, ply, b);
            }
            n_ev = nnue_eval_acc(&w->acc_stack[ply], b);
        } else {
            n_ev = nnue_evaluate(b);
        }
        if (mode == NNUE_PURE) {
            u64 orth = b->orthogonal_pieces;
            u64 tp = b->pawns & PAWN_MASK;
            u64 diag = b->diagonal_pieces;
            // Opposite-colored bishops pure endgame scaling
            if (orth == 0ull) {
                u64 f_b = b->friendly_pieces & diag;
                u64 e_b = b->enemy_pieces & diag;
                u64 f_n = b->friendly_pieces & ~(tp | diag | b->kings);
                u64 e_n = b->enemy_pieces & ~(tp | diag | b->kings);
                if (f_n == 0ull && e_n == 0ull && __builtin_popcountll(f_b) == 1
                    && __builtin_popcountll(e_b) == 1) {
                    if (((f_b & LIGHT_SQUARES) != 0ull) != ((e_b & LIGHT_SQUARES) != 0ull)) {
                        n_ev = (n_ev * 65) / 100;
                    }
                }
            }
            // No-pawn leading side with insufficient material advantage (< 400cp)
            if (n_ev > 30 && (b->friendly_pieces & tp) == 0ull) {
                int f_m
                    = __builtin_popcountll(b->friendly_pieces & orth & diag) * 900
                      + __builtin_popcountll(b->friendly_pieces & orth & ~diag) * 500
                      + __builtin_popcountll(b->friendly_pieces & ~orth & diag) * 330
                      + __builtin_popcountll(b->friendly_pieces & ~(tp | orth | diag | b->kings))
                            * 320;
                int e_m = __builtin_popcountll(b->enemy_pieces & orth & diag) * 900
                          + __builtin_popcountll(b->enemy_pieces & orth & ~diag) * 500
                          + __builtin_popcountll(b->enemy_pieces & ~orth & diag) * 330
                          + __builtin_popcountll(b->enemy_pieces & ~(tp | orth | diag | b->kings))
                                * 320;
                if (f_m - e_m < 400) {
                    n_ev = (n_ev * 35) / 100;
                }
            } else if (n_ev < -30 && (b->enemy_pieces & tp) == 0ull) {
                int f_m
                    = __builtin_popcountll(b->friendly_pieces & orth & diag) * 900
                      + __builtin_popcountll(b->friendly_pieces & orth & ~diag) * 500
                      + __builtin_popcountll(b->friendly_pieces & ~orth & diag) * 330
                      + __builtin_popcountll(b->friendly_pieces & ~(tp | orth | diag | b->kings))
                            * 320;
                int e_m = __builtin_popcountll(b->enemy_pieces & orth & diag) * 900
                          + __builtin_popcountll(b->enemy_pieces & orth & ~diag) * 500
                          + __builtin_popcountll(b->enemy_pieces & ~orth & diag) * 330
                          + __builtin_popcountll(b->enemy_pieces & ~(tp | orth | diag | b->kings))
                                * 320;
                if (e_m - f_m < 400) {
                    n_ev = (n_ev * 35) / 100;
                }
            }
            return n_ev;
        }
        int c_ev = eval_board_classical(w, b);
        return (n_ev * 25 + c_ev * 75) / 100;
    }
    return eval_board_classical(w, b);
}

int eval_board(search_worker *w, const chess *b) {
    return eval_board_ply(w, b, -1);
}

int engine_eval_static(const chess *b) {
    engine_global_init();
    return eval_board(NULL, b);
}
