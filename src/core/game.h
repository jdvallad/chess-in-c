#ifndef FULL_CHESS_H
#define FULL_CHESS_H

#include "core/primitives.h"

#include <stdbool.h>

full_chess *full_chess_create(void);
void full_chess_initialize(full_chess *game);
void reset(full_chess *input);
void empty(full_chess *game);
void full_chess_delete(full_chess **input);

u64 encode_string(const full_chess *game, const char *focus_move);
void make_move(full_chess *game, u64 focus_move);
void string_move(full_chess *game, const char *focus_move);
void unmake_move(full_chess *game);

void print_move(const full_chess *game, u64 focus_move);
void print_legal_moves(const full_chess *game);
void print_board(const full_chess *game);
u64 perft(full_chess *game, int depth, bool top_call);

int num_legal_moves(const full_chess *game);
int position_frequency(const full_chess *game);
u64 get_random_move(const full_chess *game);

void set_from_fen(full_chess *game, const char *fen);
char *get_fen(const full_chess *game, char *fen);
char get_piece_char_at_offset(const full_chess *game, int index);
void print_fen(const full_chess *game);

#endif
