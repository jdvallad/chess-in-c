#ifndef CHESS_H
#define CHESS_H
#include <stdint.h>
#include <stdbool.h>
#include "primitives.h"
bitboard VERTICAL_OFFSETS_BITBOARDS[17];
bitboard HORIZONTAL_OFFSETS_BITBOARDS[17];
bitboard offset_to_bitboard[64];
chess *chess_create();
void reset(chess *game);
void chess_delete(chess **input);
void print_bitboard(bitboard board);
bitboard compass(bitboard input, offset horizontal, offset vertical);
void print_game_state(chess *game);
bool chess_equal(chess *a, chess *b);
move encode_string(char *focus_move);
piece get_piece_at_offset(chess *game, offset index);
move encode_move(offset starting_offset, offset ending_offset,
                 piece promotion_piece);
bitboard flip_bitboard(bitboard board);
void flip_perspective(chess *game);
offset get_starting_offset(move focus_move);
offset get_ending_offset(move focus_move);
piece get_starting_piece(chess *game, move focus_move);
piece get_ending_piece(chess *game, move focus_move);
piece get_promotion_piece(move focus_move);
bool is_castle(chess *game, move focus_move);
bool is_promotion(chess *game, move focus_move);
bool is_en_passant(chess *game, move focus_move);
void castle_move(chess *game, move the_move);
void en_passant_move(chess *game, move focus_move);
void promotion_move(chess *game, move focus_move);
void standard_move(chess *game, move focus_move);
bool bit_move(chess *game, move the_move);
bool is_same_rank(offset a, offset b);
bool is_same_file(offset a, offset b);
void remove_piece(chess *game, offset focus_offset);
void add_piece(chess *game, piece focus_piece, bool is_friendly, offset focus_offset);
void move_piece(chess *game, offset start_offset, offset end_offset);
offset get_next_offset(bitboard board, offset index);
move random_bit_move(chess *game);
int random_in_range(int min, int max);
int pop_count(bitboard board);
void print_move(chess *game, move focus_move);
#endif
