#define _POSIX_C_SOURCE 200809L

#include "core/attacks.h"
#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/notation.h"
#include "core/primitives.h"
#include "engine/engine.h"
#include "engine/eval.h"

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct sample {
    chess board; // 48B position (side-to-move perspective)
    int16_t search_cp; // White-perspective deep search score
    float result; // White-perspective game outcome: 1.0, 0.5, 0.0
} sample;

typedef struct gen_ctx {
    sample *buf;
    size_t cap;
    size_t count;
    int games_target;
    int games_done;
    int search_depth;
    pthread_mutex_t mu;
} gen_ctx;

static u64 rng_next(u64 *s) {
    u64 z = (*s += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static void *datagen_worker(void *arg) {
    gen_ctx *ctx = (gen_ctx *) arg;
    engine *eng = engine_create(4);
    engine_set_use_book(eng, false);
    full_chess *game = full_chess_create();
    u64 seed = (u64) time(NULL) ^ (u64) (uintptr_t) pthread_self() ^ 0x9e3779b97f4a7c15ull;

    sample local[512];

    for (;;) {
        pthread_mutex_lock(&ctx->mu);
        if (ctx->games_done >= ctx->games_target || ctx->count >= ctx->cap) {
            pthread_mutex_unlock(&ctx->mu);
            break;
        }
        int gnum = ++ctx->games_done;
        if ((gnum % 2000) == 0) {
            printf("[DATAGEN] %d / %d games done (%zu positions)\n", gnum, ctx->games_target,
                ctx->count);
            fflush(stdout);
        }
        pthread_mutex_unlock(&ctx->mu);

        full_chess_initialize(game);
        game->is_static = true;
        engine_clear_tt(eng);

        // 4-8 random legal plies for rich opening + early middlegame diversity
        int open_plies = 4 + (int) (rng_next(&seed) % 5ull);
        for (int r = 0; r < open_plies && !game->is_game_over; r++) {
            int n = num_legal_moves(game);
            if (n <= 0) {
                break;
            }
            u64 m = game->current_legal_moves[rng_next(&seed) % (u64) n];
            make_move(game, m);
        }

        int n_loc = 0;
        int win_w = 0, win_b = 0, draw_s = 0;
        float outcome = 0.5f;

        while (!game->is_game_over && game->move_count < 220) {
            search_result sr = engine_search(eng, game, ctx->search_depth, 0);
            if (sr.best_move == NULL_MOVE) {
                break;
            }
            int eval_w = engine_score_to_white(game->game, sr.score_cp);
            bool quiet_mv = (get_end_piece(sr.best_move) == NONE)
                            && !is_en_passant_move(sr.best_move)
                            && (get_promotion_piece(sr.best_move) == get_start_piece(sr.best_move));

            // Record quiet non-check, non-mate positions from early opening onward
            if (game->move_count >= 4 && quiet_mv && !in_check(game->game) && abs(eval_w) < 2500
                && n_loc < 512) {
                local[n_loc].board = *(game->game);
                local[n_loc].search_cp = (int16_t) eval_w;
                local[n_loc].result = 0.5f;
                n_loc++;
            }

            make_move(game, sr.best_move);

            if (game->is_game_over) {
                if (!game->is_draw) {
                    outcome = is_flipped(game->game) ? 1.0f : 0.0f;
                }
                break;
            }
            if (position_frequency(game) >= 3 || game->half_move_count >= 100) {
                outcome = 0.5f;
                break;
            }
            if (eval_w >= 650) {
                win_w++;
                win_b = 0;
            } else if (eval_w <= -650) {
                win_b++;
                win_w = 0;
            } else {
                win_w = win_b = 0;
            }
            if (win_w >= 5) {
                outcome = 1.0f;
                break;
            }
            if (win_b >= 5) {
                outcome = 0.0f;
                break;
            }
            if (game->move_count >= 70 && abs(eval_w) <= 15) {
                if (++draw_s >= 12) {
                    outcome = 0.5f;
                    break;
                }
            } else {
                draw_s = 0;
            }
        }

        if (n_loc > 0) {
            pthread_mutex_lock(&ctx->mu);
            for (int i = 0; i < n_loc && ctx->count < ctx->cap; i++) {
                local[i].result = outcome;
                ctx->buf[ctx->count++] = local[i];
            }
            pthread_mutex_unlock(&ctx->mu);
        }
    }

    full_chess_delete(&game);
    engine_delete(&eng);
    return NULL;
}

typedef struct eval_slice {
    const sample *data;
    size_t start;
    size_t end;
    double K;
    double sum_sq;
} eval_slice;

static inline double sigmoid(double x) {
    return 1.0 / (1.0 + exp(-x));
}

static void *mse_worker(void *arg) {
    eval_slice *sl = (eval_slice *) arg;
    double acc = 0.0;
    for (size_t i = sl->start; i < sl->end; i++) {
        int ev_stm = eval_board_classical(NULL, &sl->data[i].board);
        int ev_w = is_flipped(&sl->data[i].board) ? -ev_stm : ev_stm;
        double pred = sigmoid(sl->K * (double) ev_w);
        double d = (double) sl->data[i].result - pred;
        acc += d * d;
    }
    sl->sum_sq = acc;
    return NULL;
}

static double compute_mse(const sample *data, size_t n, double K, int nthreads) {
    pthread_t th[16];
    eval_slice sl[16];
    if (nthreads > 16) {
        nthreads = 16;
    }
    size_t chunk = (n + (size_t) nthreads - 1) / (size_t) nthreads;
    int launched = 0;
    for (int t = 0; t < nthreads; t++) {
        size_t s = (size_t) t * chunk;
        if (s >= n) {
            break;
        }
        size_t e = s + chunk;
        if (e > n) {
            e = n;
        }
        sl[t].data = data;
        sl[t].start = s;
        sl[t].end = e;
        sl[t].K = K;
        sl[t].sum_sq = 0.0;
        pthread_create(&th[t], NULL, mse_worker, &sl[t]);
        launched++;
    }
    double total = 0.0;
    for (int t = 0; t < launched; t++) {
        pthread_join(th[t], NULL);
        total += sl[t].sum_sq;
    }
    return total / (double) n;
}

typedef struct param_spec {
    const char *name;
    int *ptr;
    int lo;
    int hi;
} param_spec;

int main(int argc, char **argv) {
    int games = 350;
    int depth = 5;
    int threads = 10;
    const char *dataset_path = "/tmp/chess_dataset.bin";
    bool load_only = false;
    bool gen_only = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--games") == 0 && i + 1 < argc) {
            games = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--depth") == 0 && i + 1 < argc) {
            depth = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--dataset") == 0 && i + 1 < argc) {
            dataset_path = argv[++i];
        } else if (strcmp(argv[i], "--load") == 0) {
            load_only = true;
        } else if (strcmp(argv[i], "--gen-only") == 0) {
            gen_only = true;
        }
    }

    attacks_init();
    engine_eval_static(&(chess) { 0 }); // trigger engine_global_init

    size_t cap = 4000000;
    sample *data = (sample *) malloc(cap * sizeof(sample));
    size_t count = 0;

    if (load_only) {
        FILE *fp = fopen(dataset_path, "rb");
        if (!fp) {
            fprintf(stderr, "Cannot open dataset %s\n", dataset_path);
            free(data);
            return 1;
        }
        count = fread(data, sizeof(sample), cap, fp);
        fclose(fp);
        printf("[TEXEL] Loaded %zu quiet positions from %s\n", count, dataset_path);
    } else {
        printf("[TEXEL] Generating dataset: %d self-play games @ depth %d on %d threads...\n",
            games, depth, threads);
        gen_ctx ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.buf = data;
        ctx.cap = cap;
        ctx.games_target = games;
        ctx.search_depth = depth;
        pthread_mutex_init(&ctx.mu, NULL);

        pthread_t th[16];
        int nt = (threads > 16) ? 16 : threads;
        for (int t = 0; t < nt; t++) {
            pthread_create(&th[t], NULL, datagen_worker, &ctx);
        }
        for (int t = 0; t < nt; t++) {
            pthread_join(th[t], NULL);
        }
        pthread_mutex_destroy(&ctx.mu);
        count = ctx.count;
        printf("[TEXEL] Harvested %zu quiet labeled positions.\n", count);

        FILE *fp = fopen(dataset_path, "wb");
        if (fp) {
            fwrite(data, sizeof(sample), count, fp);
            fclose(fp);
            printf("[TEXEL] Saved dataset to %s\n", dataset_path);
        }
    }

    if (count < 100) {
        fprintf(stderr, "Dataset too small!\n");
        free(data);
        return 1;
    }

    if (gen_only) {
        free(data);
        return 0;
    }

    // Step 1: Optimize sigmoid scaling constant K on pure game outcomes first (prevents eval inflation!)
    double K = 0.0020;
    double best_mse = compute_mse(data, count, K, threads);
    for (double cand_k = 0.0022; cand_k <= 0.0080; cand_k += 0.0002) {
        double m = compute_mse(data, count, cand_k, threads);
        if (m < best_mse) {
            best_mse = m;
            K = cand_k;
        }
    }
    double init_mse = best_mse;
    printf("[TEXEL] Optimal K=%.6f, Initial WDL MSE: %.8f\n", K, init_mse);

    // Step 2: Coordinate descent over all 35 positional parameters
    param_spec specs[] = {
        { "bishop_pair_mg", &g_eval_params.bishop_pair_mg, 10, 70 },
        { "bishop_pair_eg", &g_eval_params.bishop_pair_eg, 15, 90 },
        { "knight_mob_mg", &g_eval_params.knight_mob_mg, 0, 12 },
        { "knight_mob_eg", &g_eval_params.knight_mob_eg, 0, 12 },
        { "bishop_mob_mg", &g_eval_params.bishop_mob_mg, 0, 12 },
        { "bishop_mob_eg", &g_eval_params.bishop_mob_eg, 0, 12 },
        { "rook_mob_mg", &g_eval_params.rook_mob_mg, 0, 8 },
        { "rook_mob_eg", &g_eval_params.rook_mob_eg, 0, 8 },
        { "queen_mob_mg", &g_eval_params.queen_mob_mg, 0, 6 },
        { "queen_mob_eg", &g_eval_params.queen_mob_eg, 0, 6 },
        { "knight_outpost_mg", &g_eval_params.knight_outpost_mg, 0, 60 },
        { "knight_outpost_eg", &g_eval_params.knight_outpost_eg, 0, 40 },
        { "rook_open_mg", &g_eval_params.rook_open_mg, 0, 50 },
        { "rook_open_eg", &g_eval_params.rook_open_eg, 0, 40 },
        { "rook_semi_mg", &g_eval_params.rook_semi_mg, 0, 35 },
        { "rook_semi_eg", &g_eval_params.rook_semi_eg, 0, 25 },
        { "rook_7th_mg", &g_eval_params.rook_7th_mg, 0, 60 },
        { "rook_7th_eg", &g_eval_params.rook_7th_eg, 0, 70 },
        { "rook_conn_mg", &g_eval_params.rook_conn_mg, 0, 30 },
        { "rook_conn_eg", &g_eval_params.rook_conn_eg, 0, 25 },
        { "pawn_threat_mg", &g_eval_params.pawn_threat_mg, 0, 80 },
        { "pawn_threat_eg", &g_eval_params.pawn_threat_eg, 0, 60 },
        { "doubled_mg", &g_eval_params.doubled_mg, 0, 40 },
        { "doubled_eg", &g_eval_params.doubled_eg, 0, 50 },
        { "isolated_mg", &g_eval_params.isolated_mg, 0, 40 },
        { "isolated_eg", &g_eval_params.isolated_eg, 0, 45 },
        { "backward_mg", &g_eval_params.backward_mg, 0, 35 },
        { "backward_eg", &g_eval_params.backward_eg, 0, 35 },
        { "shield_r2_mg", &g_eval_params.shield_r2_mg, 0, 40 },
        { "shield_r3_mg", &g_eval_params.shield_r3_mg, 0, 25 },
        { "king_open_mg", &g_eval_params.king_open_mg, 0, 50 },
        { "king_semi_mg", &g_eval_params.king_semi_mg, 0, 35 },
        { "king_att_num", &g_eval_params.king_att_num, 0, 12 },
        { "passer_scale_pct", &g_eval_params.passer_scale_pct, 40, 180 },
        { "tempo", &g_eval_params.tempo, 0, 30 },
    };
    int n_params = (int) (sizeof(specs) / sizeof(specs[0]));

    const int steps[] = { 8, 4, 2, 1 };
    for (int si = 0; si < 4; si++) {
        int step = steps[si];
        bool improved = true;
        int pass = 0;
        while (improved && pass < 6) {
            improved = false;
            pass++;
            for (int p = 0; p < n_params; p++) {
                int orig = *(specs[p].ptr);
                int best_val = orig;

                int up = orig + step;
                if (up <= specs[p].hi) {
                    *(specs[p].ptr) = up;
                    double m = compute_mse(data, count, K, threads);
                    if (m < best_mse - 1e-9) {
                        best_mse = m;
                        best_val = up;
                        improved = true;
                    }
                }
                if (best_val == orig) {
                    int dn = orig - step;
                    if (dn >= specs[p].lo) {
                        *(specs[p].ptr) = dn;
                        double m = compute_mse(data, count, K, threads);
                        if (m < best_mse - 1e-9) {
                            best_mse = m;
                            best_val = dn;
                            improved = true;
                        }
                    }
                }
                *(specs[p].ptr) = best_val;
            }
        }
        printf(
            "[TEXEL] After step=%d: MSE=%.8f (delta=%.8f)\n", step, best_mse, best_mse - init_mse);
    }

    printf("\n// Tuned eval_params (MSE %.8f -> %.8f):\n", init_mse, best_mse);
    printf("eval_params g_eval_params = {\n");
    for (int p = 0; p < n_params; p++) {
        printf("    %d, // %s\n", *(specs[p].ptr), specs[p].name);
    }
    printf("};\n");

    free(data);
    return 0;
}
