#ifndef FEATURES_HISTORY_H
#define FEATURES_HISTORY_H

#include <stdbool.h>
#include <stddef.h>

#define GAME_HISTORY_FILE "data/game_history.json"
#define MAX_HISTORY_GAMES 120

typedef struct history_entry {
    int id;
    char date[32];
    char white[64];
    char black[64];
    char result[12]; // "1-0", "0-1", "1/2-1/2", "*"
    char reason[64];
    char opening[64];
    char mode[24];
    int plies;
    int white_elo; // 0 if not yet reviewed
    int black_elo; // 0 if not yet reviewed
    char *pgn; // heap-allocated PGN string
} history_entry;

typedef struct game_history {
    history_entry entries[MAX_HISTORY_GAMES];
    int count;
    int next_id;
} game_history;

// Detect classic ECO opening name from a sequence of SAN moves (returns static string).
const char *history_detect_opening(const char (*san_moves)[16], int count);

void history_init(game_history *h);
void history_free(game_history *h);
bool history_load(const char *path, game_history *h);
bool history_save(const char *path, const game_history *h);

// Prepend a new game to history (most recent first). Returns assigned entry ID.
int history_add(game_history *h, const char *white, const char *black, const char *result,
    const char *reason, const char *opening, const char *mode, int plies, int white_elo,
    int black_elo, const char *pgn);

// Update reviewed Elo estimates for an existing entry by ID.
bool history_update_elos(game_history *h, int id, int white_elo, int black_elo);

bool history_delete_by_id(game_history *h, int id);
void history_clear(game_history *h);
const history_entry *history_find_by_id(const game_history *h, int id);

#endif
