#ifndef PRIMITIVES_H
#define PRIMITIVES_H
#define MAX_POSSIBLE_MOVES 218
#define MAX_GAME_LENGTH 5870
#define HASH_LENGTH_IN_CHARS
#define ROW_1 0xff00000000000000
#define NULL_MOVE 0x0
#include <stdint.h>
#include <stdbool.h>
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

typedef struct Chess
{
    bitboard pawns; // en_passant is stored on 8th rank of bits, and is_color_flipped stored in first bit
                    // 2nd bit is in_check, 3rd is is_stalemate, 4th is game_over;
    bitboard friendly_pieces;
    bitboard enemy_pieces;
    bitboard orthogonal_pieces;
    bitboard diagonal_pieces;
    bitboard kings; // castle rights stored in corners
} Chess;
#endif
