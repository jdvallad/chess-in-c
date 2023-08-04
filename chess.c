#include "chess.h"

#include "primitives.h"

#include <stdio.h>
#include <stdlib.h>

chess *chess_create() {
    chess *game = (chess *) malloc(sizeof(chess));
    game->pawns = ROW_2 | ROW_7 | (ENEMY & FLIPPED) | ((FRIENDLY | ENEMY) & (KINGSIDE | QUEENSIDE));
    game->friendly_pieces = ROW_1 | ROW_2;
    game->enemy_pieces = ROW_7 | ROW_8;
    game->orthogonal_pieces = (ROW_1 | ROW_8) & (COL_1 | COL_4 | COL_8);
    game->diagonal_pieces = (ROW_1 | ROW_8) & (COL_3 | COL_4 | COL_6);
    game->kings = (ROW_1 | ROW_8) & COL_5;
    return game;
}

void reset(chess *game) {
    game->pawns = ROW_2 | ROW_7 | (ENEMY & FLIPPED) | ((FRIENDLY | ENEMY) & (KINGSIDE | QUEENSIDE));
    game->friendly_pieces = ROW_1 | ROW_2;
    game->enemy_pieces = ROW_7 | ROW_8;
    game->orthogonal_pieces = (ROW_1 | ROW_8) & (COL_1 | COL_4 | COL_8);
    game->diagonal_pieces = (ROW_1 | ROW_8) & (COL_3 | COL_4 | COL_6);
    game->kings = (ROW_1 | ROW_8) & COL_5;
    return;
}

void chess_delete(chess **input) {
    if (*input) {
        free(*input);
        *input = NULL;
    }
    return;
}

void print_bitboard(u64 board) {
    printf("\n    ");
    for (int i = 7; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
            printf("%lu ", 1 & (board >> (8 * i + j)));
        }
        printf("\n    ");
    }
    printf("\n");
    return;
}

void print_game_state(chess *game) {
    printf("\n\n");
    printf("     a   b   c   d   e   f   g   h     \n");
    printf("   ---------------------------------   \n");
    for (int i = 7; i >= 0; i--) {
        printf(" %d", i + 1);
        for (int j = 0; j < 8; j++) {
            printf(" | %c", piece_to_char(get_piece_at_offset(game, 8 * i + j)));
        }
        printf(" | %d\n", i + 1);
        printf("   ---------------------------------   \n");
    }
    printf("     a   b   c   d   e   f   g   h     \n\n");
    return;
}

char piece_to_char(u8 piece) {
    switch (piece) {
    case FRIENDLY_PAWN: return 'P';
    case ENEMY_PAWN: return 'p';
    case FRIENDLY_ROOK: return 'R';
    case ENEMY_ROOK: return 'r';
    case FRIENDLY_KNIGHT: return 'N';
    case ENEMY_KNIGHT: return 'n';
    case FRIENDLY_BISHOP: return 'B';
    case ENEMY_BISHOP: return 'b';
    case FRIENDLY_QUEEN: return 'Q';
    case ENEMY_QUEEN: return 'q';
    case FRIENDLY_KING: return 'K';
    case ENEMY_KING: return 'k';
    case NONE: return ' ';
    default: return '?';
    }
}

u8 char_to_piece(char piece) {
    switch (piece) {
    case 'P': return FRIENDLY_PAWN;
    case 'p': return ENEMY_PAWN;
    case 'R': return FRIENDLY_ROOK;
    case 'r': return ENEMY_ROOK;
    case 'N': return FRIENDLY_KNIGHT;
    case 'n': return ENEMY_KNIGHT;
    case 'B': return FRIENDLY_BISHOP;
    case 'b': return ENEMY_BISHOP;
    case 'Q': return FRIENDLY_QUEEN;
    case 'q': return ENEMY_QUEEN;
    case 'K': return FRIENDLY_KING;
    case 'k': return ENEMY_KING;
    case ' ': return NONE;
    default: return NONE;
    }
}

bool chess_equal(chess *a, chess *b) {
    return (a->pawns == b->pawns) && (a->kings == b->kings)
           && (a->diagonal_pieces == b->diagonal_pieces)
           && (a->orthogonal_pieces == b->orthogonal_pieces) && (a->enemy_pieces == b->enemy_pieces)
           && (a->friendly_pieces == b->friendly_pieces);
}

u8 get_piece_at_offset(chess *game, int index) {
    u8 id = 0;
    u64 board = 1ull << index;
    id |= ((board & PAWN_MASK & game->pawns) ? 1 : 0);
    id |= ((board & game->friendly_pieces) ? 1 << 1 : 0);
    id |= ((board & game->enemy_pieces) ? 1 << 2 : 0);
    id |= ((board & game->orthogonal_pieces) ? 1 << 3 : 0);
    id |= ((board & game->diagonal_pieces) ? 1 << 4 : 0);
    id |= ((board & game->kings) ? 1 << 5 : 0);
    return id;
}

void flip_bitboard(u64 *board) {
    u64 output = 0x0;
    output |= ((ROW_1 & *board) << (8 * 7)) | ((ROW_8 & *board) >> (8 * 7)); //swap rows 1 and 8
    output |= ((ROW_2 & *board) << (8 * 5)) | ((ROW_7 & *board) >> (8 * 5)); //swap rows 2 and 7
    output |= ((ROW_3 & *board) << (8 * 3)) | ((ROW_6 & *board) >> (8 * 3)); //swap rows 3 and 6
    output |= ((ROW_4 & *board) << (8 * 1)) | ((ROW_5 & *board) >> (8 * 1)); //swap rows 4 and 5
    *board = output;
    return;
}

void swap_bitboards(u64 *board, u64 *other) {
    u64 temp = *board;
    *board = *other;
    *other = temp;
    return;
}

void flip_perspective(chess *game) {
    flip_bitboard(&(game->pawns));
    flip_bitboard(&(game->orthogonal_pieces));
    flip_bitboard(&(game->diagonal_pieces));
    flip_bitboard(&(game->friendly_pieces));
    flip_bitboard(&(game->enemy_pieces));
    flip_bitboard(&(game->kings));
    swap_bitboards(&(game->friendly_pieces), &(game->enemy_pieces));
    return;
}

bool is_same_rank(int a, int b) {
    return (a / 8) == (b / 8);
}

bool is_same_file(int a, int b) {
    return (a % 8) == (b % 8);
}

void remove_piece(chess *game, int offset) {
    u64 board = 1ull << offset;
    game->pawns &= ~(board & PAWN_MASK);
    game->friendly_pieces &= ~board;
    game->enemy_pieces &= ~board;
    game->orthogonal_pieces &= ~board;
    game->diagonal_pieces &= ~board;
    game->kings &= ~board;
    return;
}

void add_piece(chess *game, u8 piece, int offset) {
    game->pawns |= PAWN_MASK & (((piece >> 0) & 1) << offset);
    game->friendly_pieces |= ((piece >> 1) & 1) << offset;
    game->enemy_pieces |= ((piece >> 2) & 1) << offset;
    game->orthogonal_pieces |= ((piece >> 3) & 1) << offset;
    game->diagonal_pieces |= ((piece >> 4) & 1) << offset;
    game->kings |= ((piece >> 5) & 1) << offset;
}

int get_next_offset(u64 board, int offset) {
    do {
        offset += 1;
    } while (offset != -64 && ((1ull << offset) & board) == 0);
    return offset;
}

int pop_count(u64 board) {
    int ret = 0;
    while (board != 0) {
        board &= (board - 1);
        ret++;
    }
    return ret;
}

// move functions begin below

u64 encode_move(int start_offset, int end_offset, bool en_passant_capture, bool castle_king,
    bool castle_queen, u8 promotion_piece, u8 start_piece, u8 end_piece, u8 en_passant_piece,
    u8 friendly_data, u8 enemy_data) {
    u64 move = 0;
    move |= ((u64) start_offset) << 0; // 6 bits
    move |= ((u64) end_offset) << 6; // 6 bits
    move |= ((u64) en_passant_capture) << 12; // 1 bit
    move |= ((u64) castle_king << 13); // 1 bit
    move |= ((u64) castle_queen << 14); // 1 bit
    move |= ((u64) promotion_piece) << 15; // 6 bits
    move |= ((u64) start_piece) << 21; // 6 bits
    move |= ((u64) end_piece) << 27; // 6 bits
    move |= ((u64) en_passant_piece) << 33; // 6 bits
    move |= ((u64) friendly_data) << 39; // 6 bits
    move |= ((u64) enemy_data) << 45; // 6 bits
    return move;
    // the last bit is empty
}

int get_start_offset(u64 move) {
    return (move >> 0) & 63;
}

int get_end_offset(u64 move) {
    return (move >> 6) & 63;
}

bool is_en_passant_move(u64 move) {
    return (move >> 7) & 1;
}

bool is_castle_king_move(u64 move) {
    return (move >> 8) & 1;
}

bool is_castle_queen_move(u64 move) {
    return (move >> 9) & 1;
}

u8 get_promotion_piece(u64 move) {
    return (move >> 15) & 63;
}

u8 get_start_piece(u64 move) {
    return (move >> 21) & 63;
}

u8 get_end_piece(u64 move) {
    return (move >> 27) & 63;
}

u8 get_en_passant_piece(u64 move) {
    return (move >> 33) & 63;
}

u8 get_friendly_data(u64 move) {
    return (move >> 39) & 63;
}

u8 get_enemy_data(u64 move) {
    return (move >> 45) & 63;
}

int get_en_passant_offset(chess *game) {
    return 40 + (((game->pawns & ENEMY & EN_PASSANT_OFFSET) >> (59)) & 7);
}

void make_move(chess *game, u64 move) {
    int start_offset = get_start_offset(move);
    int end_offset = get_end_offset(move);
    bool ep = is_en_passant_move(move);
    bool castle_king = is_castle_king_move(move);
    bool castle_queen = is_castle_queen_move(move);
    u8 promotion_piece = get_promotion_piece(move);
    u8 start_piece = get_start_piece(move);
    if (ep) {
        remove_piece(game, get_en_passant_offset(game));
    }
    if (castle_king) {
        u8 temp_start_piece = get_piece_at_offset(game, 7);
        u8 temp_end_piece = get_piece_at_offset(game, 5);
        remove_piece(game, 7);
        remove_piece(game, 5);
        add_piece(game, temp_start_piece, 5);
        add_piece(game, temp_end_piece, 7);
    }
    if (castle_queen) {
        u8 temp_start_piece = get_piece_at_offset(game, 0);
        u8 temp_end_piece = get_piece_at_offset(game, 3);
        remove_piece(game, 0);
        remove_piece(game, 3);
        add_piece(game, temp_start_piece, 3);
        add_piece(game, temp_end_piece, 0);
    }
    remove_piece(game, start_offset);
    remove_piece(game, end_offset);
    add_piece(game, promotion_piece, end_offset);
    if (start_offset == 0) {
        game->pawns &= ~(FRIENDLY & QUEENSIDE);
    }
    if (start_offset == 4) {
        game->pawns &= ~(FRIENDLY & (QUEENSIDE | KINGSIDE));
    }
    if (start_offset == 7) {
        game->pawns &= ~(FRIENDLY & KINGSIDE);
    }
    if (end_offset == 0) {
        game->pawns &= ~(FRIENDLY & QUEENSIDE);
    }
    if (end_offset == 4) {
        game->pawns &= ~(FRIENDLY & (QUEENSIDE | KINGSIDE));
    }
    if (end_offset == 7) {
        game->pawns &= ~(FRIENDLY & KINGSIDE);
    }

    if (start_offset == 0 + 56) {
        game->pawns &= ~(ENEMY & QUEENSIDE);
    }
    if (start_offset == 4 + 56) {
        game->pawns &= ~(ENEMY & (QUEENSIDE | KINGSIDE));
    }
    if (start_offset == 7 + 56) {
        game->pawns &= ~(ENEMY & KINGSIDE);
    }
    if (end_offset == 0 + 56) {
        game->pawns &= ~(ENEMY & QUEENSIDE);
    }
    if (end_offset == 4 + 56) {
        game->pawns &= ~(ENEMY & (QUEENSIDE | KINGSIDE));
    }
    if (end_offset == 7 + 56) {
        game->pawns &= ~(ENEMY & KINGSIDE);
    }
    if (start_piece == FRIENDLY_PAWN && start_offset < 16 && end_offset > 23) {
        game->pawns |= (FRIENDLY & EN_PASSANT_TOGGLED);
        game->pawns |= (end_offset % 8) << 3;
    }
    game->pawns &= ~(ENEMY & EN_PASSANT_TOGGLED);
    game->pawns &= ~(ENEMY & EN_PASSANT_OFFSET);
    flip_perspective(game);
}

void print_move(u64 move) {
    int start_offset = get_start_offset(move);
    int end_offset = get_end_offset(move);
    printf("%c", 'a' + (start_offset % 8));
    printf("%c", '1' + (start_offset / 8));
    printf("%c", 'a' + (end_offset % 8));
    printf("%c", '1' + (end_offset / 8));
    u8 promotion_piece = get_promotion_piece(move);
    u8 starting_piece = get_start_piece(move);
    if (starting_piece != promotion_piece) {
        printf("%c", piece_to_char(promotion_piece));
    }
    return;
}
