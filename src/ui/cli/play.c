#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/notation.h"
#include "core/primitives.h"
#include "engine/engine.h"
#include "features/review.h"
#include "features/uci_client.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ANSI styles & colors
#define C_RESET  "\033[0m"
#define C_BOLD   "\033[1m"
#define C_DIM    "\033[2m"
#define C_RED    "\033[31m"
#define C_GREEN  "\033[32m"
#define C_YELLOW "\033[33m"
#define C_CYAN   "\033[36m"
#define C_WHITE  "\033[97m"

#define BG_LIGHT   "\033[48;5;252m"
#define BG_DARK    "\033[48;5;242m"
#define BG_LAST_L  "\033[48;5;221m"
#define BG_LAST_D  "\033[48;5;178m"
#define BG_SEL     "\033[48;5;38m"
#define BG_DEST_L  "\033[48;5;114m"
#define BG_DEST_D  "\033[48;5;71m"
#define BG_CHECK   "\033[48;5;196m"
#define FG_WHITE_P "\033[38;5;231m"
#define FG_BLACK_P "\033[38;5;16m"

// Eval bar colors (bright white vs charcoal black)
#define BAR_BG_W "\033[48;5;255m"
#define BAR_BG_B "\033[48;5;235m"
#define BAR_FG_W "\033[38;5;255m"
#define BAR_FG_B "\033[38;5;235m"

typedef enum game_mode {
    MODE_VS_BOT_WHITE = 1,
    MODE_VS_BOT_BLACK = 2,
    MODE_TWO_PLAYER = 3,
    MODE_BOT_VS_BOT = 4,
} game_mode;

static char g_san_history[MAX_GAME_LENGTH][16];

static void clear_screen(void) {
    printf("\033[2J\033[H");
}

static void sleep_ms(int ms) {
    if (ms <= 0) {
        return;
    }
    clock_t start = clock();
    clock_t wait = (clock_t) (((double) ms * (double) CLOCKS_PER_SEC) / 1000.0);
    while (clock() - start < wait) {
    }
}

static void trim_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ')) {
        s[--len] = '\0';
    }
}

static bool read_line(char *buf, size_t size) {
    if (!fgets(buf, (int) size, stdin)) {
        return false;
    }
    trim_newline(buf);
    return true;
}

static int simple_piece_value(u8 piece) {
    switch (piece) {
    case FRIENDLY_PAWN:
    case ENEMY_PAWN: return 100;
    case FRIENDLY_KNIGHT:
    case ENEMY_KNIGHT: return 320;
    case FRIENDLY_BISHOP:
    case ENEMY_BISHOP: return 330;
    case FRIENDLY_ROOK:
    case ENEMY_ROOK: return 500;
    case FRIENDLY_QUEEN:
    case ENEMY_QUEEN: return 900;
    default: return 0;
    }
}

static const char *piece_glyph(u8 piece) {
    switch (piece) {
    case FRIENDLY_KING: return "\u2654";
    case FRIENDLY_QUEEN: return "\u2655";
    case FRIENDLY_ROOK: return "\u2656";
    case FRIENDLY_BISHOP: return "\u2657";
    case FRIENDLY_KNIGHT: return "\u2658";
    case FRIENDLY_PAWN: return "\u2659";
    case ENEMY_KING: return "\u265A";
    case ENEMY_QUEEN: return "\u265B";
    case ENEMY_ROOK: return "\u265C";
    case ENEMY_BISHOP: return "\u265D";
    case ENEMY_KNIGHT: return "\u265E";
    case ENEMY_PAWN: return "\u265F\uFE0E";
    default: return " ";
    }
}

static void play_and_record(full_chess *game, u64 move, char *san_out, size_t san_size) {
    char san[16];
    static_move_to_san(game->game, game->current_legal_moves, move, san, sizeof(san));
    if (game->move_count < MAX_GAME_LENGTH) {
        snprintf(g_san_history[game->move_count], sizeof(g_san_history[0]), "%s", san);
    }
    if (san_out && san_size > 0) {
        snprintf(san_out, san_size, "%s", san);
    }
    make_move(game, move);
}

static void compute_material_diff(
    const chess *white_view, int *white_adv, char *w_caps, char *b_caps) {
    const u8 w_types[5]
        = { FRIENDLY_QUEEN, FRIENDLY_ROOK, FRIENDLY_BISHOP, FRIENDLY_KNIGHT, FRIENDLY_PAWN };
    const u8 b_types[5] = { ENEMY_QUEEN, ENEMY_ROOK, ENEMY_BISHOP, ENEMY_KNIGHT, ENEMY_PAWN };
    const int start_cnt[5] = { 1, 2, 2, 2, 8 };
    const char *w_sym[5] = { "\u2655", "\u2656", "\u2657", "\u2658", "\u2659" };
    const char *b_sym[5] = { "\u265B", "\u265C", "\u265D", "\u265E", "\u265F\uFE0E" };

    int w_cur[5] = { 0 };
    int b_cur[5] = { 0 };
    int w_mat = 0;
    int b_mat = 0;

    for (int sq = 0; sq < 64; sq++) {
        u8 pc = get_piece_at_offset(white_view, sq);
        for (int t = 0; t < 5; t++) {
            if (pc == w_types[t]) {
                w_cur[t]++;
                w_mat += simple_piece_value(pc);
            } else if (pc == b_types[t]) {
                b_cur[t]++;
                b_mat += simple_piece_value(pc);
            }
        }
    }

    *white_adv = (w_mat - b_mat) / 100;
    w_caps[0] = '\0';
    b_caps[0] = '\0';
    for (int t = 0; t < 5; t++) {
        for (int k = b_cur[t]; k < start_cnt[t]; k++) {
            strcat(w_caps, b_sym[t]);
        }
    }
    for (int t = 0; t < 5; t++) {
        for (int k = w_cur[t]; k < start_cnt[t]; k++) {
            strcat(b_caps, w_sym[t]);
        }
    }
}

static void print_recent_history(const full_chess *game) {
    if (game->move_count == 0) {
        return;
    }
    int start_ply = (game->move_count > 12) ? (game->move_count - 12) : 0;
    if (start_ply > 0 && ((start_ply + (game->start_black_to_move ? 1 : 0)) % 2) == 1) {
        start_ply--;
    }

    printf("  " C_DIM "History:" C_RESET);
    if (start_ply > 0) {
        printf(" ...");
    }
    for (int p = start_ply; p < game->move_count; p++) {
        int abs_ply = p + (game->start_black_to_move ? 1 : 0);
        int move_num = game->start_full_move + (abs_ply / 2);
        if ((abs_ply % 2) == 0) {
            printf(" " C_DIM "%d." C_RESET " %s", move_num, g_san_history[p]);
        } else {
            if (p == start_ply) {
                printf(" " C_DIM "%d..." C_RESET " %s", move_num, g_san_history[p]);
            } else {
                printf(" %s", g_san_history[p]);
            }
        }
    }
    printf("\n");
}

// Print 1 row (of 8) of the 16-step vertical evaluation bar.
static void print_eval_bar_row(int r_idx, bool view_from_black, int white_halves) {
    // r_idx runs 0 (top) .. 7 (bottom).
    // Each row covers 2 half-steps: upper half and lower half.
    if (!view_from_black) {
        // White fills from bottom (half-step 0..white_halves-1) upward.
        int low_half = (7 - r_idx) * 2;
        int high_half = low_half + 1;
        bool low_w = (low_half < white_halves);
        bool high_w = (high_half < white_halves);
        if (high_w && low_w) {
            printf(BAR_BG_W "  " C_RESET);
        } else if (!high_w && !low_w) {
            printf(BAR_BG_B "  " C_RESET);
        } else {
            // Lower half white, upper half black -> lower half block ▄
            printf(BAR_BG_B BAR_FG_W "\u2584\u2584" C_RESET);
        }
    } else {
        // Black at bottom -> White fills from top downward.
        int high_half = r_idx * 2;
        int low_half = high_half + 1;
        bool high_w = (high_half < white_halves);
        bool low_w = (low_half < white_halves);
        if (high_w && low_w) {
            printf(BAR_BG_W "  " C_RESET);
        } else if (!high_w && !low_w) {
            printf(BAR_BG_B "  " C_RESET);
        } else {
            // Upper half white, lower half black -> upper half block ▀
            printf(BAR_BG_B BAR_FG_W "\u2580\u2580" C_RESET);
        }
    }
}

static void render_ui(const full_chess *game, bool view_from_black, int selected_white_sq,
    u64 dest_white_mask, int eval_white_cp, const char *mode_label, const char *status_msg) {
    chess white_view = *(game->game);
    bool black_to_move = is_flipped(&white_view);
    if (black_to_move) {
        flip_perspective(&white_view);
    }

    int last_s = -1;
    int last_e = -1;
    const char *last_move_str = "—";
    if (game->move_count > 0) {
        u64 lm = game->move_history[game->move_count - 1];
        const chess *prev_board = &(game->game_history[game->move_count - 1]);
        bool prev_flipped = is_flipped(prev_board);
        int s = get_start_offset(lm);
        int e = get_end_offset(lm);
        int sr = prev_flipped ? (7 - (s / 8)) : (s / 8);
        int er = prev_flipped ? (7 - (e / 8)) : (e / 8);
        last_s = 8 * sr + (s % 8);
        last_e = 8 * er + (e % 8);
        last_move_str = g_san_history[game->move_count - 1];
    }

    int check_sq = -1;
    if (in_check(game->game)) {
        u64 king_bb = white_view.kings
                      & (black_to_move ? white_view.enemy_pieces : white_view.friendly_pieces);
        check_sq = get_next_offset(king_bb, -1);
    }

    int white_adv = 0;
    char w_caps[128];
    char b_caps[128];
    compute_material_diff(&white_view, &white_adv, w_caps, b_caps);

    double wp = engine_win_prob(eval_white_cp);
    int white_halves = (int) (wp * 16.0 + 0.5);
    if (white_halves < 0) {
        white_halves = 0;
    }
    if (white_halves > 16) {
        white_halves = 16;
    }
    char eval_str[16];
    engine_format_eval(eval_white_cp, eval_str, sizeof(eval_str));

    int full_move
        = game->start_full_move + (game->move_count + (game->start_black_to_move ? 1 : 0)) / 2;

    printf("\n  " C_CYAN C_BOLD "CHESS-IN-C" C_RESET C_DIM
           "  |  %s  |  Move %d  |  Last: %s  |  Clock: %u/150" C_RESET "\n",
        mode_label, full_move, last_move_str, (unsigned) game->half_move_count);

    if (!view_from_black) {
        printf("  " C_DIM "Black:" C_RESET " %s", b_caps[0] ? b_caps : "—");
        if (white_adv < 0) {
            printf(C_GREEN C_BOLD " +%d" C_RESET, -white_adv);
        }
    } else {
        printf("  " C_DIM "White:" C_RESET " %s", w_caps[0] ? w_caps : "—");
        if (white_adv > 0) {
            printf(C_GREEN C_BOLD " +%d" C_RESET, white_adv);
        }
    }
    printf("\n\n");

    // Header row: Eval tag + File letters
    printf("  " C_BOLD "%-5s" C_RESET "   ", eval_str);
    for (int f_idx = 0; f_idx < 8; f_idx++) {
        int file = view_from_black ? (7 - f_idx) : f_idx;
        printf(" %c ", 'a' + file);
    }
    printf("\n");

    for (int r_idx = 0; r_idx < 8; r_idx++) {
        int rank = view_from_black ? r_idx : (7 - r_idx);
        printf("   ");
        print_eval_bar_row(r_idx, view_from_black, white_halves);
        printf("  %d ", rank + 1);

        for (int f_idx = 0; f_idx < 8; f_idx++) {
            int file = view_from_black ? (7 - f_idx) : f_idx;
            int sq = 8 * rank + file;
            u8 pc = get_piece_at_offset(&white_view, sq);
            bool light = ((rank + file) % 2) != 0;

            const char *bg = light ? BG_LIGHT : BG_DARK;
            if (sq == last_s || sq == last_e) {
                bg = light ? BG_LAST_L : BG_LAST_D;
            }
            if (dest_white_mask & (1ull << sq)) {
                bg = light ? BG_DEST_L : BG_DEST_D;
            }
            if (sq == selected_white_sq) {
                bg = BG_SEL;
            }
            if (sq == check_sq) {
                bg = BG_CHECK;
            }

            const char *fg = (pc & (1 << 1)) ? FG_WHITE_P : FG_BLACK_P;
            if (pc == NONE && (dest_white_mask & (1ull << sq))) {
                printf("%s" FG_BLACK_P " \u2022 " C_RESET, bg);
            } else {
                printf("%s%s %s " C_RESET, bg, fg, piece_glyph(pc));
            }
        }
        printf(" %d", rank + 1);

        if (r_idx == 0) {
            printf("    " C_BOLD "Turn: " C_RESET "%s  " C_DIM "(Eval: %s)" C_RESET,
                black_to_move ? (C_YELLOW C_BOLD "Black" C_RESET)
                              : (C_WHITE C_BOLD "White" C_RESET),
                eval_str);
        } else if (r_idx == 1) {
            if (game->is_game_over) {
                if (game->is_draw) {
                    printf("    " C_YELLOW C_BOLD "DRAW!" C_RESET);
                } else {
                    printf("    " C_RED C_BOLD "CHECKMATE! %s wins!" C_RESET,
                        black_to_move ? "White" : "Black");
                }
            } else if (in_check(game->game)) {
                printf("    " C_RED C_BOLD "CHECK!" C_RESET);
            }
        } else if (r_idx == 3) {
            printf("    " C_DIM "Move input (SAN or coords):" C_RESET);
        } else if (r_idx == 4) {
            printf("    " C_CYAN "Nf3" C_RESET ", " C_CYAN "e4" C_RESET ", " C_CYAN "exd5" C_RESET
                   ", " C_CYAN "O-O" C_RESET ", " C_CYAN "e8=Q" C_RESET);
        } else if (r_idx == 5) {
            printf("    " C_CYAN "e2" C_RESET "      highlight piece moves");
        } else if (r_idx == 6) {
            printf("    " C_CYAN "undo" C_RESET " / " C_CYAN "hint" C_RESET " / " C_CYAN
                   "moves" C_RESET " / " C_CYAN "review" C_RESET);
        } else if (r_idx == 7) {
            printf("    " C_CYAN "pgn" C_RESET " / " C_CYAN "fen" C_RESET " / " C_CYAN
                   "flip" C_RESET " / " C_CYAN "new" C_RESET " / " C_CYAN "quit" C_RESET);
        }
        printf("\n");
    }

    printf("          ");
    for (int f_idx = 0; f_idx < 8; f_idx++) {
        int file = view_from_black ? (7 - f_idx) : f_idx;
        printf(" %c ", 'a' + file);
    }
    printf("\n\n");

    if (!view_from_black) {
        printf("  " C_DIM "White:" C_RESET " %s", w_caps[0] ? w_caps : "—");
        if (white_adv > 0) {
            printf(C_GREEN C_BOLD " +%d" C_RESET, white_adv);
        }
    } else {
        printf("  " C_DIM "Black:" C_RESET " %s", b_caps[0] ? b_caps : "—");
        if (white_adv < 0) {
            printf(C_GREEN C_BOLD " +%d" C_RESET, -white_adv);
        }
    }
    printf("\n");
    print_recent_history(game);

    if (status_msg && status_msg[0] != '\0') {
        printf("\n  " C_YELLOW "%s" C_RESET "\n", status_msg);
    }
}

static uci_engine *g_sf_ref = NULL;

static search_result cli_sf_review_cb(const full_chess *pos, void *ctx) {
    uci_engine *sf = (uci_engine *) ctx;
    return uci_search_depth(sf, pos, 13, 60).base;
}

static int compute_live_eval(engine *eng, const full_chess *game) {
    if (game->is_game_over) {
        if (game->is_draw) {
            return 0;
        }
        return engine_score_to_white(game->game, -MATE_SCORE);
    }
    if (g_sf_ref) {
        uci_search_result ur = uci_search_depth(g_sf_ref, game, 12, 45);
        if (ur.ok) {
            return engine_score_to_white(game->game, ur.base.score_cp);
        }
    }
    search_result sr = engine_search(eng, game, 5, 80);
    return engine_score_to_white(game->game, sr.score_cp);
}

static void print_game_over_reason(const full_chess *game, char *out, size_t out_size) {
    bool black_to_move = is_flipped(game->game);
    if (!game->is_draw) {
        snprintf(out, out_size, "Checkmate! %s wins. Type 'review', 'undo', 'new', or 'quit'.",
            black_to_move ? "White" : "Black");
    } else if (is_insufficient_material(game->game)) {
        snprintf(out, out_size,
            "Draw by insufficient material! Type 'review', 'undo', 'new', or 'quit'.");
    } else if (position_frequency(game) >= 5) {
        snprintf(out, out_size, "Draw by 5-fold repetition! Type 'review', 'undo', 'new', 'quit'.");
    } else if (game->half_move_count >= 150) {
        snprintf(out, out_size, "Draw by 75-move rule! Type 'review', 'undo', 'new', or 'quit'.");
    } else {
        snprintf(
            out, out_size, "Stalemate — it's a draw! Type 'review', 'undo', 'new', or 'quit'.");
    }
}

// ============================================================================
// Post-Game Review (Accuracy %, Move Classifications, Eval Graph, Key Moments)
// ============================================================================

static const char *class_badge(review_move_class cls) {
    switch (cls) {
    case REV_BRILLIANT: return "\033[1;36m!! Brilliant" C_RESET;
    case REV_BEST: return C_GREEN C_BOLD "★ Best      " C_RESET;
    case REV_GOOD: return C_CYAN "✓ Good      " C_RESET;
    case REV_INACCURACY: return C_YELLOW "?! Inacc.   " C_RESET;
    case REV_MISTAKE: return "\033[38;5;208m?  Mistake  " C_RESET;
    case REV_BLUNDER: return C_RED C_BOLD "?? Blunder  " C_RESET;
    default: return "            ";
    }
}

static void cli_review_progress(int ply, int total_plies, void *user_data) {
    (void) total_plies;
    (void) user_data;
    if (ply % 4 == 0) {
        printf(".");
        fflush(stdout);
    }
}

static void print_eval_graph(const int *evals_w, int num_points) {
    const int height = 9;
    int max_cols = 60;
    int cols = (num_points < max_cols) ? num_points : max_cols;
    if (cols < 2) {
        return;
    }

    printf("\n  " C_BOLD "Evaluation Timeline (White + above / Black - below):" C_RESET "\n");
    for (int row = height - 1; row >= 0; row--) {
        if (row == height - 1) {
            printf("  " C_DIM " +W |" C_RESET);
        } else if (row == height / 2) {
            printf("  " C_DIM "  0 |" C_RESET);
        } else if (row == 0) {
            printf("  " C_DIM " -B |" C_RESET);
        } else {
            printf("  " C_DIM "    |" C_RESET);
        }

        for (int c = 0; c < cols; c++) {
            int idx = (cols == num_points) ? c : (c * (num_points - 1)) / (cols - 1);
            double wp = engine_win_prob(evals_w[idx]); // 0.0 .. 1.0
            int level = (int) (wp * (double) (height - 1) + 0.5);
            if (row == level) {
                if (level > height / 2) {
                    printf(C_WHITE C_BOLD "\u2588" C_RESET);
                } else if (level < height / 2) {
                    printf(C_RED C_BOLD "\u2588" C_RESET);
                } else {
                    printf(C_CYAN "\u2500" C_RESET);
                }
            } else if (row == height / 2) {
                printf(C_DIM "\u00B7" C_RESET);
            } else if (row > height / 2 && row < level) {
                printf(C_WHITE "\u2592" C_RESET);
            } else if (row < height / 2 && row > level) {
                printf(C_RED "\u2592" C_RESET);
            } else {
                printf(" ");
            }
        }
        printf("\n");
    }
    printf("  " C_DIM "    +");
    for (int c = 0; c < cols; c++) {
        printf("-");
    }
    printf(" (plies 0..%d)" C_RESET "\n", num_points - 1);
}

static void run_game_review(engine *eng, const full_chess *game) {
    int total_plies = (int) game->move_count;
    if (total_plies == 0) {
        printf("\n  No moves have been played yet to review.\n");
        return;
    }

    printf("\n" C_CYAN C_BOLD);
    printf("  +======================================================================+\n");
    printf("  |                         POST-GAME REVIEW                             |\n");
    printf("  +======================================================================+\n");
    printf(C_RESET);
    printf("  Analyzing %d plies (%s)", total_plies, g_sf_ref ? "Stockfish" : "Titan-C");
    fflush(stdout);

    game_review rev;
    if (!review_analyze_game_ex(eng, game, NULL, 0, 9, 100, g_sf_ref ? cli_sf_review_cb : NULL,
            g_sf_ref, cli_review_progress, NULL, &rev)) {
        printf(" failed.\n");
        return;
    }
    printf(" done!\n");

    printf("\n  " C_BOLD "%-14s  %10s  %10s" C_RESET "\n", "Summary", "White", "Black");
    printf("  ----------------------------------------\n");
    printf("  %-14s  " C_GREEN C_BOLD "%9.1f%%" C_RESET "  " C_GREEN C_BOLD "%9.1f%%" C_RESET "\n",
        "Accuracy", rev.white.accuracy, rev.black.accuracy);
    printf("  %-14s  " C_CYAN C_BOLD "%10d" C_RESET "  " C_CYAN C_BOLD "%10d" C_RESET "\n",
        "Est. Game Elo", rev.white.est_elo, rev.black.est_elo);
    printf("  %-14s  %10s  %10s\n", "Perf. Tier", review_elo_tier(rev.white.est_elo),
        review_elo_tier(rev.black.est_elo));
    printf("  %-14s  %10d  %10d\n", "Avg CP Loss", rev.white.acpl, rev.black.acpl);
    printf("  \033[1;36m%-14s" C_RESET "  %10d  %10d\n", "!! Brilliant",
        rev.white.counts[REV_BRILLIANT], rev.black.counts[REV_BRILLIANT]);
    printf("  " C_GREEN "%-14s" C_RESET "  %10d  %10d\n", "★ Best", rev.white.counts[REV_BEST],
        rev.black.counts[REV_BEST]);
    printf("  " C_CYAN "%-14s" C_RESET "  %10d  %10d\n", "✓ Good", rev.white.counts[REV_GOOD],
        rev.black.counts[REV_GOOD]);
    printf("  " C_YELLOW "%-14s" C_RESET "  %10d  %10d\n", "?! Inaccuracy",
        rev.white.counts[REV_INACCURACY], rev.black.counts[REV_INACCURACY]);
    printf("  \033[38;5;208m%-14s" C_RESET "  %10d  %10d\n", "?  Mistake",
        rev.white.counts[REV_MISTAKE], rev.black.counts[REV_MISTAKE]);
    printf("  " C_RED C_BOLD "%-14s" C_RESET "  %10d  %10d\n", "?? Blunder",
        rev.white.counts[REV_BLUNDER], rev.black.counts[REV_BLUNDER]);

    print_eval_graph(rev.evals_w, rev.total_plies + 1);

    if (rev.turning_point_ply >= 0 && rev.turning_point_drop >= 0.08) {
        const review_ply *k = &rev.plies[rev.turning_point_ply];
        char eb[16], ea[16];
        engine_format_eval(k->eval_before_w, eb, sizeof(eb));
        engine_format_eval(k->eval_after_w, ea, sizeof(ea));
        printf("\n  " C_YELLOW C_BOLD "Biggest Turning Point:" C_RESET " %d%s %s (%s -> %s)  |  "
               "Engine preferred: " C_GREEN C_BOLD "%s" C_RESET "\n",
            k->move_num, k->was_black ? "..." : ".", k->played_san, eb, ea, k->best_san);
    }

    printf("\n  " C_BOLD "%-7s %-9s %-14s %-8s %s" C_RESET "\n", "Move", "Played", "Rating", "Eval",
        "Engine Suggestion");
    printf("  ----------------------------------------------------------------\n");
    for (int p = 0; p < rev.total_plies; p++) {
        const review_ply *r = &rev.plies[p];
        char mv_label[16];
        snprintf(mv_label, sizeof(mv_label), "%d%s", r->move_num, r->was_black ? "..." : ".");
        char ev_str[16];
        engine_format_eval(r->eval_after_w, ev_str, sizeof(ev_str));

        printf("  %-7s %-9s %s %-8s ", mv_label, r->played_san, class_badge(r->cls), ev_str);
        if (r->cls >= REV_INACCURACY && strcmp(r->played_san, r->best_san) != 0) {
            printf("Best: " C_GREEN "%s" C_RESET C_DIM " (-%d cp)" C_RESET, r->best_san, r->cpl);
        }
        printf("\n");
    }
    printf("\n");

    review_free(&rev);
}

// ============================================================================
// Prompts & Main Loop
// ============================================================================

static int prompt_difficulty(const char *who, const uci_engine *sf) {
    printf("\n  Select %s difficulty:\n", who);
    int n = engine_num_presets();
    for (int i = 1; i <= n; i++) {
        const difficulty_preset *p = engine_get_preset(i);
        printf("    " C_BOLD "[%d]" C_RESET " %-14s " C_DIM "%s" C_RESET "\n", p->level, p->name,
            p->desc);
    }
    int max_lvl = n;
    if (sf) {
        int nsf = uci_num_presets();
        for (int i = 0; i < nsf; i++) {
            const sf_preset *sp = uci_get_preset(uci_first_level() + i);
            printf("    " C_BOLD "[%d]" C_RESET " " C_CYAN "%-14s" C_RESET " " C_DIM "%s" C_RESET
                   "\n",
                sp->level, sp->name, sp->desc);
            max_lvl = sp->level;
        }
    }
    printf("  Difficulty [1-%d] (default 4): ", max_lvl);
    fflush(stdout);
    char line[64];
    if (!read_line(line, sizeof(line)) || line[0] == '\0') {
        return 4;
    }
    int lvl = atoi(line);
    if (lvl < 1 || lvl > max_lvl) {
        return 4;
    }
    return lvl;
}

static game_mode prompt_mode(void) {
    printf("\n" C_CYAN C_BOLD);
    printf("  +================================================+\n");
    printf("  |            CHESS-IN-C  ::  PLAY MODE           |\n");
    printf("  +================================================+\n");
    printf(C_RESET "\n");
    printf("    " C_BOLD "[1]" C_RESET " Play as " C_WHITE C_BOLD "White" C_RESET " vs Engine\n");
    printf("    " C_BOLD "[2]" C_RESET " Play as " C_YELLOW C_BOLD "Black" C_RESET " vs Engine\n");
    printf("    " C_BOLD "[3]" C_RESET " 2-Player (Pass & Play)\n");
    printf("    " C_BOLD "[4]" C_RESET " Watch " C_GREEN C_BOLD "Bot vs Bot" C_RESET "\n\n");
    printf("  Select mode [1-4] (default 1): ");
    fflush(stdout);

    char line[64];
    if (!read_line(line, sizeof(line)) || line[0] == '\0') {
        return MODE_VS_BOT_WHITE;
    }
    switch (line[0]) {
    case '2': return MODE_VS_BOT_BLACK;
    case '3': return MODE_TWO_PLAYER;
    case '4': return MODE_BOT_VS_BOT;
    default: return MODE_VS_BOT_WHITE;
    }
}

static bool is_bot_turn(game_mode mode, const full_chess *game) {
    bool black_to_move = is_flipped(game->game);
    if (mode == MODE_BOT_VS_BOT) {
        return true;
    }
    if (mode == MODE_VS_BOT_WHITE) {
        return black_to_move;
    }
    if (mode == MODE_VS_BOT_BLACK) {
        return !black_to_move;
    }
    return false;
}

static u64 prompt_promotion_san(const full_chess *game, const char *base, bool uci_style) {
    printf("  Promote to [Q/R/B/N] (default Q): ");
    fflush(stdout);
    char pbuf[32];
    char p = 'Q';
    if (read_line(pbuf, sizeof(pbuf)) && pbuf[0] != '\0') {
        p = (char) toupper((unsigned char) pbuf[0]);
    }
    char try_str[32];
    if (uci_style) {
        snprintf(try_str, sizeof(try_str), "%s%c", base, (char) tolower((unsigned char) p));
        return static_encode_string(game, try_str);
    }
    snprintf(try_str, sizeof(try_str), "%s=%c", base, p);
    return static_encode_san(game, try_str, NULL);
}

int main(int argc, char **argv) {
    srand((unsigned) time(NULL));

    game_mode mode = (game_mode) 0;
    const char *start_fen = NULL;
    const char *sf_path = NULL;
    bool no_clear = false;
    bool auto_review = true;
    int white_level = 0;
    int black_level = 0;
    int bot_delay_ms = 250;
    int max_watch_plies = 0; // 0 = play to natural finish

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--white") == 0) {
            mode = MODE_VS_BOT_WHITE;
        } else if (strcmp(argv[i], "--black") == 0) {
            mode = MODE_VS_BOT_BLACK;
        } else if (strcmp(argv[i], "--2p") == 0) {
            mode = MODE_TWO_PLAYER;
        } else if (strcmp(argv[i], "--watch") == 0 || strcmp(argv[i], "--bot-vs-bot") == 0) {
            mode = MODE_BOT_VS_BOT;
        } else if (strcmp(argv[i], "--level") == 0 && i + 1 < argc) {
            int l = atoi(argv[++i]);
            white_level = l;
            black_level = l;
        } else if (strcmp(argv[i], "--white-level") == 0 && i + 1 < argc) {
            white_level = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--black-level") == 0 && i + 1 < argc) {
            black_level = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--delay") == 0 && i + 1 < argc) {
            bot_delay_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--max-plies") == 0 && i + 1 < argc) {
            max_watch_plies = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--no-clear") == 0) {
            no_clear = true;
        } else if (strcmp(argv[i], "--no-review") == 0) {
            auto_review = false;
        } else if (strcmp(argv[i], "--fen") == 0 && i + 1 < argc) {
            start_fen = argv[++i];
        } else if (strcmp(argv[i], "--stockfish") == 0 && i + 1 < argc) {
            sf_path = argv[++i];
        }
    }

    uci_engine *sf = uci_create(sf_path);
    g_sf_ref = uci_create(sf_path);
    if (g_sf_ref) {
        uci_set_option(g_sf_ref, "Threads", "1");
        uci_set_option(g_sf_ref, "Hash", "32");
    }

    if (mode == 0) {
        mode = prompt_mode();
    }
    if (mode == MODE_VS_BOT_WHITE && black_level == 0) {
        black_level = prompt_difficulty("Bot (Black)", sf);
    } else if (mode == MODE_VS_BOT_BLACK && white_level == 0) {
        white_level = prompt_difficulty("Bot (White)", sf);
    } else if (mode == MODE_BOT_VS_BOT) {
        if (white_level == 0) {
            white_level = prompt_difficulty("White Bot", sf);
        }
        if (black_level == 0) {
            black_level = prompt_difficulty("Black Bot", sf);
        }
    }

    const char *w_bot_name = (sf && uci_is_stockfish_level(white_level))
                                 ? uci_get_preset(white_level)->name
                                 : engine_get_preset(white_level)->name;
    const char *b_bot_name = (sf && uci_is_stockfish_level(black_level))
                                 ? uci_get_preset(black_level)->name
                                 : engine_get_preset(black_level)->name;

    char mode_label[64];
    char white_player[64];
    char black_player[64];
    if (mode == MODE_VS_BOT_WHITE) {
        snprintf(mode_label, sizeof(mode_label), "White vs %s", b_bot_name);
        snprintf(white_player, sizeof(white_player), "Human");
        snprintf(black_player, sizeof(black_player), "%s", b_bot_name);
    } else if (mode == MODE_VS_BOT_BLACK) {
        snprintf(mode_label, sizeof(mode_label), "%s vs Black", w_bot_name);
        snprintf(white_player, sizeof(white_player), "%s", w_bot_name);
        snprintf(black_player, sizeof(black_player), "Human");
    } else if (mode == MODE_TWO_PLAYER) {
        snprintf(mode_label, sizeof(mode_label), "2-Player");
        snprintf(white_player, sizeof(white_player), "White");
        snprintf(black_player, sizeof(black_player), "Black");
    } else {
        snprintf(mode_label, sizeof(mode_label), "%s (W) vs %s (B)", w_bot_name, b_bot_name);
        snprintf(white_player, sizeof(white_player), "%s", w_bot_name);
        snprintf(black_player, sizeof(black_player), "%s", b_bot_name);
    }

    full_chess *game = full_chess_create();
    game->is_static = true;
    if (start_fen) {
        set_from_fen(game, start_fen);
        game->is_static = true;
    }

    engine *eng = engine_create(16);
    engine_set_threads(eng, 4);
    int eval_white_cp = compute_live_eval(eng, game);
    bool view_from_black = (mode == MODE_VS_BOT_BLACK);
    int selected_sq = -1;
    u64 dest_mask = 0ull;
    bool reviewed_this_finish = false;
    char status[256]
        = "Enter SAN (e.g. e4, Nf3, O-O, exd5) or coords (e2e4), or a square like e2 to inspect.";
    char line[256];

    while (true) {
        // Automatic post-game review trigger on game end
        if (game->is_game_over && !reviewed_this_finish) {
            reviewed_this_finish = true;
            if (!no_clear) {
                clear_screen();
            }
            print_game_over_reason(game, status, sizeof(status));
            eval_white_cp = compute_live_eval(eng, game);
            render_ui(game, view_from_black, -1, 0ull, eval_white_cp, mode_label, status);
            if (auto_review) {
                run_game_review(eng, game);
            }
            if (mode == MODE_BOT_VS_BOT) {
                printf("\n  " C_CYAN C_BOLD "Game PGN:" C_RESET "\n\n");
                static_print_pgn(game, white_player, black_player);
                break;
            }
        }

        // Bot vs Bot ply limit (for scripted/quick demos)
        if (mode == MODE_BOT_VS_BOT && max_watch_plies > 0 && game->move_count >= max_watch_plies) {
            if (!no_clear) {
                clear_screen();
            }
            render_ui(game, view_from_black, -1, 0ull, eval_white_cp, mode_label,
                "Reached max spectate plies.");
            if (auto_review) {
                run_game_review(eng, game);
            }
            break;
        }

        // Bot turn
        if (!game->is_game_over && is_bot_turn(mode, game)) {
            bool black_turn = is_flipped(game->game);
            int lvl = black_turn ? black_level : white_level;
            const char *bot_name = (sf && uci_is_stockfish_level(lvl))
                                       ? uci_get_preset(lvl)->name
                                       : engine_get_preset(lvl)->name;

            if (!no_clear) {
                clear_screen();
            }
            char think_msg[128];
            snprintf(think_msg, sizeof(think_msg), "%s (%s) is thinking...",
                black_turn ? "Black" : "White", bot_name);
            render_ui(game, view_from_black, -1, 0ull, eval_white_cp, mode_label, think_msg);
            fflush(stdout);

            search_result sr;
            if (sf && uci_is_stockfish_level(lvl)) {
                uci_search_result ur = uci_search_level(sf, game, lvl);
                sr = ur.base;
            } else {
                sr = engine_search_level(eng, game, lvl);
            }
            char san[16];
            play_and_record(game, sr.best_move, san, sizeof(san));
            eval_white_cp = compute_live_eval(eng, game);
            selected_sq = -1;
            dest_mask = 0ull;

            if (game->is_game_over) {
                print_game_over_reason(game, status, sizeof(status));
            } else {
                snprintf(status, sizeof(status), "%s (%s) played %s  [d=%d, %lu nodes, %.2fs]",
                    black_turn ? "Black" : "White", bot_name, san, sr.depth_reached,
                    (unsigned long) sr.nodes, sr.elapsed_sec);
            }
            if (mode == MODE_BOT_VS_BOT && bot_delay_ms > 0) {
                sleep_ms(bot_delay_ms);
            }
            continue;
        }

        if (!no_clear) {
            clear_screen();
        }
        render_ui(game, view_from_black, selected_sq, dest_mask, eval_white_cp, mode_label, status);
        status[0] = '\0';

        printf("\n  " C_BOLD "%s> " C_RESET, is_flipped(game->game) ? "Black" : "White");
        fflush(stdout);

        if (!read_line(line, sizeof(line))) {
            printf("\n");
            break;
        }
        if (line[0] == '\0') {
            continue;
        }

        // Commands
        if (strcmp(line, "quit") == 0 || strcmp(line, "q") == 0 || strcmp(line, "exit") == 0) {
            break;
        }
        if (strcmp(line, "flip") == 0) {
            view_from_black = !view_from_black;
            snprintf(status, sizeof(status), "Board flipped.");
            continue;
        }
        if (strcmp(line, "new") == 0 || strcmp(line, "reset") == 0) {
            full_chess_initialize(game);
            game->is_static = true;
            engine_clear_tt(eng);
            selected_sq = -1;
            dest_mask = 0ull;
            reviewed_this_finish = false;
            eval_white_cp = compute_live_eval(eng, game);
            snprintf(status, sizeof(status), "Started a new game.");
            continue;
        }
        if (strcmp(line, "review") == 0) {
            run_game_review(eng, game);
            if (!no_clear) {
                printf("  [Press Enter to return to board]");
                fflush(stdout);
                read_line(line, sizeof(line));
            }
            continue;
        }
        if (strcmp(line, "undo") == 0 || strcmp(line, "u") == 0) {
            if (game->move_count == 0) {
                snprintf(status, sizeof(status), "Nothing to undo.");
            } else {
                unmake_move(game);
                if (mode != MODE_TWO_PLAYER && is_bot_turn(mode, game) && game->move_count > 0) {
                    unmake_move(game);
                }
                selected_sq = -1;
                dest_mask = 0ull;
                reviewed_this_finish = false;
                eval_white_cp = compute_live_eval(eng, game);
                snprintf(status, sizeof(status), "Move undone.");
            }
            continue;
        }
        if (strcmp(line, "pgn") == 0) {
            printf("\n");
            static_print_pgn(game, white_player, black_player);
            if (!no_clear) {
                printf("\n  [Press Enter to return to board]");
                fflush(stdout);
                read_line(line, sizeof(line));
            }
            continue;
        }
        if (strcmp(line, "fen") == 0) {
            char fen[MAX_FEN_LENGTH];
            get_fen(game, fen);
            snprintf(status, sizeof(status), "FEN: %s", fen);
            continue;
        }
        if (strncmp(line, "fen ", 4) == 0) {
            set_from_fen(game, line + 4);
            game->is_static = true;
            selected_sq = -1;
            dest_mask = 0ull;
            reviewed_this_finish = false;
            eval_white_cp = compute_live_eval(eng, game);
            snprintf(status, sizeof(status), "Loaded custom FEN.");
            continue;
        }
        if (strcmp(line, "moves") == 0) {
            printf("\n  Legal moves (%d): ", num_legal_moves(game));
            static_print_san_legal_moves(game);
            if (!no_clear) {
                printf("  [Press Enter to continue]");
                fflush(stdout);
                read_line(line, sizeof(line));
            }
            continue;
        }
        if (strcmp(line, "hint") == 0) {
            if (game->is_game_over) {
                snprintf(status, sizeof(status), "Game is already over.");
            } else {
                search_result hint = g_sf_ref ? uci_search_depth(g_sf_ref, game, 15, 250).base
                                              : engine_search(eng, game, 6, 300);
                eval_white_cp = engine_score_to_white(game->game, hint.score_cp);
                char san[16];
                static_move_to_san(
                    game->game, game->current_legal_moves, hint.best_move, san, sizeof(san));
                bool flipped = is_flipped(game->game);
                int s = get_start_offset(hint.best_move);
                int e = get_end_offset(hint.best_move);
                int sr = flipped ? (7 - (s / 8)) : (s / 8);
                int er = flipped ? (7 - (e / 8)) : (e / 8);
                selected_sq = 8 * sr + (s % 8);
                dest_mask = 1ull << (8 * er + (e % 8));
                snprintf(status, sizeof(status), "Hint: try %s (d=%d)", san, hint.depth_reached);
            }
            continue;
        }
        if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
            snprintf(status, sizeof(status),
                "SAN: e4, Nf3, exd5, O-O, e8=Q | Coords: e2e4 | Inspect: e2 | Cmds: moves, hint, "
                "review, pgn, fen, undo, flip, new, quit");
            continue;
        }

        if (game->is_game_over) {
            print_game_over_reason(game, status, sizeof(status));
            continue;
        }

        // 1. Try SAN
        int san_matches = 0;
        u64 move = static_encode_san(game, line, &san_matches);
        if (move == NULL_MOVE && san_matches == 0) {
            char promo_probe[32];
            snprintf(promo_probe, sizeof(promo_probe), "%s=Q", line);
            int pmatches = 0;
            if (static_encode_san(game, promo_probe, &pmatches) != NULL_MOVE) {
                move = prompt_promotion_san(game, line, false);
            }
        }
        if (san_matches > 1) {
            snprintf(status, sizeof(status),
                "Ambiguous move '%s' (%d pieces can reach that square) — add origin file or rank.",
                line, san_matches);
            continue;
        }

        // 2. Fallback to UCI coordinates
        if (move == NULL_MOVE) {
            move = static_encode_string(game, line);
            if (move == NULL_MOVE && strlen(line) == 4) {
                char promo_try[8];
                snprintf(promo_try, sizeof(promo_try), "%sq", line);
                if (static_encode_string(game, promo_try) != NULL_MOVE) {
                    move = prompt_promotion_san(game, line, true);
                }
            }
        }

        // 3. Fallback to 2-char square highlight
        if (move == NULL_MOVE && strlen(line) == 2 && line[0] >= 'a' && line[0] <= 'h'
            && line[1] >= '1' && line[1] <= '8') {
            int f = line[0] - 'a';
            int r = line[1] - '1';
            int white_sq = 8 * r + f;
            bool flipped = is_flipped(game->game);
            int native_sq = 8 * (flipped ? (7 - r) : r) + f;

            u64 mask = 0ull;
            int found = 0;
            char list_buf[128] = "";
            for (int i = 0; game->current_legal_moves[i] != NULL_MOVE; i++) {
                u64 m = game->current_legal_moves[i];
                if (get_start_offset(m) == native_sq) {
                    int e = get_end_offset(m);
                    int er = flipped ? (7 - (e / 8)) : (e / 8);
                    mask |= 1ull << (8 * er + (e % 8));
                    char sbuf[16];
                    static_move_to_san(
                        game->game, game->current_legal_moves, m, sbuf, sizeof(sbuf));
                    if (strlen(list_buf) + strlen(sbuf) + 3 < sizeof(list_buf)) {
                        if (found > 0) {
                            strcat(list_buf, ", ");
                        }
                        strcat(list_buf, sbuf);
                    }
                    found++;
                }
            }
            if (found == 0) {
                selected_sq = -1;
                dest_mask = 0ull;
                snprintf(status, sizeof(status), "No legal moves from %s.", line);
            } else {
                selected_sq = white_sq;
                dest_mask = mask;
                snprintf(status, sizeof(status), "Moves from %s: %s", line, list_buf);
            }
            continue;
        }

        if (move == NULL_MOVE) {
            snprintf(status, sizeof(status),
                "Illegal move '%s'. Use SAN (Nf3, e4, exd5, O-O) or coords (e2e4), or type "
                "'moves'.",
                line);
            continue;
        }

        char san[16];
        play_and_record(game, move, san, sizeof(san));
        selected_sq = -1;
        dest_mask = 0ull;
        if (!game->is_game_over && !is_bot_turn(mode, game)) {
            eval_white_cp = compute_live_eval(eng, game);
        }
        if (game->is_game_over) {
            print_game_over_reason(game, status, sizeof(status));
        } else if (mode == MODE_TWO_PLAYER) {
            snprintf(status, sizeof(status), "Played %s.", san);
        }
    }

    uci_delete(&g_sf_ref);
    uci_delete(&sf);
    engine_delete(&eng);
    full_chess_delete(&game);
    printf("  Thanks for playing!\n\n");
    return 0;
}
