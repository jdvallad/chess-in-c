#ifndef CHESS_H
#define CHESS_H
#include <stdint.h>
#include <stdbool.h>
#include "MoveSet.h"
#include "offset_set.h"
#include "primitives.h"
bitboard VERTICAL_OFFSETS_BITBOARDS[17];
bitboard HORIZONTAL_OFFSETS_BITBOARDS[17];
bitboard offset_to_bitboard(offset input);
offset_set *bitboard_to_offset_set(bitboard board);
Chess *chess_create();
void chess_delete(Chess **input);
void print_bitboard(bitboard board);
bitboard compass(bitboard input, offset horizontal, offset vertical);
void print_game_state(Chess *input);
piece get_piece_at_offset(Chess *game, offset index);
void bit_move(Chess *game, move the_move);
void remove_from_board(Chess *game, piece focus_piece, offset square);
void add_to_board(Chess *game, piece focus_piece, offset square);
move encode_move(offset starting_offset, offset ending_offset, piece promotion_piece);
#endif
