#ifndef FEATURES_REVIEW_H
#define FEATURES_REVIEW_H

#include "core/primitives.h"
#include "engine/engine.h"

#include <stdbool.h>

typedef enum review_move_class {
    REV_BRILLIANT = 0,
    REV_BEST = 1,
    REV_GOOD = 2,
    REV_INACCURACY = 3,
    REV_MISTAKE = 4,
    REV_BLUNDER = 5,
    REV_CLASS_COUNT = 6,
} review_move_class;

typedef struct review_ply {
    int move_num;
    bool was_black;
    char played_san[16];
    char best_san[16];
    int eval_before_w;
    int eval_after_w;
    int cpl;
    double win_drop; // 0.0 .. 1.0 (e.g. 0.20 = 20% Lichess win% drop)
    double move_acc; // 0.0 .. 100.0 per-move Lichess accuracy
    review_move_class cls;
} review_ply;

typedef struct review_side_stats {
    double accuracy;
    int acpl;
    int est_elo; // calibrated performance Elo for this game (400..2800)
    int counts[REV_CLASS_COUNT]; // indexed by review_move_class
    int move_count;
} review_side_stats;

typedef struct game_review {
    int total_plies;
    review_ply *plies; // [total_plies]
    int *evals_w; // [total_plies + 1]
    double *win_pcts_w; // [total_plies + 1] Lichess White win% (0..100)
    review_side_stats white;
    review_side_stats black;
    int turning_point_ply; // -1 if none
    double turning_point_drop;
} game_review;

typedef void (*review_progress_cb)(int ply, int total_plies, void *user_data);
typedef search_result (*review_eval_fn)(const full_chess *pos, void *ctx);

double review_lichess_win_pct(int cp);
const char *review_class_name(review_move_class cls);
const char *review_elo_tier(int elo);

// Run post-game analysis over `moves[0..move_count-1]` starting from `game`'s initial position.
// If `eval_fn` is non-NULL, uses it as the objective ground-truth evaluator (e.g. Stockfish).
bool review_analyze_game_ex(engine *eng, const full_chess *game, const u64 *moves, int move_count,
    int depth, int time_ms, review_eval_fn eval_fn, void *eval_ctx,
    review_progress_cb progress_cb, void *user_data, game_review *out);

// Standard convenience wrapper using `eng`.
bool review_analyze_game(engine *eng, const full_chess *game, const u64 *moves, int move_count,
    int depth, int time_ms, review_progress_cb progress_cb, void *user_data, game_review *out);

void review_free(game_review *rev);

#endif
