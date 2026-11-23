#ifndef FEATURES_PUZZLES_H
#define FEATURES_PUZZLES_H

#include <stdbool.h>
#include <stddef.h>

#define PUZZLE_MAX_MOVES  8
#define PUZZLE_STATS_FILE "data/puzzle_stats.json"

typedef struct chess_puzzle {
    int id;
    const char *title;
    const char *theme; // "mate", "tactic", "sacrifice", "endgame", "master"
    int difficulty; // 1..5
    int rating;
    const char *description;
    const char *hint;
    const char *fen;
    int num_moves;
    const char *moves[PUZZLE_MAX_MOVES]; // UCI sequence: player, opp, player, ...
} chess_puzzle;

typedef struct puzzle_stats {
    int rating;
    int streak;
    int best_streak;
    int solved_count;
    int attempts;
    bool solved[64];
} puzzle_stats;

int puzzle_count(void);
const chess_puzzle *puzzle_get(int idx);
const chess_puzzle *puzzle_find_by_id(int id);

void puzzle_stats_init(puzzle_stats *st);
bool puzzle_stats_load(const char *path, puzzle_stats *st);
bool puzzle_stats_save(const char *path, const puzzle_stats *st);
int puzzle_stats_record(puzzle_stats *st, int puzzle_id, int puzzle_rating, bool solved_clean);

#endif
