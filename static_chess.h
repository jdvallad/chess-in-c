#ifndef STATIC_CHESS_H
#define STATIC_CHESS_H
#include "primitives.h"
void static_bit_move(Chess *game, char *focus_move);

void static_display_move(char *focus_move, Chess *game,
                                 move *legal_moves, int *game_length,
                                 Chess *past_boards);

void static_undo_move(Chess *game, move *legal_moves, int *game_length,
               Chess *past_boards);

void static_initialize_game(Chess *game, move *legal_moves, Chess *past_boards);

void static_print_game_state(Chess *game);

move static_encode_string_move(Chess *game, char *focus_move);

void static_print_legal_moves(Chess *game, move *legal_moves);

#endif
