#ifndef PRIMITIVES_H
#define PRIMITIVES_H
#define MAX_POSSIBLE_MOVES 218
#define MAX_GAME_LENGTH 5870
#define HASH_LENGTH_IN_CHARS
#define ROW_1 0xff00000000000000
#include <stdint.h>
#include <stdbool.h>
typedef unsigned __int128 uint128_t;
typedef uint64_t bitboard;
typedef int8_t offset;
typedef uint16_t move;
typedef uint128_t hash[3];
typedef bool positionCount[2];
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

typedef struct MoveSet
{
    char temp;
} MoveSet;

typedef struct Chess
{
    // These are the fields relevant for hashing
    bitboard pawns; // en_passant is stored on 1st rank of bits, and is_color_flipped stored in first bit
    bitboard friendly_pieces;
    bitboard enemy_pieces;
    bitboard orthogonal_pieces;
    bitboard diagonal_pieces;
    bitboard kings; // castle rights stored in corners
    //
    move legal_moves[218];
    bool game_over;
    bool is_stalemate;
    bool in_check;
    uint8_t half_move_count;
    uint16_t move_count;
    uint16_t unique_positions;
    hash game_hash_list[MAX_GAME_LENGTH];
    positionCount position_occurences[MAX_GAME_LENGTH];

} Chess;
#endif
