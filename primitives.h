#ifndef PRIMITIVES_H
#define PRIMITIVES_H
#define MAX_POSSIBLE_MOVES 512
#define MAX_GAME_LENGTH 5870
#define HASH_LENGTH_IN_CHARS
#define ROW_1 0xff00000000000000
#define NULL_MOVE 0x0
#define MAX_FEN_LENGTH 100
#include <stdint.h>
#include <stdbool.h>
typedef uint64_t bitboard;
typedef int8_t offset;
typedef uint16_t move;
extern bitboard offset_to_bitboard[64];
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
                    // 2nd bit is in_check, 3rd is is_stalemate, 4th is game_over;
    bitboard friendly_pieces;
    bitboard enemy_pieces;
    bitboard orthogonal_pieces;
    bitboard diagonal_pieces;
    bitboard kings; // castle rights stored in corners
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
