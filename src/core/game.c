#include "core/game.h"

#include "core/board.h"
#include "core/movegen.h"
#include "core/notation.h"
#include "core/primitives.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void update_game_status(full_chess *game) {
    int count = set_legal_moves(game->game, game->current_legal_moves);
    game->game_history[game->move_count] = *(game->game);

    if (count == 0) {
        game->is_game_over = true;
        game->is_draw = !in_check(game->game);
        return;
    }

    // FIDE automatic draws: insufficient material, 75-move rule (150 half-moves), or 5-fold rep.
    // Evaluated after checkmate so a checkmating move is never marked a draw.
    if (is_insufficient_material(game->game) || game->half_move_count >= 150
        || position_frequency(game) >= 5) {
        game->is_game_over = true;
        game->is_draw = true;
        game->current_legal_moves[0] = NULL_MOVE;
        return;
    }

    game->is_game_over = false;
    game->is_draw = false;
    return;
}

static u64 match_move_string(const full_chess *game, const char *focus_move, bool use_static) {
    if (!focus_move || focus_move[0] == '\0' || focus_move[1] == '\0' || focus_move[2] == '\0'
        || focus_move[3] == '\0') {
        return NULL_MOVE;
    }
    if (focus_move[0] < 'a' || focus_move[0] > 'h' || focus_move[2] < 'a' || focus_move[2] > 'h'
        || focus_move[1] < '1' || focus_move[1] > '8' || focus_move[3] < '1'
        || focus_move[3] > '8') {
        return NULL_MOVE;
    }

    int start_file = focus_move[0] - 'a';
    int start_rank = focus_move[1] - '1';
    int end_file = focus_move[2] - 'a';
    int end_rank = focus_move[3] - '1';

    if (use_static && is_flipped(game->game)) {
        start_rank = 7 - start_rank;
        end_rank = 7 - end_rank;
    }

    int start_offset = 8 * start_rank + start_file;
    int end_offset = 8 * end_rank + end_file;

    u8 wanted_promo = NONE;
    if (focus_move[4] != '\0') {
        switch (tolower((unsigned char) focus_move[4])) {
        case 'q': wanted_promo = FRIENDLY_QUEEN; break;
        case 'r': wanted_promo = FRIENDLY_ROOK; break;
        case 'b': wanted_promo = FRIENDLY_BISHOP; break;
        case 'n': wanted_promo = FRIENDLY_KNIGHT; break;
        default: return NULL_MOVE;
        }
    }

    for (int i = 0; game->current_legal_moves[i] != NULL_MOVE; i++) {
        u64 m = game->current_legal_moves[i];
        if (get_start_offset(m) == start_offset && get_end_offset(m) == end_offset) {
            bool is_promo = (get_start_piece(m) != get_promotion_piece(m));
            if (is_promo) {
                if (wanted_promo == get_promotion_piece(m)) {
                    return m;
                }
            } else if (wanted_promo == NONE) {
                return m;
            }
        }
    }
    return NULL_MOVE;
}

static u64 perft_recursive(chess *board, int depth) {
    u64 moves[MAX_LEGAL_MOVES];
    int count = set_legal_moves(board, moves);
    if (depth == 1) {
        return (u64) count;
    }
    u64 nodes = 0ull;
    for (int i = 0; i < count; i++) {
        chess_make_move(board, moves[i]);
        nodes += perft_recursive(board, depth - 1);
        chess_unmake_move(board, moves[i]);
    }
    return nodes;
}

full_chess *full_chess_create(void) {
    full_chess *game = (full_chess *) malloc(sizeof(full_chess));
    if (game) {
        full_chess_initialize(game);
    }
    return game;
}

void full_chess_initialize(full_chess *game) {
    game->game = &(game->current_board);
    chess_reset(game->game);
    game->move_count = 0;
    game->start_full_move = 1;
    game->half_move_count = 0;
    game->start_black_to_move = false;
    game->is_static = false;
    game->is_draw = false;
    game->is_game_over = false;
    game->half_move_history[0] = 0;
    update_game_status(game);
    return;
}

void empty(full_chess *game) {
    game->game = &(game->current_board);
    chess_empty(game->game);
    game->move_count = 0;
    game->start_full_move = 1;
    game->half_move_count = 0;
    game->start_black_to_move = false;
    game->is_draw = false;
    game->is_game_over = false;
    game->half_move_history[0] = 0;
    game->game_history[0] = *(game->game);
    game->current_legal_moves[0] = NULL_MOVE;
    return;
}

void reset(full_chess *game) {
    game->move_count = 0;
    game->half_move_count = game->half_move_history[0];
    *(game->game) = game->game_history[0];
    update_game_status(game);
    return;
}

void full_chess_delete(full_chess **input) {
    if (input && *input) {
        free(*input);
        *input = NULL;
    }
    return;
}

void set_from_fen(full_chess *game, const char *fen) {
    bool keep_static = game->is_static;
    empty(game);
    game->is_static = keep_static;

    int i = 0;
    int rank = 7;
    int file = 0;

    while (fen[i] != '\0' && fen[i] != ' ') {
        char c = fen[i];
        if (c == '/') {
            rank--;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
        } else {
            u8 piece = char_to_piece(c);
            if (piece != NONE && rank >= 0 && rank < 8 && file >= 0 && file < 8) {
                add_piece(game->game, piece, 8 * rank + file);
            }
            file++;
        }
        i++;
    }

    // White-at-bottom orientation marker: bit 0 (FRIENDLY & FLIPPED) = 0, bit 56 (ENEMY & FLIPPED) = 1.
    game->game->pawns |= (ENEMY & FLIPPED);

    if (fen[i] == ' ') {
        i++;
    }
    bool black_to_move = (fen[i] == 'b');
    game->start_black_to_move = black_to_move;
    while (fen[i] != '\0' && fen[i] != ' ') {
        i++;
    }

    // Castling rights
    if (fen[i] == ' ') {
        i++;
    }
    while (fen[i] != '\0' && fen[i] != ' ') {
        switch (fen[i]) {
        case 'K': game->game->pawns |= (FRIENDLY & KINGSIDE); break;
        case 'Q': game->game->pawns |= (FRIENDLY & QUEENSIDE); break;
        case 'k': game->game->pawns |= (ENEMY & KINGSIDE); break;
        case 'q': game->game->pawns |= (ENEMY & QUEENSIDE); break;
        default: break;
        }
        i++;
    }

    // En passant target square
    if (fen[i] == ' ') {
        i++;
    }
    if (fen[i] != '\0' && fen[i] != '-') {
        int ep_file = fen[i] - 'a';
        int ep_rank = (fen[i + 1] != '\0') ? (fen[i + 1] - '1') : -1;
        if (ep_file >= 0 && ep_file < 8) {
            if (!black_to_move && ep_rank == 5) {
                game->game->pawns |= (ENEMY & EN_PASSANT_TOGGLED);
                game->game->pawns |= ((u64) ep_file) << 59;
            } else if (black_to_move && ep_rank == 2) {
                // Set on FRIENDLY now; flip_perspective below moves it to ENEMY.
                game->game->pawns |= (FRIENDLY & EN_PASSANT_TOGGLED);
                game->game->pawns |= ((u64) ep_file) << 3;
            }
        }
    }
    while (fen[i] != '\0' && fen[i] != ' ') {
        i++;
    }

    // Half-move clock
    if (fen[i] == ' ') {
        i++;
    }
    int half_moves = 0;
    while (fen[i] >= '0' && fen[i] <= '9') {
        half_moves = half_moves * 10 + (fen[i] - '0');
        i++;
    }
    game->half_move_count = (u8) half_moves;
    game->half_move_history[0] = game->half_move_count;

    // Full-move number
    if (fen[i] == ' ') {
        i++;
    }
    int full_moves = 0;
    while (fen[i] >= '0' && fen[i] <= '9') {
        full_moves = full_moves * 10 + (fen[i] - '0');
        i++;
    }
    game->start_full_move = (full_moves > 0) ? (u16) full_moves : 1;

    if (black_to_move) {
        flip_perspective(game->game);
    }

    update_game_status(game);
    return;
}

char *get_fen(const full_chess *game, char *fen) {
    chess temp = *(game->game);
    bool black_to_move = is_flipped(&temp);
    if (black_to_move) {
        flip_perspective(&temp);
    }

    int pos = 0;
    for (int rank = 7; rank >= 0; rank--) {
        int empty_count = 0;
        for (int file = 0; file < 8; file++) {
            char c = piece_to_char(get_piece_at_offset(&temp, 8 * rank + file));
            if (c == ' ') {
                empty_count++;
            } else {
                if (empty_count > 0) {
                    fen[pos++] = (char) ('0' + empty_count);
                    empty_count = 0;
                }
                fen[pos++] = c;
            }
        }
        if (empty_count > 0) {
            fen[pos++] = (char) ('0' + empty_count);
        }
        if (rank > 0) {
            fen[pos++] = '/';
        }
    }

    fen[pos++] = ' ';
    fen[pos++] = black_to_move ? 'b' : 'w';
    fen[pos++] = ' ';

    bool any_castle = false;
    if (temp.pawns & FRIENDLY & KINGSIDE) {
        fen[pos++] = 'K';
        any_castle = true;
    }
    if (temp.pawns & FRIENDLY & QUEENSIDE) {
        fen[pos++] = 'Q';
        any_castle = true;
    }
    if (temp.pawns & ENEMY & KINGSIDE) {
        fen[pos++] = 'k';
        any_castle = true;
    }
    if (temp.pawns & ENEMY & QUEENSIDE) {
        fen[pos++] = 'q';
        any_castle = true;
    }
    if (!any_castle) {
        fen[pos++] = '-';
    }

    fen[pos++] = ' ';
    if (!black_to_move && (temp.pawns & ENEMY & EN_PASSANT_TOGGLED)) {
        int ep_file = (int) (((temp.pawns & ENEMY & EN_PASSANT_OFFSET) >> 59) & 7ull);
        fen[pos++] = (char) ('a' + ep_file);
        fen[pos++] = '6';
    } else if (black_to_move && (temp.pawns & FRIENDLY & EN_PASSANT_TOGGLED)) {
        int ep_file = (int) (((temp.pawns & FRIENDLY & EN_PASSANT_OFFSET) >> 3) & 7ull);
        fen[pos++] = (char) ('a' + ep_file);
        fen[pos++] = '3';
    } else {
        fen[pos++] = '-';
    }

    int full_move_count
        = game->start_full_move + (game->move_count + (game->start_black_to_move ? 1 : 0)) / 2;
    snprintf(fen + pos, (size_t) (MAX_FEN_LENGTH - pos), " %u %d", (unsigned) game->half_move_count,
        full_move_count);
    return fen;
}

void print_fen(const full_chess *game) {
    char fen[MAX_FEN_LENGTH];
    printf("%s", get_fen(game, fen));
    return;
}

char get_piece_char_at_offset(const full_chess *game, int index) {
    return piece_to_char(get_piece_at_offset(game->game, index));
}

u64 encode_string(const full_chess *game, const char *focus_move) {
    return match_move_string(game, focus_move, game->is_static);
}

void make_move(full_chess *game, u64 real_move) {
    if (real_move == NULL_MOVE || game->is_game_over || game->move_count + 1 >= MAX_GAME_LENGTH) {
        return;
    }
    if (!in_set(game->current_legal_moves, real_move)) {
        printf("not a legal move!\n");
        return;
    }

    if (get_start_piece(real_move) == FRIENDLY_PAWN || get_end_piece(real_move) != NONE
        || is_en_passant_move(real_move)) {
        game->half_move_count = 0;
    } else {
        game->half_move_count += 1;
    }

    game->move_history[game->move_count] = real_move;
    chess_make_move(game->game, real_move);
    game->move_count += 1;
    game->half_move_history[game->move_count] = game->half_move_count;
    update_game_status(game);
    return;
}

void string_move(full_chess *game, const char *focus_move) {
    make_move(game, encode_string(game, focus_move));
    return;
}

void unmake_move(full_chess *game) {
    if (game->move_count == 0) {
        return;
    }
    game->move_count -= 1;
    chess_unmake_move(game->game, game->move_history[game->move_count]);
    game->half_move_count = game->half_move_history[game->move_count];
    update_game_status(game);
    return;
}

u64 perft(full_chess *game, int depth, bool top_call) {
    if (depth < 1) {
        return 0ull;
    }
    if (top_call) {
        printf("go perft %d\n", depth);
    }

    u64 moves[MAX_LEGAL_MOVES];
    chess board = *(game->game);
    int count = set_legal_moves(&board, moves);

    if (depth == 1) {
        if (top_call) {
            for (int i = 0; i < count; i++) {
                print_move(game, moves[i]);
                printf(": 1\n");
            }
            printf("\nNodes searched: %d\n\n", count);
        }
        return (u64) count;
    }

    u64 output = 0ull;
    for (int i = 0; i < count; i++) {
        chess_make_move(&board, moves[i]);
        u64 perft_result = perft_recursive(&board, depth - 1);
        chess_unmake_move(&board, moves[i]);
        output += perft_result;
        if (top_call) {
            print_move(game, moves[i]);
            printf(": %lu\n", (unsigned long) perft_result);
        }
    }

    if (top_call) {
        printf("\nNodes searched: %lu\n\n", (unsigned long) output);
    }
    return output;
}

int num_legal_moves(const full_chess *game) {
    int count = 0;
    while (game->current_legal_moves[count] != NULL_MOVE) {
        count++;
    }
    return count;
}

int position_frequency(const full_chess *game) {
    int count = 0;
    for (int i = (int) game->move_count; i >= 0; i--) {
        if (chess_equal(game->game, &(game->game_history[i]))) {
            count++;
            if (count >= 5) {
                return 5;
            }
        }
    }
    return count;
}

u64 get_random_move(const full_chess *game) {
    int n = num_legal_moves(game);
    if (n == 0) {
        return NULL_MOVE;
    }
    return game->current_legal_moves[rand() % n];
}

void print_move(const full_chess *game, u64 focus_move) {
    if (game->is_static) {
        static_print_move(game, focus_move);
    } else {
        chess_print_move(focus_move);
    }
    return;
}

void print_legal_moves(const full_chess *game) {
    printf("{");
    for (int i = 0; game->current_legal_moves[i] != NULL_MOVE; i++) {
        if (i > 0) {
            printf(",");
        }
        print_move(game, game->current_legal_moves[i]);
    }
    printf("}\n");
    return;
}

void print_board(const full_chess *game) {
    if (game->is_static) {
        static_print_board(game);
    } else {
        chess temp = *(game->game);
        print_game_state(&temp);
    }
    return;
}
