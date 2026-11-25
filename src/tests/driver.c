#include "az/az_mcts.h"
#include "core/attacks.h"
#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/notation.h"
#include "core/primitives.h"
#include "engine/book.h"
#include "engine/endgame.h"
#include "engine/engine.h"
#include "engine/nnue.h"
#include "features/history.h"
#include "features/puzzles.h"
#include "features/review.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct perft_case {
    const char *name;
    const char *fen;
    int depth;
    u64 expected;
} perft_case;

// Helper: mirror a FEN (swap colors, flip ranks 1..8, swap castling KQ<->kq, flip EP 3<->6).
static void mirror_fen(const char *src, char *dst, size_t dst_size) {
    char ranks[8][16];
    int r = 0;
    int c = 0;
    int i = 0;

    while (src[i] != '\0' && src[i] != ' ') {
        if (src[i] == '/') {
            ranks[r][c] = '\0';
            r++;
            c = 0;
        } else {
            char ch = src[i];
            if (ch >= 'A' && ch <= 'Z') {
                ch = (char) tolower((unsigned char) ch);
            } else if (ch >= 'a' && ch <= 'z') {
                ch = (char) toupper((unsigned char) ch);
            }
            ranks[r][c++] = ch;
        }
        i++;
    }
    ranks[r][c] = '\0';

    size_t pos = 0;
    for (int k = 7; k >= 0; k--) {
        size_t len = strlen(ranks[k]);
        memcpy(dst + pos, ranks[k], len);
        pos += len;
        if (k > 0) {
            dst[pos++] = '/';
        }
    }

    // Active color
    if (src[i] == ' ') {
        i++;
    }
    dst[pos++] = ' ';
    dst[pos++] = (src[i] == 'w') ? 'b' : 'w';
    while (src[i] != '\0' && src[i] != ' ') {
        i++;
    }

    // Castling rights (swap KQ <-> kq, keep canonical order KQkq)
    if (src[i] == ' ') {
        i++;
    }
    bool wK = false, wQ = false, bK = false, bQ = false;
    while (src[i] != '\0' && src[i] != ' ') {
        switch (src[i]) {
        case 'K': bK = true; break;
        case 'Q': bQ = true; break;
        case 'k': wK = true; break;
        case 'q': wQ = true; break;
        default: break;
        }
        i++;
    }
    dst[pos++] = ' ';
    if (!wK && !wQ && !bK && !bQ) {
        dst[pos++] = '-';
    } else {
        if (wK) {
            dst[pos++] = 'K';
        }
        if (wQ) {
            dst[pos++] = 'Q';
        }
        if (bK) {
            dst[pos++] = 'k';
        }
        if (bQ) {
            dst[pos++] = 'q';
        }
    }

    // En passant square
    if (src[i] == ' ') {
        i++;
    }
    dst[pos++] = ' ';
    if (src[i] == '-' || src[i] == '\0') {
        dst[pos++] = '-';
    } else {
        dst[pos++] = src[i];
        dst[pos++] = (src[i + 1] == '3') ? '6' : '3';
    }
    while (src[i] != '\0' && src[i] != ' ') {
        i++;
    }

    // Copy remaining half-move / full-move fields as-is
    if (src[i] == ' ') {
        snprintf(dst + pos, dst_size - pos, "%s", src + i);
    } else {
        dst[pos] = '\0';
    }
}

// Verify all bitboard invariants on a single chess board.
static bool check_board_invariants(const chess *b, const char *ctx) {
    u64 occ = b->friendly_pieces | b->enemy_pieces;
    u64 pawns = b->pawns & PAWN_MASK;

    if (b->friendly_pieces & b->enemy_pieces) {
        printf("[FAIL] %s: friendly_pieces overlaps enemy_pieces\n", ctx);
        return false;
    }
    if ((pawns | b->orthogonal_pieces | b->diagonal_pieces | b->kings) & ~occ) {
        printf("[FAIL] %s: piece bit set outside occupied squares\n", ctx);
        return false;
    }
    if (pawns & (b->orthogonal_pieces | b->diagonal_pieces | b->kings)) {
        printf("[FAIL] %s: pawn overlaps another piece type\n", ctx);
        return false;
    }
    if (b->kings & (b->orthogonal_pieces | b->diagonal_pieces)) {
        printf("[FAIL] %s: king overlaps slider\n", ctx);
        return false;
    }
    if (pop_count(b->kings & b->friendly_pieces) != 1
        || pop_count(b->kings & b->enemy_pieces) != 1) {
        printf("[FAIL] %s: expected 1 friendly and 1 enemy king\n", ctx);
        return false;
    }
    // Check flip_perspective involution
    chess copy = *b;
    flip_perspective(&copy);
    flip_perspective(&copy);
    if (memcmp(b, &copy, sizeof(chess)) != 0) {
        printf("[FAIL] %s: double flip_perspective is not identity\n", ctx);
        return false;
    }
    return true;
}

// Recursively check invariants, move uniqueness, and bit-for-bit unmake across a move tree.
static bool walk_invariants(chess *board, int depth, u64 *checked_moves) {
    if (!check_board_invariants(board, "tree walk")) {
        return false;
    }
    u64 moves[MAX_LEGAL_MOVES];
    int count = set_legal_moves(board, moves);

    // Ensure no duplicate moves in move list
    for (int i = 0; i < count; i++) {
        if (moves[i] == NULL_MOVE) {
            printf("[FAIL] tree walk: NULL_MOVE inside move list\n");
            return false;
        }
        for (int j = i + 1; j < count; j++) {
            if (moves[i] == moves[j]) {
                printf("[FAIL] tree walk: duplicate legal move generated\n");
                return false;
            }
        }
    }

    if (depth <= 0) {
        return true;
    }

    for (int i = 0; i < count; i++) {
        chess before = *board;
        chess_make_move(board, moves[i]);
        (*checked_moves)++;

        if (depth > 1) {
            if (!walk_invariants(board, depth - 1, checked_moves)) {
                return false;
            }
        } else if (!check_board_invariants(board, "tree leaf")) {
            return false;
        }

        chess_unmake_move(board, moves[i]);
        if (memcmp(&before, board, sizeof(chess)) != 0) {
            printf("[FAIL] tree walk: chess_unmake_move did not restore exact 48 bytes\n");
            return false;
        }
    }
    return true;
}

static bool test_tree_invariants(full_chess *game) {
    const char *positions[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
    };
    int n = (int) (sizeof(positions) / sizeof(positions[0]));
    u64 checked = 0ull;
    for (int i = 0; i < n; i++) {
        set_from_fen(game, positions[i]);
        if (!walk_invariants(game->game, 2, &checked)) {
            return false;
        }
    }
    printf("[PASS] Bitboard invariants, move uniqueness & 48B unmake (%lu moves verified)\n",
        (unsigned long) checked);
    return true;
}

static bool test_fen_roundtrips(full_chess *game) {
    const char *fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "r3k2r/pppbbppp/2n2q1P/1P2p3/3pn3/BN2PNP1/P1PPQPB1/R3K2R b KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
        "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 12 45",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        "4k2r/6P1/8/8/8/8/8/4K3 w k - 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R b Kq - 99 123",
    };
    int count = (int) (sizeof(fens) / sizeof(fens[0]));
    char buf[MAX_FEN_LENGTH];
    bool all_ok = true;
    for (int i = 0; i < count; i++) {
        set_from_fen(game, fens[i]);
        get_fen(game, buf);
        if (strcmp(fens[i], buf) != 0) {
            printf("[FAIL] FEN roundtrip mismatch:\n  in : %s\n  out: %s\n", fens[i], buf);
            all_ok = false;
        }
    }
    if (all_ok) {
        printf("[PASS] FEN round-trip (%d positions, white & black to move)\n", count);
    }
    return all_ok;
}

static bool test_edge_cases(full_chess *game) {
    bool ok = true;
    char fen[MAX_FEN_LENGTH];

    // 1. Castling edge cases (White & Black)
    // b1 attacked -> O-O-O STILL legal!
    set_from_fen(game, "4k3/8/8/8/8/8/1r6/R3K2R w KQ - 0 1");
    if (static_encode_string(game, "e1c1") == NULL_MOVE) {
        printf("[FAIL] Queenside castling should be legal when only b1 is attacked\n");
        ok = false;
    }
    // a1 rook attacked -> O-O-O STILL legal!
    set_from_fen(game, "4k3/8/8/8/8/8/r7/R3K2R w KQ - 0 1");
    if (static_encode_string(game, "e1c1") == NULL_MOVE) {
        printf("[FAIL] Queenside castling should be legal when a1 rook is attacked\n");
        ok = false;
    }
    // b1 occupied -> O-O-O illegal, O-O legal
    set_from_fen(game, "4k3/8/8/8/8/8/8/Rn2K2R w KQ - 0 1");
    if (static_encode_string(game, "e1c1") != NULL_MOVE
        || static_encode_string(game, "e1g1") == NULL_MOVE) {
        printf("[FAIL] Castling blocked piece check\n");
        ok = false;
    }
    // King in check -> neither castling legal
    set_from_fen(game, "4k3/8/8/8/8/8/4r3/R3K2R w KQ - 0 1");
    if (static_encode_string(game, "e1c1") != NULL_MOVE
        || static_encode_string(game, "e1g1") != NULL_MOVE) {
        printf("[FAIL] Castling while in check should be illegal\n");
        ok = false;
    }
    // Pass-through square f1 attacked -> O-O illegal, O-O-O legal
    set_from_fen(game, "4k3/8/8/1b6/8/8/8/R3K2R w KQ - 0 1");
    if (static_encode_string(game, "e1g1") != NULL_MOVE
        || static_encode_string(game, "e1c1") == NULL_MOVE) {
        printf("[FAIL] Castling pass-through check (White)\n");
        ok = false;
    }
    // Black castling: b8 attacked (legal), f8 attacked (illegal)
    set_from_fen(game, "r3k2r/1R6/8/8/8/8/8/4K3 b kq - 0 1");
    if (static_encode_string(game, "e8c8") == NULL_MOVE) {
        printf("[FAIL] Black queenside castling should be legal when only b8 is attacked\n");
        ok = false;
    }
    set_from_fen(game, "r3k2r/8/8/8/1B6/8/8/4K3 b kq - 0 1");
    if (static_encode_string(game, "e8g8") != NULL_MOVE
        || static_encode_string(game, "e8c8") == NULL_MOVE) {
        printf("[FAIL] Castling pass-through check (Black)\n");
        ok = false;
    }

    // 2. Capturing an unmoved rook strips opponent's castling right
    set_from_fen(game, "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    game->is_static = true;
    string_move(game, "a1a8");
    get_fen(game, fen);
    if (strcmp(fen, "R3k2r/8/8/8/8/8/8/4K2R b Kk - 0 1") != 0) {
        printf("[FAIL] Rook capture did not strip both Q and q castling rights: %s\n", fen);
        ok = false;
    }
    unmake_move(game);
    // Promotion capturing enemy rook on h8 strips Black's kingside right
    set_from_fen(game, "4k2r/6P1/8/8/8/8/8/4K3 w k - 0 1");
    game->is_static = true;
    string_move(game, "g7h8q");
    get_fen(game, fen);
    if (strcmp(fen, "4k2Q/8/8/8/8/8/8/4K3 b - - 0 1") != 0) {
        printf("[FAIL] Promotion capturing rook did not strip k castling right: %s\n", fen);
        ok = false;
    }

    // 3. Double check: only king moves are legal (blocking or capturing 1 checker is illegal)
    set_from_fen(game, "4k3/8/8/8/1b6/3R4/4r3/4K3 w - - 0 1");
    if (num_legal_moves(game) != 3 || static_encode_string(game, "d3d2") != NULL_MOVE
        || static_encode_string(game, "d3e3") != NULL_MOVE
        || static_encode_string(game, "e1e2") == NULL_MOVE) {
        printf("[FAIL] Double-check evasion (got %d legal moves, expected 3)\n",
            num_legal_moves(game));
        ok = false;
    }

    // 4. En passant edge cases: horizontal pin, diagonal pin, resolving check via EP
    // Horizontal pin (White)
    set_from_fen(game, "8/8/8/KPp4r/8/8/8/7k w - c6 0 1");
    if (static_encode_string(game, "b5c6") != NULL_MOVE) {
        printf("[FAIL] White horizontal EP pin missed\n");
        ok = false;
    }
    // Horizontal pin (Black)
    set_from_fen(game, "7K/8/8/8/R4pPk/8/8/8 b - g3 0 1");
    if (static_encode_string(game, "f4g3") != NULL_MOVE) {
        printf("[FAIL] Black horizontal EP pin missed\n");
        ok = false;
    }
    // Diagonal pin on EP pawn
    set_from_fen(game, "8/8/5b2/3pP3/8/2K5/8/7k w - d6 0 1");
    if (static_encode_string(game, "e5d6") != NULL_MOVE) {
        printf("[FAIL] Diagonal EP pin missed\n");
        ok = false;
    }
    // En passant resolving pawn check!
    set_from_fen(game, "8/8/8/2k5/3Pp3/8/8/4K3 b - d3 0 1");
    if (!in_check(game->game) || static_encode_string(game, "e4d3") == NULL_MOVE) {
        printf("[FAIL] En passant should legally resolve pawn check\n");
        ok = false;
    }

    // 5. Pinned pawn promotion: push illegal, capturing pinner with all 4 promos legal
    set_from_fen(game, "1b2k3/2P5/8/8/8/6K1/8/8 w - - 0 1");
    if (static_encode_string(game, "c7c8q") != NULL_MOVE
        || static_encode_string(game, "c7b8q") == NULL_MOVE
        || static_encode_string(game, "c7b8r") == NULL_MOVE
        || static_encode_string(game, "c7b8b") == NULL_MOVE
        || static_encode_string(game, "c7b8n") == NULL_MOVE) {
        printf("[FAIL] Pinned promotion check\n");
        ok = false;
    }

    // 6. Invalid input & boundary resilience (should never crash or mutate board)
    full_chess_initialize(game);
    game->is_static = true;
    chess start_snap = *(game->game);
    string_move(game, "");
    string_move(game, "e2");
    string_move(game, "z9a1");
    string_move(game, "e2e5");
    string_move(game, "e2e4q");
    unmake_move(game); // ply 0 no-op
    if (memcmp(&start_snap, game->game, sizeof(chess)) != 0 || game->move_count != 0) {
        printf("[FAIL] Invalid input mutated board state\n");
        ok = false;
    }

    // 7. reset() restores loaded FEN cleanly
    set_from_fen(game, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    chess kiwi_snap = *(game->game);
    static_string_move(game, "e1g1");
    static_string_move(game, "e8g8");
    reset(game);
    if (memcmp(&kiwi_snap, game->game, sizeof(chess)) != 0 || game->move_count != 0) {
        printf("[FAIL] reset() did not restore initial loaded FEN\n");
        ok = false;
    }

    if (ok) {
        printf("[PASS] Edge cases (castling, rook captures, double-check, EP pins/checks, promos, "
               "input safety)\n");
    }
    return ok;
}

static bool test_rules_and_unmake(full_chess *game) {
    bool ok = true;

    // 1. Fool's mate -> checkmate (not draw)
    full_chess_initialize(game);
    game->is_static = true;
    string_move(game, "f2f3");
    string_move(game, "e7e5");
    string_move(game, "g2g4");
    string_move(game, "d8h4");
    if (!game->is_game_over || game->is_draw || !in_check(game->game)
        || num_legal_moves(game) != 0) {
        printf("[FAIL] Fool's mate checkmate detection\n");
        ok = false;
    }
    // Making a move after game over is a no-op
    u16 plies_at_mate = game->move_count;
    string_move(game, "e2e4");
    if (game->move_count != plies_at_mate) {
        printf("[FAIL] Move accepted after checkmate\n");
        ok = false;
    }

    for (int i = 0; i < 4; i++) {
        unmake_move(game);
    }
    char fen[MAX_FEN_LENGTH];
    get_fen(game, fen);
    if (strcmp(fen, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") != 0
        || game->is_game_over || game->is_draw || num_legal_moves(game) != 20) {
        printf("[FAIL] Unmake to startpos failed: %s\n", fen);
        ok = false;
    }

    // 2. Stalemate -> game over AND draw
    set_from_fen(game, "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    if (!game->is_game_over || !game->is_draw || in_check(game->game)
        || num_legal_moves(game) != 0) {
        printf("[FAIL] Stalemate detection\n");
        ok = false;
    }

    // 3. 5-fold repetition -> automatic draw
    full_chess_initialize(game);
    game->is_static = true;
    for (int rep = 0; rep < 4; rep++) {
        string_move(game, "g1f3");
        string_move(game, "g8f6");
        string_move(game, "f3g1");
        string_move(game, "f6g8");
    }
    if (!game->is_game_over || !game->is_draw || position_frequency(game) < 5) {
        printf("[FAIL] 5-fold repetition detection\n");
        ok = false;
    }
    unmake_move(game);
    if (game->is_game_over || game->is_draw) {
        printf("[FAIL] Unmake out of 5-fold repetition failed\n");
        ok = false;
    }

    // 4. 75-move rule (150 half-moves) triggers draw, unless checkmate is delivered on ply 150
    set_from_fen(game, "7k/5KQ1/8/8/8/8/8/8 w - - 149 80");
    game->is_static = true;
    string_move(game, "g7g5"); // not mate or stalemate -> 150 half-moves -> draw
    if (!game->is_game_over || !game->is_draw) {
        printf("[FAIL] 75-move draw detection\n");
        ok = false;
    }
    unmake_move(game);
    if (game->half_move_count != 149) {
        printf("[FAIL] half_move_count not restored by unmake_move\n");
        ok = false;
    }
    string_move(game, "g7g8"); // checkmate on half-move 150 -> NOT a draw
    if (!game->is_game_over || game->is_draw || !in_check(game->game)) {
        printf("[FAIL] Checkmate priority over 75-move rule\n");
        ok = false;
    }

    // 5. En passant capture resets half_move_count and unmakes cleanly
    set_from_fen(game, "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 17 12");
    game->is_static = true;
    chess before_ep = *(game->game);
    string_move(game, "e5d6");
    if (game->half_move_count != 0) {
        printf("[FAIL] En passant did not reset half_move_count\n");
        ok = false;
    }
    get_fen(game, fen);
    if (strcmp(fen, "rnbqkbnr/ppp1pppp/3P4/8/8/8/PPPP1PPP/RNBQKBNR b KQkq - 0 12") != 0) {
        printf("[FAIL] En passant execution FEN mismatch: %s\n", fen);
        ok = false;
    }
    unmake_move(game);
    if (memcmp(game->game, &before_ep, sizeof(chess)) != 0 || game->half_move_count != 17) {
        printf("[FAIL] En passant unmake mismatch\n");
        ok = false;
    }

    // 6. Insufficient material draws (FIDE: K vs K, K+B vs K, K+N vs K, K+B vs K+B same color)
    const char *insuff_draws[] = {
        "8/8/4k3/8/8/3K4/8/8 w - - 0 1", // K vs K
        "8/8/4k3/8/2B5/3K4/8/8 w - - 0 1", // K+B vs K
        "8/8/4k3/5n2/8/3K4/8/8 b - - 0 1", // K+N vs K
        "5b2/8/4k3/8/8/3K4/8/2B5 w - - 0 1", // K+B(c1 dark) vs K+B(f8 dark)
    };
    for (int i = 0; i < 4; i++) {
        set_from_fen(game, insuff_draws[i]);
        if (!game->is_game_over || !game->is_draw || !is_insufficient_material(game->game)) {
            printf("[FAIL] Insufficient material not detected for '%s'\n", insuff_draws[i]);
            ok = false;
        }
    }
    const char *suff_mat[] = {
        "2b5/8/4k3/8/8/3K4/8/2B5 w - - 0 1", // Opposite-colored bishops (c1 dark, c8 light)
        "8/8/4k3/8/8/3K4/8/2B2B2 w - - 0 1", // Bishop pair (c1 dark, f1 light)
        "8/8/4k3/5n2/2N5/3K4/8/8 w - - 0 1", // K+N vs K+N (helpmate possible)
        "8/8/4k3/8/8/3K4/4P3/8 w - - 0 1", // K+P vs K
    };
    for (int i = 0; i < 4; i++) {
        set_from_fen(game, suff_mat[i]);
        if (game->is_game_over || game->is_draw || is_insufficient_material(game->game)) {
            printf("[FAIL] Sufficient material falsely flagged as draw for '%s'\n", suff_mat[i]);
            ok = false;
        }
    }

    if (ok) {
        printf("[PASS] Rules & unmake (checkmate, stalemate, insufficient mat, 5-fold, 75-move, "
               "EP)\n");
    }
    return ok;
}

static bool test_san_notation(full_chess *game) {
    bool ok = true;
    char buf[16];
    int matches = 0;

    // 1. Legal's Mate played entirely via SAN (piece letters, captures, +, #, pawn moves)
    full_chess_initialize(game);
    const char *legals_san[] = {
        "e4",
        "e5",
        "Nf3",
        "d6",
        "Bc4",
        "Bg4",
        "Nc3",
        "g6",
        "Nxe5",
        "Bxd1",
        "Bxf7+",
        "Ke7",
        "Nd5#",
    };
    int n = (int) (sizeof(legals_san) / sizeof(legals_san[0]));
    for (int i = 0; i < n; i++) {
        u64 m = static_encode_san(game, legals_san[i], &matches);
        if (m == NULL_MOVE || matches != 1) {
            printf("[FAIL] SAN parse failed on Legal's Mate move '%s'\n", legals_san[i]);
            return false;
        }
        static_move_to_san(game->game, game->current_legal_moves, m, buf, sizeof(buf));
        if (strcmp(buf, legals_san[i]) != 0) {
            printf("[FAIL] SAN format mismatch: got '%s', expected '%s'\n", buf, legals_san[i]);
            ok = false;
        }
        make_move(game, m);
    }
    if (!game->is_game_over || game->is_draw) {
        printf("[FAIL] Legal's Mate SAN did not reach checkmate\n");
        ok = false;
    }

    // 2. Disambiguation: file (Nbd2), rank (R1a4), and full square (Qa1e5)
    set_from_fen(game, "4k3/8/8/8/8/5N2/8/1N2K3 w - - 0 1");
    if (static_encode_san(game, "Nd2", &matches) != NULL_MOVE || matches != 2) {
        printf("[FAIL] SAN should reject ambiguous 'Nd2' (matches=%d)\n", matches);
        ok = false;
    }
    u64 nbd2 = static_encode_san(game, "Nbd2", &matches);
    if (nbd2 == NULL_MOVE
        || strcmp(static_move_to_san(game->game, game->current_legal_moves, nbd2, buf, sizeof(buf)),
               "Nbd2")
               != 0) {
        printf("[FAIL] SAN file disambiguation 'Nbd2' failed (got '%s')\n", buf);
        ok = false;
    }

    set_from_fen(game, "R7/7k/8/8/8/8/8/R3K3 w - - 0 1");
    if (static_encode_san(game, "Ra4", &matches) != NULL_MOVE || matches != 2) {
        printf("[FAIL] SAN should reject ambiguous 'Ra4'\n");
        ok = false;
    }
    u64 r1a4 = static_encode_san(game, "R1a4", &matches);
    if (r1a4 == NULL_MOVE
        || strcmp(static_move_to_san(game->game, game->current_legal_moves, r1a4, buf, sizeof(buf)),
               "R1a4")
               != 0) {
        printf("[FAIL] SAN rank disambiguation 'R1a4' failed (got '%s')\n", buf);
        ok = false;
    }

    set_from_fen(game, "8/7k/8/Q7/8/8/8/Q3Q2K w - - 0 1");
    if (static_encode_san(game, "Qae5", &matches) != NULL_MOVE || matches != 2) {
        printf("[FAIL] SAN should reject ambiguous 'Qae5'\n");
        ok = false;
    }
    u64 qa1e5 = static_encode_san(game, "Qa1e5", &matches);
    if (qa1e5 == NULL_MOVE
        || strcmp(
               static_move_to_san(game->game, game->current_legal_moves, qa1e5, buf, sizeof(buf)),
               "Qa1e5")
               != 0) {
        printf("[FAIL] SAN full-square disambiguation 'Qa1e5' failed (got '%s')\n", buf);
        ok = false;
    }

    // 3. Castling (O-O, O-O-O), en passant (exd6), promotion with check/capture (e8=Q+, gxh8=N)
    set_from_fen(game, "r3k2r/ppp1pppp/8/3pP3/8/8/PPPP1PPP/R3K2R w KQkq d6 0 1");
    u64 ep = static_encode_san(game, "exd6", &matches);
    u64 oo = static_encode_san(game, "O-O", &matches);
    u64 ooo = static_encode_san(game, "O-O-O", &matches);
    if (ep == NULL_MOVE || oo == NULL_MOVE || ooo == NULL_MOVE) {
        printf("[FAIL] SAN castling / en passant parse failed\n");
        ok = false;
    }
    if (strcmp(
            static_move_to_san(game->game, game->current_legal_moves, ep, buf, sizeof(buf)), "exd6")
            != 0
        || strcmp(static_move_to_san(game->game, game->current_legal_moves, oo, buf, sizeof(buf)),
               "O-O")
               != 0
        || strcmp(static_move_to_san(game->game, game->current_legal_moves, ooo, buf, sizeof(buf)),
               "O-O-O")
               != 0) {
        printf("[FAIL] SAN castling / en passant format failed\n");
        ok = false;
    }

    set_from_fen(game, "4k2r/4P1P1/8/8/8/8/8/4K3 w - - 0 1");
    u64 promo_cap = static_encode_san(game, "gxh8=N", &matches);
    if (promo_cap == NULL_MOVE
        || strcmp(static_move_to_san(
                      game->game, game->current_legal_moves, promo_cap, buf, sizeof(buf)),
               "gxh8=N")
               != 0) {
        printf("[FAIL] SAN underpromotion capture 'gxh8=N' failed (got '%s')\n", buf);
        ok = false;
    }
    set_from_fen(game, "k7/4P3/8/8/8/8/8/4K3 w - - 0 1");
    u64 promo_chk = static_encode_san(game, "e8=Q+", &matches);
    if (promo_chk == NULL_MOVE
        || strcmp(static_move_to_san(
                      game->game, game->current_legal_moves, promo_chk, buf, sizeof(buf)),
               "e8=Q+")
               != 0) {
        printf("[FAIL] SAN promotion with check 'e8=Q+' failed (got '%s')\n", buf);
        ok = false;
    }

    // 4. Every legal move on Kiwipete (White & Black) & Sedlak must round-trip cleanly through SAN
    const char *san_fens[] = {
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "r3k2r/pppbbppp/2n2q1P/1P2p3/3pn3/BN2PNP1/P1PPQPB1/R3K2R b KQkq - 0 1",
        "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
    };
    int checked_san = 0;
    for (int f = 0; f < 3; f++) {
        set_from_fen(game, san_fens[f]);
        for (int i = 0; game->current_legal_moves[i] != NULL_MOVE; i++) {
            u64 m = game->current_legal_moves[i];
            static_move_to_san(game->game, game->current_legal_moves, m, buf, sizeof(buf));
            u64 parsed = static_encode_san(game, buf, &matches);
            if (parsed != m || matches != 1) {
                printf("[FAIL] SAN round-trip failed for '%s' on position %d\n", buf, f);
                ok = false;
            }
            checked_san++;
        }
    }

    // 5. Missing 'x' on capture must be rejected under strict SAN rules
    set_from_fen(game, "4k3/8/8/4p3/8/5N2/8/4K3 w - - 0 1");
    if (static_encode_san(game, "Ne5", &matches) != NULL_MOVE
        || static_encode_san(game, "Nxe5", &matches) == NULL_MOVE) {
        printf("[FAIL] SAN capture 'x' rule check failed\n");
        ok = false;
    }

    // 6. PGN generation: standard startpos (in-progress '*') and custom FEN checkmate ('1-0')
    full_chess_initialize(game);
    const char *pgn_moves[] = { "e4", "e5", "Nf3", "Nc6", "Bb5" };
    for (int i = 0; i < 5; i++) {
        make_move(game, static_encode_san(game, pgn_moves[i], &matches));
    }
    char pgn_buf[2048];
    static_get_pgn(game, "Alice", "Bob", pgn_buf, sizeof(pgn_buf));
    if (!strstr(pgn_buf, "[White \"Alice\"]") || !strstr(pgn_buf, "[Black \"Bob\"]")
        || !strstr(pgn_buf, "[Result \"*\"]") || !strstr(pgn_buf, "1. e4 e5 2. Nf3 Nc6 3. Bb5 *")) {
        printf("[FAIL] PGN mid-game export mismatch:\n%s\n", pgn_buf);
        ok = false;
    }

    set_from_fen(game, "4kb1r/p2n1ppp/4q3/4p1B1/4P3/1Q6/PPP2PPP/2KR4 w k - 1 16");
    make_move(game, static_encode_san(game, "Qb8+", &matches));
    make_move(game, static_encode_san(game, "Nxb8", &matches));
    make_move(game, static_encode_san(game, "Rd8#", &matches));
    static_get_pgn(game, "Morphy", "Allies", pgn_buf, sizeof(pgn_buf));
    if (!strstr(pgn_buf, "[SetUp \"1\"]")
        || !strstr(pgn_buf, "[FEN \"4kb1r/p2n1ppp/4q3/4p1B1/4P3/1Q6/PPP2PPP/2KR4 w k - 1 16\"]")
        || !strstr(pgn_buf, "[Result \"1-0\"]") || !strstr(pgn_buf, "16. Qb8+ Nxb8 17. Rd8# 1-0")) {
        printf("[FAIL] PGN custom-FEN export mismatch:\n%s\n", pgn_buf);
        ok = false;
    }

    if (ok) {
        printf("[PASS] SAN & PGN notation (Legal's Mate, disambiguation, promos, castling, PGN, "
               "%d round-trips)\n",
            checked_san);
    }
    return ok;
}

static bool test_random_playouts(full_chess *game) {
    srand(42u);
    full_chess *clone = full_chess_create();
    char fen[MAX_FEN_LENGTH];
    int total_plies = 0;

    for (int g = 0; g < 12; g++) {
        full_chess_initialize(game);
        chess start_board = *(game->game);

        int max_plies = 180;
        while (!game->is_game_over && game->move_count < max_plies) {
            u64 m = get_random_move(game);
            make_move(game, m);
            total_plies++;

            // Verify FEN export -> re-import reproduces exact board and legal move count
            get_fen(game, fen);
            set_from_fen(clone, fen);
            if (!chess_equal(game->game, clone->game)
                || num_legal_moves(game) != num_legal_moves(clone)) {
                printf("[FAIL] Random playout FEN sync mismatch at game %d ply %u: %s\n", g,
                    (unsigned) game->move_count, fen);
                full_chess_delete(&clone);
                return false;
            }
        }

        // Rewind entire game to ply 0 and check exact 48-byte restoration
        while (game->move_count > 0) {
            unmake_move(game);
        }
        if (memcmp(&start_board, game->game, sizeof(chess)) != 0 || game->half_move_count != 0
            || game->is_game_over || game->is_draw) {
            printf("[FAIL] Random playout rewind failed on game %d\n", g);
            full_chess_delete(&clone);
            return false;
        }
    }

    full_chess_delete(&clone);
    printf("[PASS] Random playouts + FEN reload + full rewind (12 games, %d plies)\n", total_plies);
    return true;
}

static bool test_mirror_symmetry_perft(full_chess *game, bool quick) {
    typedef struct sym_case {
        const char *name;
        const char *fen;
        int depth;
    } sym_case;

    const sym_case cases[] = {
        { "Open All-Castling", "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", 4 },
        { "EP + Double Promo", "8/1P4p1/8/2pP3k/K7/8/1p4P1/8 w - c6 0 1", 4 },
        { "Italian Midgame",
            "r1bq1rk1/ppp2ppp/2np1n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 0 7", 3 },
        { "Pins + Checks + Castling",
            "r3k2r/pb1p1ppp/1b4n1/n2NP2q/1p4n1/1B3N2/PP1P1PPP/R1BQK2R w KQkq - 0 1", 3 },
        { "Partial Castling + Rook", "r3k2r/8/8/8/8/8/8/2R1K2R b Kkq - 0 1", 4 },
    };

    int n = (int) (sizeof(cases) / sizeof(cases[0]));
    char mirrored[MAX_FEN_LENGTH];
    bool all_ok = true;

    for (int i = 0; i < n; i++) {
        int d = quick ? (cases[i].depth > 3 ? 3 : cases[i].depth) : cases[i].depth;
        mirror_fen(cases[i].fen, mirrored, sizeof(mirrored));

        set_from_fen(game, cases[i].fen);
        u64 w_nodes = static_perft(game, d, false);
        u64 native_nodes = perft(game, d, false);

        set_from_fen(game, mirrored);
        u64 b_nodes = static_perft(game, d, false);

        bool ok = (w_nodes == b_nodes) && (w_nodes == native_nodes) && (w_nodes > 0);
        printf("[%s] Mirror symmetry: %-24s d=%d  orig=%8lu  mirr=%8lu\n", ok ? "PASS" : "FAIL",
            cases[i].name, d, (unsigned long) w_nodes, (unsigned long) b_nodes);
        all_ok &= ok;
    }
    return all_ok;
}

static bool walk_zobrist_check(chess *board, u64 hash, int depth, u64 *checked) {
    if (hash != engine_hash_board(board)) {
        printf("[FAIL] Incremental Zobrist hash mismatch!\n");
        return false;
    }
    if (depth <= 0) {
        return true;
    }
    u64 moves[MAX_LEGAL_MOVES];
    int count = set_legal_moves(board, moves);
    for (int i = 0; i < count; i++) {
        u64 next_h = engine_update_hash(hash, board, moves[i]);
        chess_make_move(board, moves[i]);
        (*checked)++;
        if (next_h != engine_hash_board(board)) {
            printf("[FAIL] engine_update_hash mismatch on move %d!\n", i);
            chess_unmake_move(board, moves[i]);
            return false;
        }
        if (depth > 1 && !walk_zobrist_check(board, next_h, depth - 1, checked)) {
            chess_unmake_move(board, moves[i]);
            return false;
        }
        chess_unmake_move(board, moves[i]);
    }
    return true;
}

static bool test_engine_upgrades(full_chess *game) {
    attacks_init();
    bool ok = true;

    // 1. Magic Bitboards vs slow raycasters across 4,096 random occupancies on all 64 squares
    u64 rng = 0x123456789ABCDEF0ull;
    for (int i = 0; i < 4096; i++) {
        rng = rng * 6364136223846793005ull + 1442695040888963407ull;
        u64 occ = rng;
        rng = rng * 6364136223846793005ull + 1442695040888963407ull;
        occ &= rng; // sparse & dense mix
        int sq = i & 63;
        if (rook_attacks_bb(sq, occ) != rook_attacks_slow(sq, occ)) {
            printf("[FAIL] Rook magic mismatch on sq %d\n", sq);
            return false;
        }
        if (bishop_attacks_bb(sq, occ) != bishop_attacks_slow(sq, occ)) {
            printf("[FAIL] Bishop magic mismatch on sq %d\n", sq);
            return false;
        }
    }

    // 2. Tactical + Quiet move generators partition set_legal_moves exactly, and validate_packed_move matches!
    const char *stress_fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
    };
    int n_fens = (int) (sizeof(stress_fens) / sizeof(stress_fens[0]));
    for (int f = 0; f < n_fens; f++) {
        set_from_fen(game, stress_fens[f]);
        chess b1 = *(game->game);
        chess b2 = *(game->game);
        chess b3 = *(game->game);
        u64 all[MAX_LEGAL_MOVES];
        u64 tac[MAX_LEGAL_MOVES];
        u64 qui[MAX_LEGAL_MOVES];
        int ac = set_legal_moves(&b1, all);
        int tc = set_tactical_moves(&b2, tac);
        int qc = set_quiet_moves(&b3, qui);
        int expected_tc = 0;
        for (int i = 0; i < ac; i++) {
            u64 m = all[i];
            bool is_t = (get_end_piece(m) != NONE) || is_en_passant_move(m)
                        || (get_promotion_piece(m) != get_start_piece(m));
            if (is_t) {
                expected_tc++;
                if (!in_set(tac, m)) {
                    printf("[FAIL] set_tactical_moves missed tactical move on FEN %d\n", f);
                    ok = false;
                }
            } else if (!in_set(qui, m)) {
                printf("[FAIL] set_quiet_moves missed quiet move on FEN %d\n", f);
                ok = false;
            }
            // Verify fast O(1) validate_packed_move round-trips every legal move!
            int s = get_start_offset(m);
            int e = get_end_offset(m);
            u8 pr = get_promotion_piece(m);
            int pt = (pr == FRIENDLY_PAWN)
                         ? 0
                         : ((pr == FRIENDLY_KNIGHT)
                                 ? 1
                                 : ((pr == FRIENDLY_BISHOP)
                                         ? 2
                                         : ((pr == FRIENDLY_ROOK)
                                                 ? 3
                                                 : ((pr == FRIENDLY_QUEEN) ? 4 : 5))));
            u16 tag = (u16) (0x8000u | (unsigned) s | ((unsigned) e << 6) | ((unsigned) pt << 12));
            if (validate_packed_move(&b1, tag) != m) {
                printf("[FAIL] validate_packed_move mismatch on FEN %d move %d\n", f, i);
                ok = false;
            }
        }
        if (tc != expected_tc || tc + qc != ac) {
            printf("[FAIL] staged movegen partition mismatch on FEN %d (all=%d, tac=%d, qui=%d)\n",
                f, ac, tc, qc);
            ok = false;
        }
    }

    // 3. Incremental Zobrist Hash exactness across 2-ply walk of all 6 stress positions
    u64 zob_checked = 0ull;
    for (int f = 0; f < n_fens; f++) {
        set_from_fen(game, stress_fens[f]);
        chess b = *(game->game);
        if (!walk_zobrist_check(&b, engine_hash_board(&b), 2, &zob_checked)) {
            return false;
        }
    }

    // 4. Static Exchange Evaluation (SEE) sanity
    // White Q on d1 takes pawn on d5 defended by Black pawn on c6 -> losing (-800)
    set_from_fen(game, "4k3/8/2p5/3p4/8/8/8/3QK3 w - - 0 1");
    u64 qxd5 = static_encode_string(game, "d1d5");
    if (qxd5 == NULL_MOVE || engine_see(game->game, qxd5) >= 0) {
        printf("[FAIL] SEE did not flag QxP(defended) as losing\n");
        ok = false;
    }
    // White R battery on d1+d2 takes pawn on d5 defended by Black rook on d8 -> winning (+100)
    set_from_fen(game, "3rk3/8/8/3p4/8/8/3R4/3RK3 w - - 0 1");
    u64 rxd5 = static_encode_string(game, "d2d5");
    if (rxd5 == NULL_MOVE || engine_see(game->game, rxd5) <= 0) {
        printf("[FAIL] SEE x-ray battery RxP failed (got %d)\n", engine_see(game->game, rxd5));
        ok = false;
    }

    // 5. Opening book probe
    u64 book_rng = 42ull;
    full_chess_initialize(game);
    u64 bm1 = book_probe(game, NULL, &book_rng);
    if (bm1 == NULL_MOVE || !in_set(game->current_legal_moves, bm1)) {
        printf("[FAIL] Opening book failed on Startpos\n");
        ok = false;
    }
    static_string_move(game, "e2e4");
    static_string_move(game, "e7e5");
    u64 bm2 = book_probe(game, NULL, &book_rng);
    if (bm2 == NULL_MOVE || !in_set(game->current_legal_moves, bm2)) {
        printf("[FAIL] Opening book failed after 1. e4 e5\n");
        ok = false;
    }
    set_from_fen(game, "8/8/4k3/8/8/3K4/8/R7 w - - 0 1");
    if (book_probe(game, NULL, &book_rng) != NULL_MOVE) {
        printf("[FAIL] Opening book returned hit on endgame position\n");
        ok = false;
    }

    // 6. Endgame Mop-Up Eval + Lazy SMP Morphy Mate-in-2
    set_from_fen(game, "8/8/8/3k4/8/8/8/R3K3 w - - 0 1");
    int ev_center = engine_eval_static(game->game);
    set_from_fen(game, "k7/8/2K5/8/8/8/8/R7 w - - 0 1");
    int ev_corner = engine_eval_static(game->game);
    if (ev_corner <= ev_center + 40) {
        printf("[FAIL] Endgame mop-up eval did not reward cornered king (%d vs %d)\n", ev_corner,
            ev_center);
        ok = false;
    }

    engine *eng = engine_create(4);
    engine_set_threads(eng, 2);
    set_from_fen(game, "4kb1r/p2n1ppp/4q3/4p1B1/4P3/1Q6/PPP2PPP/2KR4 w k - 1 16");
    search_result sr = engine_search(eng, game, 6, 1500);
    u64 qb8 = static_encode_string(game, "b3b8");
    if (sr.best_move != qb8 || sr.score_cp < MATE_BOUND) {
        printf("[FAIL] Lazy SMP search failed Morphy mate-in-2 (score=%d)\n", sr.score_cp);
        ok = false;
    }
    // 7. Endgame Bitbases & Recognizers (exact KPK, wrong-color bishop fortress, KBNK)
    set_from_fen(
        game, "8/4k3/8/4K3/4P3/8/8/8 b - - 0 1"); // KPK opposition WIN for White (5th rank)
    if (engine_eval_static(game->game) > -500) {
        printf("[FAIL] KPK opposition WIN not detected (got %d)\n", engine_eval_static(game->game));
        ok = false;
    }
    set_from_fen(
        game, "8/4k3/8/4K3/4P3/8/8/8 w - - 0 1"); // KPK opposition DRAW (White must give way)
    if (engine_eval_static(game->game) != 0) {
        printf("[FAIL] KPK opposition DRAW not 0 (got %d)\n", engine_eval_static(game->game));
        ok = false;
    }
    set_from_fen(game, "7k/8/5K2/7P/8/8/8/8 w - - 0 1"); // KPK rook-pawn DRAW
    if (engine_eval_static(game->game) != 0) {
        printf("[FAIL] KPK rook-pawn DRAW not 0 (got %d)\n", engine_eval_static(game->game));
        ok = false;
    }
    set_from_fen(game, "7k/8/5K2/7P/4B3/8/8/8 w - - 0 1"); // Dark-sq Be4 (wrong color for h8!) DRAW
    if (engine_eval_static(game->game) != 0) {
        printf("[FAIL] Wrong-bishop rook-pawn fortress not 0 (got %d)\n",
            engine_eval_static(game->game));
        ok = false;
    }

    // 8. Quantized NNUE v2 loading, evaluation sanity & exact incremental accumulator parity!
    if (nnue_is_loaded()) {
        full_chess_initialize(game);
        int ev_start = nnue_evaluate(game->game);
        if (abs(ev_start) > 150) {
            printf("[FAIL] NNUE startpos eval out of range (got %d)\n", ev_start);
            ok = false;
        }
        set_from_fen(game, "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        int ev_upq = nnue_evaluate(game->game);
        set_from_fen(game, "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
        int ev_dnq = nnue_evaluate(game->game);
        if (ev_upq < 400 || ev_dnq > -400) {
            printf("[FAIL] NNUE material polarity failed (+Q=%d, -Q=%d)\n", ev_upq, ev_dnq);
            ok = false;
        }
        // Verify incremental nnue_acc_make_move AND lazy Finny-cached nnue_acc_materialize
        // match scratch nnue_evaluate BIT-FOR-BIT across all legal moves (including castling,
        // king-bucket / mirror transitions, captures, promotions, and en passant)!
        nnue_finny_entry test_finny[2][NNUE_MAX_KING_BUCKETS][2];
        memset(test_finny, 0, sizeof(test_finny));
        for (int f = 0; f < n_fens; f++) {
            set_from_fen(game, stress_fens[f]);
            nnue_acc stack[4];
            nnue_acc_refresh(&stack[0], game->game);
            for (int i = 0; game->current_legal_moves[i] != NULL_MOVE; i++) {
                u64 mv = game->current_legal_moves[i];
                nnue_acc child_acc;
                nnue_acc_make_move(&child_acc, &stack[0], game->game, mv);
                chess child = *(game->game);
                chess_make_move(&child, mv);
                int inc_ev = nnue_eval_acc(&child_acc, &child);
                int scr_ev = nnue_evaluate(&child);
                nnue_acc_mark_lazy(&stack[1], game->game, mv);
                nnue_acc_materialize(stack, test_finny, 1, &child);
                int lazy_ev = nnue_eval_acc(&stack[1], &child);
                if (inc_ev != scr_ev || lazy_ev != scr_ev) {
                    printf("[FAIL] NNUE v3 parity mismatch on FEN %d move %d (inc=%d lazy=%d "
                           "scr=%d)\n",
                        f, i, inc_ev, lazy_ev, scr_ev);
                    ok = false;
                    break;
                }
            }
        }
    }

    // 9. Multi-PV search (3 distinct sorted candidate lines with valid PVs) + UCI_Elo search
    full_chess_initialize(game);
    search_result elo_low = engine_search_elo(eng, game, 1200);
    search_result elo_hi = engine_search_elo(eng, game, 2200);
    if (!in_set(game->current_legal_moves, elo_low.best_move)
        || !in_set(game->current_legal_moves, elo_hi.best_move)) {
        printf("[FAIL] engine_search_elo returned illegal move!\n");
        ok = false;
    }
    multipv_line mpv[3];
    int n_mpv = engine_search_multipv(eng, game, 5, 1000, 3, mpv, NULL, NULL);
    if (n_mpv != 3 || mpv[0].best_move == mpv[1].best_move || mpv[1].best_move == mpv[2].best_move
        || mpv[0].best_move == mpv[2].best_move || mpv[0].score_cp < mpv[1].score_cp
        || mpv[1].score_cp < mpv[2].score_cp || mpv[0].pv_len < 1) {
        printf("[FAIL] Multi-PV returned invalid lines (n=%d, scores=%d/%d/%d)\n", n_mpv,
            mpv[0].score_cp, mpv[1].score_cp, mpv[2].score_cp);
        ok = false;
    }

    // 10. Post-Game Review: Lichess Win% accuracy + walking into real forced mate (-M2) -> Blunder!
    set_from_fen(game, "3r2k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 20");
    u64 rev_moves[3];
    int rev_m = 0;
    rev_moves[0] = static_encode_san(game, "Re7", &rev_m);
    make_move(game, rev_moves[0]);
    rev_moves[1] = static_encode_san(game, "Rd1+", &rev_m);
    make_move(game, rev_moves[1]);
    rev_moves[2] = static_encode_san(game, "Re1", &rev_m);
    make_move(game, rev_moves[2]);
    game_review rev;
    if (!review_analyze_game(eng, game, rev_moves, 3, 5, 150, NULL, NULL, &rev)
        || rev.plies[0].cls != REV_BLUNDER || rev.white.est_elo < 400 || rev.white.est_elo > 2800) {
        printf("[FAIL] Post-Game Review did not flag Re7?? (walking into -M2) as Blunder or bad "
               "est_elo (%s, elo=%d)\n",
            review_class_name(rev.plies[0].cls), rev.white.est_elo);
        ok = false;
    }
    review_free(&rev);

    // 10b. Verify Lichess Win% curve + Brilliant (!!) sacrifice detection (equal-mat Queen sac Qd8+!)
    {
        set_from_fen(game, "2r3k1/5ppp/8/1q6/8/8/3Q1PPP/3R2K1 w - - 0 20");
        u64 br_moves[3];
        int bm = 0;
        br_moves[0] = static_encode_san(game, "Qd8+", &bm);
        make_move(game, br_moves[0]);
        br_moves[1] = static_encode_san(game, "Rxd8", &bm);
        make_move(game, br_moves[1]);
        br_moves[2] = static_encode_san(game, "Rxd8#", &bm);
        make_move(game, br_moves[2]);
        game_review br_rev;
        if (!review_analyze_game(eng, game, br_moves, 3, 5, 150, NULL, NULL, &br_rev)
            || br_rev.plies[0].cls != REV_BRILLIANT || br_rev.white.counts[REV_BRILLIANT] != 1) {
            printf("[FAIL] Post-Game Review did not detect Qd8+! Queen sacrifice as Brilliant "
                   "(got %s)\n",
                review_class_name(br_rev.plies[0].cls));
            ok = false;
        }
        review_free(&br_rev);
    }

    // 11. Curated Puzzles: verify EVERY puzzle's FEN & solution move sequence is 100% legal
    //     and every "mate" puzzle ends in checkmate!
    int npuz = puzzle_count();
    for (int p = 0; p < npuz; p++) {
        const chess_puzzle *pz = puzzle_get(p);
        set_from_fen(game, pz->fen);
        game->is_static = true;
        for (int k = 0; k < pz->num_moves; k++) {
            u64 pm = static_encode_string(game, pz->moves[k]);
            if (pm == NULL_MOVE) {
                printf("[FAIL] Puzzle #%d (%s) illegal solution move #%d: '%s'\n", pz->id,
                    pz->title, k + 1, pz->moves[k]);
                ok = false;
                break;
            }
            make_move(game, pm);
        }
        if (strcmp(pz->theme, "mate") == 0 && (!game->is_game_over || game->is_draw)) {
            printf("[FAIL] Mate Puzzle #%d (%s) did not end in checkmate!\n", pz->id, pz->title);
            ok = false;
        }
    }

    // 12. Opening detector & Game History storage
    const char sic_najdorf[10][16]
        = { "e4", "c5", "Nf3", "d6", "d4", "cxd4", "Nxd4", "Nf6", "Nc3", "a6" };
    if (strcmp(history_detect_opening(sic_najdorf, 10), "Sicilian Defense: Najdorf") != 0) {
        printf("[FAIL] Opening detector missed Sicilian Najdorf\n");
        ok = false;
    }
    game_history gh;
    history_init(&gh);
    int hid = history_add(&gh, "Human", "Chess-in-C", "1-0", "Checkmate",
        "Sicilian Defense: Najdorf", "white", 24, 1850, 1600, "1. e4 c5 1-0");
    if (gh.count != 1 || history_find_by_id(&gh, hid) == NULL
        || !history_update_elos(&gh, hid, 1900, 1650) || !history_delete_by_id(&gh, hid)
        || gh.count != 0) {
        printf("[FAIL] Game history CRUD check failed\n");
        ok = false;
    }
    history_free(&gh);

    // 13. Zero-C AlphaZero C MCTS: 4672 action bijection + 18-plane encoding + mate-in-1 PUCT!
    for (int f = 0; f < n_fens; f++) {
        set_from_fen(game, stress_fens[f]);
        u64 az_mvs[MAX_LEGAL_MOVES];
        int32_t az_acts[MAX_LEGAL_MOVES];
        int az_n = az_legal_actions(game->game, az_mvs, az_acts);
        bool seen_act[AZ_NUM_ACTIONS];
        memset(seen_act, 0, sizeof(seen_act));
        for (int i = 0; i < az_n; i++) {
            int a = az_acts[i];
            if (a < 0 || a >= AZ_NUM_ACTIONS || seen_act[a]) {
                printf("[FAIL] Zero-C action encoding collision/OOB on FEN %d move %d (act=%d)\n",
                    f, i, a);
                ok = false;
                break;
            }
            seen_act[a] = true;
        }
        float planes[AZ_BOARD_FLOATS];
        az_encode_board(game->game, 0, planes);
        int piece_sum = 0;
        for (int p = 0; p < 12 * 64; p++) {
            piece_sum += (planes[p] > 0.5f) ? 1 : 0;
        }
        int expected_pieces
            = __builtin_popcountll(game->game->friendly_pieces | game->game->enemy_pieces);
        if (piece_sum != expected_pieces) {
            printf("[FAIL] Zero-C 18-plane encoding piece count mismatch (%d vs %d)\n", piece_sum,
                expected_pieces);
            ok = false;
        }
    }
    {
        // Mate-in-1 check: 6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1 -> Ra8# MUST be found by pure C MCTS!
        az_config azc;
        memset(&azc, 0, sizeof(azc));
        azc.num_games = 1;
        azc.fast_sims = 64;
        azc.full_sims = 64;
        azc.full_search_prob = 1.0f;
        azc.c_puct = 1.5f;
        azc.fpu_reduction = 0.25f;
        azc.dirichlet_eps = 0.0f;
        azc.max_plies = 200;
        azc.batch_per_game = 1;
        azc.seed = 42ull;
        az_arena *aza = az_arena_create(&azc);
        az_arena_reset_game(aza, 0, "6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1");
        float pl_buf[AZ_BOARD_FLOATS];
        int32_t lc_buf[1];
        int32_t la_buf[MAX_LEGAL_MOVES];
        float pr_buf[MAX_LEGAL_MOVES];
        float wdl_buf[3] = { 0.33f, 0.34f, 0.33f };
        float ml_buf[1] = { 0.4f };
        for (int step = 0; step < 80; step++) {
            int b = az_arena_collect_leaves(aza, 1, pl_buf, lc_buf, la_buf);
            if (b <= 0) {
                break;
            }
            for (int c = 0; c < lc_buf[0]; c++) {
                pr_buf[c] = 1.0f / (float) lc_buf[0];
            }
            az_arena_apply_evals(aza, 1, pr_buf, wdl_buf, ml_buf);
        }
        char best_u[8] = "";
        float out_wdl[3] = { 0 };
        float out_ml = 0.0f;
        az_arena_best_move(aza, 0, 0.0f, best_u, out_wdl, &out_ml);
        if (strcmp(best_u, "a1a8") != 0 || out_wdl[0] < 0.90f) {
            printf("[FAIL] Zero-C C MCTS failed mate-in-1 Ra8# (got %s, W=%.2f)\n", best_u,
                out_wdl[0]);
            ok = false;
        }

        // Mate-in-2 Exact Solver check: 2r3k1/5ppp/8/8/8/8/3Q1PPP/3R2K1 w - - 0 1 -> Qd8+! Rxd8 Rxd8#
        az_arena_reset_game(aza, 0, "2r3k1/5ppp/8/8/8/8/3Q1PPP/3R2K1 w - - 0 1");
        for (int step = 0; step < 8; step++) {
            int b = az_arena_collect_leaves(aza, 1, pl_buf, lc_buf, la_buf);
            if (b <= 0) {
                break;
            }
            for (int c = 0; c < lc_buf[0]; c++) {
                pr_buf[c] = 1.0f / (float) lc_buf[0];
            }
            az_arena_apply_evals(aza, 1, pr_buf, wdl_buf, ml_buf);
        }
        az_arena_best_move(aza, 0, 0.0f, best_u, out_wdl, &out_ml);
        if (strcmp(best_u, "d2d8") != 0 || out_wdl[0] < 0.95f) {
            printf("[FAIL] Zero-C Exact MCTS Solver failed mate-in-2 Qd8+! (got %s, W=%.2f)\n",
                best_u, out_wdl[0]);
            ok = false;
        }

        // 20-plane encoding check: plane 18 = repetition flag, plane 19 = White STM color
        float ctx_p[AZ_BOARD_FLOATS];
        set_from_fen(game, "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
        az_encode_board_ctx(game->game, 0, 1, ctx_p);
        if (ctx_p[18 * 64] < 0.9f || ctx_p[19 * 64] > 0.1f) {
            printf("[FAIL] Zero-C 20-plane encoding failed (rep=%.1f, stm=%.1f)\n",
                ctx_p[18 * 64], ctx_p[19 * 64]);
            ok = false;
        }
        az_arena_destroy(aza);

        // Multi-threaded C MCTS arena stress test (16 parallel games x 4 pthreads)
        az_config mtc;
        memset(&mtc, 0, sizeof(mtc));
        mtc.num_games = 16;
        mtc.fast_sims = 8;
        mtc.full_sims = 12;
        mtc.full_search_prob = 0.25f;
        mtc.c_puct = 1.75f;
        mtc.fpu_reduction = 0.25f;
        mtc.dirichlet_alpha = 0.30f;
        mtc.dirichlet_eps = 0.25f;
        mtc.temp_plies = 10;
        mtc.max_plies = 40;
        mtc.batch_per_game = 3;
        mtc.forced_playouts = 1;
        mtc.num_threads = 4;
        mtc.seed = 12345ull;
        az_arena *mta = az_arena_create(&mtc);
        if (!mta || az_arena_num_threads(mta) != 4) {
            printf("[FAIL] Zero-C multi-threaded arena init failed\n");
            ok = false;
        } else {
            int max_b = 48;
            float *mt_pl = (float *) calloc((size_t) max_b * AZ_BOARD_FLOATS, sizeof(float));
            int32_t *mt_lc = (int32_t *) calloc((size_t) max_b, sizeof(int32_t));
            int32_t *mt_la = (int32_t *) calloc((size_t) max_b * MAX_LEGAL_MOVES, sizeof(int32_t));
            float *mt_pr = (float *) calloc((size_t) max_b * MAX_LEGAL_MOVES, sizeof(float));
            float *mt_wdl = (float *) calloc((size_t) max_b * 3, sizeof(float));
            float *mt_ml = (float *) calloc((size_t) max_b, sizeof(float));
            int total_mt_leaves = 0;
            for (int tick = 0; tick < 12; tick++) {
                int b = az_arena_collect_leaves(mta, max_b, mt_pl, mt_lc, mt_la);
                total_mt_leaves += b;
                for (int i = 0; i < b; i++) {
                    int nl = mt_lc[i];
                    float inv = (nl > 0) ? (1.0f / (float) nl) : 0.0f;
                    for (int c = 0; c < nl; c++) {
                        mt_pr[(size_t) i * MAX_LEGAL_MOVES + c] = inv;
                    }
                    mt_wdl[i * 3 + 0] = 0.33f;
                    mt_wdl[i * 3 + 1] = 0.34f;
                    mt_wdl[i * 3 + 2] = 0.33f;
                    mt_ml[i] = 0.5f;
                }
                if (b > 0) {
                    az_arena_apply_evals(mta, b, mt_pr, mt_wdl, mt_ml);
                }
                int32_t ww = 0, dr = 0, bw = 0, pl = 0;
                az_arena_step_moves(mta, 1, &ww, &dr, &bw, &pl);
            }
            if (total_mt_leaves < 100) {
                printf("[FAIL] Zero-C multi-threaded MCTS collected too few leaves (%d)\n",
                    total_mt_leaves);
                ok = false;
            }
            free(mt_pl);
            free(mt_lc);
            free(mt_la);
            free(mt_pr);
            free(mt_wdl);
            free(mt_ml);
            az_arena_destroy(mta);
        }
    }

    engine_delete(&eng);

    if (ok) {
        printf("[PASS] Engine upgrades (Magics, tactical gen, %lu incremental Zobrist, SEE, Book, "
               "Mop-Up, Lazy SMP, KPK Bitbase, NNUE v3, Multi-PV, Review+Elo, %d Puzzles, History, "
               "Zero-C MCTS)\n",
            (unsigned long) zob_checked, npuz);
    }
    return ok;
}

static bool check_perft(full_chess *game, const perft_case *tc, u64 *total_nodes) {
    set_from_fen(game, tc->fen);
    clock_t begin = clock();
    u64 nodes = static_perft(game, tc->depth, false);
    clock_t end = clock();
    double elapsed = (double) (end - begin) / CLOCKS_PER_SEC;
    bool ok = (nodes == tc->expected);
    *total_nodes += nodes;
    printf("[%s] %-26s d=%d: %10lu (expected %10lu) [%.3fs]\n", ok ? "PASS" : "FAIL", tc->name,
        tc->depth, (unsigned long) nodes, (unsigned long) tc->expected, elapsed);
    return ok;
}

int main(int argc, char **argv) {
    bool quick = false;
    bool deep = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--quick") == 0) {
            quick = true;
        } else if (strcmp(argv[i], "--deep") == 0) {
            deep = true;
        }
    }

    full_chess *game = full_chess_create();
    bool all_ok = true;

    all_ok &= test_tree_invariants(game);
    all_ok &= test_fen_roundtrips(game);
    all_ok &= test_edge_cases(game);
    all_ok &= test_rules_and_unmake(game);
    all_ok &= test_san_notation(game);
    all_ok &= test_random_playouts(game);
    all_ok &= test_engine_upgrades(game);
    all_ok &= test_mirror_symmetry_perft(game, quick);

    // Verified Chess Programming Wiki & Martin Sedlak perft test suite
    const perft_case suite[] = {
        // 1. Standard starting position
        { "Startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1, 20 },
        { "Startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 2, 400 },
        { "Startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902 },
        { "Startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281 },
        { "Startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609 },
        // 2. Kiwipete (White & Mirrored Black)
        { "Kiwipete (White)",
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1, 48 },
        { "Kiwipete (White)",
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039 },
        { "Kiwipete (White)",
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862 },
        { "Kiwipete (White)",
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603 },
        { "Kiwipete (Mirrored B)",
            "r3k2r/pppbbppp/2n2q1P/1P2p3/3pn3/BN2PNP1/P1PPQPB1/R3K2R b KQkq - 0 1", 3, 97862 },
        { "Kiwipete (Mirrored B)",
            "r3k2r/pppbbppp/2n2q1P/1P2p3/3pn3/BN2PNP1/P1PPQPB1/R3K2R b KQkq - 0 1", 4, 4085603 },
        // 3. Position 3 — Endgame EP pins (White & Mirrored Black)
        { "Pos 3 EP Pins (White)", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 3, 2812 },
        { "Pos 3 EP Pins (White)", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238 },
        { "Pos 3 EP Pins (White)", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624 },
        { "Pos 3 EP Pins (Mirr B)", "8/4p1p1/8/1r3P1K/kp5R/3P4/2P5/8 b - - 0 1", 5, 674624 },
        // 4. Position 4 — Castling + Promotions + Checks (White & Mirrored Black)
        { "Pos 4 (White)", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3,
            9467 },
        { "Pos 4 (White)", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4,
            422333 },
        { "Pos 4 (Mirrored B)", "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
            4, 422333 },
        // 5. Position 5 — Promotions + Castling + Discovered Checks (White & Mirrored Black)
        { "Pos 5 (White)", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379 },
        { "Pos 5 (White)", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4,
            2103487 },
        { "Pos 5 (Mirrored B)", "rnbqk2r/ppp1nNpp/8/2b5/8/2P5/PP1pBPPP/RNBQ1K1R b kq - 1 8", 4,
            2103487 },
        // 6. Position 6 — Symmetrical Midgame
        { "Pos 6 (Midgame)",
            "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 3, 89890 },
        { "Pos 6 (Midgame)",
            "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4,
            3894594 },
        // 7. Martin Sedlak promotion chaos (Black & Mirrored White)
        { "Sedlak Promos (Black)", "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 1, 24 },
        { "Sedlak Promos (Black)", "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 2, 496 },
        { "Sedlak Promos (Black)", "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 3, 9483 },
        { "Sedlak Promos (Black)", "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 4, 182838 },
        { "Sedlak Promos (Black)", "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 5, 3605103 },
        { "Sedlak Promos (Mirr W)", "5n1n/4kPPP/8/8/8/8/pppK4/N1N5 w - - 0 1", 5, 3605103 },
    };

    const perft_case deep_suite[] = {
        { "Pos 3 EP Pins (DEEP)", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 6, 11030083 },
        { "Pos 4 (DEEP)", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5,
            15833292 },
        { "Startpos (DEEP)", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6,
            119060324 },
    };

    u64 total_nodes = 0ull;
    int suite_len = (int) (sizeof(suite) / sizeof(suite[0]));
    for (int i = 0; i < suite_len; i++) {
        if (quick && suite[i].expected > 100000) {
            continue;
        }
        all_ok &= check_perft(game, &suite[i], &total_nodes);
    }

    if (deep) {
        int deep_len = (int) (sizeof(deep_suite) / sizeof(deep_suite[0]));
        for (int i = 0; i < deep_len; i++) {
            all_ok &= check_perft(game, &deep_suite[i], &total_nodes);
        }
    }

    full_chess_delete(&game);
    printf("\nVerified %lu perft nodes total. Overall: %s\n", (unsigned long) total_nodes,
        all_ok ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return all_ok ? 0 : 1;
}
