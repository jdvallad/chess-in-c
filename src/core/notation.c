#include "core/notation.h"

#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/primitives.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static char san_piece_letter(u8 piece) {
    switch (piece) {
    case FRIENDLY_KNIGHT:
    case ENEMY_KNIGHT: return 'N';
    case FRIENDLY_BISHOP:
    case ENEMY_BISHOP: return 'B';
    case FRIENDLY_ROOK:
    case ENEMY_ROOK: return 'R';
    case FRIENDLY_QUEEN:
    case ENEMY_QUEEN: return 'Q';
    case FRIENDLY_KING:
    case ENEMY_KING: return 'K';
    default: return '\0';
    }
}

u64 static_encode_string(const full_chess *game, const char *focus_move) {
    full_chess view = *game;
    view.game = &view.current_board;
    view.is_static = true;
    return encode_string(&view, focus_move);
}

void static_string_move(full_chess *game, const char *focus_move) {
    bool prev = game->is_static;
    game->is_static = true;
    string_move(game, focus_move);
    game->is_static = prev;
    return;
}

void static_print_move(const full_chess *game, u64 the_move) {
    if (!is_flipped(game->game)) {
        chess_print_move(the_move);
        return;
    }

    int start_offset = get_start_offset(the_move);
    int end_offset = get_end_offset(the_move);
    printf("%c%c%c%c", 'a' + (start_offset % 8), '8' - (start_offset / 8), 'a' + (end_offset % 8),
        '8' - (end_offset / 8));
    u8 promotion_piece = get_promotion_piece(the_move);
    u8 starting_piece = get_start_piece(the_move);
    if (starting_piece != promotion_piece) {
        char c = piece_to_char(promotion_piece);
        if (c >= 'A' && c <= 'Z') {
            c = (char) (c + ('a' - 'A'));
        }
        printf("%c", c);
    }
    return;
}

void static_print_legal_moves(const full_chess *game) {
    printf("{");
    for (int i = 0; game->current_legal_moves[i] != NULL_MOVE; i++) {
        if (i > 0) {
            printf(",");
        }
        static_print_move(game, game->current_legal_moves[i]);
    }
    printf("}\n");
    return;
}

void static_print_board(const full_chess *full_game) {
    chess temp = *(full_game->game);
    if (is_flipped(&temp)) {
        flip_perspective(&temp);
    }
    print_game_state(&temp);
    return;
}

u64 static_perft(full_chess *game, int depth, bool top_call) {
    bool prev = game->is_static;
    game->is_static = true;
    u64 result = perft(game, depth, top_call);
    game->is_static = prev;
    return result;
}

char *static_move_to_san(
    const chess *board, const u64 *legal_moves, u64 move, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return buf;
    }
    if (move == NULL_MOVE) {
        buf[0] = '\0';
        return buf;
    }

    char tmp[16];
    int pos = 0;

    if (is_castle_king_move(move)) {
        tmp[pos++] = 'O';
        tmp[pos++] = '-';
        tmp[pos++] = 'O';
    } else if (is_castle_queen_move(move)) {
        tmp[pos++] = 'O';
        tmp[pos++] = '-';
        tmp[pos++] = 'O';
        tmp[pos++] = '-';
        tmp[pos++] = 'O';
    } else {
        bool flipped = is_flipped(board);
        int s = get_start_offset(move);
        int e = get_end_offset(move);
        int sf = s % 8;
        int sr = flipped ? (7 - (s / 8)) : (s / 8);
        int ef = e % 8;
        int er = flipped ? (7 - (e / 8)) : (e / 8);

        u8 sp = get_start_piece(move);
        u8 promo = get_promotion_piece(move);
        bool is_cap = (get_end_piece(move) != NONE) || is_en_passant_move(move);

        if (sp == FRIENDLY_PAWN) {
            if (is_cap) {
                tmp[pos++] = (char) ('a' + sf);
                tmp[pos++] = 'x';
            }
        } else {
            tmp[pos++] = san_piece_letter(sp);

            // Disambiguation against other legal moves of the same piece type to the same square
            bool need_disambig = false;
            bool same_file = false;
            bool same_rank = false;
            if (legal_moves) {
                for (int i = 0; legal_moves[i] != NULL_MOVE; i++) {
                    u64 other = legal_moves[i];
                    if (other == move) {
                        continue;
                    }
                    if (get_start_piece(other) == sp && get_end_offset(other) == e
                        && get_start_offset(other) != s) {
                        need_disambig = true;
                        int os = get_start_offset(other);
                        if ((os % 8) == sf) {
                            same_file = true;
                        }
                        if ((os / 8) == (s / 8)) {
                            same_rank = true;
                        }
                    }
                }
            }
            if (need_disambig) {
                if (!same_file) {
                    tmp[pos++] = (char) ('a' + sf);
                } else if (!same_rank) {
                    tmp[pos++] = (char) ('1' + sr);
                } else {
                    tmp[pos++] = (char) ('a' + sf);
                    tmp[pos++] = (char) ('1' + sr);
                }
            }
            if (is_cap) {
                tmp[pos++] = 'x';
            }
        }

        tmp[pos++] = (char) ('a' + ef);
        tmp[pos++] = (char) ('1' + er);

        if (promo != sp) {
            tmp[pos++] = '=';
            tmp[pos++] = san_piece_letter(promo);
        }
    }

    // Determine check (+) or checkmate (#) suffix
    chess next = *board;
    chess_make_move(&next, move);
    int opp_king = get_next_offset(next.kings & next.friendly_pieces, -1);
    if (opp_king < 64 && attacked_offset(&next, opp_king)) {
        u64 opp_moves[MAX_LEGAL_MOVES];
        int opp_count = set_legal_moves(&next, opp_moves);
        tmp[pos++] = (opp_count == 0) ? '#' : '+';
    }

    tmp[pos] = '\0';
    snprintf(buf, buf_size, "%s", tmp);
    return buf;
}

void static_print_san_move(const full_chess *game, u64 focus_move) {
    char buf[16];
    printf("%s",
        static_move_to_san(game->game, game->current_legal_moves, focus_move, buf, sizeof(buf)));
    return;
}

void static_print_san_legal_moves(const full_chess *game) {
    printf("{");
    for (int i = 0; game->current_legal_moves[i] != NULL_MOVE; i++) {
        if (i > 0) {
            printf(", ");
        }
        static_print_san_move(game, game->current_legal_moves[i]);
    }
    printf("}\n");
    return;
}

u64 static_encode_san(const full_chess *game, const char *san, int *match_count) {
    if (match_count) {
        *match_count = 0;
    }
    if (!san || san[0] == '\0') {
        return NULL_MOVE;
    }

    // Copy and strip trailing check/mate/annotation markers (+, #, !, ?)
    char s[32];
    size_t len = strlen(san);
    if (len >= sizeof(s)) {
        return NULL_MOVE;
    }
    memcpy(s, san, len + 1);
    while (len > 0
           && (s[len - 1] == '+' || s[len - 1] == '#' || s[len - 1] == '!' || s[len - 1] == '?')) {
        s[--len] = '\0';
    }
    if (len == 0) {
        return NULL_MOVE;
    }

    // Castling (O-O, 0-0, o-o, O-O-O, 0-0-0, o-o-o)
    bool want_castle_k = (strcmp(s, "O-O") == 0 || strcmp(s, "0-0") == 0 || strcmp(s, "o-o") == 0);
    bool want_castle_q
        = (strcmp(s, "O-O-O") == 0 || strcmp(s, "0-0-0") == 0 || strcmp(s, "o-o-o") == 0);
    if (want_castle_k || want_castle_q) {
        for (int i = 0; game->current_legal_moves[i] != NULL_MOVE; i++) {
            u64 m = game->current_legal_moves[i];
            if ((want_castle_k && is_castle_king_move(m))
                || (want_castle_q && is_castle_queen_move(m))) {
                if (match_count) {
                    *match_count = 1;
                }
                return m;
            }
        }
        return NULL_MOVE;
    }

    // Parse optional promotion suffix at end (=Q or Q on rank 1/8)
    u8 want_promo = NONE;
    if (len >= 2 && s[len - 2] == '=') {
        switch (toupper((unsigned char) s[len - 1])) {
        case 'Q': want_promo = FRIENDLY_QUEEN; break;
        case 'R': want_promo = FRIENDLY_ROOK; break;
        case 'B': want_promo = FRIENDLY_BISHOP; break;
        case 'N': want_promo = FRIENDLY_KNIGHT; break;
        default: return NULL_MOVE;
        }
        len -= 2;
        s[len] = '\0';
    } else if (len >= 3
               && (s[len - 1] == 'Q' || s[len - 1] == 'R' || s[len - 1] == 'B'
                   || s[len - 1] == 'N')) {
        // Also accept shorthand promotion without '=' (e.g. e8Q, exd8N)
        if (s[len - 2] == '8' || s[len - 2] == '1') {
            switch (s[len - 1]) {
            case 'Q': want_promo = FRIENDLY_QUEEN; break;
            case 'R': want_promo = FRIENDLY_ROOK; break;
            case 'B': want_promo = FRIENDLY_BISHOP; break;
            case 'N': want_promo = FRIENDLY_KNIGHT; break;
            default: break;
            }
            len -= 1;
            s[len] = '\0';
        }
    }

    // Destination square must be the last 2 characters
    if (len < 2) {
        return NULL_MOVE;
    }
    char dest_file_ch = s[len - 2];
    char dest_rank_ch = s[len - 1];
    if (dest_file_ch < 'a' || dest_file_ch > 'h' || dest_rank_ch < '1' || dest_rank_ch > '8') {
        return NULL_MOVE;
    }
    int dest_file = dest_file_ch - 'a';
    int dest_rank_static = dest_rank_ch - '1';
    bool flipped = is_flipped(game->game);
    int dest_offset = 8 * (flipped ? (7 - dest_rank_static) : dest_rank_static) + dest_file;

    // Parse prefix before destination square: [Piece][from_file][from_rank][x]
    size_t idx = 0;
    size_t prefix_end = len - 2;

    u8 want_piece = FRIENDLY_PAWN;
    if (idx < prefix_end) {
        switch (s[idx]) {
        case 'N':
            want_piece = FRIENDLY_KNIGHT;
            idx++;
            break;
        case 'B':
            want_piece = FRIENDLY_BISHOP;
            idx++;
            break;
        case 'R':
            want_piece = FRIENDLY_ROOK;
            idx++;
            break;
        case 'Q':
            want_piece = FRIENDLY_QUEEN;
            idx++;
            break;
        case 'K':
            want_piece = FRIENDLY_KING;
            idx++;
            break;
        default: break;
        }
    }

    bool has_x = false;
    if (prefix_end > idx && s[prefix_end - 1] == 'x') {
        has_x = true;
        prefix_end--;
    }

    int want_from_file = -1;
    int want_from_rank_native = -1;
    if (idx < prefix_end && s[idx] >= 'a' && s[idx] <= 'h') {
        want_from_file = s[idx] - 'a';
        idx++;
    }
    if (idx < prefix_end && s[idx] >= '1' && s[idx] <= '8') {
        int r = s[idx] - '1';
        want_from_rank_native = flipped ? (7 - r) : r;
        idx++;
    }
    // Reject any leftover garbage characters in the prefix
    if (idx != prefix_end) {
        return NULL_MOVE;
    }

    // Standard SAN rules for pawn moves:
    // - Pawn captures must specify origin file AND 'x' (e.g. exd5).
    // - Non-capturing pawn pushes must not specify origin file or rank (e.g. e4, not e2e4).
    if (want_piece == FRIENDLY_PAWN) {
        if (has_x && want_from_file == -1) {
            return NULL_MOVE;
        }
        if (!has_x && (want_from_file != -1 || want_from_rank_native != -1)) {
            return NULL_MOVE;
        }
    } else if (want_promo != NONE) {
        return NULL_MOVE;
    }

    u64 matched = NULL_MOVE;
    int matches = 0;

    for (int i = 0; game->current_legal_moves[i] != NULL_MOVE; i++) {
        u64 m = game->current_legal_moves[i];
        if (get_start_piece(m) != want_piece || get_end_offset(m) != dest_offset) {
            continue;
        }
        bool is_cap = (get_end_piece(m) != NONE) || is_en_passant_move(m);
        if (has_x != is_cap) {
            continue;
        }
        int ms = get_start_offset(m);
        if (want_from_file != -1 && (ms % 8) != want_from_file) {
            continue;
        }
        if (want_from_rank_native != -1 && (ms / 8) != want_from_rank_native) {
            continue;
        }
        bool is_promo = (get_promotion_piece(m) != get_start_piece(m));
        if (is_promo) {
            if (want_promo != get_promotion_piece(m)) {
                continue;
            }
        } else if (want_promo != NONE) {
            continue;
        }

        matched = m;
        matches++;
    }

    if (match_count) {
        *match_count = matches;
    }
    return (matches == 1) ? matched : NULL_MOVE;
}

static void append_str(char *buf, size_t buf_size, size_t *pos, const char *text) {
    if (*pos >= buf_size) {
        return;
    }
    int written = snprintf(buf + *pos, buf_size - *pos, "%s", text);
    if (written > 0) {
        size_t w = (size_t) written;
        *pos += (*pos + w < buf_size) ? w : (buf_size - *pos - 1);
    }
}

char *static_get_pgn(const full_chess *game, const char *white_name, const char *black_name,
    char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return buf;
    }
    buf[0] = '\0';
    size_t pos = 0;

    const char *w_name = (white_name && white_name[0]) ? white_name : "White";
    const char *b_name = (black_name && black_name[0]) ? black_name : "Black";

    const char *result = "*";
    if (game->is_game_over) {
        if (game->is_draw) {
            result = "1/2-1/2";
        } else {
            result = is_flipped(game->game) ? "1-0" : "0-1";
        }
    }

    char date_str[16] = "????.??.??";
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        snprintf(date_str, sizeof(date_str), "%04d.%02d.%02d", tm_info->tm_year + 1900,
            tm_info->tm_mon + 1, tm_info->tm_mday);
    }

    char header[512];
    snprintf(header, sizeof(header),
        "[Event \"Chess-in-C Game\"]\n"
        "[Site \"Terminal\"]\n"
        "[Date \"%s\"]\n"
        "[Round \"-\"]\n"
        "[White \"%s\"]\n"
        "[Black \"%s\"]\n"
        "[Result \"%s\"]\n",
        date_str, w_name, b_name, result);
    append_str(buf, buf_size, &pos, header);

    // Emit SetUp + FEN tags if the game started from a non-standard position
    chess std_start;
    chess_reset(&std_start);
    bool custom_start = !chess_equal(&game->game_history[0], &std_start)
                        || game->start_full_move != 1 || game->start_black_to_move
                        || game->half_move_history[0] != 0;
    if (custom_start) {
        full_chess start_view;
        start_view.current_board = game->game_history[0];
        start_view.game = &start_view.current_board;
        start_view.move_count = 0;
        start_view.half_move_count = game->half_move_history[0];
        start_view.start_full_move = game->start_full_move;
        start_view.start_black_to_move = game->start_black_to_move;
        char init_fen[MAX_FEN_LENGTH];
        get_fen(&start_view, init_fen);
        char fen_tags[256];
        snprintf(fen_tags, sizeof(fen_tags), "[SetUp \"1\"]\n[FEN \"%s\"]\n", init_fen);
        append_str(buf, buf_size, &pos, fen_tags);
    }

    append_str(buf, buf_size, &pos, "\n");

    // Movetext wrapped cleanly at ~80 columns
    size_t col = 0;
    u64 legal_moves[MAX_LEGAL_MOVES];
    for (int p = 0; p < game->move_count; p++) {
        chess board = game->game_history[p];
        set_legal_moves(&board, legal_moves);
        char san[16];
        static_move_to_san(&board, legal_moves, game->move_history[p], san, sizeof(san));

        int abs_ply = p + (game->start_black_to_move ? 1 : 0);
        int move_num = game->start_full_move + (abs_ply / 2);
        char token[32];
        if ((abs_ply % 2) == 0) {
            snprintf(token, sizeof(token), "%d. %s", move_num, san);
        } else if (p == 0) {
            snprintf(token, sizeof(token), "%d... %s", move_num, san);
        } else {
            snprintf(token, sizeof(token), "%s", san);
        }

        size_t tlen = strlen(token);
        if (col > 0 && col + 1 + tlen > 80) {
            append_str(buf, buf_size, &pos, "\n");
            col = 0;
        } else if (col > 0) {
            append_str(buf, buf_size, &pos, " ");
            col++;
        }
        append_str(buf, buf_size, &pos, token);
        col += tlen;
    }

    size_t rlen = strlen(result);
    if (col > 0 && col + 1 + rlen > 80) {
        append_str(buf, buf_size, &pos, "\n");
    } else if (col > 0) {
        append_str(buf, buf_size, &pos, " ");
    }
    append_str(buf, buf_size, &pos, result);
    append_str(buf, buf_size, &pos, "\n");
    return buf;
}

void static_print_pgn(const full_chess *game, const char *white_name, const char *black_name) {
    char pgn[65536];
    printf("%s", static_get_pgn(game, white_name, black_name, pgn, sizeof(pgn)));
    return;
}
