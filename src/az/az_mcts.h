#ifndef AZ_MCTS_H
#define AZ_MCTS_H

#include "core/board.h"
#include "core/game.h"
#include "core/primitives.h"

#include <stdbool.h>
#include <stdint.h>

// =========================================================================
// Zero-C v2: Pure Tabula Rasa AlphaZero / KataGo Batched C MCTS Engine
//            + Optional Path-C Tactical Hybrid Play Booster
//   - Canonical 20-plane side-to-move 8x8 board tensor (18 base + rep + stm)
//   - Exact AlphaZero 4,672-action encoding (64 from-squares x 73 move planes)
//   - PUCT + Virtual Loss + FPU Reduction + Root Dirichlet Noise
//   - Exact MCTS Solver (Proven Win/Loss Propagation) + Rules Mate-in-1/2
//   - True MCTS Subtree Reuse across plies (self-play & UCI)
//   - KataGo Playout Cap Randomization + Forced Playouts + Policy Pruning
//   - Q-Blended WDL + Moves-Left Utility + Smart Resignation
//   - Optional Tactical Hybrid Mode (zero-bias capture qsearch delta, OFF in RL)
// =========================================================================

#define AZ_PLANES       20
#define AZ_BOARD_FLOATS (AZ_PLANES * 64)
#define AZ_NUM_ACTIONS  (64 * 73) // 4672 canonical AlphaZero move indices
#define AZ_MAX_GAMES    512

typedef struct az_config {
    int num_games; // concurrent self-play games (1..512)
    int fast_sims; // playout cap fast simulation count (e.g. 36..48)
    int full_sims; // playout cap full simulation count (e.g. 200..400)
    float full_search_prob; // probability of full search per move (e.g. 0.25)
    float c_puct; // PUCT exploration constant (e.g. 1.75)
    float fpu_reduction; // First Play Urgency reduction (e.g. 0.25)
    float dirichlet_alpha; // root Dirichlet alpha (e.g. 0.30)
    float dirichlet_eps; // root Dirichlet blend weight (0.0 for eval/match)
    int temp_plies; // plies using temperature exploration before dropping to greedy
    int max_plies; // max plies per self-play game before draw adjudication
    int batch_per_game; // virtual-loss leaves collected per game per GPU batch (1..16)
    int forced_playouts; // 1 = enable KataGo forced playouts + policy target pruning
    int num_threads; // C MCTS worker threads (0 = auto-detect CPU cores, 1 = single-threaded)
    uint64_t seed;
    float resign_threshold; // e.g. -0.94 in self-play (0.0 = disabled)
    int tactical_mode; // 0 = pure tabula rasa RL, 1 = Path-C Hybrid Tactical MCTS (play mode)
} az_config;

// One completed training sample harvested from a finished self-play game.
typedef struct az_sample {
    float planes[AZ_BOARD_FLOATS];
    int32_t actions[MAX_LEGAL_MOVES];
    float probs[MAX_LEGAL_MOVES];
    int32_t num_legal;
    float wdl[3]; // [Win, Draw, Loss] from side-to-move perspective
    float moves_left; // normalized remaining plies / 100.0
    int32_t is_full_search; // 1 if policy target came from full playout cap search
} az_sample;

// Compact live snapshot of a concurrent self-play game (for WebUI streaming).
typedef struct az_game_snapshot {
    int32_t game_id;
    int32_t game_seq;
    int32_t ply;
    int32_t sims_done;
    int32_t sims_target;
    int32_t is_full_search;
    float root_w;
    float root_d;
    float root_l;
    float root_ml;
    char fen[MAX_FEN_LENGTH];
    char board[65]; // 64 White-perspective chars (a1..h8) + NUL
    char last_san[16];
    char last_uci[8];
    int32_t top_count;
    char top_san[3][16];
    char top_uci[3][8];
    int32_t top_visits[3];
    float top_prior[3];
    float top_q[3];
} az_game_snapshot;

typedef struct az_arena az_arena;

// Low-level deterministic encoding / decoding (also unit-tested in driver.c)
void az_global_init(void);
int az_move_to_action(u64 move);
void az_encode_board_ctx(
    const chess *b, int half_move, int is_rep, float out_planes[AZ_BOARD_FLOATS]);
void az_encode_board(const chess *b, int half_move, float out_planes[AZ_BOARD_FLOATS]);
int az_legal_actions(
    const chess *b, u64 out_moves[MAX_LEGAL_MOVES], int32_t out_actions[MAX_LEGAL_MOVES]);
void az_move_to_uci(const chess *b, u64 m, char out[8]);

// Batched Multi-Game MCTS Arena API (called from Python trainer via ctypes & C tests)
az_arena *az_arena_create(const az_config *cfg);
void az_arena_destroy(az_arena *arena);
int az_arena_num_threads(const az_arena *arena);
void az_arena_reset_game(az_arena *arena, int game_idx, const char *fen_or_null);
void az_arena_set_position_uci(
    az_arena *arena, int game_idx, const char *fen_or_null, const char *moves_space_sep);

// Collect up to `max_batch` unexpanded leaf nodes across active games.
// Terminal nodes are resolved immediately in C without touching the GPU!
// Returns `B` = number of neural evaluations required (0 means all games ready to advance).
int az_arena_collect_leaves(az_arena *arena, int max_batch, float *out_planes,
    int32_t *out_legal_counts, int32_t *out_legal_actions);

// Feed GPU evaluation outputs back into the pending leaves and backpropagate WDL + Moves-Left.
void az_arena_apply_evals(az_arena *arena, int batch_size, const float *policy_priors,
    const float *wdl, const float *moves_left);

// Advance any game whose root visits reached `sims_target` by sampling/picking a move.
// Reuses the chosen child's MCTS subtree!
int az_arena_step_moves(az_arena *arena, int auto_reset, int32_t *out_white_wins,
    int32_t *out_draws, int32_t *out_black_wins, int32_t *out_total_plies_finished);

// Drain completed training samples from the arena queue (up to `max_samples`).
int az_arena_pop_samples(az_arena *arena, az_sample *out_buf, int max_samples);
int az_arena_pending_samples(const az_arena *arena);

// Live inspection & single-move UCI extraction for game `game_idx`.
bool az_arena_get_snapshot(const az_arena *arena, int game_idx, az_game_snapshot *out);
bool az_arena_best_move(const az_arena *arena, int game_idx, float temperature, char out_uci[8],
    float out_wdl[3], float *out_ml);

#endif
