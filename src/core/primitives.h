#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_LEGAL_MOVES 512
#define MAX_GAME_LENGTH 5870
#define NULL_MOVE       0ull
#define MAX_FEN_LENGTH  100

#define ROW_1 0xffull
#define ROW_2 0xff00ull
#define ROW_3 0xff0000ull
#define ROW_4 0xff000000ull
#define ROW_5 0xff00000000ull
#define ROW_6 0xff0000000000ull
#define ROW_7 0xff000000000000ull
#define ROW_8 0xff00000000000000ull

#define COL_1 0x0101010101010101ull
#define COL_2 0x0202020202020202ull
#define COL_3 0x0404040404040404ull
#define COL_4 0x0808080808080808ull
#define COL_5 0x1010101010101010ull
#define COL_6 0x2020202020202020ull
#define COL_7 0x4040404040404040ull
#define COL_8 0x8080808080808080ull

// Metadata packed into unused rank 1 (friendly) and rank 8 (enemy) bits of `pawns`.
#define FRIENDLY           ROW_1
#define ENEMY              ROW_8
#define FLIPPED            COL_1
#define KINGSIDE           COL_2
#define QUEENSIDE          COL_3
#define EN_PASSANT_OFFSET  (COL_4 | COL_5 | COL_6)
#define EN_PASSANT_TOGGLED COL_7
#define IN_CHECK           COL_8

#define PAWN_MASK              (ROW_2 | ROW_3 | ROW_4 | ROW_5 | ROW_6 | ROW_7)
#define LIGHT_SQUARES          0x55aa55aa55aa55aaull
#define DARK_SQUARES           0xaa55aa55aa55aa55ull
#define FRIENDLY_PIECES_MASK   (~0ull)
#define ENEMY_PIECES_MASK      (~0ull)
#define ORTHOGONAL_PIECES_MASK (~0ull)
#define DIAGONAL_PIECES_MASK   (~0ull)
#define KINGS_MASK             (~0ull)

// 6-bit piece bitmasks matching the 6 bitboards in `struct chess`:
// bit 0: pawns, bit 1: friendly, bit 2: enemy, bit 3: orthogonal, bit 4: diagonal, bit 5: kings.
#define NONE            0
#define FRIENDLY_KNIGHT (1 << 1)
#define FRIENDLY_PAWN   ((1 << 1) | (1 << 0))
#define ENEMY_KNIGHT    (1 << 2)
#define ENEMY_PAWN      ((1 << 2) | (1 << 0))
#define FRIENDLY_ROOK   ((1 << 1) | (1 << 3))
#define ENEMY_ROOK      ((1 << 2) | (1 << 3))
#define FRIENDLY_BISHOP ((1 << 1) | (1 << 4))
#define ENEMY_BISHOP    ((1 << 2) | (1 << 4))
#define FRIENDLY_QUEEN  ((1 << 1) | (1 << 3) | (1 << 4))
#define ENEMY_QUEEN     ((1 << 2) | (1 << 3) | (1 << 4))
#define FRIENDLY_KING   ((1 << 1) | (1 << 5))
#define ENEMY_KING      ((1 << 2) | (1 << 5))

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct chess {
    u64 pawns, friendly_pieces, enemy_pieces, orthogonal_pieces, diagonal_pieces, kings;
} chess;

typedef struct full_chess {
    chess game_history[MAX_GAME_LENGTH]; // past board positions, indexed 0..move_count.
    u64 move_history[MAX_GAME_LENGTH]; // all moves made thus far, indexed 0..move_count-1.
    u8 half_move_history[MAX_GAME_LENGTH]; // half_move_count at each ply for unmake_move.
    u64 current_legal_moves[MAX_LEGAL_MOVES]; // current legal moves, delimited by NULL_MOVE.
    chess current_board; // active board state.
    chess *game; // pointer to active board state (&current_board).
    u16 move_count; // number of plies played since start/FEN load.
    u16 start_full_move; // FEN full-move number at move_count == 0.
    u8 half_move_count; // half-moves since last capture or pawn push.
    bool start_black_to_move; // true if initial/FEN position had black to move.
    bool is_static; // if true, printing and input use White-at-bottom chess coordinates.
    bool is_draw; // enabled on 5-fold repetition, 75-move rule, or stalemate (false on mate).
    bool is_game_over; // true on checkmate or draw.
} full_chess;

#endif
