#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/notation.h"
#include "core/primitives.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ANSI color palette
#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_DIM     "\033[2m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"
#define C_CYAN    "\033[36m"
#define C_WHITE   "\033[97m"
#define BG_LIGHT  "\033[48;5;250m"
#define BG_DARK   "\033[48;5;240m"
#define BG_ATK    "\033[48;5;160m"
#define BG_CTRL   "\033[48;5;28m"
#define BG_BOTH   "\033[48;5;136m"
#define BG_CHECK  "\033[48;5;196m"
#define FG_WHITE  "\033[38;5;231m"
#define FG_BLACK  "\033[38;5;16m"

static bool g_step_mode = false;

static void pause_if_stepping(void) {
    if (!g_step_mode) {
        return;
    }
    printf(C_DIM "\n  [Press Enter for next chapter...]" C_RESET);
    fflush(stdout);
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
}

static void print_banner(void) {
    printf("\n" C_CYAN C_BOLD);
    printf("  +======================================================================+\n");
    printf("  |                    CHESS-IN-C  ::  v2 ENGINE SHOWCASE                |\n");
    printf("  |      6-Bitboard Core  |  Perspective Flipping  |  Reversible u64     |\n");
    printf("  +======================================================================+\n");
    printf(C_RESET "\n");
}

static void print_chapter(int num, const char *title, const char *subtitle) {
    printf("\n" C_YELLOW C_BOLD "  [%d/8] %s" C_RESET "\n", num, title);
    printf(C_DIM "  %s\n", subtitle);
    printf("  ----------------------------------------------------------------------" C_RESET "\n");
}

static const char *piece_unicode(u8 piece, bool white_perspective_color) {
    // When rendering a White-at-bottom board, FRIENDLY = White, ENEMY = Black.
    // When white_perspective_color is false (native flipped Black-to-move), FRIENDLY = Black!
    bool is_friendly = (piece & (1 << 1)) != 0;
    bool is_white = white_perspective_color ? is_friendly : !is_friendly;
    if (piece == NONE) {
        return " ";
    }
    if (piece & (1 << 5)) {
        return is_white ? "\u2654" : "\u265A"; // King
    }
    if ((piece & (1 << 3)) && (piece & (1 << 4))) {
        return is_white ? "\u2655" : "\u265B"; // Queen
    }
    if (piece & (1 << 3)) {
        return is_white ? "\u2656" : "\u265C"; // Rook
    }
    if (piece & (1 << 4)) {
        return is_white ? "\u2657" : "\u265D"; // Bishop
    }
    if (piece & (1 << 0)) {
        return is_white ? "\u2659" : "\u265F\uFE0E"; // Pawn
    }
    return is_white ? "\u2658" : "\u265E"; // Knight
}

static void print_fancy_board(const chess *board, const char *label, bool unflip_to_white,
    u64 highlight_red, u64 highlight_green) {
    chess view = *board;
    bool flipped_originally = is_flipped(&view);
    bool white_is_friendly = true;
    if (unflip_to_white && flipped_originally) {
        flip_perspective(&view);
        flip_bitboard(&highlight_red);
        flip_bitboard(&highlight_green);
    } else if (!unflip_to_white && flipped_originally) {
        white_is_friendly = false;
    }

    printf("\n    " C_BOLD "%s" C_RESET "\n", label);
    printf("       a  b  c  d  e  f  g  h\n");
    for (int rank = 7; rank >= 0; rank--) {
        int display_rank = rank + 1;
        printf("    %d ", display_rank);
        for (int file = 0; file < 8; file++) {
            int sq = 8 * rank + file;
            u64 bit = 1ull << sq;
            u8 pc = get_piece_at_offset(&view, sq);
            bool is_light = ((rank + file) % 2) != 0;
            bool red = (highlight_red & bit) != 0;
            bool grn = (highlight_green & bit) != 0;
            const char *bg = is_light ? BG_LIGHT : BG_DARK;
            if (red && grn) {
                bg = BG_BOTH;
            } else if (red) {
                bg = BG_ATK;
            } else if (grn) {
                bg = BG_CTRL;
            }
            if ((pc == FRIENDLY_KING || pc == ENEMY_KING) && red) {
                bg = BG_CHECK;
            }
            bool is_friendly = (pc & (1 << 1)) != 0;
            bool is_white = white_is_friendly ? is_friendly : !is_friendly;
            const char *fg = is_white ? FG_WHITE : FG_BLACK;
            printf("%s%s %s " C_RESET, bg, fg, piece_unicode(pc, white_is_friendly));
        }
        printf(" %d\n", display_rank);
    }
    printf("       a  b  c  d  e  f  g  h\n");
}

static void print_bitboard_trio(
    const char *t1, u64 b1, bool meta1, const char *t2, u64 b2, const char *t3, u64 b3) {
    printf("\n    " C_CYAN C_BOLD "%-22s" C_RESET "  " C_CYAN C_BOLD "%-22s" C_RESET
           "  " C_CYAN C_BOLD "%-22s" C_RESET "\n",
        t1, t2, t3);
    for (int rank = 7; rank >= 0; rank--) {
        printf("    ");
        for (int k = 0; k < 3; k++) {
            u64 b = (k == 0) ? b1 : ((k == 1) ? b2 : b3);
            bool show_meta = (k == 0 && meta1 && (rank == 0 || rank == 7));
            for (int file = 0; file < 8; file++) {
                u64 on = 1ull & (b >> (8 * rank + file));
                if (on) {
                    if (show_meta) {
                        printf(C_MAGENTA C_BOLD "M " C_RESET);
                    } else {
                        printf(C_GREEN C_BOLD "1 " C_RESET);
                    }
                } else {
                    if (show_meta) {
                        printf(C_YELLOW C_DIM ". " C_RESET);
                    } else {
                        printf(C_DIM ". " C_RESET);
                    }
                }
            }
            printf("        ");
        }
        printf("\n");
    }
}

static void print_move_anatomy(u64 move) {
    int s = get_start_offset(move);
    int e = get_end_offset(move);
    printf("    Raw u64: " C_CYAN C_BOLD "0x%016lx" C_RESET "  (", (unsigned long) move);
    chess_print_move(move);
    printf(")\n");
    printf("      [0..5]   start_offset     = %2d (%c%c)\n", s, 'a' + (s % 8), '1' + (s / 8));
    printf("      [6..11]  end_offset       = %2d (%c%c)\n", e, 'a' + (e % 8), '1' + (e / 8));
    printf("      [12..14] flags            = ep:%d  castle_K:%d  castle_Q:%d\n",
        is_en_passant_move(move), is_castle_king_move(move), is_castle_queen_move(move));
    printf("      [15..20] promotion_piece  = %2u ('%c')\n", get_promotion_piece(move),
        piece_to_char(get_promotion_piece(move)));
    printf("      [21..26] start_piece      = %2u ('%c')\n", get_start_piece(move),
        piece_to_char(get_start_piece(move)));
    printf("      [27..32] end_piece        = %2u ('%c')\n", get_end_piece(move),
        piece_to_char(get_end_piece(move)));
    printf("      [33..38] en_passant_piece = %2u ('%c')\n", get_en_passant_piece(move),
        piece_to_char(get_en_passant_piece(move)));
    printf("      [39..46] friendly_meta    = 0x%02x  (prior Rank 1 flags)\n",
        get_friendly_data(move));
    printf(
        "      [47..54] enemy_meta       = 0x%02x  (prior Rank 8 flags)\n", get_enemy_data(move));
}

static void chapter_1_bitboard_xray(full_chess *game) {
    print_chapter(1, "Inside the 48-Byte Brain (Bitboard X-Ray)",
        "The entire board + castling + en passant + turn + check fits in 6 uint64_t values.");

    full_chess_initialize(game);
    const chess *b = game->game;
    u64 occupied = b->friendly_pieces | b->enemy_pieces;
    u64 knights = occupied & ~(b->pawns & PAWN_MASK) & ~b->orthogonal_pieces & ~b->diagonal_pieces
                  & ~b->kings;
    u64 queens = b->orthogonal_pieces & b->diagonal_pieces;

    printf("    sizeof(chess) = " C_GREEN C_BOLD "%zu bytes" C_RESET " (6 bitboards x 8 bytes)\n",
        sizeof(chess));
    printf("    Legend: " C_GREEN C_BOLD "1" C_RESET " = piece bit, " C_MAGENTA C_BOLD "M" C_RESET
           " = packed metadata bit on rank 1 / rank 8 of `pawns`\n");

    print_bitboard_trio("1. pawns (+ metadata)", b->pawns, true, "2. friendly_pieces",
        b->friendly_pieces, "3. enemy_pieces", b->enemy_pieces);
    print_bitboard_trio("4. orthogonal (R+Q)", b->orthogonal_pieces, false, "5. diagonal (B+Q)",
        b->diagonal_pieces, "6. kings", b->kings);
    print_bitboard_trio("Derived: queens (O&D)", queens, false, "Derived: knights", knights,
        "Derived: occupied", occupied);
    pause_if_stepping();
}

static void chapter_2_perspective_flip(full_chess *game) {
    print_chapter(2, "The Perspective-Flipping Trick (Native vs Static)",
        "After every ply, the engine flips vertically & swaps colors so move-gen is always "
        "bottom-up.");

    full_chess_initialize(game);
    game->is_static = true;
    string_move(game, "e2e4");

    printf("    Played " C_CYAN C_BOLD "1. e2e4" C_RESET
           " — now it is Black's turn (is_flipped = %s, has_en_passant = %s).\n",
        is_flipped(game->game) ? C_GREEN "true" C_RESET : "false",
        has_en_passant(game->game) ? C_GREEN "true" C_RESET : "false");

    print_fancy_board(game->game, "Engine's Native View (Black flipped to bottom)", false, 0ull,
        1ull << get_en_passant_offset(game->game));
    print_fancy_board(game->game, "Static View (White anchored at bottom)", true, 0ull,
        1ull << get_en_passant_offset(game->game));

    printf("\n    " C_DIM
           "Green square = en passant target (e6 in flipped coords, e3 in standard chess)" C_RESET
           "\n");
    printf("    Native legal moves (%d): ", num_legal_moves(game));
    game->is_static = false;
    print_legal_moves(game);
    printf("    Static legal moves (%d): ", num_legal_moves(game));
    static_print_legal_moves(game);
    pause_if_stepping();
}

static void chapter_3_reversible_moves(full_chess *game) {
    print_chapter(3, "Anatomy of a Reversible 64-Bit Move & Time Travel",
        "Every u64 move packs captured pieces and prior metadata so unmake_move is 100% lossless.");

    set_from_fen(game, "r3k2r/ppp1pppp/8/3pP3/8/8/PPPP1PPP/R3K2R w KQkq d6 0 1");
    game->is_static = true;
    chess snapshot = *(game->game);

    print_fancy_board(game->game, "Starting Position (White to move, EP on d6, all castling)", true,
        0ull, 1ull << 43);

    u64 ep_move = static_encode_string(game, "e5d6");
    printf("\n    Inspecting en passant capture " C_CYAN C_BOLD "e5d6" C_RESET ":\n");
    print_move_anatomy(ep_move);

    make_move(game, ep_move);
    static_string_move(game, "e8c8"); // Black castles queenside!
    static_string_move(game, "e1g1"); // White castles kingside!

    print_fancy_board(
        game->game, "After 1. exd6 (en passant)  1... O-O-O  2. O-O", true, 0ull, 0ull);
    printf("    FEN now: " C_CYAN);
    print_fen(game);
    printf(C_RESET "\n");

    // Rewind all 3 moves
    unmake_move(game);
    unmake_move(game);
    unmake_move(game);
    bool exact = (memcmp(&snapshot, game->game, sizeof(chess)) == 0);
    printf("\n    Rewound 3 plies via " C_BOLD "unmake_move()" C_RESET
           " -> bit-for-bit memcmp match: %s\n",
        exact ? C_GREEN C_BOLD "EXACT MATCH (48/48 bytes)" C_RESET : C_RED "MISMATCH" C_RESET);
    pause_if_stepping();
}

static void chapter_4_attack_maps(full_chess *game) {
    print_chapter(4, "Attack Maps & Tactical Radar",
        "Visualizing get_attacked_board() and get_attacking_board() on Kiwipete (Black to move).");

    set_from_fen(game, "r3k2r/pppbbppp/2n2q1P/1P2p3/3p4/BN2PNP1/P1PnQPB1/R3K2R b KQkq - 0 1");
    u64 enemy_attacks = get_attacked_board(game->game);
    u64 friendly_attacks = get_attacking_board(game->game);

    printf("    FEN: " C_CYAN);
    print_fen(game);
    printf(C_RESET "\n");
    printf(
        "    Legend: " BG_ATK FG_WHITE " Red " C_RESET " = White attacks  " BG_CTRL FG_WHITE
        " Green " C_RESET " = Black controls  " BG_BOTH FG_WHITE " Gold " C_RESET " = Contested\n");

    print_fancy_board(game->game, "Kiwipete Tactical Control Map (Static View)", true,
        enemy_attacks, friendly_attacks);

    printf("    Squares attacked by opponent: " C_RED C_BOLD "%d" C_RESET
           "  |  Squares controlled by side to move: " C_GREEN C_BOLD "%d" C_RESET "\n",
        pop_count(enemy_attacks), pop_count(friendly_attacks));
    printf("    Raw enemy-attack bitboard (engine native print_bitboard):");
    print_bitboard(enemy_attacks);
    pause_if_stepping();
}

static void chapter_5_special_rules(full_chess *game) {
    print_chapter(5, "Edge-Case Rules: EP Pins, 4-Way Promotions & Castling Through Check",
        "The subtle rules that trip up chess engines.");

    // 1. Horizontal en passant discovered-check pin
    set_from_fen(game, "8/8/8/KPp4r/8/8/8/7k w - c6 0 1");
    print_fancy_board(game->game, "1) Horizontal En Passant Pin (b5xc6 would expose Ka5 to Rh5!)",
        true, get_attacked_board(game->game), 1ull << 42);
    bool ep_allowed = (static_encode_string(game, "b5c6") != NULL_MOVE);
    printf("    Legal moves: ");
    static_print_legal_moves(game);
    printf("    Is b5c6 (en passant) allowed? %s\n",
        ep_allowed ? C_RED "YES (BUG)" C_RESET : C_GREEN C_BOLD "NO (pin detected!)" C_RESET);

    // 2. 4-way pawn promotion
    set_from_fen(game, "8/4P3/8/8/8/8/k6K/8 w - - 0 1");
    print_fancy_board(game->game, "2) 4-Way Pawn Promotion on e7->e8", true, 0ull, 1ull << 60);
    printf("    Legal moves (%d): ", num_legal_moves(game));
    static_print_legal_moves(game);
    static_string_move(game, "e7e8n");
    printf("    Underpromoted with " C_CYAN "e7e8n" C_RESET " -> FEN: " C_CYAN);
    print_fen(game);
    printf(C_RESET "\n");

    // 3. Castling through check blocked by enemy bishop
    set_from_fen(game, "4k3/8/8/1b6/8/8/8/R3K2R w KQ - 0 1");
    print_fancy_board(game->game,
        "3) Castling Through Check (Bb5 slices f1 -> O-O blocked, O-O-O legal!)", true,
        get_attacked_board(game->game), 0ull);
    printf(
        "    Kingside O-O (e1g1) legal?   %s\n", (static_encode_string(game, "e1g1") != NULL_MOVE)
                                                     ? C_RED "YES" C_RESET
                                                     : C_GREEN C_BOLD "NO (f1 attacked)" C_RESET);
    printf("    Queenside O-O-O (e1c1) legal? %s\n",
        (static_encode_string(game, "e1c1") != NULL_MOVE) ? C_GREEN C_BOLD "YES" C_RESET
                                                          : C_RED "NO" C_RESET);
    pause_if_stepping();
}

static void chapter_6_legals_mate(full_chess *game) {
    print_chapter(6, "Live Miniature: Legal's Mate (1750) + FEN Serialization",
        "Playing a 7-move queen-sacrifice checkmate via static string moves.");

    full_chess_initialize(game);
    game->is_static = true;

    const char *moves[] = {
        "e2e4",
        "e7e5",
        "g1f3",
        "d7d6",
        "f1c4",
        "c8g4",
        "b1c3",
        "g7g6",
        "f3e5",
        "g4d1",
        "c4f7",
        "e8e7",
        "c3d5",
    };
    const char *san[] = {
        "1. e4",
        "e5",
        "2. Nf3",
        "d6",
        "3. Bc4",
        "Bg4",
        "4. Nc3",
        "g6",
        "5. Nxe5!",
        "Bxd1??",
        "6. Bxf7+",
        "Ke7",
        "7. Nd5#",
    };
    int n = (int) (sizeof(moves) / sizeof(moves[0]));
    printf("    Moves: ");
    for (int i = 0; i < n; i++) {
        string_move(game, moves[i]);
        printf(C_CYAN "%s" C_RESET " ", san[i]);
    }
    printf("\n");

    print_fancy_board(game->game, "Final Position: Legal's Mate (7. Nd5#)", true,
        get_attacked_board(game->game), 0ull);

    char fen[MAX_FEN_LENGTH];
    get_fen(game, fen);
    printf("    Status: in_check=" C_RED C_BOLD "%s" C_RESET
           "  legal_moves=%d  is_game_over=" C_GREEN "%s" C_RESET "  is_draw=%s\n",
        in_check(game->game) ? "true" : "false", num_legal_moves(game),
        game->is_game_over ? "true" : "false", game->is_draw ? "true" : "false");
    printf("    FEN: " C_CYAN "%s" C_RESET "\n", fen);

    printf("    Built-in ASCII board (static_print_board):");
    static_print_board(game);
    pause_if_stepping();
}

static void chapter_7_fide_draws(full_chess *game) {
    print_chapter(7, "FIDE Draw Rules & Checkmate Priority",
        "Stalemate, 5-fold repetition, and the 75-move rule (150 plies) vs checkmate.");

    // Stalemate
    set_from_fen(game, "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    printf("    1) Stalemate (7k/5Q2/6K1/8/8/8/8/8 b): in_check=%s, moves=%d -> " C_YELLOW C_BOLD
           "is_draw=%s, is_game_over=%s" C_RESET "\n",
        in_check(game->game) ? "true" : "false", num_legal_moves(game),
        game->is_draw ? "true" : "false", game->is_game_over ? "true" : "false");

    // 5-fold repetition
    full_chess_initialize(game);
    game->is_static = true;
    printf("    2) 5-fold repetition (Nf3 Nf6 Ng1 Ng8 x4): frequency = 1");
    for (int r = 0; r < 4; r++) {
        string_move(game, "g1f3");
        string_move(game, "g8f6");
        string_move(game, "f3g1");
        string_move(game, "f6g8");
        printf(" -> %d", position_frequency(game));
    }
    printf("  => " C_YELLOW C_BOLD "is_draw=%s, is_game_over=%s" C_RESET "\n",
        game->is_draw ? "true" : "false", game->is_game_over ? "true" : "false");

    // 75-move rule vs checkmate
    set_from_fen(game, "7k/5KQ1/8/8/8/8/8/8 w - - 149 80");
    game->is_static = true;
    string_move(game, "g7g5");
    printf("    3) Ply 150 non-mating move (Qg5):  half_moves=%u -> " C_YELLOW C_BOLD
           "is_draw=%s, is_game_over=%s" C_RESET "\n",
        (unsigned) game->half_move_count, game->is_draw ? "true" : "false",
        game->is_game_over ? "true" : "false");
    unmake_move(game);
    string_move(game, "g7g8");
    printf("    4) Unmake + Ply 150 checkmate (Qg8#): half_moves=%u -> " C_GREEN C_BOLD
           "is_draw=%s, is_game_over=%s (CHECKMATE WINS!)" C_RESET "\n",
        (unsigned) game->half_move_count, game->is_draw ? "true" : "false",
        game->is_game_over ? "true" : "false");
    pause_if_stepping();
}

static void chapter_8_perft_benchmark(full_chess *game) {
    print_chapter(8, "Stockfish-Style Perft Divide & Speed Benchmark",
        "Exact move-tree node counts verified against Chess Programming Wiki reference values.");

    full_chess_initialize(game);
    printf("    Live perft divide from Startpos (depth 3):\n\n");
    static_perft(game, 3, true);

    typedef struct bench_case {
        const char *name;
        const char *fen;
        int depth;
        u64 expected;
    } bench_case;

    const bench_case cases[] = {
        { "Startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281 },
        { "Startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609 },
        { "Kiwipete (White)",
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603 },
        { "Pos 3 (EP Pins)", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624 },
        { "Pos 4 (Mirrored B)", "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
            4, 422333 },
        { "Pos 5 (Promotions)", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4,
            2103487 },
        { "Pos 6 (Midgame)",
            "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4,
            3894594 },
    };

    printf("    " C_BOLD "%-20s  %-5s  %10s  %8s  %12s  %s" C_RESET "\n", "Position", "Depth",
        "Nodes", "Time", "Speed", "Check");
    printf("    ----------------------------------------------------------------------\n");

    int count = (int) (sizeof(cases) / sizeof(cases[0]));
    u64 total_nodes = 0ull;
    double total_sec = 0.0;
    bool all_ok = true;

    for (int i = 0; i < count; i++) {
        set_from_fen(game, cases[i].fen);
        clock_t t0 = clock();
        u64 nodes = static_perft(game, cases[i].depth, false);
        clock_t t1 = clock();
        double sec = (double) (t1 - t0) / CLOCKS_PER_SEC;
        double nps = (sec > 0.0) ? ((double) nodes / sec) : 0.0;
        bool ok = (nodes == cases[i].expected);
        all_ok &= ok;
        total_nodes += nodes;
        total_sec += sec;

        printf("    %-20s  d=%-3d  %10lu  %6.3fs  %9.2f M/s  %s\n", cases[i].name, cases[i].depth,
            (unsigned long) nodes, sec, nps / 1e6,
            ok ? C_GREEN C_BOLD "EXACT" C_RESET : C_RED C_BOLD "FAIL" C_RESET);
    }

    printf("    ----------------------------------------------------------------------\n");
    printf("    " C_BOLD "Total: %lu nodes in %.3fs (%.2f M nodes/sec) — %s" C_RESET "\n\n",
        (unsigned long) total_nodes, total_sec,
        (total_sec > 0.0) ? ((double) total_nodes / total_sec) / 1e6 : 0.0,
        all_ok ? C_GREEN "100% VERIFIED" C_RESET : C_RED "ERRORS DETECTED" C_RESET);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--step") == 0) {
            g_step_mode = true;
        }
    }

    full_chess *game = full_chess_create();
    print_banner();

    chapter_1_bitboard_xray(game);
    chapter_2_perspective_flip(game);
    chapter_3_reversible_moves(game);
    chapter_4_attack_maps(game);
    chapter_5_special_rules(game);
    chapter_6_legals_mate(game);
    chapter_7_fide_draws(game);
    chapter_8_perft_benchmark(game);

    full_chess_delete(&game);
    return 0;
}
