#define _POSIX_C_SOURCE 200809L

#include "features/gauntlet.h"

#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/notation.h"
#include "core/primitives.h"
#include "engine/engine.h"
#include "features/uci_client.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char *g_openings[] = {
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
    "rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 1",
    "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2",
    "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3",
    "rnbqkbnr/ppp2ppp/4p3/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3",
    "rnbqkbnr/pp2pppp/2p5/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3",
    "rnbqkbnr/ppp1pppp/8/3p4/2PP4/8/PP2PPPP/RNBQKBNR b KQkq - 0 2",
    "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3",
    "rnbqkb1r/pppppp1p/5np1/8/2PP4/2N5/PP2PPPP/R1BQKBNR b KQkq - 1 3",
    "rnbqkbnr/pppp1ppp/8/4p3/2P5/8/PP1PPPPP/RNBQKBNR w KQkq - 0 2",
};

static const int g_sf_anchors[6] = { 1350, 1450, 1650, 1950, 2250, 2500 };

struct gauntlet {
    pthread_mutex_t mu;
    pthread_t coord_thread;
    bool has_coord;

    char sf_path[512];
    char elo_file[256];
    bool sf_available;
    char sf_name[96];

    // Active run configuration
    int cfg_games;
    int cfg_threads;
    int cfg_only_level;
    bool cfg_use_sf;
    int cfg_sf_ms;

    // Live shared state
    bool running;
    bool stop_requested;
    double start_wall;
    double end_wall;
    gauntlet_status status;
    elo_record saved_elos[6];

    // Per-level worker coordination
    int active_level;
    int active_anchor;
    bool active_vs_sf;
    int next_game_idx;
};

static double now_wall(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec * 1e-9;
}

static double score_to_elo_diff(double score) {
    if (score < 0.02) {
        score = 0.02;
    }
    if (score > 0.98) {
        score = 0.98;
    }
    return -400.0 * log10((1.0 / score) - 1.0);
}

int gauntlet_cpu_count(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) {
        return 2;
    }
    if (n > MAX_GAUNTLET_THREADS) {
        return MAX_GAUNTLET_THREADS;
    }
    return (int) n;
}

// ============================================================================
// Persistent Elo JSON Save / Load
// ============================================================================

static bool extract_json_str(const char *src, const char *key, char *out, size_t out_size) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(src, pat);
    if (!p) {
        return false;
    }
    p += strlen(pat);
    while (*p == ' ' || *p == ':' || *p == '\t') {
        p++;
    }
    if (*p != '"') {
        return false;
    }
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_size) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return true;
}

static int extract_json_int(const char *src, const char *key, int def_val) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(src, pat);
    if (!p) {
        return def_val;
    }
    p += strlen(pat);
    while (*p == ' ' || *p == ':' || *p == '\t') {
        p++;
    }
    if (*p != '-' && (*p < '0' || *p > '9')) {
        return def_val;
    }
    return atoi(p);
}

bool elo_load_file(const char *path, elo_record out_levels[6], char *opponent_out, size_t opp_size,
    char *updated_out, size_t upd_size) {
    memset(out_levels, 0, 6 * sizeof(elo_record));
    for (int i = 0; i < 6; i++) {
        out_levels[i].level = i + 1;
    }
    if (opponent_out && opp_size > 0) {
        opponent_out[0] = '\0';
    }
    if (updated_out && upd_size > 0) {
        updated_out[0] = '\0';
    }

    FILE *fp = fopen(path ? path : ELO_RATINGS_FILE, "rb");
    if (!fp) {
        return false;
    }
    char buf[16384];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';

    char mode_str[32] = "";
    extract_json_str(buf, "mode", mode_str, sizeof(mode_str));
    bool is_rel = (strcmp(mode_str, "internal") == 0);
    if (opponent_out && opp_size > 0) {
        extract_json_str(buf, "opponent", opponent_out, opp_size);
    }
    if (updated_out && upd_size > 0) {
        extract_json_str(buf, "updated_at", updated_out, upd_size);
    }

    const char *arr = strstr(buf, "\"levels\"");
    if (!arr) {
        return false;
    }
    const char *p = arr;
    while ((p = strchr(p, '{')) != NULL) {
        const char *end = strchr(p, '}');
        if (!end) {
            break;
        }
        char obj[512];
        size_t olen = (size_t) (end - p + 1);
        if (olen >= sizeof(obj)) {
            olen = sizeof(obj) - 1;
        }
        memcpy(obj, p, olen);
        obj[olen] = '\0';

        int lvl = extract_json_int(obj, "level", 0);
        if (lvl >= 1 && lvl <= 6) {
            elo_record *r = &out_levels[lvl - 1];
            r->level = lvl;
            r->est_elo = extract_json_int(obj, "est_elo", 0);
            r->anchor_elo = extract_json_int(obj, "anchor_elo", 0);
            r->wins = extract_json_int(obj, "wins", 0);
            r->draws = extract_json_int(obj, "draws", 0);
            r->losses = extract_json_int(obj, "losses", 0);
            r->games = extract_json_int(obj, "games", 0);
            r->has_elo = (r->games > 0) || (strstr(obj, "\"has_elo\": true") != NULL)
                         || (strstr(obj, "\"has_elo\":true") != NULL);
            r->is_relative = is_rel || (strstr(obj, "\"is_relative\": true") != NULL)
                             || (strstr(obj, "\"is_relative\":true") != NULL);
        }
        p = end + 1;
    }
    return true;
}

bool elo_save_file(
    const char *path, const elo_record levels[6], const char *opponent, bool vs_stockfish) {
    const char *target = (path && path[0]) ? path : ELO_RATINGS_FILE;
    FILE *fp = fopen(target, "wb");
    if (!fp) {
        return false;
    }

    char date_buf[32] = "unknown";
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"updated_at\": \"%s\",\n", date_buf);
    fprintf(fp, "  \"mode\": \"%s\",\n", vs_stockfish ? "stockfish" : "internal");
    fprintf(fp, "  \"opponent\": \"%s\",\n",
        (opponent && opponent[0]) ? opponent : (vs_stockfish ? "Stockfish" : "Internal Ladder"));
    fprintf(fp, "  \"levels\": [\n");
    for (int i = 0; i < 6; i++) {
        const elo_record *r = &levels[i];
        const difficulty_preset *pr = engine_get_preset(i + 1);
        fprintf(fp,
            "    {\"level\": %d, \"name\": \"%s\", \"has_elo\": %s, \"is_relative\": %s, "
            "\"est_elo\": %d, \"anchor_elo\": %d, \"wins\": %d, \"draws\": %d, \"losses\": %d, "
            "\"games\": %d}%s\n",
            i + 1, pr->name, r->has_elo ? "true" : "false", r->is_relative ? "true" : "false",
            r->est_elo, r->anchor_elo, r->wins, r->draws, r->losses, r->games, (i < 5) ? "," : "");
    }
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    fclose(fp);
    return true;
}

// ============================================================================
// Worker Threads & Coordinator
// ============================================================================

typedef struct worker_arg {
    gauntlet *g;
    int worker_id;
} worker_arg;

static void fill_board_chars(const chess *b, char out[65]) {
    chess wv = *b;
    if (is_flipped(&wv)) {
        flip_perspective(&wv);
    }
    for (int sq = 0; sq < 64; sq++) {
        out[sq] = piece_to_char(get_piece_at_offset(&wv, sq));
    }
    out[64] = '\0';
}

static void *worker_main(void *arg) {
    worker_arg *wa = (worker_arg *) arg;
    gauntlet *g = wa->g;
    int wid = wa->worker_id;

    engine *c_eng = engine_create(8);
    engine_set_use_book(c_eng, false); // pure search strength calibration (no opening book bias)
    uci_engine *sf = NULL;
    if (g->active_vs_sf && g->sf_path[0]) {
        sf = uci_create(g->sf_path);
    }

    int n_open = (int) (sizeof(g_openings) / sizeof(g_openings[0]));

    while (true) {
        pthread_mutex_lock(&g->mu);
        if (g->stop_requested || g->next_game_idx >= g->cfg_games) {
            g->status.workers[wid].active = false;
            pthread_mutex_unlock(&g->mu);
            break;
        }
        int gidx = g->next_game_idx++;
        int lvl = g->active_level;
        int anchor = g->active_anchor;
        bool vs_sf = g->active_vs_sf;
        int sf_ms = g->cfg_sf_ms;
        int total_games = g->cfg_games;
        pthread_mutex_unlock(&g->mu);

        const char *fen = g_openings[(gidx / 2) % n_open];
        bool subject_is_white = ((gidx % 2) == 0);
        char subj_name[48];
        snprintf(subj_name, sizeof(subj_name), "Titan-C L%d", lvl);
        char opp_label[48];
        if (vs_sf) {
            snprintf(opp_label, sizeof(opp_label), "SF %d", anchor);
        } else {
            snprintf(opp_label, sizeof(opp_label), "Titan-C L%d", anchor);
        }

        full_chess *game = full_chess_create();
        set_from_fen(game, fen);
        game->is_static = true;
        engine_clear_tt(c_eng);
        if (sf) {
            uci_new_game(sf);
        }

        pthread_mutex_lock(&g->mu);
        worker_snapshot *ws = &g->status.workers[wid];
        ws->active = true;
        ws->worker_id = wid;
        ws->level = lvl;
        ws->game_num = gidx + 1;
        ws->total_games = total_games;
        ws->ply_count = 0;
        ws->eval_white_cp = 0;
        snprintf(ws->white_label, sizeof(ws->white_label), "%s",
            subject_is_white ? subj_name : opp_label);
        snprintf(ws->black_label, sizeof(ws->black_label), "%s",
            subject_is_white ? opp_label : subj_name);
        ws->last_san[0] = '\0';
        get_fen(game, ws->fen);
        fill_board_chars(game->game, ws->board);
        pthread_mutex_unlock(&g->mu);

        int max_plies = 400;
        bool aborted = false;
        int adjudicated = -1; // -1 = none, 0 = Black win, 1 = Draw, 2 = White win
        int win_w_streak = 0;
        int win_b_streak = 0;
        int draw_streak = 0;
        int last_eval_w_by_side[2] = { 0, 0 }; // [0] = White's eval, [1] = Black's eval

        while (!game->is_game_over && game->move_count < max_plies) {
            pthread_mutex_lock(&g->mu);
            aborted = g->stop_requested;
            pthread_mutex_unlock(&g->mu);
            if (aborted) {
                break;
            }

            bool black_turn = is_flipped(game->game);
            bool subj_turn = subject_is_white ? !black_turn : black_turn;

            u64 m = NULL_MOVE;
            int eval_w = 0;
            if (subj_turn) {
                search_result sr = engine_search_level(c_eng, game, lvl);
                m = sr.best_move;
                eval_w = engine_score_to_white(game->game, sr.score_cp);
            } else if (vs_sf && sf) {
                bool lim = (anchor < 3200);
                uci_search_result ur = uci_search(sf, game, lim, anchor, sf_ms);
                m = ur.base.best_move;
                eval_w = engine_score_to_white(game->game, ur.base.score_cp);
            } else {
                search_result sr = engine_search_level(c_eng, game, anchor);
                m = sr.best_move;
                eval_w = engine_score_to_white(game->game, sr.score_cp);
            }
            if (m == NULL_MOVE) {
                break;
            }

            char san[16];
            static_move_to_san(game->game, game->current_legal_moves, m, san, sizeof(san));
            make_move(game, m);

            pthread_mutex_lock(&g->mu);
            ws->ply_count = (int) game->move_count;
            ws->eval_white_cp = eval_w;
            snprintf(ws->last_san, sizeof(ws->last_san), "%s", san);
            get_fen(game, ws->fen);
            fill_board_chars(game->game, ws->board);
            pthread_mutex_unlock(&g->mu);

            if (game->is_game_over) {
                break;
            }

            // Standard FIDE 3-fold repetition & 50-move draw
            if (position_frequency(game) >= 3 || game->half_move_count >= 100) {
                adjudicated = 1;
                break;
            }

            // Two-sided resign & draw adjudication (requires BOTH engines to agree!)
            last_eval_w_by_side[black_turn ? 1 : 0] = eval_w;
            if (eval_w >= 650) {
                win_w_streak++;
                win_b_streak = 0;
                draw_streak = 0;
            } else if (eval_w <= -650) {
                win_b_streak++;
                win_w_streak = 0;
                draw_streak = 0;
            } else {
                win_w_streak = 0;
                win_b_streak = 0;
                if (game->move_count >= 80 && game->half_move_count >= 16 && abs(eval_w) <= 15) {
                    draw_streak++;
                } else {
                    draw_streak = 0;
                }
            }

            if (win_w_streak >= 6) {
                adjudicated = 2;
                break;
            }
            if (win_b_streak >= 6) {
                adjudicated = 0;
                break;
            }
            if (draw_streak >= 16) {
                adjudicated = 1;
                break;
            }
        }

        double w_res = 0.5;
        if (game->is_game_over) {
            if (!game->is_draw) {
                w_res = is_flipped(game->game) ? 1.0 : 0.0;
            }
        } else if (adjudicated == 2) {
            w_res = 1.0;
        } else if (adjudicated == 0) {
            w_res = 0.0;
        } else if (adjudicated == 1) {
            w_res = 0.5;
        } else {
            // Fallback if ply limit reached: adjudicate if both engines agree on >= 400cp edge
            if (last_eval_w_by_side[0] >= 400 && last_eval_w_by_side[1] >= 400) {
                w_res = 1.0;
            } else if (last_eval_w_by_side[0] <= -400 && last_eval_w_by_side[1] <= -400) {
                w_res = 0.0;
            }
        }
        double subj_res = subject_is_white ? w_res : (1.0 - w_res);
        full_chess_delete(&game);

        if (!aborted) {
            pthread_mutex_lock(&g->mu);
            level_progress *lp = &g->status.levels[lvl - 1];
            lp->done_games++;
            g->status.completed_games_total++;
            if (subj_res > 0.75) {
                lp->wins++;
            } else if (subj_res < 0.25) {
                lp->losses++;
            } else {
                lp->draws++;
            }
            double score = ((double) lp->wins + 0.5 * (double) lp->draws) / (double) lp->done_games;
            double diff = score_to_elo_diff(score);
            if (vs_sf) {
                lp->est_elo = (int) lround((double) anchor + diff);
                lp->is_relative = false;
            } else {
                int base_elo = (lvl > 1 && g->status.levels[lvl - 2].has_elo)
                                   ? g->status.levels[lvl - 2].est_elo
                                   : 0;
                lp->est_elo = base_elo + (int) lround(diff);
                lp->is_relative = true;
            }
            lp->has_elo = true;
            pthread_mutex_unlock(&g->mu);
        }
    }

    uci_delete(&sf);
    engine_delete(&c_eng);
    return NULL;
}

static void *coordinator_main(void *arg) {
    gauntlet *g = (gauntlet *) arg;

    bool vs_sf = g->cfg_use_sf && g->sf_available;
    int start_l
        = (g->cfg_only_level >= 1 && g->cfg_only_level <= 6) ? g->cfg_only_level : (vs_sf ? 1 : 2);
    int end_l = (g->cfg_only_level >= 1 && g->cfg_only_level <= 6) ? g->cfg_only_level : 6;

    for (int lvl = start_l; lvl <= end_l; lvl++) {
        pthread_mutex_lock(&g->mu);
        if (g->stop_requested) {
            pthread_mutex_unlock(&g->mu);
            break;
        }
        g->active_level = lvl;
        g->active_vs_sf = vs_sf;
        g->active_anchor = vs_sf ? g_sf_anchors[lvl - 1] : ((lvl > 1) ? (lvl - 1) : 1);
        g->next_game_idx = 0;

        level_progress *lp = &g->status.levels[lvl - 1];
        lp->state = 1; // running
        lp->done_games = 0;
        lp->total_games = g->cfg_games;
        lp->wins = 0;
        lp->draws = 0;
        lp->losses = 0;
        lp->anchor_elo = g->active_anchor;
        lp->is_relative = !vs_sf;
        int nthreads = g->cfg_threads;
        if (nthreads > g->cfg_games) {
            nthreads = g->cfg_games;
        }
        g->status.worker_count = nthreads;
        pthread_mutex_unlock(&g->mu);

        pthread_t threads[MAX_GAUNTLET_THREADS];
        worker_arg wargs[MAX_GAUNTLET_THREADS];
        for (int t = 0; t < nthreads; t++) {
            wargs[t].g = g;
            wargs[t].worker_id = t;
            pthread_create(&threads[t], NULL, worker_main, &wargs[t]);
        }
        for (int t = 0; t < nthreads; t++) {
            pthread_join(threads[t], NULL);
        }

        pthread_mutex_lock(&g->mu);
        lp->state = g->stop_requested ? 3 : 2;
        if (lp->done_games > 0) {
            elo_record *rec = &g->saved_elos[lvl - 1];
            rec->level = lvl;
            rec->est_elo = lp->est_elo;
            rec->anchor_elo = lp->anchor_elo;
            rec->wins = lp->wins;
            rec->draws = lp->draws;
            rec->losses = lp->losses;
            rec->games = lp->done_games;
            rec->has_elo = true;
            rec->is_relative = lp->is_relative;

            // In internal ladder mode, ensure L1 baseline is saved as 0 relative
            if (!vs_sf && !g->saved_elos[0].has_elo) {
                g->saved_elos[0].level = 1;
                g->saved_elos[0].est_elo = 0;
                g->saved_elos[0].has_elo = true;
                g->saved_elos[0].is_relative = true;
            }
            elo_save_file(g->elo_file, g->saved_elos, g->status.opponent_name, vs_sf);
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            if (tm_info) {
                strftime(g->status.updated_at, sizeof(g->status.updated_at), "%Y-%m-%d %H:%M:%S",
                    tm_info);
            }
        }
        pthread_mutex_unlock(&g->mu);
    }

    pthread_mutex_lock(&g->mu);
    g->end_wall = now_wall();
    g->running = false;
    g->status.running = false;
    g->status.elapsed_sec = g->end_wall - g->start_wall;
    for (int t = 0; t < MAX_GAUNTLET_THREADS; t++) {
        g->status.workers[t].active = false;
    }
    pthread_mutex_unlock(&g->mu);
    return NULL;
}

// ============================================================================
// Public Lifecycle API
// ============================================================================

gauntlet *gauntlet_create(const char *sf_path, const char *elo_file_path) {
    gauntlet *g = (gauntlet *) calloc(1, sizeof(gauntlet));
    if (!g) {
        return NULL;
    }
    pthread_mutex_init(&g->mu, NULL);
    snprintf(g->elo_file, sizeof(g->elo_file), "%s",
        (elo_file_path && elo_file_path[0]) ? elo_file_path : ELO_RATINGS_FILE);

    if (uci_find_stockfish(sf_path, g->sf_path, sizeof(g->sf_path))) {
        uci_engine *probe = uci_create(g->sf_path);
        if (probe) {
            g->sf_available = true;
            snprintf(g->sf_name, sizeof(g->sf_name), "%s", uci_get_name(probe));
            uci_delete(&probe);
        }
    }

    g->status.cpu_cores = gauntlet_cpu_count();
    g->status.vs_stockfish = g->sf_available;

    // Pre-populate from saved elo_ratings.json if present
    char saved_opp[96] = "", saved_upd[32] = "";
    elo_load_file(
        g->elo_file, g->saved_elos, saved_opp, sizeof(saved_opp), saved_upd, sizeof(saved_upd));
    snprintf(g->status.opponent_name, sizeof(g->status.opponent_name), "%s",
        saved_opp[0] ? saved_opp : (g->sf_available ? g->sf_name : "Internal Ladder"));
    snprintf(g->status.updated_at, sizeof(g->status.updated_at), "%s", saved_upd);

    for (int i = 0; i < 6; i++) {
        level_progress *lp = &g->status.levels[i];
        lp->level = i + 1;
        lp->state = g->saved_elos[i].has_elo ? 2 : 0;
        lp->done_games = g->saved_elos[i].games;
        lp->total_games = g->saved_elos[i].games;
        lp->wins = g->saved_elos[i].wins;
        lp->draws = g->saved_elos[i].draws;
        lp->losses = g->saved_elos[i].losses;
        lp->anchor_elo = g->saved_elos[i].anchor_elo;
        lp->est_elo = g->saved_elos[i].est_elo;
        lp->has_elo = g->saved_elos[i].has_elo;
        lp->is_relative = g->saved_elos[i].is_relative;
        if (lp->is_relative && i > 0) {
            snprintf(lp->opponent_label, sizeof(lp->opponent_label), "vs L%d %s", i,
                engine_get_preset(i)->name);
        } else if (!lp->is_relative && lp->anchor_elo > 0) {
            snprintf(lp->opponent_label, sizeof(lp->opponent_label), "vs SF %d", lp->anchor_elo);
        } else if (i == 0 && lp->is_relative) {
            snprintf(lp->opponent_label, sizeof(lp->opponent_label), "Baseline");
        } else {
            snprintf(lp->opponent_label, sizeof(lp->opponent_label), "—");
        }
    }
    return g;
}

void gauntlet_delete(gauntlet **g) {
    if (!g || !*g) {
        return;
    }
    gauntlet_stop(*g);
    gauntlet_wait(*g);
    pthread_mutex_destroy(&(*g)->mu);
    free(*g);
    *g = NULL;
}

bool gauntlet_start(gauntlet *g, int games_per_level, int num_threads, int only_level,
    bool use_stockfish, int sf_time_ms) {
    if (!g) {
        return false;
    }
    pthread_mutex_lock(&g->mu);
    if (g->running) {
        pthread_mutex_unlock(&g->mu);
        return false;
    }
    if (g->has_coord) {
        pthread_mutex_unlock(&g->mu);
        pthread_join(g->coord_thread, NULL);
        pthread_mutex_lock(&g->mu);
        g->has_coord = false;
    }

    if (games_per_level < 2) {
        games_per_level = 2;
    }
    int max_cores = gauntlet_cpu_count();
    if (num_threads <= 0) {
        num_threads = max_cores;
    }
    if (num_threads > MAX_GAUNTLET_THREADS) {
        num_threads = MAX_GAUNTLET_THREADS;
    }

    bool vs_sf = use_stockfish && g->sf_available;
    g->cfg_games = games_per_level;
    g->cfg_threads = num_threads;
    g->cfg_only_level = only_level;
    g->cfg_use_sf = vs_sf;
    g->cfg_sf_ms = (sf_time_ms > 20) ? sf_time_ms : 500;

    g->running = true;
    g->stop_requested = false;
    g->start_wall = now_wall();
    g->end_wall = g->start_wall;

    g->status.running = true;
    g->status.stop_requested = false;
    g->status.vs_stockfish = vs_sf;
    g->status.num_threads = num_threads;
    g->status.games_per_level = games_per_level;
    g->status.completed_games_total = 0;
    g->status.elapsed_sec = 0.0;
    snprintf(g->status.opponent_name, sizeof(g->status.opponent_name), "%s",
        vs_sf ? g->sf_name : "Internal Ladder");

    int start_l = (only_level >= 1 && only_level <= 6) ? only_level : (vs_sf ? 1 : 2);
    int end_l = (only_level >= 1 && only_level <= 6) ? only_level : 6;
    g->status.scheduled_games_total = (end_l - start_l + 1) * games_per_level;

    for (int i = 0; i < 6; i++) {
        int lvl = i + 1;
        level_progress *lp = &g->status.levels[i];
        if (lvl >= start_l && lvl <= end_l) {
            lp->state = 0;
            lp->done_games = 0;
            lp->total_games = games_per_level;
            lp->wins = 0;
            lp->draws = 0;
            lp->losses = 0;
            lp->anchor_elo = vs_sf ? g_sf_anchors[i] : ((lvl > 1) ? (lvl - 1) : 1);
            lp->is_relative = !vs_sf;
            if (vs_sf) {
                snprintf(
                    lp->opponent_label, sizeof(lp->opponent_label), "vs SF %d", lp->anchor_elo);
            } else {
                snprintf(lp->opponent_label, sizeof(lp->opponent_label), "vs L%d %s",
                    lp->anchor_elo, engine_get_preset(lp->anchor_elo)->name);
            }
        } else if (!vs_sf && lvl == 1) {
            lp->state = 2;
            lp->est_elo = 0;
            lp->has_elo = true;
            lp->is_relative = true;
            snprintf(lp->opponent_label, sizeof(lp->opponent_label), "Baseline");
        }
    }
    for (int t = 0; t < MAX_GAUNTLET_THREADS; t++) {
        g->status.workers[t].active = false;
    }

    // Ensure Zobrist tables are initialized on main thread before spawning workers
    chess dummy;
    chess_reset(&dummy);
    (void) engine_hash_board(&dummy);

    g->has_coord = true;
    pthread_create(&g->coord_thread, NULL, coordinator_main, g);
    pthread_mutex_unlock(&g->mu);
    return true;
}

void gauntlet_stop(gauntlet *g) {
    if (!g) {
        return;
    }
    pthread_mutex_lock(&g->mu);
    g->stop_requested = true;
    g->status.stop_requested = true;
    pthread_mutex_unlock(&g->mu);
}

bool gauntlet_is_running(gauntlet *g) {
    if (!g) {
        return false;
    }
    pthread_mutex_lock(&g->mu);
    bool r = g->running;
    pthread_mutex_unlock(&g->mu);
    return r;
}

void gauntlet_wait(gauntlet *g) {
    if (!g) {
        return;
    }
    pthread_mutex_lock(&g->mu);
    bool hc = g->has_coord;
    pthread_mutex_unlock(&g->mu);
    if (hc) {
        pthread_join(g->coord_thread, NULL);
        pthread_mutex_lock(&g->mu);
        g->has_coord = false;
        pthread_mutex_unlock(&g->mu);
    }
}

void gauntlet_snapshot(gauntlet *g, gauntlet_status *out) {
    if (!g || !out) {
        return;
    }
    pthread_mutex_lock(&g->mu);
    *out = g->status;
    if (g->running) {
        out->elapsed_sec = now_wall() - g->start_wall;
    }
    pthread_mutex_unlock(&g->mu);
}
