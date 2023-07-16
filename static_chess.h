#ifndef STATIC_CHESS_H
#define STATIC_CHESS_H
#include "primitives.h"
void static_bit_move(chess *game, char *focus_move);

void static_display_move(char *focus_move, chess *game,
                                 move *legal_moves, int *game_length,
                                 chess *past_boards);

void static_undo_move(chess *game, move *legal_moves, int *game_length,
               chess *past_boards);

void static_initialize_game(chess *game, move *legal_moves, chess *past_boards);

void static_print_game_state(chess *game);

move static_encode_string_move(chess *game, char *focus_move);

void static_print_legal_moves(chess *game, move *legal_moves);

#endif
