#include <stdbool.h>
#include <stdint.h>
#ifndef PRIMITIVES_H
#define PRIMITIVES_H
#define MAX_LEGAL_MOVES        512
#define MAX_GAME_LENGTH        5870
#define NULL_MOVE              0x0
#define MAX_FEN_LENGTH         100
#define ROW_1                  0xff
#define ROW_2                  0xff00
#define ROW_3                  0xff0000
#define ROW_4                  0xff000000
#define ROW_5                  0xff00000000
#define ROW_6                  0xff0000000000
#define ROW_7                  0xff000000000000
#define ROW_8                  0xff00000000000000
#define COL_1                  0x101010101010101
#define COL_2                  0x202020202020202
#define COL_3                  0x404040404040404
#define COL_4                  0x808080808080808
#define COL_5                  0x1010101010101010
#define COL_6                  0x2020202020202020
#define COL_7                  0x4040404040404040
#define COL_8                  0x8080808080808080
#define FRIENDLY               ROW_1
#define ENEMY                  ROW_8
#define FLIPPED                COL_1
#define KINGSIDE               COL_2
#define QUEENSIDE              COL_3
#define EN_PASSANT_OFFSET      COL_4 | COL_5 | COL_6
#define EN_PASSANT_TOGGLED     COL_7
#define IN_CHECK               COL_8
#define PAWN_MASK              ROW_2 | ROW_3 | ROW_4 | ROW_5 | ROW_6 | ROW_7
#define FRIENDLY_PIECES_MASK   ~0
#define ENEMY_PIECES_MASK      ~0
#define ORTHOGONAL_PIECES_MASK ~0
#define DIAGONAL_PIECES_MASK   ~0
#define KINGS_MASK             ~0
#define NONE 0
#define FRIENDLY_PAWN 1
#define ENEMY_PAWN 2
#define FRIENDLY_ROOK 3
#define ENEMY_ROOK 4
#define FRIENDLY_KNIGHT 5
#define ENEMY_KNIGHT 6
#define FRIENDLY_BISHOP 7
#define ENEMY_BISHOP 8
#define FRIENDLY_QUEEN 9
#define ENEMY_QUEEN 10
#define FRIENDLY_KING 11
#define ENEMY_KING 12

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct chess {
    u64 pawns, friendly_pieces, enemy_pieces, orthogonal_pieces, diagonal_pieces, kings;
} chess;

typedef struct full_chess {
   chess game_history
        [MAX_GAME_LENGTH]; // all past board positions, NOT DELIMITED. use move_count to index.
    u32 move_history
        [MAX_GAME_LENGTH]; // all moves made thus far, NOT DELIMITED. use move_count to index.
    u32 current_legal_moves
        [MAX_LEGAL_MOVES]; // the current legal move list, delimited by a NULL_MOVE.
    chess *game; // the game! is baller.
    u16 move_count; // number of moves, at init has value 0 (should really be called ply count).
    u8 half_move_count; // keeps track of moves since last caputure/pawn push.
    bool
        is_static; // if true, printing boards, printing moves, and inputting moves will be consistent with true chess.
    bool
        is_draw; // this gets enabled due to 7-fold repetition, or due to 75 move rule, or due to stalemate.
    // if the 75 move rule activates as checkmate is delivered, this flag will be FALSE.
} full_chess;

#endif
