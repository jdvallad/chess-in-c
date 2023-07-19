#ifndef PRIMITIVES_H
#define PRIMITIVES_H
#define MAX_POSSIBLE_MOVES 512
#define MAX_GAME_LENGTH 5870
#include <stdint.h>
#include <stdbool.h>
#define ROW_1 0xff00000000000000
#define ROW_2 0x00ff000000000000
#define ROW_3 0x0000ff0000000000
#define ROW_4 0x000000ff00000000
#define ROW_5 0x00000000ff000000
#define ROW_6 0x0000000000ff0000
#define ROW_7 0x000000000000ff00
#define ROW_8 0x00000000000000ff
#define PAWN_MASK 0xff000000000000ff
#define NULL_MOVE 0x0
#define MAX_FEN_LENGTH 100

typedef uint64_t bitboard;
typedef int8_t offset;
typedef uint16_t move;
typedef enum piece
{
    rook = 5,
    knight = 1,
    bishop = 3,
    queen = 7,
    king = 9,
    pawn = 17,
    none = 0
} piece;
typedef struct offset_set
{
    uint64_t board;
    uint8_t size;
    uint8_t index;
    uint8_t remaining;
} offset_set;

typedef uint64_t bitboard;

typedef int8_t offset;

typedef struct chess
{
    bitboard pawns; // en_passant is stored on 8th rank of bits, and is_color_flipped stored in first bit
    // castle rights stored in next 4 bits
    // is_color_flipped = game->pawns & 1
    // K = game->pawns & 2
    // Q = game->pawns & 4
    // k = game->pawns & 8
    // q = game->pawns & 16

    bitboard friendly_pieces;
    bitboard enemy_pieces;
    bitboard orthogonal_pieces;
    bitboard diagonal_pieces;
    bitboard kings;
} chess;

typedef struct full_chess
{
    chess board_history[MAX_GAME_LENGTH];
    chess *current_board;
    int move_count;
    int half_move_count;
    move legal_moves_history[MAX_POSSIBLE_MOVES][MAX_GAME_LENGTH];
    move *current_legal_moves;
    bool is_static;
} full_chess;

#endif
