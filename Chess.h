#ifndef CHESS_H
#define CHESS_H
#include "chess.h"
#include "primitives.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

chess *chess_create();
void reset(chess *game);
void chess_delete(chess **input);
void print_bitboard(u64 board);
void print_game_state(chess *game);
char piece_to_char(u8 piece);
u8 char_to_piece(char piece);
bool chess_equal(chess *a, chess *b);
u8 get_piece_at_offset(chess *game, int index);
void flip_bitboard(u64 *board);
void swap_bitboards(u64 *board, u64 *other);
void flip_perspective(chess *game);
bool is_same_rank(int a, int b);
bool is_same_file(int a, int b);
void remove_piece(chess *game, int offset);
void add_piece(chess *game, u8 piece, int offset);
int get_next_offset(u64 board, int offset);
int pop_count(u64 board);
// move functions begin below

u64 encode_move(int start_offset, int end_offset, bool en_passant_capture, bool castle_king,
    bool castle_queen, u8 promotion_piece, u8 start_piece, u8 end_piece, u8 en_passant_piece,
    u8 friendly_data, u8 enemy_data);
int get_start_offset(u64 move);
int get_end_offset(u64 move);
bool is_en_passant_move(u64 move);
bool is_castle_king_move(u64 move);
bool is_castle_queen_move(u64 move);
u8 get_promotion_piece(u64 move);
u8 get_start_piece(u64 move);
u8 get_end_piece(u64 move);
u8 get_en_passant_piece(u64 move);
u8 get_friendly_data(u64 move);
u8 get_enemy_data(u64 move);
int get_en_passant_offset(chess *game);
void make_move(chess *game, u64 move);
void print_move(u64 move);
#endif
