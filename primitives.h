#include <stdbool.h>
#include <stdint.h>
#ifndef PRIMITIVES_H
#define PRIMITIVES_H
#define MAX_POSSIBLE_MOVES 512
#define MAX_GAME_LENGTH    5870
#define NULL_MOVE          0x0
#define MAX_FEN_LENGTH     100
#define ROW_1              0xff
#define ROW_2              0xff00
#define ROW_3              0xff0000
#define ROW_4              0xff000000
#define ROW_5              0xff00000000
#define ROW_6              0xff0000000000
#define ROW_7              0xff000000000000
#define ROW_8              0xff00000000000000
#define COL_1              0x101010101010101
#define COL_2              0x202020202020202
#define COL_3              0x404040404040404
#define COL_4              0x808080808080808
#define COL_5              0x1010101010101010
#define COL_6              0x2020202020202020
#define COL_7              0x4040404040404040
#define COL_8              0x8080808080808080
#define FRIENDLY           ROW_1
#define ENEMY              ROW_8
#define FLIPPED            COL_1
#define KINGSIDE           COL_2
#define QUEENSIDE          COL_3
#define EN_PASSANT_OFFSET  COL_4 | COL_5 | COL_6
#define EN_PASSANT_TOGGLED COL_7
#define IN_CHECK           COL_8
#define PAWN_MASK          ROW_2 | ROW_3 | ROW_4 | ROW_5 | ROW_6 | ROW_7
#define ROOK               5
#define KNIGHT             1
#define BISHOP             3
#define QUEEN              7
#define KING               9
#define PAWN               17
#define NONE               0

typedef uint64_t bitboard;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct chess {
    bitboard
        pawns; // en_passant is stored on 8th rank of bits, and is_color_flipped stored in first bit
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

typedef struct full_chess {
    chess board_history[MAX_GAME_LENGTH];
    chess *current_board;
    int move_count;
    int half_move_count;
    move legal_moves_history[MAX_POSSIBLE_MOVES][MAX_GAME_LENGTH];
    move *current_legal_moves;
    bool is_static;
} full_chess;

#endif
