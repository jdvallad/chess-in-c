#ifndef STATIC_CHESS_H
#define STATIC_CHESS_H

#include "core/primitives.h"

#include <stdbool.h>
#include <stddef.h>

u64 static_encode_string(const full_chess *game, const char *focus_move);
void static_string_move(full_chess *game, const char *focus_move);
void static_print_move(const full_chess *game, u64 focus_move);
void static_print_legal_moves(const full_chess *game);
void static_print_board(const full_chess *game);
u64 static_perft(full_chess *game, int depth, bool top_call);

// Standard Algebraic Notation (SAN) formatting & parsing (e.g. Nf3, exd5, O-O, e8=Q+, Qxf7#)
char *static_move_to_san(
    const chess *board, const u64 *legal_moves, u64 move, char *buf, size_t buf_size);
void static_print_san_move(const full_chess *game, u64 focus_move);
void static_print_san_legal_moves(const full_chess *game);
u64 static_encode_san(const full_chess *game, const char *san, int *match_count);

// Portable Game Notation (PGN) export up to the current ply
char *static_get_pgn(const full_chess *game, const char *white_name, const char *black_name,
    char *buf, size_t buf_size);
void static_print_pgn(const full_chess *game, const char *white_name, const char *black_name);

#endif
