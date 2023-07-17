#ifndef CHESS_H
#define CHESS_H
#include <stdint.h>
#include <stdbool.h>
#include "primitives.h"
bitboard VERTICAL_OFFSETS_BITBOARDS[17];
bitboard HORIZONTAL_OFFSETS_BITBOARDS[17];
bitboard offset_to_bitboard(offset input);
chess *chess_create();
void chess_delete(chess **input);
void print_bitboard(bitboard board);
bitboard compass(bitboard input, offset horizontal, offset vertical);
void print_game_state(chess *input);
piece get_piece_at_offset(chess *game, offset index);
void bit_move(chess *game, move the_move);
void remove_from_board(chess *game, piece focus_piece, offset square);
void add_to_board(chess *game, piece focus_piece, offset square);
move encode_move(offset starting_offset, offset ending_offset, piece promotion_piece);
bitboard castle_rights(chess *game);
void flip_perspective(chess *game);
bool is_same_rank(offset a, offset b);
bool is_same_file(offset a, offset b);
void print_offset_table();
move encode_string_move(char *focus_move);
bitboard flip_bitboard(bitboard board);
void make_move(chess *game, char *focus_move);
int set_pseudo_legal_moves(chess *game,move *legal_moves,int index_offset);
int set_legal_moves(chess *game, move *legal_moves, int index_offset);
chess *clone(chess *game);
void draw(chess *game);
void set_from_source(chess *dest, chess *source);
void reset(chess *output);
offset get_next_offset(bitboard board, offset index);
bool in_set(move *legal_moves, move focus_move);
void print_legal_moves(chess *game, move *legal_moves, int index_offset);
void display_move(char *focus_move, chess *game, move *legal_moves,
                          int *game_length, chess *past_boards);
void undo_move(chess *game, move *legal_moves, int *game_length,
                      chess *past_boards);
void initialize_game(chess *game, move *legal_moves,
                            chess *past_boards) ;
int add_en_passant_capture(chess *game, move *legal_moves, int legal_moves_count);
int add_castling_moves(chess *game, move *legal_moves, int legal_moves_count);
int add_double_pawn_pushes(chess *game, move *legal_moves, int legal_moves_count);
int add_single_pawn_pushes(chess *game, move *legal_moves, int legal_moves_count);
int add_left_pawn_captures(chess *game, move *legal_moves, int legal_moves_count);
int add_right_pawn_captures(chess *game, move *legal_moves, int legal_moves_count);

int add_king_moves(chess *game, move *legal_moves, int legal_moves_count);
int add_diagonal_moves(chess *game, move *legal_moves, int legal_moves_count);
int add_orthogonal_moves(chess *game, move *legal_moves, int legal_moves_count);
int add_knight_moves(chess *game, move *legal_moves, int legal_moves_count);
bitboard true_pawns(chess *game);
bitboard true_kings(chess *game);
bitboard get_knight_destination_bitboard(chess *game,offset start_index);
bitboard get_diagonal_destination_bitboard(chess *game, offset start_index);
bitboard get_orthogonal_destination_bitboard(chess *game, offset start_index);
bool is_legal_move(chess *game, move focus_move,move *legal_moves, int index_offset);
#endif
