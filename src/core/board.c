#include "core/board.h"

#include "core/primitives.h"

#include <stdio.h>
#include <stdlib.h>

chess *chess_create(void) {
    chess *game = (chess *) malloc(sizeof(chess));
    if (game) {
        chess_reset(game);
    }
    return game;
}

void chess_reset(chess *game) {
    game->pawns = ROW_2 | ROW_7 | (ENEMY & FLIPPED) | ((FRIENDLY | ENEMY) & (KINGSIDE | QUEENSIDE));
    game->friendly_pieces = ROW_1 | ROW_2;
    game->enemy_pieces = ROW_7 | ROW_8;
    game->orthogonal_pieces = (ROW_1 | ROW_8) & (COL_1 | COL_4 | COL_8);
    game->diagonal_pieces = (ROW_1 | ROW_8) & (COL_3 | COL_4 | COL_6);
    game->kings = (ROW_1 | ROW_8) & COL_5;
    return;
}

void chess_empty(chess *game) {
    game->pawns = 0ull;
    game->friendly_pieces = 0ull;
    game->enemy_pieces = 0ull;
    game->orthogonal_pieces = 0ull;
    game->diagonal_pieces = 0ull;
    game->kings = 0ull;
    return;
}

void chess_delete(chess **input) {
    if (input && *input) {
        free(*input);
        *input = NULL;
    }
    return;
}

void print_bitboard(u64 board) {
    printf("\n    ");
    for (int i = 7; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
            printf("%lu ", (unsigned long) (1ull & (board >> (8 * i + j))));
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

bool chess_equal(const chess *a, const chess *b) {
    u64 ignore_check = ~((FRIENDLY | ENEMY) & IN_CHECK);
    return ((a->pawns & ignore_check) == (b->pawns & ignore_check)) && (a->kings == b->kings)
           && (a->diagonal_pieces == b->diagonal_pieces)
           && (a->orthogonal_pieces == b->orthogonal_pieces) && (a->enemy_pieces == b->enemy_pieces)
           && (a->friendly_pieces == b->friendly_pieces);
}

bool is_flipped(const chess *game) {
    return (game->pawns & FRIENDLY & FLIPPED) != 0;
}

bool is_insufficient_material(const chess *game) {
    // Any pawn, rook, or queen can force checkmate — exit in 1 bitwise op in ~99% of positions.
    if (((game->pawns & PAWN_MASK) | game->orthogonal_pieces) != 0ull) {
        return false;
    }
    u64 minors = (game->friendly_pieces | game->enemy_pieces) & ~game->kings;
    if (pop_count(minors) <= 1) {
        // K vs K, K+B vs K, or K+N vs K
        return true;
    }
    u64 knights = minors & ~game->diagonal_pieces;
    if (knights == 0ull) {
        // Only kings and bishops remain: draw iff all bishops share the same square color
        u64 bishops = game->diagonal_pieces;
        return ((bishops & LIGHT_SQUARES) == 0ull) || ((bishops & DARK_SQUARES) == 0ull);
    }
    return false;
}

u8 get_piece_at_offset(const chess *game, int index) {
    u8 id = (u8) ((((game->pawns & PAWN_MASK) >> index) & 1ull)
                  | (((game->friendly_pieces >> index) & 1ull) << 1)
                  | (((game->enemy_pieces >> index) & 1ull) << 2)
                  | (((game->orthogonal_pieces >> index) & 1ull) << 3)
                  | (((game->diagonal_pieces >> index) & 1ull) << 4)
                  | (((game->kings >> index) & 1ull) << 5));
    return id;
}

void flip_bitboard(u64 *board) {
    *board = __builtin_bswap64(*board);
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
    game->pawns |= PAWN_MASK & (((u64) ((piece >> 0) & 1)) << offset);
    game->friendly_pieces |= ((u64) ((piece >> 1) & 1)) << offset;
    game->enemy_pieces |= ((u64) ((piece >> 2) & 1)) << offset;
    game->orthogonal_pieces |= ((u64) ((piece >> 3) & 1)) << offset;
    game->diagonal_pieces |= ((u64) ((piece >> 4) & 1)) << offset;
    game->kings |= ((u64) ((piece >> 5) & 1)) << offset;
    return;
}

int get_next_offset(u64 board, int offset) {
    int next = offset + 1;
    if (next >= 64) {
        return 64;
    }
    u64 shifted = board >> next;
    if (shifted == 0ull) {
        return 64;
    }
    return next + __builtin_ctzll(shifted);
}

int pop_count(u64 board) {
    return __builtin_popcountll(board);
}

u64 compass(u64 board, int dx, int dy) {
    if (dx == 1) {
        board = (board & ~COL_8) << 1;
    } else if (dx == 2) {
        board = (board & ~(COL_7 | COL_8)) << 2;
    } else if (dx == -1) {
        board = (board & ~COL_1) >> 1;
    } else if (dx == -2) {
        board = (board & ~(COL_1 | COL_2)) >> 2;
    } else if (dx != 0) {
        return 0ull;
    }

    if (dy > 0) {
        if (dy >= 8) {
            return 0ull;
        }
        board <<= (8 * dy);
    } else if (dy < 0) {
        if (dy <= -8) {
            return 0ull;
        }
        board >>= (8 * (-dy));
    }
    return board;
}

// Reversible 64-bit move functions

u64 encode_move(int start_offset, int end_offset, bool en_passant_capture, bool castle_king,
    bool castle_queen, u8 promotion_piece, u8 start_piece, u8 end_piece, u8 en_passant_piece,
    u8 friendly_data, u8 enemy_data) {
    u64 move = 0ull;
    move |= ((u64) (start_offset & 63)) << 0; // 6 bits  [0..5]
    move |= ((u64) (end_offset & 63)) << 6; // 6 bits  [6..11]
    move |= ((u64) (en_passant_capture ? 1 : 0)) << 12; // 1 bit   [12]
    move |= ((u64) (castle_king ? 1 : 0)) << 13; // 1 bit   [13]
    move |= ((u64) (castle_queen ? 1 : 0)) << 14; // 1 bit   [14]
    move |= ((u64) (promotion_piece & 63)) << 15; // 6 bits  [15..20]
    move |= ((u64) (start_piece & 63)) << 21; // 6 bits  [21..26]
    move |= ((u64) (end_piece & 63)) << 27; // 6 bits  [27..32]
    move |= ((u64) (en_passant_piece & 63)) << 33; // 6 bits  [33..38]
    move |= ((u64) friendly_data) << 39; // 8 bits  [39..46]
    move |= ((u64) enemy_data) << 47; // 8 bits  [47..54]
    return move;
}

u64 build_move(const chess *game, int start_offset, int end_offset, bool en_passant_capture,
    bool castle_king, bool castle_queen, u8 promotion_piece) {
    u8 start_piece = get_piece_at_offset(game, start_offset);
    u8 end_piece = get_piece_at_offset(game, end_offset);
    u8 promo = (promotion_piece == NONE) ? start_piece : promotion_piece;
    u8 ep_piece = en_passant_capture ? get_piece_at_offset(game, end_offset - 8) : NONE;
    u8 friendly_data = (u8) (game->pawns & 0xffull);
    u8 enemy_data = (u8) ((game->pawns >> 56) & 0xffull);
    return encode_move(start_offset, end_offset, en_passant_capture, castle_king, castle_queen,
        promo, start_piece, end_piece, ep_piece, friendly_data, enemy_data);
}

int get_start_offset(u64 move) {
    return (int) ((move >> 0) & 63ull);
}

int get_end_offset(u64 move) {
    return (int) ((move >> 6) & 63ull);
}

bool is_en_passant_move(u64 move) {
    return ((move >> 12) & 1ull) != 0;
}

bool is_castle_king_move(u64 move) {
    return ((move >> 13) & 1ull) != 0;
}

bool is_castle_queen_move(u64 move) {
    return ((move >> 14) & 1ull) != 0;
}

u8 get_promotion_piece(u64 move) {
    return (u8) ((move >> 15) & 63ull);
}

u8 get_start_piece(u64 move) {
    return (u8) ((move >> 21) & 63ull);
}

u8 get_end_piece(u64 move) {
    return (u8) ((move >> 27) & 63ull);
}

u8 get_en_passant_piece(u64 move) {
    return (u8) ((move >> 33) & 63ull);
}

u8 get_friendly_data(u64 move) {
    return (u8) ((move >> 39) & 0xffull);
}

u8 get_enemy_data(u64 move) {
    return (u8) ((move >> 47) & 0xffull);
}

bool has_en_passant(const chess *game) {
    return (game->pawns & ENEMY & EN_PASSANT_TOGGLED) != 0;
}

int get_en_passant_offset(const chess *game) {
    return 40 + (int) (((game->pawns & ENEMY & EN_PASSANT_OFFSET) >> 59) & 7ull);
}

// Precomputed castling-rights mask per square (replaces 6 branches in chess_make_move)
static const u64 g_castle_clear[64] = {
    ~(FRIENDLY & QUEENSIDE),
    ~0ull,
    ~0ull,
    ~0ull,
    ~(FRIENDLY &(QUEENSIDE | KINGSIDE)),
    ~0ull,
    ~0ull,
    ~(FRIENDLY &KINGSIDE),
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~0ull,
    ~(ENEMY &QUEENSIDE),
    ~0ull,
    ~0ull,
    ~0ull,
    ~(ENEMY &(QUEENSIDE | KINGSIDE)),
    ~0ull,
    ~0ull,
    ~(ENEMY &KINGSIDE),
};

void chess_make_move(chess *game, u64 move) {
    int start_offset = get_start_offset(move);
    int end_offset = get_end_offset(move);
    u8 promotion_piece = get_promotion_piece(move);
    u8 start_piece = get_start_piece(move);
    u8 end_piece = get_end_piece(move);

    if (__builtin_expect(is_en_passant_move(move), 0)) {
        u64 ep_bb = 1ull << (end_offset - 8);
        game->enemy_pieces &= ~ep_bb;
        game->pawns &= ~(ep_bb & PAWN_MASK);
    } else if (__builtin_expect(is_castle_king_move(move), 0)) {
        u64 rb = (1ull << 7) | (1ull << 5);
        game->friendly_pieces ^= rb;
        game->orthogonal_pieces ^= rb;
    } else if (__builtin_expect(is_castle_queen_move(move), 0)) {
        u64 rb = (1ull << 0) | (1ull << 3);
        game->friendly_pieces ^= rb;
        game->orthogonal_pieces ^= rb;
    }

    u64 from_bb = 1ull << start_offset;
    u64 to_bb = 1ull << end_offset;
    if (end_piece != NONE) {
        u64 n_to = ~to_bb;
        game->pawns &= ~(to_bb & PAWN_MASK);
        game->enemy_pieces &= n_to;
        game->orthogonal_pieces &= n_to;
        game->diagonal_pieces &= n_to;
        game->kings &= n_to;
    }

    u64 move_bb = from_bb | to_bb;
    game->friendly_pieces ^= move_bb;

    if (__builtin_expect(start_piece == promotion_piece, 1)) {
        if (start_piece & 1) {
            game->pawns ^= (move_bb & PAWN_MASK);
        } else {
            if (start_piece & (1 << 3)) {
                game->orthogonal_pieces ^= move_bb;
            }
            if (start_piece & (1 << 4)) {
                game->diagonal_pieces ^= move_bb;
            }
            if (start_piece & (1 << 5)) {
                game->kings ^= move_bb;
            }
        }
    } else {
        game->pawns &= ~(from_bb & PAWN_MASK);
        if (promotion_piece & (1 << 3)) {
            game->orthogonal_pieces |= to_bb;
        }
        if (promotion_piece & (1 << 4)) {
            game->diagonal_pieces |= to_bb;
        }
    }

    game->pawns &= g_castle_clear[start_offset] & g_castle_clear[end_offset];
    game->pawns &= ~((FRIENDLY | ENEMY) & (EN_PASSANT_TOGGLED | EN_PASSANT_OFFSET | IN_CHECK));

    if (start_piece == FRIENDLY_PAWN && start_offset < 16 && end_offset > 23) {
        game->pawns |= (FRIENDLY & EN_PASSANT_TOGGLED);
        game->pawns |= ((u64) (end_offset & 7)) << 3;
    }

    flip_perspective(game);
}

void chess_unmake_move(chess *game, u64 move) {
    flip_perspective(game);

    int start_offset = get_start_offset(move);
    int end_offset = get_end_offset(move);
    bool ep = is_en_passant_move(move);
    bool castle_king = is_castle_king_move(move);
    bool castle_queen = is_castle_queen_move(move);
    u8 start_piece = get_start_piece(move);
    u8 end_piece = get_end_piece(move);

    remove_piece(game, end_offset);
    if (end_piece != NONE) {
        add_piece(game, end_piece, end_offset);
    }
    add_piece(game, start_piece, start_offset);

    if (ep) {
        add_piece(game, get_en_passant_piece(move), end_offset - 8);
    }
    if (castle_king) {
        u8 rook = get_piece_at_offset(game, 5);
        remove_piece(game, 5);
        add_piece(game, rook, 7);
    }
    if (castle_queen) {
        u8 rook = get_piece_at_offset(game, 3);
        remove_piece(game, 3);
        add_piece(game, rook, 0);
    }

    game->pawns &= PAWN_MASK;
    game->pawns |= (u64) get_friendly_data(move);
    game->pawns |= ((u64) get_enemy_data(move)) << 56;
    return;
}

void chess_print_move(u64 move) {
    int start_offset = get_start_offset(move);
    int end_offset = get_end_offset(move);
    printf("%c%c%c%c", 'a' + (start_offset % 8), '1' + (start_offset / 8), 'a' + (end_offset % 8),
        '1' + (end_offset / 8));
    u8 promotion_piece = get_promotion_piece(move);
    u8 starting_piece = get_start_piece(move);
    if (starting_piece != promotion_piece) {
        char c = piece_to_char(promotion_piece);
        if (c >= 'A' && c <= 'Z') {
            c = (char) (c + ('a' - 'A'));
        }
        printf("%c", c);
    }
    return;
}
