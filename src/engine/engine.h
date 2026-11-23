#ifndef ENGINE_H
#define ENGINE_H

#include "core/primitives.h"

#include <stdbool.h>
#include <stddef.h>

#define MATE_SCORE  100000
#define MATE_BOUND  90000
#define MAX_MULTIPV 5

typedef struct difficulty_preset {
    int level;
    const char *name;
    const char *desc;
    int max_depth;
    int time_ms;
    int blunder_pct; // chance of picking a suboptimal move (Novice only)
} difficulty_preset;

typedef struct search_result {
    u64 best_move;
    int score_cp; // from side-to-move perspective (or MATE_SCORE - ply)
    int depth_reached;
    u64 nodes;
    double elapsed_sec;
} search_result;

typedef struct multipv_line {
    u64 best_move;
    int score_cp; // from side-to-move perspective
    int depth_reached;
    u64 pv[16];
    int pv_len;
} multipv_line;

typedef void (*engine_info_cb)(int depth, int score_cp, u64 nodes, double elapsed_sec,
    const u64 *pv, int pv_len, void *user_data);

typedef void (*engine_multipv_cb)(int depth, int multipv_idx, int score_cp, u64 nodes,
    double elapsed_sec, const u64 *pv, int pv_len, void *user_data);

typedef struct engine engine;

engine *engine_create(size_t tt_mb);
void engine_clear_tt(engine *eng);
void engine_delete(engine **eng);

// Configuration (Lazy SMP threads, Opening Book, UCI info callback, async stop)
void engine_set_threads(engine *eng, int num_threads);
int engine_get_threads(const engine *eng);
void engine_set_use_book(engine *eng, bool use_book);
bool engine_get_use_book(const engine *eng);
void engine_set_book_path(engine *eng, const char *path);
void engine_set_info_callback(engine *eng, engine_info_cb cb, void *user_data);
void engine_set_multipv_callback(engine *eng, engine_multipv_cb cb, void *user_data);
void engine_request_stop(engine *eng);

// Runtime search parameter override (used by SPSA tuning & UCI options)
bool engine_set_param(const char *name, int val);

// UCI_LimitStrength / UCI_Elo calibrated strength control (800..2600 Elo)
void engine_set_limit_strength(engine *eng, bool enable, int target_elo);
bool engine_get_limit_strength(const engine *eng, int *target_elo_out);
search_result engine_search_elo(engine *eng, const full_chess *game, int target_elo);

// Zobrist hashing (static White-at-bottom coordinates + O(1) incremental update)
u64 engine_hash_board(const chess *b);
u64 engine_update_hash(u64 hash, const chess *before, u64 move);

// Static Exchange Evaluation & static position evaluation (exposed for tests / tooling)
int engine_see(const chess *b, u64 move);
int engine_eval_static(const chess *b);

const difficulty_preset *engine_get_preset(int level);
int engine_num_presets(void);

// Search from `game` with explicit depth & time limits (time_ms <= 0 means depth-only).
search_result engine_search(engine *eng, const full_chess *game, int max_depth, int time_ms);

// Clock-aware search with separate soft (early-stop on stability) and hard (max panic) limits.
search_result engine_search_timed(
    engine *eng, const full_chess *game, int max_depth, int soft_ms, int hard_ms);

// Search top `num_pvs` distinct candidate lines (1..MAX_MULTIPV), sorted best-first.
// Returns the number of lines written into `out_lines`.
int engine_search_multipv(engine *eng, const full_chess *game, int max_depth, int time_ms,
    int num_pvs, multipv_line *out_lines, u64 *total_nodes_out, double *elapsed_out);

// Search using a difficulty preset (1..6).
search_result engine_search_level(engine *eng, const full_chess *game, int level);

// Extract Principal Variation (PV) moves from the transposition table after a search.
int engine_extract_pv(
    engine *eng, const full_chess *game, u64 first_move, u64 *pv_out, int max_len);

// Format a White-perspective centipawn/mate score (e.g. "+1.4", "-0.3", "M3", "-M2", "0.0").
void engine_format_eval(int white_cp, char *buf, size_t buf_size);

// Convert side-to-move score to White-perspective score.
int engine_score_to_white(const chess *b, int stm_score);

// Convert White centipawn score to White winning expectation [0.0, 1.0].
double engine_win_prob(int white_cp);

#endif
