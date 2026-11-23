#define _POSIX_C_SOURCE 200809L

#include "core/attacks.h"
#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/notation.h"
#include "core/primitives.h"
#include "engine/book.h"
#include "engine/engine.h"
#include "engine/nnue.h"

#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct uci_state {
    engine *eng;
    full_chess *game;
    size_t hash_mb;
    int threads;
    int multipv;
    bool own_book;
    char book_path[256];
    u64 rng_state;

    pthread_t search_thread;
    bool searching;
    int go_depth;
    int go_soft_ms;
    int go_time_ms;
    bool go_infinite;
    bool limit_strength;
    int uci_elo;
} uci_state;

static void move_to_uci(const chess *board, u64 m, char out[8]) {
    if (m == NULL_MOVE) {
        snprintf(out, 8, "0000");
        return;
    }
    bool flip = is_flipped(board);
    int s = get_start_offset(m);
    int e = get_end_offset(m);
    if (flip) {
        s ^= 56;
        e ^= 56;
    }
    out[0] = (char) ('a' + (s & 7));
    out[1] = (char) ('1' + (s >> 3));
    out[2] = (char) ('a' + (e & 7));
    out[3] = (char) ('1' + (e >> 3));
    int pos = 4;
    u8 sp = get_start_piece(m);
    u8 pr = get_promotion_piece(m);
    if (pr != sp) {
        out[pos++] = (char) tolower((unsigned char) piece_to_char(pr));
    }
    out[pos] = '\0';
}

static void uci_info_callback(int depth, int score_cp, u64 nodes, double elapsed_sec, const u64 *pv,
    int pv_len, void *user_data) {
    const full_chess *game = (const full_chess *) user_data;
    long time_ms = (long) (elapsed_sec * 1000.0);
    if (time_ms < 1) {
        time_ms = 1;
    }
    u64 nps = (elapsed_sec > 1e-6) ? (u64) ((double) nodes / elapsed_sec) : nodes;

    printf("info depth %d ", depth);
    if (score_cp > MATE_BOUND) {
        int plies = MATE_SCORE - score_cp;
        int mate_in = (plies + 1) / 2;
        if (mate_in < 1) {
            mate_in = 1;
        }
        printf("score mate %d ", mate_in);
    } else if (score_cp < -MATE_BOUND) {
        int plies = MATE_SCORE + score_cp;
        int mate_in = (plies + 1) / 2;
        if (mate_in < 1) {
            mate_in = 1;
        }
        printf("score mate -%d ", mate_in);
    } else {
        printf("score cp %d ", score_cp);
    }
    printf("nodes %lu nps %lu time %ld", (unsigned long) nodes, (unsigned long) nps, time_ms);

    if (pv && pv_len > 0) {
        printf(" pv");
        chess b = *(game->game);
        for (int i = 0; i < pv_len; i++) {
            char mv[8];
            move_to_uci(&b, pv[i], mv);
            printf(" %s", mv);
            chess_make_move(&b, pv[i]);
        }
    }
    printf("\n");
    fflush(stdout);
}

static void uci_multipv_callback(int depth, int multipv_idx, int score_cp, u64 nodes,
    double elapsed_sec, const u64 *pv, int pv_len, void *user_data) {
    const full_chess *game = (const full_chess *) user_data;
    long time_ms = (long) (elapsed_sec * 1000.0);
    if (time_ms < 1) {
        time_ms = 1;
    }
    u64 nps = (elapsed_sec > 1e-6) ? (u64) ((double) nodes / elapsed_sec) : nodes;

    printf("info depth %d multipv %d ", depth, multipv_idx);
    if (score_cp > MATE_BOUND) {
        int plies = MATE_SCORE - score_cp;
        int mate_in = (plies + 1) / 2;
        if (mate_in < 1) {
            mate_in = 1;
        }
        printf("score mate %d ", mate_in);
    } else if (score_cp < -MATE_BOUND) {
        int plies = MATE_SCORE + score_cp;
        int mate_in = (plies + 1) / 2;
        if (mate_in < 1) {
            mate_in = 1;
        }
        printf("score mate -%d ", mate_in);
    } else {
        printf("score cp %d ", score_cp);
    }
    printf("nodes %lu nps %lu time %ld", (unsigned long) nodes, (unsigned long) nps, time_ms);

    if (pv && pv_len > 0) {
        printf(" pv");
        chess b = *(game->game);
        for (int i = 0; i < pv_len; i++) {
            char mv[8];
            move_to_uci(&b, pv[i], mv);
            printf(" %s", mv);
            chess_make_move(&b, pv[i]);
        }
    }
    printf("\n");
    fflush(stdout);
}

static void *search_thread_fn(void *arg) {
    uci_state *st = (uci_state *) arg;

    if (st->own_book && !st->go_infinite && st->multipv <= 1) {
        const char *bpath = st->book_path[0] ? st->book_path : NULL;
        u64 bm = book_probe(st->game, bpath, &st->rng_state);
        if (bm != NULL_MOVE) {
            char mv[8];
            move_to_uci(st->game->game, bm, mv);
            printf("info string opening book hit\n");
            printf("bestmove %s\n", mv);
            fflush(stdout);
            return NULL;
        }
    }

    u64 best_mv = NULL_MOVE;
    if (st->multipv > 1) {
        multipv_line lines[MAX_MULTIPV];
        engine_set_multipv_callback(st->eng, uci_multipv_callback, st->game);
        int n = engine_search_multipv(
            st->eng, st->game, st->go_depth, st->go_time_ms, st->multipv, lines, NULL, NULL);
        engine_set_multipv_callback(st->eng, NULL, NULL);
        if (n > 0) {
            best_mv = lines[0].best_move;
        }
    } else if (st->limit_strength && !st->go_infinite) {
        search_result sr = engine_search_elo(st->eng, st->game, st->uci_elo);
        best_mv = sr.best_move;
    } else {
        engine_set_info_callback(st->eng, uci_info_callback, st->game);
        search_result sr
            = engine_search_timed(st->eng, st->game, st->go_depth, st->go_soft_ms, st->go_time_ms);
        engine_set_info_callback(st->eng, NULL, NULL);
        best_mv = sr.best_move;
    }

    char best_str[8];
    move_to_uci(st->game->game, best_mv, best_str);
    printf("bestmove %s\n", best_str);
    fflush(stdout);
    return NULL;
}

static void stop_search_if_running(uci_state *st) {
    if (st->searching) {
        engine_request_stop(st->eng);
        pthread_join(st->search_thread, NULL);
        st->searching = false;
    }
}

static void handle_position(uci_state *st, const char *line) {
    stop_search_if_running(st);
    const char *p = line + 8; // skip "position"
    while (*p && isspace((unsigned char) *p)) {
        p++;
    }

    const char *moves_kw = strstr(p, " moves");
    if (strncmp(p, "startpos", 8) == 0) {
        full_chess_initialize(st->game);
        st->game->is_static = true;
    } else if (strncmp(p, "fen", 3) == 0) {
        p += 3;
        while (*p && isspace((unsigned char) *p)) {
            p++;
        }
        char fen[MAX_FEN_LENGTH];
        size_t len = moves_kw ? (size_t) (moves_kw - p) : strlen(p);
        if (len >= sizeof(fen)) {
            len = sizeof(fen) - 1;
        }
        memcpy(fen, p, len);
        fen[len] = '\0';
        while (len > 0 && isspace((unsigned char) fen[len - 1])) {
            fen[--len] = '\0';
        }
        set_from_fen(st->game, fen);
        st->game->is_static = true;
    }

    if (moves_kw) {
        const char *mptr = moves_kw + 6;
        char tok[16];
        while (*mptr) {
            while (*mptr && isspace((unsigned char) *mptr)) {
                mptr++;
            }
            if (!*mptr) {
                break;
            }
            size_t ti = 0;
            while (*mptr && !isspace((unsigned char) *mptr) && ti + 1 < sizeof(tok)) {
                tok[ti++] = *mptr++;
            }
            tok[ti] = '\0';
            u64 m = static_encode_string(st->game, tok);
            if (m != NULL_MOVE) {
                make_move(st->game, m);
            }
        }
    }
}

static void handle_setoption(uci_state *st, const char *line) {
    stop_search_if_running(st);
    const char *name_p = strstr(line, "name ");
    if (!name_p) {
        return;
    }
    name_p += 5;
    const char *val_p = strstr(name_p, " value ");
    char name[64];
    char val[256];
    val[0] = '\0';

    if (val_p) {
        size_t nlen = (size_t) (val_p - name_p);
        if (nlen >= sizeof(name)) {
            nlen = sizeof(name) - 1;
        }
        memcpy(name, name_p, nlen);
        name[nlen] = '\0';
        snprintf(val, sizeof(val), "%s", val_p + 7);
    } else {
        snprintf(name, sizeof(name), "%s", name_p);
    }

    // Trim trailing whitespace
    for (size_t l = strlen(name); l > 0 && isspace((unsigned char) name[l - 1]); l--) {
        name[l - 1] = '\0';
    }
    for (size_t l = strlen(val); l > 0 && isspace((unsigned char) val[l - 1]); l--) {
        val[l - 1] = '\0';
    }

    if (strcmp(name, "Hash") == 0) {
        long mb = strtol(val, NULL, 10);
        if (mb < 1) {
            mb = 1;
        }
        if (mb > 256) {
            mb = 256;
        }
        st->hash_mb = (size_t) mb;
        engine_delete(&st->eng);
        st->eng = engine_create(st->hash_mb);
        engine_set_threads(st->eng, st->threads);
        engine_set_use_book(st->eng, st->own_book);
        engine_set_book_path(st->eng, st->book_path);
    } else if (strcmp(name, "Threads") == 0) {
        long th = strtol(val, NULL, 10);
        if (th < 1) {
            th = 1;
        }
        if (th > 16) {
            th = 16;
        }
        st->threads = (int) th;
        engine_set_threads(st->eng, st->threads);
    } else if (strcmp(name, "MultiPV") == 0) {
        long mpv = strtol(val, NULL, 10);
        if (mpv < 1) {
            mpv = 1;
        }
        if (mpv > MAX_MULTIPV) {
            mpv = MAX_MULTIPV;
        }
        st->multipv = (int) mpv;
    } else if (strcmp(name, "OwnBook") == 0) {
        st->own_book = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        engine_set_use_book(st->eng, st->own_book);
    } else if (strcmp(name, "BookFile") == 0) {
        snprintf(st->book_path, sizeof(st->book_path), "%s", val);
        engine_set_book_path(st->eng, st->book_path);
    } else if (strcmp(name, "UseNNUE") == 0) {
        bool on = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        nnue_set_mode(on ? NNUE_PURE : NNUE_OFF);
    } else if (strcmp(name, "NNUEMode") == 0) {
        if (strcmp(val, "off") == 0 || strcmp(val, "0") == 0) {
            nnue_set_mode(NNUE_OFF);
        } else if (strcmp(val, "hybrid") == 0 || strcmp(val, "1") == 0) {
            nnue_set_mode(NNUE_HYBRID);
        } else if (strcmp(val, "pure") == 0 || strcmp(val, "2") == 0) {
            nnue_set_mode(NNUE_PURE);
        }
    } else if (strcmp(name, "EvalFile") == 0) {
        if (nnue_load_file(val)) {
            printf("info string loaded NNUE %s\n", val);
        } else {
            printf("info string failed to load NNUE %s\n", val);
        }
    } else if (strcmp(name, "Clear Hash") == 0) {
        engine_clear_tt(st->eng);
    } else if (strcmp(name, "UCI_LimitStrength") == 0) {
        st->limit_strength = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        engine_set_limit_strength(st->eng, st->limit_strength, st->uci_elo);
    } else if (strcmp(name, "UCI_Elo") == 0) {
        int e = atoi(val);
        if (e < 800) {
            e = 800;
        } else if (e > 2600) {
            e = 2600;
        }
        st->uci_elo = e;
        engine_set_limit_strength(st->eng, st->limit_strength, st->uci_elo);
    } else {
        (void) engine_set_param(name, atoi(val));
    }
}

static void handle_go(uci_state *st, const char *line) {
    stop_search_if_running(st);

    int wtime = -1, btime = -1, winc = 0, binc = 0, movestogo = 0;
    int movetime = -1, depth = -1;
    bool infinite = false;

    char copy[1024];
    snprintf(copy, sizeof(copy), "%s", line);
    char *save = NULL;
    char *tok = strtok_r(copy, " \t\r\n", &save);
    while ((tok = strtok_r(NULL, " \t\r\n", &save)) != NULL) {
        if (strcmp(tok, "wtime") == 0) {
            char *v = strtok_r(NULL, " \t\r\n", &save);
            if (v) {
                wtime = atoi(v);
            }
        } else if (strcmp(tok, "btime") == 0) {
            char *v = strtok_r(NULL, " \t\r\n", &save);
            if (v) {
                btime = atoi(v);
            }
        } else if (strcmp(tok, "winc") == 0) {
            char *v = strtok_r(NULL, " \t\r\n", &save);
            if (v) {
                winc = atoi(v);
            }
        } else if (strcmp(tok, "binc") == 0) {
            char *v = strtok_r(NULL, " \t\r\n", &save);
            if (v) {
                binc = atoi(v);
            }
        } else if (strcmp(tok, "movestogo") == 0) {
            char *v = strtok_r(NULL, " \t\r\n", &save);
            if (v) {
                movestogo = atoi(v);
            }
        } else if (strcmp(tok, "movetime") == 0) {
            char *v = strtok_r(NULL, " \t\r\n", &save);
            if (v) {
                movetime = atoi(v);
            }
        } else if (strcmp(tok, "depth") == 0) {
            char *v = strtok_r(NULL, " \t\r\n", &save);
            if (v) {
                depth = atoi(v);
            }
        } else if (strcmp(tok, "infinite") == 0) {
            infinite = true;
        }
    }

    int use_depth = (depth > 0) ? depth : 60;
    int use_soft_ms = 0;
    int use_time_ms = 0;

    if (infinite) {
        use_depth = 60;
        use_soft_ms = 0;
        use_time_ms = 0;
    } else if (movetime > 0) {
        use_soft_ms = movetime;
        use_time_ms = movetime;
    } else if (wtime >= 0 || btime >= 0) {
        bool black_to_move = is_flipped(st->game->game);
        int my_time = black_to_move ? btime : wtime;
        int my_inc = black_to_move ? binc : winc;
        if (my_time < 0) {
            my_time = 1000;
        }
        int divisor = (movestogo > 0) ? (movestogo + 2) : 28;
        int soft_budget = (my_time / divisor) + (my_inc * 3 / 4);
        int hard_budget = soft_budget * 2;
        int max_safe = my_time / 3;
        if (hard_budget > max_safe && max_safe > 50) {
            hard_budget = max_safe;
        }
        if (soft_budget > hard_budget) {
            soft_budget = hard_budget;
        }
        if (soft_budget < 20) {
            soft_budget = 20;
        }
        if (hard_budget < soft_budget) {
            hard_budget = soft_budget;
        }
        use_soft_ms = soft_budget;
        use_time_ms = hard_budget;
    } else if (depth <= 0) {
        use_soft_ms = 1500;
        use_time_ms = 1500;
    }

    st->go_depth = use_depth;
    st->go_soft_ms = use_soft_ms;
    st->go_time_ms = use_time_ms;
    st->go_infinite = infinite;
    st->searching = (pthread_create(&st->search_thread, NULL, search_thread_fn, st) == 0);
}

int main(void) {
    attacks_init();
    setvbuf(stdout, NULL, _IONBF, 0);

    uci_state st;
    memset(&st, 0, sizeof(st));
    st.hash_mb = 16;
    st.threads = 1;
    st.multipv = 1;
    st.own_book = true;
    st.limit_strength = false;
    st.uci_elo = 1800;
    st.rng_state = (u64) time(NULL) ^ 0x517cc1b727220a95ull;
    st.eng = engine_create(st.hash_mb);
    st.game = full_chess_create();
    st.game->is_static = true;

    char line[4096];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) {
            continue;
        }

        if (strcmp(line, "uci") == 0) {
            printf("id name Titan-C v4 (Chess-in-C)\n");
            printf("id author Chess-in-C\n");
            printf("option name Hash type spin default 16 min 1 max 256\n");
            printf("option name Threads type spin default 1 min 1 max 16\n");
            printf("option name MultiPV type spin default 1 min 1 max %d\n", MAX_MULTIPV);
            printf("option name OwnBook type check default true\n");
            printf("option name BookFile type string default \n");
            printf("option name UCI_LimitStrength type check default false\n");
            printf("option name UCI_Elo type spin default 1800 min 800 max 2600\n");
            printf("option name UseNNUE type check default %s\n",
                (nnue_get_mode() != NNUE_OFF) ? "true" : "false");
            printf(
                "option name NNUEMode type spin default %d min 0 max 2\n", (int) nnue_get_mode());
            printf("option name EvalFile type string default %s\n", NNUE_FILE);
            printf("option name Clear Hash type button\n");
            printf("uciok\n");
        } else if (strcmp(line, "isready") == 0) {
            printf("readyok\n");
        } else if (strcmp(line, "ucinewgame") == 0) {
            stop_search_if_running(&st);
            engine_clear_tt(st.eng);
            full_chess_initialize(st.game);
            st.game->is_static = true;
        } else if (strncmp(line, "setoption ", 10) == 0) {
            handle_setoption(&st, line);
        } else if (strncmp(line, "position", 8) == 0) {
            handle_position(&st, line);
        } else if (strncmp(line, "go", 2) == 0
                   && (line[2] == '\0' || isspace((unsigned char) line[2]))) {
            handle_go(&st, line);
        } else if (strcmp(line, "stop") == 0) {
            stop_search_if_running(&st);
        } else if (strcmp(line, "quit") == 0) {
            stop_search_if_running(&st);
            break;
        } else if (strcmp(line, "d") == 0) {
            stop_search_if_running(&st);
            char fen[MAX_FEN_LENGTH];
            get_fen(st.game, fen);
            static_print_board(st.game);
            printf("FEN: %s\n", fen);
            printf("Key: %016lx\n", (unsigned long) engine_hash_board(st.game->game));
        } else if (strncmp(line, "perft ", 6) == 0) {
            stop_search_if_running(&st);
            int d = atoi(line + 6);
            if (d < 1) {
                d = 1;
            }
            struct timespec t0, t1;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            u64 nodes = static_perft(st.game, d, true);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            double sec
                = (double) (t1.tv_sec - t0.tv_sec) + (double) (t1.tv_nsec - t0.tv_nsec) * 1e-9;
            printf("Nodes: %lu (%.3fs, %.2f MNPS)\n", (unsigned long) nodes, sec,
                sec > 1e-6 ? ((double) nodes / sec / 1e6) : 0.0);
        }
    }

    stop_search_if_running(&st);
    engine_delete(&st.eng);
    full_chess_delete(&st.game);
    return 0;
}
