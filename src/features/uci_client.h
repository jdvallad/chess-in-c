#ifndef UCI_H
#define UCI_H

#include "core/primitives.h"
#include "engine/engine.h"

#include <stdbool.h>
#include <stddef.h>

#define UCI_MAX_PV 10

typedef struct sf_preset {
    int level; // 7..11
    const char *name;
    const char *desc;
    bool limit_strength;
    int elo; // Stockfish UCI_Elo (1320..3190) when limit_strength is true
    int time_ms;
} sf_preset;

typedef struct uci_search_result {
    search_result base;
    u64 pv_moves[UCI_MAX_PV];
    int pv_len;
    bool ok;
} uci_search_result;

typedef struct uci_engine uci_engine;

// Locate Stockfish binary (checks explicit path, STOCKFISH_PATH, ./stockfish, $PATH, /usr/games).
bool uci_find_stockfish(const char *explicit_path, char *out_path, size_t out_size);

// Spawn a UCI engine process (e.g. Stockfish). Returns NULL if unavailable or handshake fails.
uci_engine *uci_create(const char *binary_path);
void uci_new_game(uci_engine *eng);
void uci_set_option(uci_engine *eng, const char *name, const char *value);
void uci_delete(uci_engine **eng);

const char *uci_get_name(const uci_engine *eng);
const char *uci_get_path(const uci_engine *eng);
int uci_get_min_elo(const uci_engine *eng);
int uci_get_max_elo(const uci_engine *eng);

// Apply SPSA search parameters from a JSON file (e.g. data/spsa_best.json). Returns count applied.
int uci_apply_spsa_file(uci_engine *eng, const char *json_path);

// Stockfish level presets (levels 7..11).
int uci_first_level(void);
int uci_num_presets(void);
const sf_preset *uci_get_preset(int level);
bool uci_is_stockfish_level(int level);

// Search a position with explicit strength/time settings.
uci_search_result uci_search(
    uci_engine *eng, const full_chess *game, bool limit_strength, int elo, int time_ms);

// Fast full-strength search capped by BOTH depth and movetime (for objective Stockfish referee evals).
uci_search_result uci_search_depth(
    uci_engine *eng, const full_chess *game, int depth, int time_ms);

// Search using one of the Stockfish presets (levels 7..11).
uci_search_result uci_search_level(uci_engine *eng, const full_chess *game, int level);

#endif
