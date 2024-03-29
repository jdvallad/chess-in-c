#include "primitives.h"
#ifndef FULL_CHESS_H
#define FULL_CHESS_H

void make_move(full_chess *game, move focus_move);
void string_move(full_chess *game, char *focus_move);
void unmake_move(full_chess *game);
void print_move(full_chess *game, move focus_move);
void print_legal_moves(full_chess *game);
void print_board(full_chess *game);
int perft(full_chess *game, int depth, bool top_call);
full_chess *full_chess_create();
void reset(full_chess *input);
void full_chess_delete(full_chess **input);
int num_legal_moves(full_chess *game);
int position_frequency(full_chess *game);
move get_random_move(full_chess *game);
void full_chess_initialize(full_chess *game);
void set_from_fen(full_chess *game, char *fen);
char * get_fen(full_chess *game, char *fen);
char get_piece_char_at_offset(full_chess *game, offset index);
void print_fen(full_chess *game);
void empty(full_chess *game);
#endif
