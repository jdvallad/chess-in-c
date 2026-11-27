#define _POSIX_C_SOURCE 200809L

#include "features/gauntlet.h"

#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/notation.h"
#include "core/primitives.h"
#include "engine/engine.h"
#include "engine/nnue.h"
#include "engine/syzygy.h"
#include "features/uci_client.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// 30 diverse master opening FENs (x2 color reversals = 60 unique tournament setups)
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
    "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2",
    "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
    "rnbqkbnr/pp2pppp/3p4/2p5/3PP3/5N2/PPP2PPP/RNBQKB1R b KQkq - 0 3",
    "rnbqkbnr/ppp2ppp/3p4/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3",
    "rnbqkbnr/ppp1pppp/8/3p4/3P1B2/8/PPP1PPPP/RN1QKBNR b KQkq - 1 2",
    "rnbqkb1r/ppp2ppp/4pn2/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 0 4",
    "rnbqkb1r/pp2pppp/2p2n2/3p4/2PP4/2N2N2/PP2PPPP/R1BQKB1R b KQkq - 3 4",
    "rnbqkbnr/ppp1pppp/8/8/2pP4/5N2/PP2PPPP/RNBQKB1R b KQkq - 1 3",
    "rnbqk2r/pppp1ppp/4pn2/8/1bPP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 2 4",
    "rnbqkb1r/p1pp1ppp/1p2pn2/8/2PP4/5N2/PP2PPPP/RNBQKB1R w KQkq - 0 4",
    "rnbqkb1r/pppppppp/5n2/8/2P5/5N2/PP1PPPPP/RNBQKB1R b KQkq - 2 2",
    "rnbqkbnr/pp1ppppp/8/2p5/2P5/8/PP1PPPPP/RNBQKBNR w KQkq - 0 2",
    "rnbqkbnr/pppp1ppp/8/4p3/4PP2/8/PPPP2PP/RNBQKBNR b KQkq - 0 2",
    "r1bqkbnr/pppp1ppp/2n5/4p3/3PP3/5N2/PPP2PPP/RNBQKB1R b KQkq - 0 3",
    "rnbqkb1r/pp2pppp/3p1n2/2p5/4P3/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 2 4",
    "rnbqkb1r/pppppp1p/5np1/8/3P4/5N2/PPP1PPPP/RNBQKB1R w KQkq - 1 3",
    "rnbqkbnr/pp1p1ppp/4p3/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3",
    "rnbqkbnr/ppp2ppp/4p3/3p4/3P4/5N2/PPP1PPPP/RNBQKB1R w KQkq - 0 3",
    "rnbqkbnr/pppp1ppp/4p3/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2",
    "rnbqkb1r/ppp1pppp/5n2/3p4/3P4/5NP1/PPP1PP1P/RNBQKB1R b KQkq - 0 3",
};

// Canonical bot slots (0..14) -> stable preset level IDs & default Stockfish Elo anchors
static const int g_slot_levels[GAUNTLET_NUM_LEVELS] = {
    1, 2, 3, 4, 5, 6,
    GAUNTLET_ZERO_C_LEVEL,
    GAUNTLET_ZERO_C_V1_LEVEL,
    GAUNTLET_TOY_LEVEL,
    GAUNTLET_HYBRID_DUAL_LEVEL,
    GAUNTLET_HYBRID_LMR_LEVEL,
    GAUNTLET_HYBRID_OMEGA_LEVEL,
    GAUNTLET_HYBRID_NNUE_LEVEL,
    GAUNTLET_APEX_PURE_LEVEL,
    GAUNTLET_APEX_SYZYGY_LEVEL,
};

static const int g_sf_anchors[GAUNTLET_NUM_LEVELS] = {
    1350, 1450, 1650, 1950, 2250, 2500, // L1..L6 Titan-C
    1400, // L7  Zero-C v2
    1350, // L13 Zero-C v1
    1350, // L20 Zero-C Lite (Toy)
    2450, // L14 Gemini Arbiter
    2450, // L15 Oracle of LMR
    2100, // L16 Zero-C Omega
    2350, // L17 Singularity v5
    2400, // L18 Apex-C Pure
    2450, // L19 Apex-C + Syzygy
};

int gauntlet_slot_to_level(int slot) {
    if (slot < 0 || slot >= GAUNTLET_NUM_LEVELS) {
        return 1;
    }
    return g_slot_levels[slot];
}

int gauntlet_level_to_slot(int level) {
    if (level == 12) {
        level = GAUNTLET_ZERO_C_LEVEL;
    }
    for (int i = 0; i < GAUNTLET_NUM_LEVELS; i++) {
        if (g_slot_levels[i] == level) {
            return i;
        }
    }
    return -1;
}

typedef struct calc_game_rec {
    int anchor_elo;
    double score; // 1.0 = win, 0.5 = draw, 0.0 = loss
} calc_game_rec;

struct gauntlet {
    pthread_mutex_t mu;
    pthread_t coord_thread;
    bool has_coord;

    char sf_path[512];
    char elo_file[256];
    bool sf_available;
    bool az_available;
    int sf_min_elo;
    int sf_max_elo;
    char sf_name[96];

    // Active run configuration
    bool cfg_calc_mode;
    int cfg_games;
    int cfg_threads;
    int cfg_only_level;
    bool cfg_use_sf;
    int cfg_sf_ms;

    // Calculator configuration
    int cfg_calc_level;
    int cfg_calc_custom_elo;
    char cfg_calc_az_ckpt[256];
    int cfg_calc_target_ci;
    int cfg_calc_max_games;

    // Live shared state
    bool running;
    bool stop_requested;
    double start_wall;
    double end_wall;
    gauntlet_status status;
    elo_record saved_elos[GAUNTLET_NUM_LEVELS];

    // Per-level / per-batch worker coordination
    int active_level;
    int active_custom_elo;
    int active_anchor;
    bool active_vs_sf;
    int batch_game_start;
    int batch_game_end;
    int next_game_idx;

    // Calculator game history for MLE Elo + 95% CI
    calc_game_rec calc_games[GAUNTLET_MAX_CALC_GAMES];
    int calc_game_count;
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

static bool detect_zero_c_available(void) {
    return (access("src/az/az_uci_run.sh", X_OK) == 0)
           && (access("build/libaz_mcts.so", R_OK) == 0);
}

static bool is_gauntlet_az_level(int level) {
    return level == GAUNTLET_ZERO_C_LEVEL || level == 12 || level == GAUNTLET_ZERO_C_V1_LEVEL
           || level == GAUNTLET_TOY_LEVEL;
}

static bool is_gauntlet_hybrid_gpu_level(int level) {
    return level == GAUNTLET_HYBRID_DUAL_LEVEL || level == GAUNTLET_HYBRID_LMR_LEVEL
           || level == GAUNTLET_HYBRID_OMEGA_LEVEL;
}

static bool is_gauntlet_apex_level(int level) {
    return level == GAUNTLET_APEX_PURE_LEVEL || level == GAUNTLET_APEX_SYZYGY_LEVEL;
}

static bool detect_apex_available(void) {
    return access("build/chess_uci", X_OK) == 0;
}

bool gauntlet_level_available(const gauntlet *g, int level) {
    if (level >= 1 && level <= 6) {
        return true;
    }
    if (level == GAUNTLET_ZERO_C_LEVEL || level == 12) {
        return detect_zero_c_available() && access("data/az/latest.pt", R_OK) == 0
               && access("data/az/replay.npz", R_OK) == 0;
    }
    if (level == GAUNTLET_ZERO_C_V1_LEVEL) {
        return detect_zero_c_available() && access("data/az_v1_gen4320/latest.pt", R_OK) == 0;
    }
    if (level == GAUNTLET_TOY_LEVEL) {
        return detect_zero_c_available() && access("data/az_toy/latest.pt", R_OK) == 0
               && access("data/az_toy/replay.npz", R_OK) == 0;
    }
    if (level == GAUNTLET_HYBRID_NNUE_LEVEL) {
        return access("data/chess_net_zero_v4.nnue", R_OK) == 0 && detect_apex_available();
    }
    // Hybrids #14..16 and Apex-C #18..19 unlock once Zero-C v2 + Singularity v5 are finished AND Apex Forge has begun
    bool forge_prereq = (access("data/az/v2_complete.flag", R_OK) == 0)
                        && (access("data/chess_net_zero_v4.nnue", R_OK) == 0);
    struct stat st_dg;
    bool has_dg = (stat("data/datagen_v4.bin", &st_dg) == 0 && st_dg.st_size >= 3600);
    bool forge_begun = forge_prereq && (has_dg || access("data/titan_v6_1024.nnue", R_OK) == 0 || syzygy_installed_pieces() >= 3);
    if (is_gauntlet_hybrid_gpu_level(level)) {
        return forge_begun && access("data/az/latest.pt", R_OK) == 0
               && access("src/az/hybrid_uci_run.sh", X_OK) == 0 && detect_zero_c_available()
               && detect_apex_available();
    }
    if (level == GAUNTLET_APEX_PURE_LEVEL) {
        return detect_apex_available() && forge_begun && (has_dg || access("data/titan_v6_1024.nnue", R_OK) == 0);
    }
    if (level == GAUNTLET_APEX_SYZYGY_LEVEL) {
        return detect_apex_available() && forge_begun && (has_dg || access("data/titan_v6_1024.nnue", R_OK) == 0)
               && g && g->sf_path[0] != '\0' && syzygy_installed_pieces() >= 3;
    }
    return false;
}

const char *gauntlet_level_engine(int level) {
    if (is_gauntlet_az_level(level)) {
        return "az";
    }
    if (is_gauntlet_hybrid_gpu_level(level) || level == GAUNTLET_HYBRID_NNUE_LEVEL) {
        return "hybrid";
    }
    if (is_gauntlet_apex_level(level)) {
        return "apex";
    }
    return "c";
}

const char *gauntlet_level_name(int level) {
    if (level == GAUNTLET_ZERO_C_LEVEL || level == 12) {
        return "Zero-C v2 (AlphaZero)";
    }
    if (level == GAUNTLET_ZERO_C_V1_LEVEL) {
        return "Zero-C v1 (Gen 4320)";
    }
    if (level == GAUNTLET_TOY_LEVEL) {
        return "Zero-C Lite (Toy Net)";
    }
    if (level == GAUNTLET_HYBRID_DUAL_LEVEL) {
        return "Gemini Arbiter (Dual-Brain)";
    }
    if (level == GAUNTLET_HYBRID_LMR_LEVEL) {
        return "Oracle of LMR (Neural Search)";
    }
    if (level == GAUNTLET_HYBRID_OMEGA_LEVEL) {
        return "Zero-C Omega (Tactical MCTS)";
    }
    if (level == GAUNTLET_HYBRID_NNUE_LEVEL) {
        return "Singularity v5 (Zero-NNUE)";
    }
    if (level == GAUNTLET_APEX_PURE_LEVEL) {
        return "Apex-C (Pure)";
    }
    if (level == GAUNTLET_APEX_SYZYGY_LEVEL) {
        return "Apex-C + Syzygy";
    }
    return engine_get_preset(level)->name;
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
// Exact Logistic MLE Elo Estimator + 95% Fisher Confidence Interval
// ============================================================================

static void compute_mle_elo_ci(const calc_game_rec *games, int n, int prior_mean, int *out_elo,
    int *out_ci_half, int *out_low, int *out_high) {
    if (n <= 0) {
        *out_elo = prior_mean;
        *out_ci_half = 400;
        *out_low = prior_mean - 400;
        *out_high = prior_mean + 400;
        return;
    }

    const double k = log(10.0) / 400.0;
    const double prior_sigma = 450.0;
    const double inv_prior_var = 1.0 / (prior_sigma * prior_sigma);

    double r = (double) prior_mean;
    for (int iter = 0; iter < 30; iter++) {
        double grad = -(r - (double) prior_mean) * inv_prior_var;
        double hess = inv_prior_var;
        for (int i = 0; i < n; i++) {
            double x = k * (r - (double) games[i].anchor_elo);
            double p = 1.0 / (1.0 + exp(-x));
            grad += k * (games[i].score - p);
            hess += k * k * p * (1.0 - p);
        }
        if (hess < 1e-12) {
            break;
        }
        double step = grad / hess;
        if (step > 250.0) {
            step = 250.0;
        } else if (step < -250.0) {
            step = -250.0;
        }
        r += step;
        if (r < 400.0) {
            r = 400.0;
        } else if (r > 3650.0) {
            r = 3650.0;
        }
        if (fabs(step) < 1e-4) {
            break;
        }
    }

    // Pure data Fisher Information (+ tiny floor) for honest 95% confidence interval
    double fisher = 0.0;
    for (int i = 0; i < n; i++) {
        double x = k * (r - (double) games[i].anchor_elo);
        double p = 1.0 / (1.0 + exp(-x));
        fisher += k * k * p * (1.0 - p);
    }
    double se = (fisher > 1e-9) ? (1.0 / sqrt(fisher + inv_prior_var * 0.25)) : 350.0;
    int ci = (int) ceil(1.96 * se);
    if (ci < 15) {
        ci = 15;
    }
    if (ci > 450) {
        ci = 450;
    }
    int est = (int) lround(r);
    *out_elo = est;
    *out_ci_half = ci;
    *out_low = est - ci;
    *out_high = est + ci;
}

static void record_calc_bucket(elo_calc_status *cs, int anchor, double score) {
    int idx = -1;
    for (int i = 0; i < cs->num_buckets; i++) {
        if (cs->buckets[i].anchor_elo == anchor) {
            idx = i;
            break;
        }
    }
    if (idx < 0 && cs->num_buckets < GAUNTLET_MAX_CALC_BUCKETS) {
        idx = cs->num_buckets++;
        cs->buckets[idx].anchor_elo = anchor;
        cs->buckets[idx].wins = 0;
        cs->buckets[idx].draws = 0;
        cs->buckets[idx].losses = 0;
    }
    if (idx >= 0) {
        if (score > 0.75) {
            cs->buckets[idx].wins++;
        } else if (score < 0.25) {
            cs->buckets[idx].losses++;
        } else {
            cs->buckets[idx].draws++;
        }
    }
}

// ============================================================================
// Persistent Elo JSON Save / Load (Supports L1..L6 Titan-C + L7 Zero-C)
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

bool elo_load_file(const char *path, elo_record out_levels[GAUNTLET_NUM_LEVELS], char *opponent_out,
    size_t opp_size, char *updated_out, size_t upd_size) {
    memset(out_levels, 0, GAUNTLET_NUM_LEVELS * sizeof(elo_record));
    for (int i = 0; i < GAUNTLET_NUM_LEVELS; i++) {
        out_levels[i].level = gauntlet_slot_to_level(i);
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
        int slot = gauntlet_level_to_slot(lvl);
        if (slot >= 0 && slot < GAUNTLET_NUM_LEVELS) {
            elo_record *r = &out_levels[slot];
            r->level = gauntlet_slot_to_level(slot);
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

bool elo_save_file(const char *path, const elo_record levels[GAUNTLET_NUM_LEVELS],
    const char *opponent, bool vs_stockfish) {
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
    for (int i = 0; i < GAUNTLET_NUM_LEVELS; i++) {
        const elo_record *r = &levels[i];
        int lvl = gauntlet_slot_to_level(i);
        const char *name = gauntlet_level_name(lvl);
        fprintf(fp,
            "    {\"level\": %d, \"name\": \"%s\", \"has_elo\": %s, \"is_relative\": %s, "
            "\"est_elo\": %d, \"anchor_elo\": %d, \"wins\": %d, \"draws\": %d, \"losses\": %d, "
            "\"games\": %d}%s\n",
            lvl, name, r->has_elo ? "true" : "false", r->is_relative ? "true" : "false",
            r->est_elo, r->anchor_elo, r->wins, r->draws, r->losses, r->games,
            (i + 1 < GAUNTLET_NUM_LEVELS) ? "," : "");
    }
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    fclose(fp);
    return true;
}

// ============================================================================
// Worker Threads & Game Runner (Supports Titan-C, Custom Elo, and Zero-C!)
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

    pthread_mutex_lock(&g->mu);
    int init_lvl = g->active_level;
    bool init_vs_sf = g->active_vs_sf;
    char az_ckpt[256];
    snprintf(az_ckpt, sizeof(az_ckpt), "%s", g->cfg_calc_az_ckpt);
    pthread_mutex_unlock(&g->mu);

    engine *c_eng = engine_create(8);
    engine_set_use_book(c_eng, false); // Pure search calibration (openings come from g_openings[])
    uci_engine *sf = NULL;
    if (init_vs_sf && g->sf_path[0]) {
        sf = uci_create(g->sf_path);
        if (sf) {
            uci_set_option(sf, "Threads", "1");
            uci_set_option(sf, "Hash", "16");
        }
    }
    uci_engine *az = NULL;
    if (is_gauntlet_az_level(init_lvl) && detect_zero_c_available()) {
        az = uci_create("src/az/az_uci_run.sh");
        if (az) {
            uci_set_option(az, "OpeningVariety", "false");
            uci_set_option(az, "TacticalHybrid", "true");
            const char *use_ckpt = az_ckpt;
            if (use_ckpt[0] == '\0') {
                if (init_lvl == GAUNTLET_ZERO_C_V1_LEVEL) {
                    use_ckpt = "data/az_v1_gen4320/latest.pt";
                } else if (init_lvl == GAUNTLET_TOY_LEVEL) {
                    use_ckpt = "data/az_toy/latest.pt";
                } else {
                    use_ckpt = "data/az/latest.pt";
                }
            }
            bool is_v1 = (strstr(use_ckpt, "az_v1") != NULL);
            bool is_toy = (strstr(use_ckpt, "az_toy") != NULL);
            uci_set_option(az, "Simulations", is_v1 ? "1000" : (is_toy ? "300" : "600"));
            uci_set_option(az, "Checkpoint", use_ckpt);
        }
    }
    uci_engine *hyb = NULL;
    if (is_gauntlet_hybrid_gpu_level(init_lvl) && access("src/az/hybrid_uci_run.sh", X_OK) == 0) {
        hyb = uci_create("src/az/hybrid_uci_run.sh");
        if (hyb) {
            const char *mode = (init_lvl == GAUNTLET_HYBRID_LMR_LEVEL)
                                   ? "neural_lmr"
                                   : ((init_lvl == GAUNTLET_HYBRID_OMEGA_LEVEL) ? "omega_mcts"
                                                                                : "dual_brain");
            uci_set_option(hyb, "HybridMode", mode);
        }
    }
    uci_engine *apex = NULL;
    uci_engine *apex_tb = NULL;
    int apex_syz_pieces = 0;
    if ((is_gauntlet_apex_level(init_lvl) || init_lvl == GAUNTLET_HYBRID_NNUE_LEVEL)
        && detect_apex_available()) {
        apex = uci_create("build/chess_uci");
        if (apex) {
            uci_set_option(apex, "Threads", "1");
            uci_set_option(apex, "Hash", "64");
            uci_set_option(apex, "OwnBook", "false");
            if (init_lvl == GAUNTLET_HYBRID_NNUE_LEVEL) {
                uci_set_option(apex, "EvalFile", "data/chess_net_zero_v4.nnue");
            } else {
                if (access(NNUE_V4_FILE, R_OK) == 0) {
                    uci_set_option(apex, "EvalFile", NNUE_V4_FILE);
                }
                (void) uci_apply_spsa_file(apex, "data/spsa_best.json");
            }
        }
        if (init_lvl == GAUNTLET_APEX_SYZYGY_LEVEL && g->sf_path[0] && syzygy_installed_pieces() >= 3) {
            apex_tb = uci_create(g->sf_path);
            if (apex_tb) {
                uci_set_option(apex_tb, "Threads", "1");
                uci_set_option(apex_tb, "Hash", "16");
                uci_set_option(apex_tb, "SyzygyPath", SYZYGY_DEFAULT_DIR);
                uci_set_option(apex_tb, "SyzygyProbeDepth", "1");
                apex_syz_pieces = syzygy_installed_pieces();
            }
        }
    }

    int n_open = (int) (sizeof(g_openings) / sizeof(g_openings[0]));

    while (true) {
        pthread_mutex_lock(&g->mu);
        if (g->stop_requested || g->next_game_idx >= g->batch_game_end) {
            g->status.workers[wid].active = false;
            pthread_mutex_unlock(&g->mu);
            break;
        }
        int gidx = g->next_game_idx++;
        int lvl = g->active_level;
        int celo = g->active_custom_elo;
        int anchor = g->active_anchor;
        bool vs_sf = g->active_vs_sf;
        bool is_calc = g->cfg_calc_mode;
        int sf_ms = g->cfg_sf_ms;
        int total_games = is_calc ? g->cfg_calc_max_games : g->cfg_games;
        pthread_mutex_unlock(&g->mu);

        if ((is_gauntlet_az_level(lvl) && !az) || (is_gauntlet_hybrid_gpu_level(lvl) && !hyb)
            || ((is_gauntlet_apex_level(lvl) || lvl == GAUNTLET_HYBRID_NNUE_LEVEL) && !apex)) {
            break;
        }

        const char *fen = g_openings[(gidx / 2) % n_open];
        bool subject_is_white = ((gidx % 2) == 0);
        char subj_name[48];
        if (lvl == 0 && celo >= 800) {
            snprintf(subj_name, sizeof(subj_name), "Titan-C ~%d", celo);
        } else {
            snprintf(subj_name, sizeof(subj_name), "%s", gauntlet_level_name(lvl));
        }
        char opp_label[48];
        if (vs_sf) {
            if (anchor > g->sf_max_elo) {
                snprintf(opp_label, sizeof(opp_label), "SF Max");
            } else {
                snprintf(opp_label, sizeof(opp_label), "SF %d", anchor);
            }
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
        if (az) {
            uci_new_game(az);
        }
        if (hyb) {
            uci_new_game(hyb);
        }
        if (apex) {
            uci_new_game(apex);
        }
        if (apex_tb) {
            uci_new_game(apex_tb);
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

        int max_plies = 300;
        bool aborted = false;
        int adjudicated = -1; // -1 = none, 0 = Black win, 1 = Draw, 2 = White win
        int win_w_streak = 0;
        int win_b_streak = 0;
        int draw_streak = 0;
        int last_eval_w_by_side[2] = { 0, 0 };

        while (!game->is_game_over && (int) game->move_count < max_plies) {
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
                if (is_gauntlet_az_level(lvl) && az) {
                    uci_search_result ur = uci_search(az, game, false, 0, 750);
                    m = ur.base.best_move;
                    eval_w = engine_score_to_white(game->game, ur.base.score_cp);
                } else if (is_gauntlet_hybrid_gpu_level(lvl) && hyb) {
                    uci_search_result ur = uci_search(hyb, game, false, 0, 900);
                    m = ur.base.best_move;
                    eval_w = engine_score_to_white(game->game, ur.base.score_cp);
                } else if ((is_gauntlet_apex_level(lvl) || lvl == GAUNTLET_HYBRID_NNUE_LEVEL) && apex) {
                    int pc_cnt = (apex_tb && apex_syz_pieces >= 3) ? syzygy_piece_count(game->game) : 99;
                    uci_search_result ur = (apex_tb && pc_cnt <= apex_syz_pieces)
                                               ? uci_search_depth(apex_tb, game, 6, 150)
                                               : uci_search(apex, game, false, 0, sf_ms);
                    m = ur.base.best_move;
                    eval_w = engine_score_to_white(game->game, ur.base.score_cp);
                } else if (lvl == 0 && celo >= 800) {
                    search_result sr = engine_search_elo(c_eng, game, celo);
                    m = sr.best_move;
                    eval_w = engine_score_to_white(game->game, sr.score_cp);
                } else {
                    search_result sr = engine_search_level(c_eng, game, lvl);
                    m = sr.best_move;
                    eval_w = engine_score_to_white(game->game, sr.score_cp);
                }
            } else if (vs_sf && sf) {
                bool lim = (anchor <= g->sf_max_elo);
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

            // Two-sided resign & draw adjudication (disabled early for Zero-C so endgame skill is truly tested!)
            last_eval_w_by_side[black_turn ? 1 : 0] = eval_w;
            int resign_thresh
                = (is_gauntlet_az_level(lvl) || is_gauntlet_hybrid_gpu_level(lvl)) ? 900 : 650;
            if (eval_w >= resign_thresh) {
                win_w_streak++;
                win_b_streak = 0;
                draw_streak = 0;
            } else if (eval_w <= -resign_thresh) {
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

            if (win_w_streak >= 8) {
                adjudicated = 2;
                break;
            }
            if (win_b_streak >= 8) {
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
            g->status.completed_games_total++;
            if (is_calc) {
                elo_calc_status *cs = &g->status.calc;
                cs->games_done++;
                if (subj_res > 0.75) {
                    cs->wins++;
                } else if (subj_res < 0.25) {
                    cs->losses++;
                } else {
                    cs->draws++;
                }
                if (g->calc_game_count < GAUNTLET_MAX_CALC_GAMES) {
                    g->calc_games[g->calc_game_count].anchor_elo = anchor;
                    g->calc_games[g->calc_game_count].score = subj_res;
                    g->calc_game_count++;
                }
                record_calc_bucket(cs, anchor, subj_res);
                int pslot = gauntlet_level_to_slot(lvl);
                int prior = (pslot >= 0) ? g_sf_anchors[pslot] : (celo >= 800 ? celo : 1600);
                if (is_gauntlet_apex_level(lvl)) {
                    prior = (access(NNUE_V4_FILE, R_OK) == 0) ? 2600 : 2450;
                }
                compute_mle_elo_ci(g->calc_games, g->calc_game_count, prior, &cs->est_elo,
                    &cs->ci_half, &cs->ci_low, &cs->ci_high);
                cs->hit_sf_floor = (cs->est_elo < g->sf_min_elo);
                if (cs->games_done >= 12 && cs->ci_half <= cs->target_ci) {
                    cs->converged = true;
                }
            } else {
                int slot = gauntlet_level_to_slot(lvl);
                if (slot >= 0) {
                    level_progress *lp = &g->status.levels[slot];
                    lp->done_games++;
                    if (subj_res > 0.75) {
                        lp->wins++;
                    } else if (subj_res < 0.25) {
                        lp->losses++;
                    } else {
                        lp->draws++;
                    }
                    double score
                        = ((double) lp->wins + 0.5 * (double) lp->draws) / (double) lp->done_games;
                    double diff = score_to_elo_diff(score);
                    if (vs_sf) {
                        lp->est_elo = (int) lround((double) anchor + diff);
                        lp->is_relative = false;
                    } else {
                        int base_elo = (slot > 0 && g->status.levels[slot - 1].has_elo)
                                           ? g->status.levels[slot - 1].est_elo
                                           : 0;
                        lp->est_elo = base_elo + (int) lround(diff);
                        lp->is_relative = true;
                    }
                    lp->has_elo = true;
                }
            }
            pthread_mutex_unlock(&g->mu);
        }
    }

    uci_delete(&apex_tb);
    uci_delete(&apex);
    uci_delete(&hyb);
    uci_delete(&az);
    uci_delete(&sf);
    engine_delete(&c_eng);
    return NULL;
}

static int clamp_calc_anchor(const gauntlet *g, int target_elo) {
    int sf_min = (g->sf_min_elo > 0) ? g->sf_min_elo : 1350;
    int sf_max = (g->sf_max_elo >= sf_min) ? g->sf_max_elo : 2850;
    // Never send an unsupported UCI_Elo above sf_max (Stockfish 14.1 rejects >2850 back to 1350!):
    // if estimated Elo exceeds sf_max + 75, step up to unthrottled Stockfish Max (anchor 3500).
    if (target_elo > sf_max + 75) {
        return 3500;
    }
    int a = ((target_elo + 12) / 25) * 25;
    if (a < sf_min) {
        a = sf_min;
    }
    if (a > sf_max) {
        a = sf_max;
    }
    return a;
}

static void *coordinator_calc_main(void *arg) {
    gauntlet *g = (gauntlet *) arg;
    int lvl = g->cfg_calc_level;
    int celo = g->cfg_calc_custom_elo;
    int max_g = g->cfg_calc_max_games;
    int target_ci = g->cfg_calc_target_ci;

    // Initial Stockfish anchor guess (strictly within Stockfish's true UCI_Elo range!)
    int cur_anchor = 1650;
    int init_slot = gauntlet_level_to_slot(lvl);
    if (init_slot >= 0) {
        if (g->saved_elos[init_slot].has_elo && !g->saved_elos[init_slot].is_relative
            && g->saved_elos[init_slot].est_elo >= g->sf_min_elo) {
            cur_anchor = g->saved_elos[init_slot].est_elo;
        } else {
            cur_anchor = g_sf_anchors[init_slot];
        }
    } else if (celo >= 800) {
        cur_anchor = celo;
    }
    cur_anchor = clamp_calc_anchor(g, cur_anchor);

    int done = 0;
    while (done < max_g) {
        pthread_mutex_lock(&g->mu);
        bool stop = g->stop_requested;
        bool conv = (g->status.calc.games_done >= 12 && g->status.calc.ci_half <= target_ci);
        pthread_mutex_unlock(&g->mu);
        if (stop || conv) {
            break;
        }

        int batch = 4; // 2 White + 2 Black color-balanced adaptive batch
        if (done + batch > max_g) {
            batch = max_g - done;
        }
        int nthreads = g->cfg_threads;
        if ((is_gauntlet_az_level(lvl) || is_gauntlet_hybrid_gpu_level(lvl)) && nthreads > 2) {
            nthreads = 2; // Cap GPU worker processes to 2
        }
        if (nthreads > batch) {
            nthreads = batch;
        }
        if (nthreads < 1) {
            nthreads = 1;
        }

        pthread_mutex_lock(&g->mu);
        g->active_level = lvl;
        g->active_custom_elo = celo;
        g->active_vs_sf = true;
        g->active_anchor = cur_anchor;
        g->batch_game_start = done;
        g->batch_game_end = done + batch;
        g->next_game_idx = done;
        g->status.calc.current_anchor = cur_anchor;
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

        done += batch;

        // Adapt Stockfish anchor right onto our latest MLE Elo estimate & persist live batch progress!
        pthread_mutex_lock(&g->mu);
        cur_anchor = clamp_calc_anchor(g, g->status.calc.est_elo);
        int batch_slot = gauntlet_level_to_slot(lvl);
        if (g->status.calc.games_done > 0 && batch_slot >= 0) {
            elo_calc_status *bcs = &g->status.calc;
            elo_record *brec = &g->saved_elos[batch_slot];
            brec->level = gauntlet_slot_to_level(batch_slot);
            brec->est_elo = bcs->est_elo;
            brec->anchor_elo = bcs->current_anchor;
            brec->wins = bcs->wins;
            brec->draws = bcs->draws;
            brec->losses = bcs->losses;
            brec->games = bcs->games_done;
            brec->has_elo = true;
            brec->is_relative = false;
            level_progress *blp = &g->status.levels[batch_slot];
            blp->done_games = bcs->games_done;
            blp->total_games = bcs->max_games;
            blp->wins = bcs->wins;
            blp->draws = bcs->draws;
            blp->losses = bcs->losses;
            blp->anchor_elo = bcs->current_anchor;
            blp->est_elo = bcs->est_elo;
            blp->has_elo = true;
            blp->is_relative = false;
            elo_save_file(g->elo_file, g->saved_elos, g->status.opponent_name, true);
        }
        pthread_mutex_unlock(&g->mu);
    }

    // Save pinned Elo back into elo_ratings.json for any calibrated bot
    pthread_mutex_lock(&g->mu);
    int save_slot = gauntlet_level_to_slot(lvl);
    if (g->status.calc.games_done > 0 && save_slot >= 0) {
        elo_calc_status *cs = &g->status.calc;
        elo_record *rec = &g->saved_elos[save_slot];
        rec->level = gauntlet_slot_to_level(save_slot);
        rec->est_elo = cs->est_elo;
        rec->anchor_elo = cs->current_anchor;
        rec->wins = cs->wins;
        rec->draws = cs->draws;
        rec->losses = cs->losses;
        rec->games = cs->games_done;
        rec->has_elo = true;
        rec->is_relative = false;

        level_progress *lp = &g->status.levels[save_slot];
        lp->state = 2;
        lp->done_games = cs->games_done;
        lp->total_games = cs->games_done;
        lp->wins = cs->wins;
        lp->draws = cs->draws;
        lp->losses = cs->losses;
        lp->anchor_elo = cs->current_anchor;
        lp->est_elo = cs->est_elo;
        lp->has_elo = true;
        lp->is_relative = false;
        snprintf(lp->opponent_label, sizeof(lp->opponent_label), "Adaptive SF (±%d)", cs->ci_half);

        elo_save_file(g->elo_file, g->saved_elos, g->sf_name, true);
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        if (tm_info) {
            strftime(
                g->status.updated_at, sizeof(g->status.updated_at), "%Y-%m-%d %H:%M:%S", tm_info);
        }
    }
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

static void *coordinator_main(void *arg) {
    gauntlet *g = (gauntlet *) arg;

    bool vs_sf = g->cfg_use_sf && g->sf_available;
    int only_slot = (g->cfg_only_level > 0) ? gauntlet_level_to_slot(g->cfg_only_level) : -1;
    int start_s = (only_slot >= 0) ? only_slot : (vs_sf ? 0 : 1);
    int end_s = (only_slot >= 0) ? only_slot : (GAUNTLET_NUM_LEVELS - 1);

    for (int slot = start_s; slot <= end_s; slot++) {
        int lvl = gauntlet_slot_to_level(slot);
        if (!gauntlet_level_available(g, lvl)) {
            pthread_mutex_lock(&g->mu);
            g->status.levels[slot].state = 3; // locked / unavailable
            pthread_mutex_unlock(&g->mu);
            continue;
        }
        pthread_mutex_lock(&g->mu);
        if (g->stop_requested) {
            pthread_mutex_unlock(&g->mu);
            break;
        }
        g->active_level = lvl;
        g->active_custom_elo = 0;
        g->active_vs_sf = vs_sf;
        g->active_anchor = vs_sf ? g_sf_anchors[slot]
                                 : ((lvl >= 1 && lvl <= 6) ? ((lvl > 1) ? (lvl - 1) : 1) : 6);
        if (vs_sf && g->active_anchor > g->sf_max_elo) {
            g->active_anchor = g->sf_max_elo;
        }
        g->batch_game_start = 0;
        g->batch_game_end = g->cfg_games;
        g->next_game_idx = 0;

        level_progress *lp = &g->status.levels[slot];
        lp->state = 1; // running
        lp->done_games = 0;
        lp->total_games = g->cfg_games;
        lp->wins = 0;
        lp->draws = 0;
        lp->losses = 0;
        lp->anchor_elo = g->active_anchor;
        lp->is_relative = !vs_sf;
        int nthreads = g->cfg_threads;
        if ((is_gauntlet_az_level(lvl) || is_gauntlet_hybrid_gpu_level(lvl)) && nthreads > 2) {
            nthreads = 2;
        }
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
            elo_record *rec = &g->saved_elos[slot];
            rec->level = lvl;
            rec->est_elo = lp->est_elo;
            rec->anchor_elo = lp->anchor_elo;
            rec->wins = lp->wins;
            rec->draws = lp->draws;
            rec->losses = lp->losses;
            rec->games = lp->done_games;
            rec->has_elo = true;
            rec->is_relative = lp->is_relative;

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

    g->sf_min_elo = 1350;
    g->sf_max_elo = 2850;
    if (uci_find_stockfish(sf_path, g->sf_path, sizeof(g->sf_path))) {
        uci_engine *probe = uci_create(g->sf_path);
        if (probe) {
            g->sf_available = true;
            g->sf_min_elo = uci_get_min_elo(probe);
            g->sf_max_elo = uci_get_max_elo(probe);
            snprintf(g->sf_name, sizeof(g->sf_name), "%s", uci_get_name(probe));
            uci_delete(&probe);
        }
    }
    g->az_available = detect_zero_c_available();

    g->status.cpu_cores = gauntlet_cpu_count();
    g->status.vs_stockfish = g->sf_available;
    g->status.az_available = g->az_available;

    char saved_opp[96] = "", saved_upd[32] = "";
    elo_load_file(
        g->elo_file, g->saved_elos, saved_opp, sizeof(saved_opp), saved_upd, sizeof(saved_upd));
    snprintf(g->status.opponent_name, sizeof(g->status.opponent_name), "%s",
        saved_opp[0] ? saved_opp : (g->sf_available ? g->sf_name : "Internal Ladder"));
    snprintf(g->status.updated_at, sizeof(g->status.updated_at), "%s", saved_upd);

    for (int i = 0; i < GAUNTLET_NUM_LEVELS; i++) {
        int lvl = gauntlet_slot_to_level(i);
        level_progress *lp = &g->status.levels[i];
        lp->level = lvl;
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
            int opp = (lvl >= 1 && lvl <= 6) ? (lvl - 1) : 6;
            snprintf(lp->opponent_label, sizeof(lp->opponent_label), "vs %s",
                gauntlet_level_name(opp));
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

    g->az_available = detect_zero_c_available();
    g->status.az_available = g->az_available;
    int only_slot = (only_level > 0) ? gauntlet_level_to_slot(only_level) : -1;
    if (only_level > 0 && (only_slot < 0 || !gauntlet_level_available(g, only_level))) {
        pthread_mutex_unlock(&g->mu);
        return false;
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
    g->cfg_calc_mode = false;
    g->cfg_calc_az_ckpt[0] = '\0';
    g->cfg_games = games_per_level;
    g->cfg_threads = num_threads;
    g->cfg_only_level = (only_slot >= 0) ? gauntlet_slot_to_level(only_slot) : 0;
    g->cfg_use_sf = vs_sf;
    g->cfg_sf_ms = (sf_time_ms > 20) ? sf_time_ms : 500;

    g->running = true;
    g->stop_requested = false;
    g->start_wall = now_wall();
    g->end_wall = g->start_wall;

    g->status.running = true;
    g->status.calc_mode = false;
    g->status.calc.active = false;
    g->status.stop_requested = false;
    g->status.vs_stockfish = vs_sf;
    g->status.num_threads = num_threads;
    g->status.games_per_level = games_per_level;
    g->status.completed_games_total = 0;
    g->status.elapsed_sec = 0.0;
    snprintf(g->status.opponent_name, sizeof(g->status.opponent_name), "%s",
        vs_sf ? g->sf_name : "Internal Ladder");

    int start_s = (only_slot >= 0) ? only_slot : (vs_sf ? 0 : 1);
    int end_s = (only_slot >= 0) ? only_slot : (GAUNTLET_NUM_LEVELS - 1);
    int scheduled = 0;

    for (int i = 0; i < GAUNTLET_NUM_LEVELS; i++) {
        int lvl = gauntlet_slot_to_level(i);
        level_progress *lp = &g->status.levels[i];
        lp->level = lvl;
        if (i >= start_s && i <= end_s && gauntlet_level_available(g, lvl)) {
            lp->state = 0;
            lp->done_games = 0;
            lp->total_games = games_per_level;
            lp->wins = 0;
            lp->draws = 0;
            lp->losses = 0;
            lp->anchor_elo = vs_sf ? g_sf_anchors[i]
                                   : ((lvl >= 1 && lvl <= 6) ? ((lvl > 1) ? (lvl - 1) : 1) : 6);
            if (vs_sf && lp->anchor_elo > g->sf_max_elo) {
                lp->anchor_elo = g->sf_max_elo;
            }
            lp->is_relative = !vs_sf;
            if (vs_sf) {
                snprintf(
                    lp->opponent_label, sizeof(lp->opponent_label), "vs SF %d", lp->anchor_elo);
            } else {
                snprintf(lp->opponent_label, sizeof(lp->opponent_label), "vs %s",
                    gauntlet_level_name(lp->anchor_elo));
            }
            scheduled += games_per_level;
        } else if (!vs_sf && i == 0) {
            lp->state = 2;
            lp->est_elo = 0;
            lp->has_elo = true;
            lp->is_relative = true;
            snprintf(lp->opponent_label, sizeof(lp->opponent_label), "Baseline");
        } else if (i >= start_s && i <= end_s) {
            lp->state = 3; // skipped (needs compute / unlock)
        }
    }
    g->status.scheduled_games_total = scheduled;
    for (int t = 0; t < MAX_GAUNTLET_THREADS; t++) {
        g->status.workers[t].active = false;
    }

    chess dummy;
    chess_reset(&dummy);
    (void) engine_hash_board(&dummy);

    g->has_coord = true;
    pthread_create(&g->coord_thread, NULL, coordinator_main, g);
    pthread_mutex_unlock(&g->mu);
    return true;
}

bool gauntlet_start_calculator(gauntlet *g, int subject_level, int custom_elo,
    const char *az_checkpoint, int target_ci, int max_games, int num_threads, int sf_time_ms) {
    if (!g || !g->sf_available) {
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

    g->az_available = detect_zero_c_available();
    g->status.az_available = g->az_available;
    if (subject_level == 12) {
        subject_level = GAUNTLET_ZERO_C_LEVEL;
    }
    if (subject_level != 0 && !gauntlet_level_available(g, subject_level)) {
        pthread_mutex_unlock(&g->mu);
        return false;
    }
    if (subject_level != 0 && gauntlet_level_to_slot(subject_level) < 0) {
        subject_level = 6;
    }
    if (max_games < 8) {
        max_games = 8;
    }
    if (max_games > GAUNTLET_MAX_CALC_GAMES) {
        max_games = GAUNTLET_MAX_CALC_GAMES;
    }
    if (target_ci < 25) {
        target_ci = 25;
    }
    if (target_ci > 200) {
        target_ci = 200;
    }

    int max_cores = gauntlet_cpu_count();
    if (num_threads <= 0) {
        num_threads = max_cores;
    }
    if (num_threads > MAX_GAUNTLET_THREADS) {
        num_threads = MAX_GAUNTLET_THREADS;
    }

    if (custom_elo > 2600) {
        custom_elo = 2600;
    }
    g->cfg_calc_mode = true;
    g->cfg_calc_level = subject_level;
    g->cfg_calc_custom_elo = (subject_level == 0 && custom_elo >= 800) ? custom_elo : 0;
    snprintf(g->cfg_calc_az_ckpt, sizeof(g->cfg_calc_az_ckpt), "%s",
        (az_checkpoint && az_checkpoint[0]) ? az_checkpoint : "");
    g->cfg_calc_target_ci = target_ci;
    g->cfg_calc_max_games = max_games;
    g->cfg_threads = num_threads;
    g->cfg_use_sf = true;
    g->cfg_sf_ms = (sf_time_ms > 20) ? sf_time_ms : 450;
    g->calc_game_count = 0;

    g->running = true;
    g->stop_requested = false;
    g->start_wall = now_wall();
    g->end_wall = g->start_wall;

    g->status.running = true;
    g->status.calc_mode = true;
    g->status.vs_stockfish = true;
    g->status.stop_requested = false;
    bool cap_gpu = is_gauntlet_az_level(subject_level) || is_gauntlet_hybrid_gpu_level(subject_level);
    g->status.num_threads = (cap_gpu && num_threads > 2) ? 2 : num_threads;
    g->status.games_per_level = max_games;
    g->status.completed_games_total = 0;
    g->status.scheduled_games_total = max_games;
    g->status.elapsed_sec = 0.0;
    snprintf(g->status.opponent_name, sizeof(g->status.opponent_name), "%s (Adaptive)", g->sf_name);

    elo_calc_status *cs = &g->status.calc;
    memset(cs, 0, sizeof(*cs));
    cs->active = true;
    cs->subject_level = subject_level;
    cs->subject_custom_elo = g->cfg_calc_custom_elo;
    cs->sf_min_elo = g->sf_min_elo;
    cs->sf_max_elo = g->sf_max_elo;
    snprintf(cs->az_checkpoint, sizeof(cs->az_checkpoint), "%s", g->cfg_calc_az_ckpt);
    if (subject_level == GAUNTLET_ZERO_C_LEVEL && g->cfg_calc_az_ckpt[0] != '\0'
        && strstr(g->cfg_calc_az_ckpt, "az_v1") != NULL) {
        snprintf(cs->subject_label, sizeof(cs->subject_label), "Zero-C v1 Champion (Gen 4320 • Max)");
    } else if (subject_level == GAUNTLET_ZERO_C_LEVEL && g->cfg_calc_az_ckpt[0] != '\0'
               && strstr(g->cfg_calc_az_ckpt, "az_toy") != NULL) {
        snprintf(cs->subject_label, sizeof(cs->subject_label), "Zero-C Lite (Toy Net)");
    } else if (subject_level == GAUNTLET_APEX_PURE_LEVEL) {
        snprintf(cs->subject_label, sizeof(cs->subject_label),
            (access(NNUE_V4_FILE, R_OK) == 0) ? "Apex-C 3000 (Pure • 1024 NNUE)"
                                              : "Apex-C (Pure Prototype)");
    } else if (subject_level == GAUNTLET_APEX_SYZYGY_LEVEL) {
        snprintf(cs->subject_label, sizeof(cs->subject_label),
            (access(NNUE_V4_FILE, R_OK) == 0) ? "Apex-C 3000 + Syzygy (5-Man TB)"
                                              : "Apex-C + Syzygy (Prototype)");
    } else if (subject_level == 0 && g->cfg_calc_custom_elo >= 800) {
        snprintf(cs->subject_label, sizeof(cs->subject_label), "Titan-C (~%d Elo)",
            g->cfg_calc_custom_elo);
    } else {
        snprintf(cs->subject_label, sizeof(cs->subject_label), "%s",
            gauntlet_level_name(subject_level));
    }
    cs->target_ci = target_ci;
    cs->max_games = max_games;
    cs->ci_half = 400;

    for (int t = 0; t < MAX_GAUNTLET_THREADS; t++) {
        g->status.workers[t].active = false;
    }

    chess dummy;
    chess_reset(&dummy);
    (void) engine_hash_board(&dummy);

    g->has_coord = true;
    pthread_create(&g->coord_thread, NULL, coordinator_calc_main, g);
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
