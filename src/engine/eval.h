#ifndef ENGINE_EVAL_H
#define ENGINE_EVAL_H

#include "engine/engine_internal.h"

typedef struct eval_params {
    int bishop_pair_mg;
    int bishop_pair_eg;
    int knight_mob_mg;
    int knight_mob_eg;
    int bishop_mob_mg;
    int bishop_mob_eg;
    int rook_mob_mg;
    int rook_mob_eg;
    int queen_mob_mg;
    int queen_mob_eg;
    int knight_outpost_mg;
    int knight_outpost_eg;
    int rook_open_mg;
    int rook_open_eg;
    int rook_semi_mg;
    int rook_semi_eg;
    int rook_7th_mg;
    int rook_7th_eg;
    int rook_conn_mg;
    int rook_conn_eg;
    int pawn_threat_mg;
    int pawn_threat_eg;
    int doubled_mg;
    int doubled_eg;
    int isolated_mg;
    int isolated_eg;
    int backward_mg;
    int backward_eg;
    int shield_r2_mg;
    int shield_r3_mg;
    int king_open_mg;
    int king_semi_mg;
    int king_att_num; // scaled by / 10
    int passer_scale_pct; // percentage scale (default 100)
    int tempo;
} eval_params;

extern eval_params g_eval_params;

int eval_board(search_worker *w, const chess *b);
int eval_board_ply(search_worker *w, const chess *b, int ply);
int eval_board_classical(search_worker *w, const chess *b);

#endif
