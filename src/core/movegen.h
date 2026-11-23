#ifndef LEGAL_MOVES_H
#define LEGAL_MOVES_H

#include "core/primitives.h"

#include <stdbool.h>

int set_pseudo_legal_moves(const chess *game, u64 *legal_moves);
int set_legal_moves(chess *game, u64 *legal_moves);
int set_tactical_moves(chess *game, u64 *tactical_moves);
int set_quiet_moves(chess *game, u64 *quiet_moves);
u64 validate_packed_move(chess *game, u16 tag);
bool is_legal_move(chess *game, u64 focus_move);
int trim_legal_moves(chess *game, u64 *legal_moves);

int add_single_pawn_pushes(const chess *game, u64 *legal_moves, int legal_moves_count);
int add_double_pawn_pushes(const chess *game, u64 *legal_moves, int legal_moves_count);
int add_left_pawn_captures(const chess *game, u64 *legal_moves, int legal_moves_count);
int add_right_pawn_captures(const chess *game, u64 *legal_moves, int legal_moves_count);
int add_en_passant_capture(const chess *game, u64 *legal_moves, int legal_moves_count);
int add_knight_moves(const chess *game, u64 *legal_moves, int legal_moves_count);
int add_diagonal_moves(const chess *game, u64 *legal_moves, int legal_moves_count);
int add_orthogonal_moves(const chess *game, u64 *legal_moves, int legal_moves_count);
int add_king_moves(const chess *game, u64 *legal_moves, int legal_moves_count);
int add_castling_moves(const chess *game, u64 *legal_moves, int legal_moves_count);

u64 get_king_destination_bitboard(const chess *game, int king_index);
u64 get_knight_destination_bitboard(const chess *game, int start_index);
u64 get_diagonal_destination_bitboard(const chess *game, int start_index);
u64 get_orthogonal_destination_bitboard(const chess *game, int start_index);

bool in_set(const u64 *legal_moves, u64 focus_move);
bool attacked_offset(const chess *game, int focus_offset);
bool attacking_offset(const chess *game, int focus_offset);
u64 get_attacked_board(const chess *game);
u64 get_attacking_board(const chess *game);
bool in_check(const chess *game);

#endif
