#include "chess.h"

#include "primitives.h"

#include <stdio.h>
#include <stdlib.h>

chess *chess_create() {
    chess *game = (chess *) malloc(sizeof(chess));
    game->kings = (ROW_1 | ROW_8) & COL_5;
    game->friendly_pieces = ROW_1 | ROW_2;
    game->enemy_pieces = ROW_7 | ROW_8;
    game->diagonal_pieces = (ROW_1 | ROW_8) & (COL_3 | COL_4 | COL_6);
    game->orthogonal_pieces = (ROW_1 | ROW_8) & (COL_1 | COL_4 | COL_8);
    game->pawns = ROW_2 | ROW_7;
    game->pawns &= ~(FRIENDLY & FLIPPED); // friendly perspective not color-flipped
    game->pawns &= ~((FRIENDLY | ENEMY) & EN_PASSANT); // neither player has en-passant flag
    game->pawns |= ENEMY & FLIPPED; // enemy position is color-flipped
    game->pawns |= (FRIENDLY | ENEMY) & (KINGSIDE | QUEENSIDE); // add all castling flags
    return game;
}

void reset(chess *game) {
    game->kings = (ROW_1 | ROW_8) & COL_5;
    game->friendly_pieces = ROW_1 | ROW_2;
    game->enemy_pieces = ROW_7 | ROW_8;
    game->diagonal_pieces = (ROW_1 | ROW_8) & (COL_3 | COL_4 | COL_6);
    game->orthogonal_pieces = (ROW_1 | ROW_8) & (COL_1 | COL_4 | COL_8);
    game->pawns = ROW_2 | ROW_7;
    game->pawns &= ~(FRIENDLY & FLIPPED); // friendly perspective not color-flipped
    game->pawns &= ~((FRIENDLY | ENEMY) & EN_PASSANT); // neither player has en-passant flag
    game->pawns |= ENEMY & FLIPPED; // enemy position is color-flipped
    game->pawns |= (FRIENDLY | ENEMY) & (KINGSIDE | QUEENSIDE); // add all castling flags
    return;
}

void chess_delete(chess **input) {
    if (*input) {
        free(*input);
        *input = NULL;
    }
    return;
}

void print_bitboard(bitboard board) {
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
            char square = game->friendly_pieces & (1ull << (8 * i + j)) ? 0 : 32;
            switch (get_piece_at_offset(game, 8 * i + j)) {
            case none: square = ' '; break;
            case rook: square += 'R'; break;
            case knight: square += 'N'; break;
            case bishop: square += 'B'; break;
            case queen: square += 'Q'; break;
            case king: square += 'K'; break;
            case pawn: square += 'P'; break;
            default: square = '?'; break;
            }
            printf(" | %c", square);
        }
        printf(" | %d\n", i + 1);
        printf("   ---------------------------------   \n");
    }
    printf("     a   b   c   d   e   f   g   h     \n\n");
    return;
}

bool chess_equal(chess *a, chess *b) {
    return (a->pawns == b->pawns) && (a->kings == b->kings)
           && (a->diagonal_pieces == b->diagonal_pieces)
           && (a->orthogonal_pieces == b->orthogonal_pieces) && (a->enemy_pieces == b->enemy_pieces)
           && (a->friendly_pieces == b->friendly_pieces);
}

u32 encode_string(char *move) {
    if (move[0] == 'i' && move[1] == 'n' && move[2] == 'i' && move[3] == 't' && move[4] == '\0') {
        return NULL_MOVE; // rejects moves aren't long enough
    }
    if (move[0] == '\0' || move[1] == '\0' || move[2] == '\0' || move[3] == '\0') {
        return NULL_MOVE; // rejects moves aren't long enough
    }
    int start_offset = (move[0] - 'a') + 8 * (move[1] - '1');
    int end_offset = (move[2] - 'a') + 8 * (move[3] - '1');
    uint8_t promotion_piece = none;
    switch (move[4]) {
    case '\0': promotion_piece = none; break;
    case 'q': promotion_piece = queen; break;
    case 'r': promotion_piece = rook; break;
    case 'b': promotion_piece = bishop; break;
    case 'n': promotion_piece = knight; break;
    default: promotion_piece = none; break;
    }
    return encode_move(start_offset, end_offset, promotion_piece);
}

u8 get_piece_at_offset(chess *game, int index) {
    u8 piece = 0;
    id |= (!!(board & (game->friendly_pieces | game->enemy_pieces))) << 0;
    id |= (!!(board & game->orthogonal_pieces)) << 1;
    id |= (!!(board & game->diagonal_pieces)) << 2;
    id |= (!!(board & game->kings)) << 3;
    id |= (!!(board & PAWN_MASK & game->pawns)) << 4;
    return id;
}

u32 encode_move(chess *game, int start_offset, int end_offset, u8 promotion_piece) {
    // moves are encoded in the following bit positions (1ull << x)
    // promotion piece: 0-1
    // start offset: 2-7
    // end offset: 8-13
    // move type: 14-15
    // previous castle rights: 16-19
    // previous en passant squares: 20-23
    // previous in check bits: 24-25
    // captured piece: 26-28
    // 2nd captured piece: 29-31
    u32 move = 0;
    switch (promotion_piece) {
    case queen: output |= 0; break;
    case rook: output |= 1; break;
    case knight: output |= 2; break;
    case bishop: output |= 3; break;
    case none: output |= 0; break;
    default: output |= 0; break;
    }
    output |= end_offset << 6;
    output |= start_offset << 12;
    return output;
}

void flip_bitboard(bitboard *board) {
    bitboard output = 0x0;
    output |= ((ROW_1 & *board) << (8 * 7)) | ((ROW_8 & *board) >> (8 * 7)); //swap rows 1 and 8
    output |= ((ROW_2 & *board) << (8 * 5)) | ((ROW_7 & *board) >> (8 * 5)); //swap rows 2 and 7
    output |= ((ROW_3 & *board) << (8 * 3)) | ((ROW_6 & *board) >> (8 * 3)); //swap rows 3 and 6
    output |= ((ROW_4 & *board) << (8 * 1)) | ((ROW_5 & *board) >> (8 * 1)); //swap rows 4 and 5
    *board = output;
    return;
}

void swap_bitboards(bitboard *board, bitboard *other) {
    bitboard temp = *board;
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

int get_start_offset(uint16_t move) {
    return (move >> 12) & 63;
}

int get_end_offset(uint16_t move) {
    return (move >> 6) & 63;
}

uint8_t get_promotion_piece(uint16_t move) {
    switch (focus_move & 3) {
    case 0: return queen;
    case 1: return rook;
    case 2: return knight;
    case 3: return bishop;
    default: return none;
    }
}

uint8_t get_start_piece(chess *game, uint16_t move) {
    return get_piece_at_offset(game, get_start_offset(move));
}

uint8_t get_ending_piece(chess *game, uint16_t move) {
    return get_piece_at_offset(game, get_end_offset(move));
}

bool is_castle(chess *game, uint16_t move) {
    int start_offset = get_start_offset(move);
    int end_offset = get_end_offset(move);
    uint8_t start_piece = get_piece_at_offset(game, start_offset);
    return (start_piece == king) && (start_offset == 4)
           && (((game->pawns & FRIENDLY & KINGSIDE) && end_offset == 6)
               || ((game->pawns & FRIENDLY & QUEENSIDE) && end_offset == 2));
}

bool is_promotion(chess *game, uint16_t move) {
    return (get_start_piece(game, move) == pawn) && (get_end_offset(move) > 55);
}

int get_enemy_ep_offset(chess *game) {
    return (game->pawns >> (8 * 7 + 3)) & 0x7;
}

int get_friendly_ep_offset(chess *game) {
    return (game->pawns >> 3) & 0x7;
}

bool is_ep(chess *game, uint16_t move) {
    offset starting_offset = get_starting_offset(focus_move);
    piece starting_piece = get_piece_at_offset(game, starting_offset);
    bitboard starting_board = offset_to_bitboard[starting_offset];
    bitboard en_passant = game->pawns & EN_PASSANT &;
    return (starting_piece == pawn) && (starting_board & ROW_5)
           && ((compass(starting_board, 1, 1) & compass(en_passant, 0, 5))
               || (compass(starting_board, -1, 1) & compass(en_passant, 0, 5)));
}

void castle_move(chess *game, move focus_move) {
    offset starting_offset = get_starting_offset(focus_move);
    offset ending_offset = get_ending_offset(focus_move);
    move_piece(game, starting_offset, ending_offset);
    if (ending_offset == 2) {
        move_piece(game, 0, 3);
    } else {
        move_piece(game, 7, 5);
    }
    game->pawns &= ~6; // remove friendly castling rights
    game->pawns &= ~ROW_1; // castle_moves aren't pawn pushes
}

void en_passant_move(chess *game, move focus_move) {
    offset starting_offset = get_starting_offset(focus_move);
    offset ending_offset = get_ending_offset(focus_move);
    move_piece(game, starting_offset, ending_offset);
    remove_piece(game, ending_offset - 8);
    game->pawns &= ~ROW_1; // en-passant move isn't double pawn push
}

void promotion_move(chess *game, move focus_move) {
    offset starting_offset = get_starting_offset(focus_move);
    offset ending_offset = get_ending_offset(focus_move);
    piece promotion_piece = get_promotion_piece(focus_move);
    piece ending_piece = get_piece_at_offset(game, ending_offset);
    remove_piece(game, starting_offset);
    add_piece(game, promotion_piece, true, starting_offset);
    move_piece(game, starting_offset, ending_offset);
    game->pawns &= ~ROW_1; // promotions aren't double pushes
    if (ending_offset == 56) {
        game->pawns &= ~16; // capturing enemy queenside rook (maybe)
    }
    if (ending_offset == 63) {
        game->pawns &= ~8;
    }
    if (ending_piece == king) {
        game->pawns &= ~24; // removing enemy castle rights
    }
}

void standard_move(chess *game, move focus_move) {
    offset starting_offset = get_starting_offset(focus_move);
    offset ending_offset = get_ending_offset(focus_move);
    piece starting_piece = get_piece_at_offset(game, starting_offset);
    piece ending_piece = get_piece_at_offset(game, ending_offset);
    move_piece(game, get_starting_offset(focus_move), get_ending_offset(focus_move));
    if (ending_offset == 56) {
        game->pawns &= ~16; // capturing enemy queenside rook (maybe)
    }
    if (ending_offset == 63) {
        game->pawns &= ~8; // capturing enemy kingside rook (maybe)
    }
    if (starting_offset == 0) {
        game->pawns &= ~4; // moving friendly queenside rook (maybe)
    }
    if (ending_offset == 7) {
        game->pawns &= ~2; // moving friendly kingside rook (maybe)
    }
    if (starting_piece == king) {
        game->pawns &= ~6; // removing friendly castle rights
    }
    if (ending_piece == king) {
        game->pawns &= ~24; // removing enemy castle rights
    }
    game->pawns &= ~ROW_1;
    if ((starting_piece == pawn) && (starting_offset > 7) && (starting_offset < 16)
        && (ending_offset > 23) && (ending_offset < 32)) {
        game->pawns |= offset_to_bitboard[starting_offset - 8];
    }
    return;
}

bool bit_move(chess *game, move focus_move) {
    if ((offset_to_bitboard[get_starting_offset(focus_move)] & game->friendly_pieces)
        && (!(offset_to_bitboard[get_ending_offset(focus_move)] & game->friendly_pieces))) {
        if (is_castle(game, focus_move)) {
            if (((get_ending_offset(focus_move) == 2)
                    && !(game->friendly_pieces & offset_to_bitboard[3]))
                || ((get_ending_offset(focus_move) == 6)
                    && !(game->friendly_pieces & offset_to_bitboard[5]))) {
                castle_move(game, focus_move);
                flip_perspective(game);
                return true;
            } else {
                return false;
            }
        } else if (is_promotion(game, focus_move)) {
            promotion_move(game, focus_move);
            flip_perspective(game);
            return true;
        } else if (is_en_passant(game, focus_move)) {
            if (!(offset_to_bitboard[get_ending_offset(focus_move) - 8] & game->friendly_pieces)) {
                en_passant_move(game, focus_move);
                flip_perspective(game);
                return true;
            } else {
                return false;
            }
        } else {
            standard_move(game, focus_move);
            flip_perspective(game);
            return true;
        }
    } else {
        return false;
    }
}

bool is_same_rank(int a, int b) {
    return (a / 8) == (b / 8);
}

bool is_same_file(int a, int b) {
    return (a % 8) == (b % 8);
}

void remove_piece(chess *game, int offset) {
    bitboard board = 1ull << offset;
    game->pawns &= ~(board & ~PAWN_MASK);
    game->kings &= ~board;
    game->friendly_pieces &= ~board;
    game->enemy_pieces &= ~board;
    game->orthogonal_pieces &= ~board;
    game->diagonal_pieces &= ~board;
    return;
}

void add_piece(chess *game, uint8_t piece, bool friendly, int offset) {
    bitboard board = 1ull << offset;
    remove_piece(game, offset);
    switch (piece) {
    case none: remove_piece(game, offset);
    case pawn:
        game->pawns |= board & PAWN_MASK;
        if (friendly) {
            game->friendly_pieces |= board & PAWN_MASK;
        } else {
            game->enemy_pieces |= board & PAWN_MASK;
        }
        return;
    case rook:
        game->orthogonal_pieces |= board;
        if (is_friendly) {
            game->friendly_pieces |= board;
        } else {
            game->enemy_pieces |= board;
        }
        return;
    case knight:
        if (is_friendly) {
            game->friendly_pieces |= board;
        } else {
            game->enemy_pieces |= board;
        }
        return;
    case bishop:
        game->diagonal_pieces |= board;
        if (is_friendly) {
            game->friendly_pieces |= board;
        } else {
            game->enemy_pieces |= board;
        }
        return;
    case queen:
        game->orthogonal_pieces |= board;
        game->diagonal_pieces |= board;
        if (is_friendly) {
            game->friendly_pieces |= board;
        } else {
            game->enemy_pieces |= board;
        }
        return;
    case king:
        game->kings |= board;
        if (is_friendly) {
            game->friendly_pieces |= board;
        } else {
            game->enemy_pieces |= board;
        }
        return;
    default: return;
    }
}

void move_piece(chess *game, int start_offset, int end_offset) {
    uint8_t start_piece = get_piece_at_offset(game, start_offset);
    if (start_piece == none) {
        printf("You can't move an empty square.\n");
        return;
    }
    bool friendly = game->friendly_pieces & (1ull << start_offset);
    remove_piece(game, start_offset);
    add_piece(game, start_piece, friendly, end_offset);
    return;
}

int get_next_offset(bitboard board, int offset) {
    do {
        offset += 1;
    } while (offset != -64 && ((1ull << offset) & board) == 0);
    return offset;
}

int pop_count(bitboard board) {
    int ret = 0;
    while (board != 0) {
        board &= (board - 1);
        ret++;
    }
    return ret;
}

void print_move(chess *game, uint16_t move) {
    int start_offset = get_start_offset(move);
    int end_offset = get_end_offset(move);
    printf("%c", 'a' + (start_offset % 8));
    printf("%c", '1' + (start_offset / 8));
    printf("%c", 'a' + (end_offset % 8));
    printf("%c", '1' + (end_offset / 8));
    uint8_t start_piece = get_piece_at_offset(game, start_offset);
    switch (get_promotion_piece(move)) {

    case rook: printf("r"); break;
    case king: printf("k"); break;
    case bishop: printf("b"); break;
    case queen:
        if ((starting_piece == pawn) && ending_offset > 55) {
            printf("q");
        }
        break;
    default: break;
    }
    return;
}
