#ifndef STATIC_CHESS_H
#define STATIC_CHESS_H
#include "primitives.h"

void static_string_move(full_chess *game, char *focus_move);
void static_print_move(full_chess *game, move focus_move);
void static_print_legal_moves(full_chess *game);
void static_print_board(full_chess *game);
int static_perft(full_chess *game, int depth, bool top_call);
#endif
