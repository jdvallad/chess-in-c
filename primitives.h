#ifndef PRIMITIVES_H
#define PRIMITIVES_H
#define MAX_POSSIBLE_MOVES 218
#define MAX_GAME_LENGTH 5900
#define HASH_LENGTH_IN_CHARS
#include <stdint.h>
#include <stdbool.h>
typedef unsigned __int128 uint128_t;
typedef uint64_t bitboard;
typedef int8_t offset;
typedef uint16_t move;
typedef uint8_t hash[41];
typedef enum piece{rook = 5, knight = 1, bishop = 3, queen = 7, king = 9, pawn = 17, none = 0} piece;
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
    bitboard pawns; // en_passant is stored on 1st rank of bits
    bitboard friendly_pieces;
    bitboard enemy_pieces;
    bitboard orthogonal_pieces;
    bitboard diagonal_pieces;
    uint8_t kings[2]; //castle rights stored in last 2 bits of each king
    bool is_color_flipped; // for hashing, this is stored in the first bit of pawns
    //
    move legal_moves[218];
    bool game_over;
    bool is_stalemate;
    bool in_check;
    uint8_t half_move_count;
    uint16_t move_count;
    hash game_hash_list[MAX_GAME_LENGTH];
} Chess;
#endif
