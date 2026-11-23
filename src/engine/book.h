#ifndef BOOK_H
#define BOOK_H

#include "core/primitives.h"

#include <stdbool.h>

// Probe opening book for the current position.
// Checks optional Polyglot .bin book (path or "./book.bin") first, then built-in master repertoire.
// Returns a legal u64 move or NULL_MOVE if out of book.
u64 book_probe(const full_chess *game, const char *polyglot_path, u64 *rng_state);

// Compute standard Polyglot 64-bit Zobrist hash (for .bin book lookups & testing).
u64 book_polyglot_hash(const full_chess *game);

#endif
