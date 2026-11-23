#ifndef CHESS_H
#define CHESS_H

#include "core/primitives.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

chess *chess_create(void);
void chess_reset(chess *game);
void chess_empty(chess *game);
void chess_delete(chess **input);
void print_bitboard(u64 board);
void print_game_state(chess *game);
char piece_to_char(u8 piece);
u8 char_to_piece(char piece);
bool chess_equal(const chess *a, const chess *b);
bool is_flipped(const chess *game);
bool is_insufficient_material(const chess *game);
u8 get_piece_at_offset(const chess *game, int index);
void flip_bitboard(u64 *board);
void swap_bitboards(u64 *board, u64 *other);
void flip_perspective(chess *game);
bool is_same_rank(int a, int b);
bool is_same_file(int a, int b);
void remove_piece(chess *game, int offset);
void add_piece(chess *game, u8 piece, int offset);
int get_next_offset(u64 board, int offset);
int pop_count(u64 board);
u64 compass(u64 board, int dx, int dy);

// Reversible 64-bit move functions
u64 encode_move(int start_offset, int end_offset, bool en_passant_capture, bool castle_king,
    bool castle_queen, u8 promotion_piece, u8 start_piece, u8 end_piece, u8 en_passant_piece,
    u8 friendly_data, u8 enemy_data);
u64 build_move(const chess *game, int start_offset, int end_offset, bool en_passant_capture,
    bool castle_king, bool castle_queen, u8 promotion_piece);
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
int get_en_passant_offset(const chess *game);
bool has_en_passant(const chess *game);
void chess_make_move(chess *game, u64 move);
void chess_unmake_move(chess *game, u64 move);
void chess_print_move(u64 move);

#endif
