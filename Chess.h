#ifndef CHESS_H
#define CHESS_H
#include <stdint.h>
#include <stdbool.h>
#include "primitives.h"
bitboard VERTICAL_OFFSETS_BITBOARDS[17];
bitboard HORIZONTAL_OFFSETS_BITBOARDS[17];
bitboard offset_to_bitboard(offset input);
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
bitboard castle_rights(Chess *game);
void flip_perspective(Chess *game);
bool is_same_rank(offset a, offset b);
bool is_same_file(offset a, offset b);
void print_offset_table();
move encode_string_move(char *focus_move);
bitboard flip_bitboard(bitboard board);
void make_move(Chess *game, char *focus_move);
void set_legal_moves(Chess *game,move *legal_moves);
Chess *clone(Chess *game);
void draw(Chess *game);
void set_from_source(Chess *dest, Chess *source);
void reset(Chess *output);
offset get_next_offset(bitboard board, offset index);
bool in_set(move *legal_moves, move focus_move);
void print_legal_moves(Chess *game, move *legal_moves);
#endif
