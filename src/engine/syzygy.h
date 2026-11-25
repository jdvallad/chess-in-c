#ifndef ENGINE_SYZYGY_H
#define ENGINE_SYZYGY_H

#include "core/primitives.h"

#include <stdbool.h>

#define SYZYGY_DEFAULT_DIR "data/syzygy"

// Inspect a directory for Syzygy 3/4/5-piece .rtbw/.rtbz tablebase files.
// Returns max piece count found (0 if none, 3, 4, or 5).
int syzygy_scan_dir(const char *dir_path);

// Quick check whether 5-piece (or 3/4-piece) Syzygy files are installed in `data/syzygy`.
int syzygy_installed_pieces(void);

// Count total pieces on board (including both kings).
static inline int syzygy_piece_count(const chess *b) {
    return __builtin_popcountll(b->friendly_pieces | b->enemy_pieces);
}

#endif
