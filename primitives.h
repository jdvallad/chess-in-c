#include <stdbool.h>
#include <stdint.h>
#ifndef PRIMITIVES_H
#define PRIMITIVES_H
#define MAX_LEGAL_MOVES    512
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
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct chess {
    bitboard pawns; // ROW_1 and ROW_8 are populated with important information about the game
    // COL 1 contains is_color_flipped bools.
    // COLS 2-3 contain castle rights.
    // COLS 4-6 contain en-passant offsets.
    // COL 7 contains en-passant is_toggled bools.
    // COL 8 contains in-check bools.
    // ROW_1 contains friendly information.
    // ROW_8 contains enemy information.
    // ex: ROW_1 & COL_8 contains the bit for whether you are in check.
    // ex: ROW_8 & COL_8 contains the bit for whether the enemy is in check (aka you can capture their king).
    bitboard friendly_pieces; // all other bitboards only contain piece location information.
    bitboard enemy_pieces;
    bitboard orthogonal_pieces;
    bitboard diagonal_pieces;
    bitboard kings;
} chess;

typedef struct full_chess {
    chess board_history
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
