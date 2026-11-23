#ifndef GAUNTLET_H
#define GAUNTLET_H

#include "core/primitives.h"

#include <stdbool.h>
#include <stddef.h>

#define MAX_GAUNTLET_THREADS 16
#define ELO_RATINGS_FILE     "data/elo_ratings.json"

typedef struct elo_record {
    int level; // 1..6
    int est_elo;
    int anchor_elo; // Stockfish UCI_Elo if vs_stockfish, else opponent level
    int wins;
    int draws;
    int losses;
    int games;
    bool has_elo;
    bool is_relative; // true when calibrated via internal ladder without Stockfish
} elo_record;

typedef struct worker_snapshot {
    bool active;
    int worker_id;
    int level;
    int game_num;
    int total_games;
    int ply_count;
    int eval_white_cp;
    char white_label[48];
    char black_label[48];
    char last_san[16];
    char fen[MAX_FEN_LENGTH];
    char board[65]; // 64 piece chars (' ' for empty), White-at-bottom a1..h8
} worker_snapshot;

typedef struct level_progress {
    int level;
    int state; // 0=pending, 1=running, 2=done, 3=skipped
    int done_games;
    int total_games;
    int wins;
    int draws;
    int losses;
    int anchor_elo;
    int est_elo;
    bool has_elo;
    bool is_relative;
    char opponent_label[48];
} level_progress;

typedef struct gauntlet_status {
    bool running;
    bool vs_stockfish;
    bool stop_requested;
    int num_threads;
    int cpu_cores;
    int games_per_level;
    int completed_games_total;
    int scheduled_games_total;
    double elapsed_sec;
    char opponent_name[96];
    char updated_at[32];
    level_progress levels[6];
    worker_snapshot workers[MAX_GAUNTLET_THREADS];
    int worker_count;
} gauntlet_status;

typedef struct gauntlet gauntlet;

// Save/load calibrated Elo ratings JSON file.
bool elo_load_file(const char *path, elo_record out_levels[6], char *opponent_out, size_t opp_size,
    char *updated_out, size_t upd_size);
bool elo_save_file(
    const char *path, const elo_record levels[6], const char *opponent, bool vs_stockfish);

int gauntlet_cpu_count(void);
gauntlet *gauntlet_create(const char *sf_path, const char *elo_file_path);
void gauntlet_delete(gauntlet **g);

bool gauntlet_start(gauntlet *g, int games_per_level, int num_threads, int only_level,
    bool use_stockfish, int sf_time_ms);
void gauntlet_stop(gauntlet *g);
bool gauntlet_is_running(gauntlet *g);
void gauntlet_wait(gauntlet *g);
void gauntlet_snapshot(gauntlet *g, gauntlet_status *out);

#endif
