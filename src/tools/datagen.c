#define _POSIX_C_SOURCE 200809L

#include "core/attacks.h"
#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/primitives.h"
#include "engine/engine.h"
#include "engine/nnue.h"

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Compact 36-byte position record (zero-copy np.memmap compatible in train_big_nnue.py):
//   board_packed[32]: 64 squares packed 2 per byte (lo nibble = even sq, hi nibble = odd sq),
//                     in SIDE-TO-MOVE perspective:
//                       0 = empty
//                       1..6  = STM  [P, N, B, R, Q, K]
//                       7..12 = OPP  [P, N, B, R, Q, K]
//   score_cp:         int16 search score from STM perspective (clamped to [-3000, 3000])
//   result:           uint8 game result from STM perspective (0=Loss, 1=Draw, 2=Win)
//   ply:              uint8 game ply
typedef struct __attribute__((packed)) datagen_rec {
    uint8_t board_packed[32];
    int16_t score_cp;
    uint8_t result;
    uint8_t ply;
} datagen_rec;

_Static_assert(sizeof(datagen_rec) == 36, "datagen_rec must be exactly 36 bytes");

static volatile sig_atomic_t g_stop = 0;

static void handle_sig(int sig) {
    (void) sig;
    g_stop = 1;
}

static double wall_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec * 1e-9;
}

static void write_datagen_status(const char *status_path, bool running, uint64_t existing,
    uint64_t session_pos, uint64_t target_pos, uint64_t games, int threads, u64 nodes, double dt,
    bool force_hist) {
    if (!status_path || status_path[0] == '\0') {
        return;
    }
    char tmp[320];
    snprintf(tmp, sizeof(tmp), "%s.tmp", status_path);
    FILE *sf = fopen(tmp, "wb");
    if (!sf) {
        return;
    }
    double pps = (dt > 0.1) ? ((double) session_pos / dt) : 0.0;
    uint64_t file_pos = existing + session_pos;
    uint64_t goal_total = existing + target_pos;
    double pct = (target_pos > 0) ? (100.0 * (double) session_pos / (double) target_pos) : 100.0;
    if (pct > 100.0) {
        pct = 100.0;
    }
    fprintf(sf,
        "{\"ok\":true,\"running\":%s,\"file_positions\":%llu,\"session_positions\":%llu,"
        "\"target_positions\":%llu,\"goal_total\":%llu,\"games\":%llu,\"pos_per_sec\":%.1f,"
        "\"threads\":%d,\"nodes\":%llu,\"progress_pct\":%.1f,\"elapsed_sec\":%.1f}\n",
        running ? "true" : "false", (unsigned long long) file_pos, (unsigned long long) session_pos,
        (unsigned long long) target_pos, (unsigned long long) goal_total,
        (unsigned long long) games, pps, threads, (unsigned long long) nodes, pct, dt);
    fclose(sf);
    rename(tmp, status_path);

    // Record live throughput telemetry into p3k_datagen_history.json every ~12s (plus final point)
    typedef struct {
        double elapsed_sec;
        double pos_per_sec;
        uint64_t file_positions;
        uint64_t games;
    } dg_pt;
    static dg_pt hist[240];
    static int hist_n = 0;
    static double last_hist_dt = -99.0;
    if (session_pos > 0 && (force_hist || (dt - last_hist_dt) >= 12.0)) {
        last_hist_dt = dt;
        if (hist_n >= 240) {
            memmove(&hist[0], &hist[1], sizeof(dg_pt) * 239);
            hist_n = 239;
        }
        hist[hist_n++] = (dg_pt) { dt, pps, file_pos, games };
        char hpath[320];
        const char *suf = strstr(status_path, "_status.json");
        if (suf) {
            size_t pref = (size_t) (suf - status_path);
            if (pref + 16 < sizeof(hpath)) {
                memcpy(hpath, status_path, pref);
                snprintf(hpath + pref, sizeof(hpath) - pref, "_history.json");
            } else {
                snprintf(hpath, sizeof(hpath), "data/p3k_datagen_history.json");
            }
        } else {
            snprintf(hpath, sizeof(hpath), "data/p3k_datagen_history.json");
        }
        char htmp[336];
        snprintf(htmp, sizeof(htmp), "%s.tmp", hpath);
        FILE *hf = fopen(htmp, "wb");
        if (hf) {
            fprintf(hf, "{\"points\":[");
            for (int i = 0; i < hist_n; i++) {
                fprintf(hf,
                    "%s{\"elapsed_sec\":%.1f,\"pos_per_sec\":%.0f,\"file_positions\":%llu,\"games\":%llu}",
                    i > 0 ? "," : "", hist[i].elapsed_sec, hist[i].pos_per_sec,
                    (unsigned long long) hist[i].file_positions, (unsigned long long) hist[i].games);
            }
            fprintf(hf, "]}\n");
            fclose(hf);
            rename(htmp, hpath);
        }
    }
}

static inline uint64_t xorshift64(uint64_t *s) {
    uint64_t x = *s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *s = x;
    return x;
}

static void pack_board_stm(const chess *b, uint8_t out[32]) {
    memset(out, 0, 32);
    u64 f = b->friendly_pieces;
    u64 e = b->enemy_pieces;
    u64 tp = b->pawns & PAWN_MASK;
    u64 orth = b->orthogonal_pieces;
    u64 diag = b->diagonal_pieces;
    u64 kings = b->kings;

    u64 bbs[12] = {
        f & tp,
        f & ~(tp | orth | diag | kings),
        f & diag & ~orth,
        f & orth & ~diag,
        f & diag & orth,
        f & kings,
        e & tp,
        e & ~(tp | orth | diag | kings),
        e & diag & ~orth,
        e & orth & ~diag,
        e & diag & orth,
        e & kings,
    };

    for (int code = 1; code <= 12; code++) {
        u64 bb = bbs[code - 1];
        while (bb) {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1ull;
            if ((sq & 1) == 0) {
                out[sq >> 1] |= (uint8_t) (code & 0x0F);
            } else {
                out[sq >> 1] |= (uint8_t) ((code & 0x0F) << 4);
            }
        }
    }
}

typedef struct pending_pos {
    uint8_t board_packed[32];
    int16_t score_cp;
    uint8_t side_is_black;
    uint8_t ply;
} pending_pos;

typedef struct worker_cfg {
    int id;
    u64 nodes_per_move;
    u64 target_positions;
    int random_plies;
    uint64_t seed;
    FILE *out_fp;
    pthread_mutex_t *io_mu;
    _Atomic uint64_t *total_pos;
    _Atomic uint64_t *total_games;
} worker_cfg;

static void *datagen_worker(void *arg) {
    worker_cfg *cfg = (worker_cfg *) arg;
    engine *eng = engine_create(16);
    engine_set_threads(eng, 1);
    engine_set_use_book(eng, false);
    full_chess *game = full_chess_create();
    game->is_static = true;

    pending_pos buf[512];
    datagen_rec out_batch[512];
    uint64_t rng = cfg->seed;

    while (!g_stop) {
        if (atomic_load_explicit(cfg->total_pos, memory_order_relaxed) >= cfg->target_positions) {
            break;
        }

        full_chess_initialize(game);
        game->is_static = true;
        engine_clear_tt(eng);

        // Random opening plies for huge positional diversity
        bool aborted_opening = false;
        for (int p = 0; p < cfg->random_plies; p++) {
            int n = num_legal_moves(game);
            if (n <= 0 || game->is_game_over) {
                aborted_opening = true;
                break;
            }
            u64 m = game->current_legal_moves[xorshift64(&rng) % (uint64_t) n];
            make_move(game, m);
        }
        if (aborted_opening || game->is_game_over) {
            continue;
        }

        // Skip wildly lopsided random openings
        search_result probe = engine_search_nodes(eng, game, 12, cfg->nodes_per_move / 2);
        if (abs(probe.score_cp) > 650) {
            continue;
        }

        int n_buf = 0;
        int win_streak = 0;
        int draw_streak = 0;
        int white_res = 1; // 0=Black win, 1=Draw, 2=White win

        while (!game->is_game_over && game->move_count < 260 && !g_stop) {
            bool btm = is_flipped(game->game);
            bool in_chk = in_check(game->game);

            search_result sr = engine_search_nodes(eng, game, 24, cfg->nodes_per_move);
            if (sr.best_move == NULL_MOVE) {
                break;
            }

            bool is_cap = (get_end_piece(sr.best_move) != NONE) || is_en_passant_move(sr.best_move);
            bool is_promo = (get_promotion_piece(sr.best_move) != get_start_piece(sr.best_move));

            // Record clean quiet non-check positions with non-mate scores
            if (!in_chk && !is_cap && !is_promo && abs(sr.score_cp) < 3000 && n_buf < 512) {
                pack_board_stm(game->game, buf[n_buf].board_packed);
                buf[n_buf].score_cp = (int16_t) sr.score_cp;
                buf[n_buf].side_is_black = btm ? 1u : 0u;
                buf[n_buf].ply = (uint8_t) (game->move_count < 255 ? game->move_count : 255);
                n_buf++;
            }

            int white_sc = btm ? -sr.score_cp : sr.score_cp;
            if (abs(white_sc) >= 1800) {
                win_streak++;
                draw_streak = 0;
                if (win_streak >= 4) {
                    white_res = (white_sc > 0) ? 2 : 0;
                    break;
                }
            } else if (game->move_count >= 56 && abs(white_sc) <= 12) {
                draw_streak++;
                win_streak = 0;
                if (draw_streak >= 8) {
                    white_res = 1;
                    break;
                }
            } else {
                win_streak = 0;
                draw_streak = 0;
            }

            make_move(game, sr.best_move);
        }

        if (game->is_game_over) {
            if (game->is_draw) {
                white_res = 1;
            } else {
                // Side to move is checkmated -> opponent won
                white_res = is_flipped(game->game) ? 2 : 0;
            }
        }

        if (n_buf > 0) {
            for (int i = 0; i < n_buf; i++) {
                memcpy(out_batch[i].board_packed, buf[i].board_packed, 32);
                out_batch[i].score_cp = buf[i].score_cp;
                out_batch[i].ply = buf[i].ply;
                out_batch[i].result
                    = (uint8_t) (buf[i].side_is_black ? (2 - white_res) : white_res);
            }
            pthread_mutex_lock(cfg->io_mu);
            fwrite(out_batch, sizeof(datagen_rec), (size_t) n_buf, cfg->out_fp);
            pthread_mutex_unlock(cfg->io_mu);
            atomic_fetch_add_explicit(cfg->total_pos, (uint64_t) n_buf, memory_order_relaxed);
        }
        atomic_fetch_add_explicit(cfg->total_games, 1ull, memory_order_relaxed);
    }

    full_chess_delete(&game);
    engine_delete(&eng);
    return NULL;
}

int main(int argc, char **argv) {
    const char *out_path = "data/datagen_v4.bin";
    const char *status_path = "data/p3k_datagen_status.json";
    const char *nnue_path = NULL;
    int threads = 4;
    u64 nodes = 5000;
    u64 target_pos = 10000000ull; // 10M default (pass --positions 50000000 for 50M!)
    int rand_plies = 8;
    bool append_mode = true;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(argv[i], "--status") == 0 && i + 1 < argc) {
            status_path = argv[++i];
        } else if (strcmp(argv[i], "--nnue") == 0 && i + 1 < argc) {
            nnue_path = argv[++i];
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--nodes") == 0 && i + 1 < argc) {
            nodes = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--positions") == 0 && i + 1 < argc) {
            target_pos = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--rand-plies") == 0 && i + 1 < argc) {
            rand_plies = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--overwrite") == 0) {
            append_mode = false;
        }
    }
    if (threads < 1) {
        threads = 1;
    }
    if (threads > 16) {
        threads = 16;
    }

    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    attacks_init();
    nnue_init();
    if (nnue_path && nnue_path[0] != '\0') {
        if (nnue_load_file(nnue_path)) {
            printf("[datagen] Loaded NNUE: %s (v%u, hidden=%d)\n", nnue_path,
                nnue_get_loaded_version(), nnue_get_loaded_hidden());
        } else {
            fprintf(stderr, "[datagen] Warning: could not load %s, using default NNUE.\n",
                nnue_path);
        }
    }

    FILE *fp = fopen(out_path, append_mode ? "ab" : "wb");
    if (!fp) {
        perror("fopen");
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    uint64_t existing = (uint64_t) ftell(fp) / sizeof(datagen_rec);

    printf("[datagen] Output: %s (%llu existing pos) | threads=%d | nodes=%llu | target=+%llu pos\n",
        out_path, (unsigned long long) existing, threads, (unsigned long long) nodes,
        (unsigned long long) target_pos);
    fflush(stdout);

    pthread_mutex_t io_mu = PTHREAD_MUTEX_INITIALIZER;
    _Atomic uint64_t total_pos = 0;
    _Atomic uint64_t total_games = 0;
    pthread_t tids[16];
    worker_cfg cfgs[16];
    uint64_t base_seed = ((uint64_t) time(NULL) << 20) ^ 0x9e3779b97f4a7c15ull;

    double t0 = wall_sec();
    write_datagen_status(
        status_path, true, existing, 0, target_pos, 0, threads, nodes, 0.0, false);
    for (int t = 0; t < threads; t++) {
        cfgs[t].id = t;
        cfgs[t].nodes_per_move = nodes;
        cfgs[t].target_positions = target_pos;
        cfgs[t].random_plies = rand_plies;
        cfgs[t].seed = base_seed + (uint64_t) (t + 1) * 0xbf58476d1ce4e5b9ull;
        cfgs[t].out_fp = fp;
        cfgs[t].io_mu = &io_mu;
        cfgs[t].total_pos = &total_pos;
        cfgs[t].total_games = &total_games;
        pthread_create(&tids[t], NULL, datagen_worker, &cfgs[t]);
    }

    while (!g_stop) {
        struct timespec ts = { 1, 500000000L };
        nanosleep(&ts, NULL);
        uint64_t p = atomic_load_explicit(&total_pos, memory_order_relaxed);
        uint64_t g = atomic_load_explicit(&total_games, memory_order_relaxed);
        double dt = wall_sec() - t0;
        if (dt < 0.1) {
            dt = 0.1;
        }
        printf("[datagen] +%llu / %llu pos (%.0f pos/s) | %llu games | total file: %llu pos\n",
            (unsigned long long) p, (unsigned long long) target_pos, (double) p / dt,
            (unsigned long long) g, (unsigned long long) (existing + p));
        fflush(stdout);
        pthread_mutex_lock(&io_mu);
        fflush(fp);
        pthread_mutex_unlock(&io_mu);
        write_datagen_status(
            status_path, true, existing, p, target_pos, g, threads, nodes, dt, false);
        if (p >= target_pos) {
            break;
        }
    }

    g_stop = 1;
    for (int t = 0; t < threads; t++) {
        pthread_join(tids[t], NULL);
    }
    fflush(fp);
    fclose(fp);
    pthread_mutex_destroy(&io_mu);

    uint64_t final_p = atomic_load(&total_pos);
    uint64_t final_g = atomic_load(&total_games);
    write_datagen_status(status_path, false, existing, final_p, target_pos, final_g, threads,
        nodes, wall_sec() - t0, true);
    printf("[datagen] Finished cleanly! Wrote +%llu positions (file total: %llu) in %.1fs.\n",
        (unsigned long long) final_p, (unsigned long long) (existing + final_p), wall_sec() - t0);
    return 0;
}
