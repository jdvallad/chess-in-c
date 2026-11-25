#define _POSIX_C_SOURCE 200809L

#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/notation.h"
#include "core/primitives.h"
#include "engine/book.h"
#include "engine/engine.h"
#include "engine/nnue.h"
#include "engine/syzygy.h"
#include "features/gauntlet.h"
#include "features/history.h"
#include "features/puzzles.h"
#include "features/review.h"
#include "features/uci_client.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define AZ_BOT_LEVEL        12
#define AZ_V1_BOT_LEVEL     13
#define HYBRID_DUAL_LEVEL   14
#define HYBRID_LMR_LEVEL    15
#define HYBRID_OMEGA_LEVEL  16
#define HYBRID_NNUE_LEVEL   17
#define APEX_PURE_LEVEL     18
#define APEX_SYZYGY_LEVEL   19
#define AZ_V1_CKPT_PATH     "data/az_v1_gen4320/latest.pt"
#define HYBRID_UNLOCK_FLAG  "data/az/v2_complete.flag"
#define HYBRID_NNUE_FILE    "data/chess_net_zero_v4.nnue"

typedef enum web_mode {
    WMODE_WHITE = 1,
    WMODE_BLACK = 2,
    WMODE_2P = 3,
    WMODE_WATCH = 4,
    WMODE_ANALYSIS = 5,
    WMODE_PUZZLE = 6,
} web_mode;

typedef struct app_state {
    full_chess *game;
    engine *eng;
    uci_engine *sf;
    uci_engine *sf_ref; // Dedicated full-strength Stockfish objective referee
    uci_engine *az_uci;
    uci_engine *az_v1_uci; // Dedicated max-strength handle for immortalized Zero-C v1 Champion
    uci_engine *hybrid_uci; // Mythic Titan-Zero hybrid boss engine (unlocks after Zero-C v2)
    char hybrid_last_mode[32];
    uci_engine *apex_uci; // Project 3000 Apex-C (Upgraded Search + Big NNUE v4 when trained)
    bool apex_loaded_v4;
    uci_engine *sf_tb; // Syzygy 5-piece root oracle (only active when data/syzygy/ is installed)
    pid_t az_pid;
    bool az_running;
    pid_t distill_pid;
    bool distill_running;
    pid_t dg_pid;
    bool dg_running;
    pid_t spsa_pid;
    bool spsa_running;
    pid_t nnue_pid;
    bool nnue_running;
    pid_t syz_pid;
    bool syz_running;
    pid_t toy_pid;
    bool toy_running;
    char az_checkpoint[256];
    char az_last_ckpt[256];
    gauntlet *gt;
    game_history hist;
    int current_hist_id;
    bool auto_saved_game;
    puzzle_stats pstats;
    int active_puzzle_id;
    int puzzle_step;
    bool puzzle_failed;
    bool puzzle_solved;
    web_mode mode;
    int white_level;
    int black_level;
    int custom_elo; // 0 = preset level, 800..2600 = calibrated custom Elo slider
    int eval_white_cp;
    u64 book_rng;
    u64 mainline_moves[MAX_GAME_LENGTH];
    char mainline_san[MAX_GAME_LENGTH][16];
    int mainline_len;
    char white_name[64];
    char black_name[64];
    bool custom_names;
} app_state;

static volatile sig_atomic_t g_running = 1;
static int g_server_fd = -1;

static void handle_sigint(int sig) {
    (void) sig;
    g_running = 0;
    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }
}

// ============================================================================
// Dynamic String Buffer for JSON Responses
// ============================================================================

typedef struct strbuf {
    char *data;
    size_t len;
    size_t cap;
} strbuf;

static void sb_init(strbuf *sb, size_t initial_cap) {
    sb->cap = (initial_cap > 64) ? initial_cap : 64;
    sb->data = (char *) malloc(sb->cap);
    sb->len = 0;
    if (sb->data) {
        sb->data[0] = '\0';
    }
}

static void sb_free(strbuf *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void sb_ensure(strbuf *sb, size_t extra) {
    if (sb->len + extra + 1 <= sb->cap) {
        return;
    }
    size_t new_cap = sb->cap * 2;
    while (new_cap < sb->len + extra + 1) {
        new_cap *= 2;
    }
    char *nd = (char *) realloc(sb->data, new_cap);
    if (nd) {
        sb->data = nd;
        sb->cap = new_cap;
    }
}

static void sb_append_n(strbuf *sb, const char *s, size_t n) {
    sb_ensure(sb, n);
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void sb_append(strbuf *sb, const char *s) {
    sb_append_n(sb, s, strlen(s));
}

static void sb_printf(strbuf *sb, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed <= 0) {
        return;
    }
    sb_ensure(sb, (size_t) needed);
    va_start(ap, fmt);
    vsnprintf(sb->data + sb->len, (size_t) needed + 1, fmt, ap);
    va_end(ap);
    sb->len += (size_t) needed;
}

static void sb_json_str(strbuf *sb, const char *s) {
    sb_append(sb, "\"");
    if (s) {
        for (const unsigned char *p = (const unsigned char *) s; *p; p++) {
            switch (*p) {
            case '"': sb_append(sb, "\\\""); break;
            case '\\': sb_append(sb, "\\\\"); break;
            case '\n': sb_append(sb, "\\n"); break;
            case '\r': sb_append(sb, "\\r"); break;
            case '\t': sb_append(sb, "\\t"); break;
            default:
                if (*p < 0x20) {
                    sb_printf(sb, "\\u%04x", (unsigned) *p);
                } else {
                    char c = (char) *p;
                    sb_append_n(sb, &c, 1);
                }
                break;
            }
        }
    }
    sb_append(sb, "\"");
}

// ============================================================================
// Minimal JSON Request Field Parsers
// ============================================================================

static bool json_get_string(const char *json, const char *key, char *out, size_t out_size) {
    if (!json || !key || !out || out_size == 0) {
        return false;
    }
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) {
        return false;
    }
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':') {
        p++;
    }
    if (*p != '"') {
        return false;
    }
    p++;
    size_t idx = 0;
    while (*p && *p != '"' && idx + 1 < out_size) {
        if (*p == '\\' && p[1] != '\0') {
            p++;
            switch (*p) {
            case 'n': out[idx++] = '\n'; break;
            case 'r': out[idx++] = '\r'; break;
            case 't': out[idx++] = '\t'; break;
            default: out[idx++] = *p; break;
            }
        } else {
            out[idx++] = *p;
        }
        p++;
    }
    out[idx] = '\0';
    return true;
}

static int json_get_int(const char *json, const char *key, int default_val) {
    if (!json || !key) {
        return default_val;
    }
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) {
        return default_val;
    }
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':') {
        p++;
    }
    if (*p != '-' && (*p < '0' || *p > '9')) {
        return default_val;
    }
    return atoi(p);
}

// ============================================================================
// Game State & Timeline Helpers
// ============================================================================

static int max_allowed_level(const app_state *st) {
    (void) st;
    return APEX_SYZYGY_LEVEL;
}

static bool apex_v4_nnue_ready(void) {
    return access(NNUE_V4_FILE, R_OK) == 0;
}

static bool apex_syzygy_ready(const app_state *st) {
    return st->sf != NULL && syzygy_installed_pieces() >= 3;
}

typedef struct {
    unsigned long long datagen_pos;
    bool datagen_done;
    bool spsa_done;
    bool nnue_v4_done;
    bool syzygy_done;
    int syzygy_pieces;
    int pure_stage;
    int pure_max;
    int pure_pct;
    int pure_elo;
    const char *pure_tier;
    int syz_stage;
    int syz_max;
    int syz_pct;
    int syz_elo;
    const char *syz_tier;
} apex_progress_info;

static apex_progress_info apex_get_progress(const app_state *st) {
    apex_progress_info ap;
    memset(&ap, 0, sizeof(ap));

    struct stat dgst;
    if (stat("data/datagen_v4.bin", &dgst) == 0 && dgst.st_size > 0) {
        ap.datagen_pos = (unsigned long long) dgst.st_size / 36ull;
    }
    ap.datagen_done = (ap.datagen_pos >= 25000000ull);
    ap.spsa_done = (access("data/spsa_best.json", R_OK) == 0);
    ap.nnue_v4_done = apex_v4_nnue_ready();
    ap.syzygy_pieces = syzygy_installed_pieces();
    ap.syzygy_done = apex_syzygy_ready(st);

    double dg_frac = (double) ap.datagen_pos / 25000000.0;
    if (dg_frac > 1.0) {
        dg_frac = 1.0;
    }

    // Pure Apex-C: 4 tasks (Base C11 Search + Datagen 25M + SPSA + 1024 NNUE v4)
    ap.pure_max = 4;
    ap.pure_stage = 1 + (ap.datagen_done ? 1 : 0) + (ap.spsa_done ? 1 : 0) + (ap.nnue_v4_done ? 1 : 0);
    ap.pure_pct = (int) (15.0 + 30.0 * dg_frac + (ap.spsa_done ? 15.0 : 0.0) + (ap.nnue_v4_done ? 40.0 : 0.0) + 0.5);
    if (ap.pure_pct > 100) {
        ap.pure_pct = 100;
    }
    ap.pure_elo = 2580 + (ap.spsa_done ? 60 : 0) + (ap.nnue_v4_done ? 410 : 0);
    if (ap.pure_stage >= 4) {
        ap.pure_tier = "APEX 3000";
    } else if (ap.nnue_v4_done) {
        ap.pure_tier = "v4 Awakened";
    } else if (ap.pure_stage == 3) {
        ap.pure_tier = "Forged";
    } else if (ap.pure_stage == 2) {
        ap.pure_tier = "Tempered";
    } else {
        ap.pure_tier = "Prototype";
    }

    // Apex-C + Syzygy: 5 tasks (Base C11 Search + 5-Man Syzygy + Datagen 25M + SPSA + 1024 NNUE v4)
    ap.syz_max = 5;
    ap.syz_stage = 1 + (ap.syzygy_done ? 1 : 0) + (ap.datagen_done ? 1 : 0) + (ap.spsa_done ? 1 : 0)
                   + (ap.nnue_v4_done ? 1 : 0);
    ap.syz_pct = (int) (12.0 + (ap.syzygy_done ? 18.0 : 0.0) + 24.0 * dg_frac + (ap.spsa_done ? 12.0 : 0.0)
                        + (ap.nnue_v4_done ? 34.0 : 0.0) + 0.5);
    if (ap.syz_pct > 100) {
        ap.syz_pct = 100;
    }
    ap.syz_elo = 2580 + (ap.syzygy_done ? 70 : 0) + (ap.spsa_done ? 60 : 0) + (ap.nnue_v4_done ? 410 : 0);
    if (!ap.syzygy_done) {
        ap.syz_tier = "Sealed (Needs TB)";
    } else if (ap.syz_stage >= 5) {
        ap.syz_tier = "GODMODE 3100";
    } else if (ap.nnue_v4_done) {
        ap.syz_tier = "v4 + TB Awakened";
    } else if (ap.syz_stage == 4) {
        ap.syz_tier = "Forged + TB";
    } else if (ap.syz_stage == 3) {
        ap.syz_tier = "Tempered + TB";
    } else {
        ap.syz_tier = "TB Prototype";
    }

    return ap;
}

static bool is_apex_level(int lvl) {
    return lvl == APEX_PURE_LEVEL || lvl == APEX_SYZYGY_LEVEL;
}

static bool hybrid_is_unlocked(void) {
    return (access(HYBRID_UNLOCK_FLAG, R_OK) == 0)
           && (access("src/az/hybrid_uci_run.sh", X_OK) == 0);
}

static bool hybrid_nnue_is_unlocked(void) {
    return hybrid_is_unlocked() && (access(HYBRID_NNUE_FILE, R_OK) == 0);
}

static bool hybrid_level_unlocked(int lvl) {
    if (lvl == HYBRID_NNUE_LEVEL) {
        return hybrid_nnue_is_unlocked();
    }
    return hybrid_is_unlocked();
}

static bool is_hybrid_level(int lvl) {
    return lvl >= HYBRID_DUAL_LEVEL && lvl <= HYBRID_NNUE_LEVEL;
}

static const char *hybrid_mode_for_level(int lvl) {
    switch (lvl) {
    case HYBRID_DUAL_LEVEL: return "dual_brain";
    case HYBRID_LMR_LEVEL: return "neural_lmr";
    case HYBRID_OMEGA_LEVEL: return "omega_mcts";
    case HYBRID_NNUE_LEVEL: return "distilled_nnue";
    default: return "dual_brain";
    }
}

static bool az_python_path(char *out, size_t out_sz) {
    if (access(".venv-az/bin/python3", X_OK) == 0) {
        snprintf(out, out_sz, ".venv-az/bin/python3");
        return true;
    }
    snprintf(out, out_sz, "python3");
    return false;
}

static bool az_is_available(void) {
    return (access("build/libaz_mcts.so", R_OK) == 0)
           && (access("src/az/az_uci_run.sh", X_OK) == 0);
}

static bool az_v1_is_available(void) {
    return az_is_available() && (access(AZ_V1_CKPT_PATH, R_OK) == 0);
}

static void az_check_alive(app_state *st) {
    if (st->az_running && st->az_pid > 0) {
        int status = 0;
        pid_t r = waitpid(st->az_pid, &status, WNOHANG);
        if (r == st->az_pid || r < 0) {
            st->az_pid = 0;
            st->az_running = false;
        }
    }
    if (st->distill_running && st->distill_pid > 0) {
        int status = 0;
        pid_t r = waitpid(st->distill_pid, &status, WNOHANG);
        if (r == st->distill_pid || r < 0) {
            st->distill_pid = 0;
            st->distill_running = false;
        }
    }
    if (st->dg_running && st->dg_pid > 0) {
        int status = 0;
        pid_t r = waitpid(st->dg_pid, &status, WNOHANG);
        if (r == st->dg_pid || r < 0) {
            st->dg_pid = 0;
            st->dg_running = false;
        }
    }
    if (st->spsa_running && st->spsa_pid > 0) {
        int status = 0;
        pid_t r = waitpid(st->spsa_pid, &status, WNOHANG);
        if (r == st->spsa_pid || r < 0) {
            st->spsa_pid = 0;
            st->spsa_running = false;
        }
    }
    if (st->nnue_running && st->nnue_pid > 0) {
        int status = 0;
        pid_t r = waitpid(st->nnue_pid, &status, WNOHANG);
        if (r == st->nnue_pid || r < 0) {
            st->nnue_pid = 0;
            st->nnue_running = false;
        }
    }
    if (st->syz_running && st->syz_pid > 0) {
        int status = 0;
        pid_t r = waitpid(st->syz_pid, &status, WNOHANG);
        if (r == st->syz_pid || r < 0) {
            st->syz_pid = 0;
            st->syz_running = false;
        }
    }
    if (st->toy_running && st->toy_pid > 0) {
        int status = 0;
        pid_t r = waitpid(st->toy_pid, &status, WNOHANG);
        if (r == st->toy_pid || r < 0) {
            st->toy_pid = 0;
            st->toy_running = false;
        }
    }
}

static void p3k_stop_child(pid_t *pid_p, bool *run_p) {
    if (*run_p && *pid_p > 0) {
        kill(*pid_p, SIGTERM);
        for (int i = 0; i < 60; i++) {
            int status = 0;
            if (waitpid(*pid_p, &status, WNOHANG) == *pid_p) {
                break;
            }
            struct timespec ts = { 0, 50000000L };
            nanosleep(&ts, NULL);
        }
        int status = 0;
        if (waitpid(*pid_p, &status, WNOHANG) == 0) {
            kill(*pid_p, SIGKILL);
            waitpid(*pid_p, &status, 0);
        }
        *pid_p = 0;
        *run_p = false;
    }
}

static bool p3k_start_datagen(app_state *st, int threads, unsigned long long target_pos, int nodes) {
    az_check_alive(st);
    if (st->dg_running) {
        return true;
    }
    if (access("build/datagen", X_OK) != 0) {
        return false;
    }
    mkdir("data", 0755);
    if (threads < 1) {
        threads = 1;
    }
    if (threads > 12) {
        threads = 12;
    }
    if (nodes < 1000) {
        nodes = 1000;
    }
    if (target_pos < 10000ull) {
        target_pos = 10000ull;
    }
    char th_s[16], pos_s[32], nd_s[16];
    snprintf(th_s, sizeof(th_s), "%d", threads);
    snprintf(pos_s, sizeof(pos_s), "%llu", target_pos);
    snprintf(nd_s, sizeof(nd_s), "%d", nodes);

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        (void) setpriority(PRIO_PROCESS, 0, 15);
        int logfd = open("data/p3k_datagen.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd >= 0) {
            dup2(logfd, STDOUT_FILENO);
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }
        char *argv[] = {
            "build/datagen",
            "--threads", th_s,
            "--positions", pos_s,
            "--nodes", nd_s,
            "--out", "data/datagen_v4.bin",
            "--status", "data/p3k_datagen_status.json",
            NULL,
        };
        execvp("build/datagen", argv);
        _exit(127);
    }
    st->dg_pid = pid;
    st->dg_running = true;
    return true;
}

static bool p3k_start_spsa(app_state *st, int threads, int iters, int nodes) {
    az_check_alive(st);
    if (st->spsa_running) {
        return true;
    }
    if (access("build/chess_uci", X_OK) != 0) {
        return false;
    }
    mkdir("data", 0755);
    if (threads < 1) {
        threads = 1;
    }
    if (threads > 10) {
        threads = 10;
    }
    if (iters < 5) {
        iters = 5;
    }
    if (nodes < 2000) {
        nodes = 2000;
    }
    char th_s[16], it_s[16], nd_s[16];
    snprintf(th_s, sizeof(th_s), "%d", threads);
    snprintf(it_s, sizeof(it_s), "%d", iters);
    snprintf(nd_s, sizeof(nd_s), "%d", nodes);

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        (void) setpriority(PRIO_PROCESS, 0, 15);
        int logfd = open("data/p3k_spsa.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd >= 0) {
            dup2(logfd, STDOUT_FILENO);
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }
        char *argv[] = {
            "python3",
            "src/tools/spsa_tuner.py",
            "--threads", th_s,
            "--iters", it_s,
            "--pairs-per-iter", "4",
            "--nodes", nd_s,
            "--out", "data/spsa_best.json",
            "--status", "data/p3k_spsa_status.json",
            NULL,
        };
        execvp("python3", argv);
        _exit(127);
    }
    st->spsa_pid = pid;
    st->spsa_running = true;
    return true;
}

static bool p3k_start_nnue(app_state *st, int epochs) {
    az_check_alive(st);
    if (st->nnue_running) {
        return true;
    }
    if (access("data/datagen_v4.bin", R_OK) != 0) {
        return false;
    }
    if (epochs < 4) {
        epochs = 4;
    }
    if (epochs > 60) {
        epochs = 60;
    }
    char py[256], ep_s[16];
    az_python_path(py, sizeof(py));
    snprintf(ep_s, sizeof(ep_s), "%d", epochs);

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        int logfd = open("data/p3k_nnue.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd >= 0) {
            dup2(logfd, STDOUT_FILENO);
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }
        char *argv[] = {
            py,
            "src/tools/train_big_nnue.py",
            "--data", "data/datagen_v4.bin",
            "--out", NNUE_V4_FILE,
            "--status", "data/p3k_nnue_status.json",
            "--hidden", "1024",
            "--epochs", ep_s,
            NULL,
        };
        execvp(py, argv);
        _exit(127);
    }
    st->nnue_pid = pid;
    st->nnue_running = true;
    return true;
}

static bool p3k_start_syzygy(app_state *st) {
    az_check_alive(st);
    if (st->syz_running) {
        return true;
    }
    if (access("scripts/download_syzygy5.sh", R_OK) != 0) {
        return false;
    }
    mkdir("data", 0755);
    mkdir(SYZYGY_DEFAULT_DIR, 0755);
    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        int logfd = open("data/p3k_syzygy.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd >= 0) {
            dup2(logfd, STDOUT_FILENO);
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }
        char *argv[] = { "bash", "scripts/download_syzygy5.sh", SYZYGY_DEFAULT_DIR, NULL };
        execvp("bash", argv);
        _exit(127);
    }
    st->syz_pid = pid;
    st->syz_running = true;
    return true;
}

static bool toy_start_training(app_state *st, const char *arch, bool reset) {
    az_check_alive(st);
    if (st->toy_running) {
        return true;
    }
    if (access("build/libaz_mcts.so", R_OK) != 0) {
        return false;
    }
    mkdir("data", 0755);
    mkdir("data/az_toy", 0755);
    char py[256];
    az_python_path(py, sizeof(py));
    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        int logfd = open("data/az_toy/trainer.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd >= 0) {
            dup2(logfd, STDOUT_FILENO);
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }
        char *argv[24];
        int ai = 0;
        argv[ai++] = py;
        argv[ai++] = "src/az/trainer.py";
        argv[ai++] = "--arch";
        argv[ai++] = (char *) arch;
        argv[ai++] = "--games";
        argv[ai++] = "256";
        argv[ai++] = "--fast-sims";
        argv[ai++] = "16";
        argv[ai++] = "--full-sims";
        argv[ai++] = "64";
        argv[ai++] = "--games-per-gen";
        argv[ai++] = "64";
        argv[ai++] = "--replay-size";
        argv[ai++] = "200000";
        argv[ai++] = "--data-dir";
        argv[ai++] = "data/az_toy";
        if (reset) {
            argv[ai++] = "--reset";
        }
        argv[ai] = NULL;
        execvp(py, argv);
        _exit(127);
    }
    st->toy_pid = pid;
    st->toy_running = true;
    return true;
}

static void append_raw_json_or_null(strbuf *sb, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (sz > 2 && sz < 65536) {
            char *buf = (char *) malloc((size_t) sz + 1);
            if (buf) {
                size_t n = fread(buf, 1, (size_t) sz, fp);
                buf[n] = '\0';
                fclose(fp);
                sb_append_n(sb, buf, n);
                free(buf);
                return;
            }
        }
        fclose(fp);
    }
    sb_append(sb, "null");
}

static void handle_toy_status_json(app_state *st, strbuf *sb) {
    az_check_alive(st);
    bool has_ckpt = (access("data/az_toy/latest.pt", R_OK) == 0);
    sb_printf(sb, "{\"ok\":true,\"running\":%s,\"az_running\":%s,\"has_ckpt\":%s,\"detail\":",
        st->toy_running ? "true" : "false", st->az_running ? "true" : "false",
        has_ckpt ? "true" : "false");
    append_raw_json_or_null(sb, "data/az_toy/status.json");
    sb_append(sb, "}");
}

static void handle_p3k_status_json(app_state *st, strbuf *sb) {
    az_check_alive(st);

    // Live scan of data/syzygy/ for 3-4-5 piece tablebase download progress (290 total files!)
    int syz_files = 0;
    int syz_wdl = 0;
    int syz_dtz = 0;
    unsigned long long syz_bytes = 0;
    DIR *sd = opendir(SYZYGY_DEFAULT_DIR);
    if (sd) {
        struct dirent *de;
        while ((de = readdir(sd)) != NULL) {
            const char *dot = strrchr(de->d_name, '.');
            if (!dot) {
                continue;
            }
            bool is_w = (strcmp(dot, ".rtbw") == 0);
            bool is_z = (strcmp(dot, ".rtbz") == 0);
            if (is_w || is_z) {
                syz_files++;
                if (is_w) {
                    syz_wdl++;
                } else {
                    syz_dtz++;
                }
                char fullp[512];
                snprintf(fullp, sizeof(fullp), "%s/%s", SYZYGY_DEFAULT_DIR, de->d_name);
                struct stat sst;
                if (stat(fullp, &sst) == 0 && sst.st_size > 0) {
                    syz_bytes += (unsigned long long) sst.st_size;
                }
            }
        }
        closedir(sd);
    }
    int syz_max_p = syzygy_installed_pieces();
    bool syz_ready = apex_syzygy_ready(st);

    // Exact live datagen file count from disk (36 bytes/position)
    unsigned long long dg_file_bytes = 0;
    unsigned long long dg_file_pos = 0;
    struct stat dgst;
    if (stat("data/datagen_v4.bin", &dgst) == 0 && dgst.st_size > 0) {
        dg_file_bytes = (unsigned long long) dgst.st_size;
        dg_file_pos = dg_file_bytes / 36ull;
    }

    sb_printf(sb,
        "{\"ok\":true,\"az_running\":%s,\"syzygy\":{\"files\":%d,\"wdl_files\":%d,\"dtz_files\":%d,"
        "\"target_files\":290,\"mb\":%.1f,\"max_pieces\":%d,\"ready\":%s,\"downloading\":%s},",
        st->az_running ? "true" : "false", syz_files, syz_wdl, syz_dtz,
        (double) syz_bytes / (1024.0 * 1024.0), syz_max_p, syz_ready ? "true" : "false",
        st->syz_running ? "true" : "false");

    sb_printf(sb,
        "\"datagen\":{\"running\":%s,\"file_positions\":%llu,\"file_mb\":%.1f,\"detail\":",
        st->dg_running ? "true" : "false", dg_file_pos,
        (double) dg_file_bytes / (1024.0 * 1024.0));
    append_raw_json_or_null(sb, "data/p3k_datagen_status.json");
    sb_append(sb, "},");

    sb_printf(sb, "\"spsa\":{\"running\":%s,\"detail\":", st->spsa_running ? "true" : "false");
    append_raw_json_or_null(sb, "data/p3k_spsa_status.json");
    sb_append(sb, "},");

    sb_printf(sb, "\"nnue_v4\":{\"running\":%s,\"ready\":%s,\"detail\":",
        st->nnue_running ? "true" : "false", apex_v4_nnue_ready() ? "true" : "false");
    append_raw_json_or_null(sb, "data/p3k_nnue_status.json");
    sb_append(sb, "}}");
}

static void distill_stop(app_state *st) {
    az_check_alive(st);
    if (st->distill_running && st->distill_pid > 0) {
        kill(st->distill_pid, SIGTERM);
        for (int i = 0; i < 60; i++) {
            int status = 0;
            if (waitpid(st->distill_pid, &status, WNOHANG) == st->distill_pid) {
                break;
            }
            struct timespec ts = { 0, 50000000L };
            nanosleep(&ts, NULL);
        }
        int status = 0;
        if (waitpid(st->distill_pid, &status, WNOHANG) == 0) {
            kill(st->distill_pid, SIGKILL);
            waitpid(st->distill_pid, &status, 0);
        }
        st->distill_pid = 0;
        st->distill_running = false;
    }
}

static bool distill_start(app_state *st) {
    az_check_alive(st);
    if (st->distill_running) {
        return true;
    }
    if (!hybrid_is_unlocked() || access("data/az/latest.pt", R_OK) != 0
        || access("data/az/replay.npz", R_OK) != 0) {
        return false;
    }
    char py[256];
    az_python_path(py, sizeof(py));
    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        int logfd = open("data/az/distill.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd >= 0) {
            dup2(logfd, STDOUT_FILENO);
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }
        char *argv[8];
        argv[0] = py;
        argv[1] = "src/az/distill_nnue.py";
        argv[2] = NULL;
        execvp(py, argv);
        _exit(127);
    }
    st->distill_pid = pid;
    st->distill_running = true;
    return true;
}

static void az_stop_training(app_state *st) {
    az_check_alive(st);
    if (st->az_running && st->az_pid > 0) {
        kill(st->az_pid, SIGTERM);
        for (int i = 0; i < 100; i++) {
            int status = 0;
            if (waitpid(st->az_pid, &status, WNOHANG) == st->az_pid) {
                break;
            }
            struct timespec ts = { 0, 50000000L };
            nanosleep(&ts, NULL);
        }
        int status = 0;
        if (waitpid(st->az_pid, &status, WNOHANG) == 0) {
            kill(st->az_pid, SIGKILL);
            waitpid(st->az_pid, &status, 0);
        }
        st->az_pid = 0;
        st->az_running = false;
    }
}

static bool az_start_training(app_state *st, const char *arch, int games, int fast_sims,
    int full_sims, int games_per_gen, int max_gens, bool reset, bool full_throttle) {
    az_check_alive(st);
    if (st->az_running) {
        return true;
    }
    if (access("build/libaz_mcts.so", R_OK) != 0) {
        return false;
    }
    mkdir("data", 0755);
    mkdir("data/az", 0755);

    char py[256];
    az_python_path(py, sizeof(py));

    char g_str[16], fs_str[16], full_str[16], gpg_str[16], mg_str[16];
    snprintf(g_str, sizeof(g_str), "%d", games);
    snprintf(fs_str, sizeof(fs_str), "%d", fast_sims);
    snprintf(full_str, sizeof(full_str), "%d", full_sims);
    snprintf(gpg_str, sizeof(gpg_str), "%d", games_per_gen);
    snprintf(mg_str, sizeof(mg_str), "%d", max_gens);

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        int logfd = open("data/az/trainer.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd >= 0) {
            dup2(logfd, STDOUT_FILENO);
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }
        char *argv[20];
        int ai = 0;
        argv[ai++] = py;
        argv[ai++] = "src/az/trainer.py";
        argv[ai++] = "--arch";
        argv[ai++] = (char *) arch;
        argv[ai++] = "--games";
        argv[ai++] = g_str;
        argv[ai++] = "--fast-sims";
        argv[ai++] = fs_str;
        argv[ai++] = "--full-sims";
        argv[ai++] = full_str;
        argv[ai++] = "--games-per-gen";
        argv[ai++] = gpg_str;
        argv[ai++] = "--max-gens";
        argv[ai++] = mg_str;
        if (full_throttle) {
            argv[ai++] = "--full-throttle";
        }
        if (reset) {
            argv[ai++] = "--reset";
        }
        argv[ai] = NULL;
        execvp(py, argv);
        _exit(127);
    }
    st->az_pid = pid;
    st->az_running = true;
    return true;
}

static void az_reset_files(app_state *st) {
    az_stop_training(st);
    unlink("data/az/latest.pt");
    unlink("data/az/status.json");
    unlink("data/az/replay.npz");
    DIR *d = opendir("data/az/checkpoints");
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (strncmp(de->d_name, "gen_", 4) == 0) {
                char p[512];
                snprintf(p, sizeof(p), "data/az/checkpoints/%s", de->d_name);
                unlink(p);
            }
        }
        closedir(d);
    }
}

static uci_engine *az_ensure_uci(app_state *st) {
    if (!az_is_available()) {
        return NULL;
    }
    if (!st->az_uci) {
        st->az_uci = uci_create("src/az/az_uci_run.sh");
        st->az_last_ckpt[0] = '\0';
    }
    if (st->az_uci) {
        const char *want = st->az_checkpoint[0] ? st->az_checkpoint : "data/az/latest.pt";
        if (strcmp(want, st->az_last_ckpt) != 0) {
            uci_set_option(st->az_uci, "Checkpoint", want);
            snprintf(st->az_last_ckpt, sizeof(st->az_last_ckpt), "%s", want);
        }
    }
    return st->az_uci;
}

static uci_engine *az_ensure_v1_uci(app_state *st) {
    if (!az_v1_is_available()) {
        return NULL;
    }
    if (!st->az_v1_uci) {
        st->az_v1_uci = uci_create("src/az/az_uci_run.sh");
        if (st->az_v1_uci) {
            uci_set_option(st->az_v1_uci, "Checkpoint", AZ_V1_CKPT_PATH);
            uci_set_option(st->az_v1_uci, "Simulations", "1000");
            uci_set_option(st->az_v1_uci, "TacticalHybrid", "true");
        }
    }
    return st->az_v1_uci;
}

static uci_engine *hybrid_ensure_uci(app_state *st, int lvl) {
    if (!hybrid_level_unlocked(lvl)) {
        return NULL;
    }
    if (!st->hybrid_uci) {
        st->hybrid_uci = uci_create("src/az/hybrid_uci_run.sh");
        st->hybrid_last_mode[0] = '\0';
    }
    if (st->hybrid_uci) {
        const char *mode = hybrid_mode_for_level(lvl);
        if (strcmp(mode, st->hybrid_last_mode) != 0) {
            uci_set_option(st->hybrid_uci, "HybridMode", mode);
            snprintf(st->hybrid_last_mode, sizeof(st->hybrid_last_mode), "%s", mode);
        }
    }
    return st->hybrid_uci;
}

static uci_engine *apex_ensure_uci(app_state *st) {
    if (access("build/chess_uci", X_OK) != 0) {
        return NULL;
    }
    if (!st->apex_uci) {
        st->apex_uci = uci_create("build/chess_uci");
        st->apex_loaded_v4 = false;
        if (st->apex_uci) {
            uci_set_option(st->apex_uci, "Threads", "4");
            uci_set_option(st->apex_uci, "Hash", "128");
        }
    }
    if (st->apex_uci && !st->apex_loaded_v4 && apex_v4_nnue_ready()) {
        uci_set_option(st->apex_uci, "EvalFile", NNUE_V4_FILE);
        st->apex_loaded_v4 = true;
    }
    return st->apex_uci;
}

static uci_engine *apex_ensure_tb_oracle(app_state *st) {
    if (!apex_syzygy_ready(st)) {
        return NULL;
    }
    if (!st->sf_tb) {
        st->sf_tb = uci_create(uci_get_path(st->sf));
        if (st->sf_tb) {
            uci_set_option(st->sf_tb, "Threads", "1");
            uci_set_option(st->sf_tb, "Hash", "16");
            uci_set_option(st->sf_tb, "SyzygyPath", SYZYGY_DEFAULT_DIR);
            uci_set_option(st->sf_tb, "SyzygyProbeDepth", "1");
        }
    }
    return st->sf_tb;
}

static void append_distill_status_field(const app_state *st, strbuf *sb) {
    sb_printf(sb, ",\"nnue_distilled\":%s,\"distill_running\":%s,\"distill\":",
        hybrid_nnue_is_unlocked() ? "true" : "false", st->distill_running ? "true" : "false");
    FILE *df = fopen("data/az/distill_status.json", "rb");
    if (df) {
        fseek(df, 0, SEEK_END);
        long dsz = ftell(df);
        fseek(df, 0, SEEK_SET);
        if (dsz > 2 && dsz < 65536) {
            char *dbuf = (char *) malloc((size_t) dsz + 1);
            if (dbuf) {
                size_t dn = fread(dbuf, 1, (size_t) dsz, df);
                dbuf[dn] = '\0';
                fclose(df);
                sb_append_n(sb, dbuf, dn);
                free(dbuf);
                return;
            }
        }
        fclose(df);
    }
    sb_append(sb, "null");
}

static void handle_az_status_json(app_state *st, strbuf *sb) {
    az_check_alive(st);
    bool lib_ok = (access("build/libaz_mcts.so", R_OK) == 0);
    bool venv_ok = (access(".venv-az/bin/python3", X_OK) == 0);
    bool has_latest = (access("data/az/latest.pt", R_OK) == 0);

    FILE *fp = fopen("data/az/status.json", "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (sz > 2 && sz < 512000) {
            char *buf = (char *) malloc((size_t) sz + 1);
            if (buf) {
                size_t n = fread(buf, 1, (size_t) sz, fp);
                buf[n] = '\0';
                fclose(fp);
                // Trim trailing '}' and append live process/env fields cleanly
                while (n > 0 && isspace((unsigned char) buf[n - 1])) {
                    buf[--n] = '\0';
                }
                if (n > 1 && buf[n - 1] == '}') {
                    buf[--n] = '\0';
                    sb_append_n(sb, buf, n);
                    sb_printf(sb,
                        ",\"proc_alive\":%s,\"lib_built\":%s,\"venv_ready\":%s,"
                        "\"has_latest\":%s,\"hybrids_unlocked\":%s",
                        st->az_running ? "true" : "false", lib_ok ? "true" : "false",
                        venv_ok ? "true" : "false", has_latest ? "true" : "false",
                        hybrid_is_unlocked() ? "true" : "false");
                    append_distill_status_field(st, sb);
                    sb_append(sb, "}");
                    free(buf);
                    return;
                }
                free(buf);
            } else {
                fclose(fp);
            }
        } else {
            fclose(fp);
        }
    }

    // No status.json yet -> return crisp ready idle state!
    sb_printf(sb,
        "{\"ok\":true,\"running\":%s,\"proc_alive\":%s,\"phase\":\"idle\","
        "\"message\":\"Ready — click Start Training to begin tabula rasa self-play.\","
        "\"device\":\"%s\",\"fp16\":true,\"vram_mb\":0,\"arch\":\"6x96 SE-ResNet\","
        "\"params\":1100000,\"gen\":0,\"gen_games\":0,\"games_per_gen\":128,"
        "\"total_games\":0,\"total_positions\":0,\"replay_size\":0,"
        "\"evals_per_sec\":0,\"games_per_hr\":0,\"elapsed_sec\":0,"
        "\"outcomes\":{\"white\":0,\"draw\":0,\"black\":0},\"history\":[],"
        "\"checkpoints\":[],\"live_games\":[],\"lib_built\":%s,\"venv_ready\":%s,"
        "\"has_latest\":%s,\"hybrids_unlocked\":%s",
        st->az_running ? "true" : "false", st->az_running ? "true" : "false",
        venv_ok ? "RTX GPU (PyTorch CUDA ready)" : "Run 'make az-setup' for PyTorch CUDA",
        lib_ok ? "true" : "false", venv_ok ? "true" : "false", has_latest ? "true" : "false",
        hybrid_is_unlocked() ? "true" : "false");
    append_distill_status_field(st, sb);
    sb_append(sb, "}");
}

static void format_bot_name(const app_state *st, int level, char *out, size_t out_size) {
    if (level == APEX_PURE_LEVEL) {
        apex_progress_info ap = apex_get_progress(st);
        snprintf(out, out_size, "Apex-C [Lv %d/%d • %s]", ap.pure_stage, ap.pure_max, ap.pure_tier);
    } else if (level == APEX_SYZYGY_LEVEL) {
        apex_progress_info ap = apex_get_progress(st);
        snprintf(out, out_size, "Apex-C + Syzygy [Lv %d/%d • %s]", ap.syz_stage, ap.syz_max, ap.syz_tier);
    } else if (level == HYBRID_DUAL_LEVEL) {
        snprintf(out, out_size, "Gemini Arbiter (Dual-Brain)");
    } else if (level == HYBRID_LMR_LEVEL) {
        snprintf(out, out_size, "Oracle of LMR (Neural Search)");
    } else if (level == HYBRID_OMEGA_LEVEL) {
        snprintf(out, out_size, "Zero-C Omega (Tactical MCTS)");
    } else if (level == HYBRID_NNUE_LEVEL) {
        snprintf(out, out_size, "Singularity v5 (Zero-NNUE)");
    } else if (level == AZ_V1_BOT_LEVEL) {
        snprintf(out, out_size, "Zero-C v1 Champion (Gen 4320)");
    } else if (level == AZ_BOT_LEVEL) {
        if (st->az_checkpoint[0] != '\0') {
            const char *base = strrchr(st->az_checkpoint, '/');
            base = base ? (base + 1) : st->az_checkpoint;
            if (strncmp(base, "gen_", 4) == 0) {
                snprintf(out, out_size, "Zero-C (Gen %d)", atoi(base + 4));
                return;
            }
        }
        snprintf(out, out_size, "Zero-C v2 (Latest)");
    } else if (st->sf && uci_is_stockfish_level(level)) {
        snprintf(out, out_size, "%s", uci_get_preset(level)->name);
    } else if (st->custom_elo >= 800) {
        snprintf(out, out_size, "Titan-C (~%d Elo)", st->custom_elo);
    } else {
        snprintf(out, out_size, "Titan-C (%s)", engine_get_preset(level)->name);
    }
}

static void update_player_names(app_state *st) {
    if (st->custom_names && st->mode == WMODE_ANALYSIS) {
        return;
    }
    if (st->mode == WMODE_WHITE) {
        snprintf(st->white_name, sizeof(st->white_name), "Human");
        format_bot_name(st, st->black_level, st->black_name, sizeof(st->black_name));
    } else if (st->mode == WMODE_BLACK) {
        format_bot_name(st, st->white_level, st->white_name, sizeof(st->white_name));
        snprintf(st->black_name, sizeof(st->black_name), "Human");
    } else if (st->mode == WMODE_2P) {
        snprintf(st->white_name, sizeof(st->white_name), "White");
        snprintf(st->black_name, sizeof(st->black_name), "Black");
    } else if (st->mode == WMODE_WATCH) {
        format_bot_name(st, st->white_level, st->white_name, sizeof(st->white_name));
        format_bot_name(st, st->black_level, st->black_name, sizeof(st->black_name));
    } else {
        if (st->white_name[0] == '\0') {
            snprintf(st->white_name, sizeof(st->white_name), "White");
        }
        if (st->black_name[0] == '\0') {
            snprintf(st->black_name, sizeof(st->black_name), "Black");
        }
    }
}

static bool state_is_bot_turn(const app_state *st) {
    if (st->game->is_game_over || st->mode == WMODE_ANALYSIS || st->mode == WMODE_2P
        || st->mode == WMODE_PUZZLE) {
        return false;
    }
    bool black_turn = is_flipped(st->game->game);
    if (st->mode == WMODE_WATCH) {
        return true;
    }
    if (st->mode == WMODE_WHITE) {
        return black_turn;
    }
    if (st->mode == WMODE_BLACK) {
        return !black_turn;
    }
    return false;
}

static int compute_quick_eval(app_state *st) {
    if (st->game->is_game_over) {
        if (st->game->is_draw) {
            return 0;
        }
        return engine_score_to_white(st->game->game, -MATE_SCORE);
    }
    if (st->sf_ref) {
        uci_search_result ur = uci_search_depth(st->sf_ref, st->game, 12, 45);
        if (ur.ok) {
            return engine_score_to_white(st->game->game, ur.base.score_cp);
        }
    }
    search_result sr = engine_search(st->eng, st->game, 5, 70);
    return engine_score_to_white(st->game->game, sr.score_cp);
}

static const char *game_over_reason_str(const full_chess *game) {
    if (!game->is_game_over) {
        return "";
    }
    if (!game->is_draw) {
        return is_flipped(game->game) ? "Checkmate — White wins!" : "Checkmate — Black wins!";
    }
    if (is_insufficient_material(game->game)) {
        return "Draw by insufficient material";
    }
    if (position_frequency(game) >= 5) {
        return "Draw by 5-fold repetition";
    }
    if (game->half_move_count >= 150) {
        return "Draw by 75-move rule";
    }
    return "Draw by stalemate";
}

static const char *move_sound_name(const full_chess *game_after, u64 m) {
    if (game_after->is_game_over) {
        return "game_over";
    }
    if (in_check(game_after->game)) {
        return "check";
    }
    if (get_promotion_piece(m) != get_start_piece(m)) {
        return "promote";
    }
    if (is_castle_king_move(m) || is_castle_queen_move(m)) {
        return "castle";
    }
    if (get_end_piece(m) != NONE || is_en_passant_move(m)) {
        return "capture";
    }
    return "move";
}

static void record_and_make_move(app_state *st, u64 move, char *san_out, size_t san_size) {
    char san[16];
    static_move_to_san(st->game->game, st->game->current_legal_moves, move, san, sizeof(san));
    int ply = (int) st->game->move_count;
    if (ply < MAX_GAME_LENGTH) {
        st->mainline_moves[ply] = move;
        snprintf(st->mainline_san[ply], sizeof(st->mainline_san[0]), "%s", san);
        st->mainline_len = ply + 1;
    }
    if (san_out && san_size > 0) {
        snprintf(san_out, san_size, "%s", san);
    }
    make_move(st->game, move);
}

static void goto_ply(app_state *st, int target_ply) {
    if (target_ply < 0) {
        target_ply = 0;
    }
    if (target_ply > st->mainline_len) {
        target_ply = st->mainline_len;
    }
    while ((int) st->game->move_count > target_ply) {
        unmake_move(st->game);
    }
    while ((int) st->game->move_count < target_ply && !st->game->is_game_over) {
        make_move(st->game, st->mainline_moves[st->game->move_count]);
    }
}

static const char *mode_to_str(web_mode m) {
    switch (m) {
    case WMODE_WHITE: return "white";
    case WMODE_BLACK: return "black";
    case WMODE_2P: return "2p";
    case WMODE_WATCH: return "watch";
    case WMODE_ANALYSIS: return "analysis";
    case WMODE_PUZZLE: return "puzzle";
    default: return "white";
    }
}

static const char *current_result_str(const full_chess *game) {
    if (!game || !game->is_game_over) {
        return "*";
    }
    if (game->is_draw) {
        return "1/2-1/2";
    }
    return is_flipped(game->game) ? "1-0" : "0-1";
}

static void save_current_game_to_history(app_state *st, int white_elo, int black_elo) {
    if (!st || st->mainline_len <= 0 || st->mode == WMODE_PUZZLE) {
        return;
    }
    char pgn_buf[32768];
    int saved_ply = (int) st->game->move_count;
    goto_ply(st, st->mainline_len);
    static_get_pgn(st->game, st->white_name, st->black_name, pgn_buf, sizeof(pgn_buf));
    const char *res_str = current_result_str(st->game);
    const char *reason = game_over_reason_str(st->game);
    const char *opening = history_detect_opening(st->mainline_san, st->mainline_len);
    goto_ply(st, saved_ply);

    if (st->current_hist_id > 0 && history_find_by_id(&st->hist, st->current_hist_id)) {
        if (white_elo > 0 || black_elo > 0) {
            history_update_elos(&st->hist, st->current_hist_id, white_elo, black_elo);
            history_save(GAME_HISTORY_FILE, &st->hist);
        }
        return;
    }
    st->current_hist_id = history_add(&st->hist, st->white_name, st->black_name, res_str, reason,
        opening, mode_to_str(st->mode), st->mainline_len, white_elo, black_elo, pgn_buf);
    st->auto_saved_game = true;
    history_save(GAME_HISTORY_FILE, &st->hist);
}

static void maybe_auto_save_completed_game(app_state *st) {
    if (!st || st->auto_saved_game || !st->game->is_game_over || st->mainline_len <= 0) {
        return;
    }
    if (st->mode == WMODE_PUZZLE || st->mode == WMODE_ANALYSIS) {
        return;
    }
    save_current_game_to_history(st, 0, 0);
}

static void serialize_state_json(
    const app_state *st, strbuf *sb, const char *extra_sound, const char *extra_msg) {
    const full_chess *game = st->game;
    chess white_view = *(game->game);
    bool black_to_move = is_flipped(&white_view);
    if (black_to_move) {
        flip_perspective(&white_view);
    }

    char fen[MAX_FEN_LENGTH];
    get_fen(game, fen);

    char eval_str[16];
    engine_format_eval(st->eval_white_cp, eval_str, sizeof(eval_str));
    double wp = engine_win_prob(st->eval_white_cp);

    int check_sq = -1;
    if (in_check(game->game)) {
        u64 king_bb = white_view.kings
                      & (black_to_move ? white_view.enemy_pieces : white_view.friendly_pieces);
        check_sq = get_next_offset(king_bb, -1);
    }

    int full_move
        = game->start_full_move + (game->move_count + (game->start_black_to_move ? 1 : 0)) / 2;

    sb_append(sb, "{");
    sb_append(sb, "\"ok\":true,");
    sb_append(sb, "\"fen\":");
    sb_json_str(sb, fen);
    sb_printf(sb,
        ",\"turn\":\"%s\",\"move_number\":%d,\"ply_count\":%u,\"cursor_ply\":%u,\"total_plies\":%d,"
        "\"half_move_count\":%u,",
        black_to_move ? "b" : "w", full_move, (unsigned) game->move_count,
        (unsigned) game->move_count, st->mainline_len, (unsigned) game->half_move_count);
    sb_printf(sb, "\"in_check\":%s,\"check_sq\":%d,\"is_game_over\":%s,\"is_draw\":%s,",
        in_check(game->game) ? "true" : "false", check_sq, game->is_game_over ? "true" : "false",
        game->is_draw ? "true" : "false");
    sb_append(sb, "\"game_over_reason\":");
    sb_json_str(sb, game_over_reason_str(game));
    sb_printf(sb, ",\"eval_cp\":%d,\"eval_str\":", st->eval_white_cp);
    sb_json_str(sb, eval_str);
    sb_printf(sb,
        ",\"win_prob\":%.4f,\"mode\":\"%s\",\"white_level\":%d,\"black_level\":%d,"
        "\"custom_elo\":%d,",
        wp, mode_to_str(st->mode), st->white_level, st->black_level, st->custom_elo);
    sb_append(sb, "\"white_name\":");
    sb_json_str(sb, st->white_name);
    sb_append(sb, ",\"black_name\":");
    sb_json_str(sb, st->black_name);
    sb_printf(sb, ",\"is_bot_turn\":%s,", state_is_bot_turn(st) ? "true" : "false");
    sb_append(sb, "\"sound\":");
    sb_json_str(sb, extra_sound ? extra_sound : "");
    sb_append(sb, ",\"message\":");
    sb_json_str(sb, extra_msg ? extra_msg : "");

    // 64-square board array (0 = a1 .. 63 = h8)
    sb_append(sb, ",\"board\":[");
    for (int sq = 0; sq < 64; sq++) {
        if (sq > 0) {
            sb_append(sb, ",");
        }
        char c = piece_to_char(get_piece_at_offset(&white_view, sq));
        if (c == ' ') {
            sb_append(sb, "\"\"");
        } else {
            char tmp[2] = { c, '\0' };
            sb_json_str(sb, tmp);
        }
    }
    sb_append(sb, "]");

    // Last move at current cursor
    if (game->move_count > 0) {
        u64 lm = game->move_history[game->move_count - 1];
        const chess *pb = &game->game_history[game->move_count - 1];
        bool pf = is_flipped(pb);
        int s = get_start_offset(lm);
        int e = get_end_offset(lm);
        int sr = pf ? (7 - (s / 8)) : (s / 8);
        int er = pf ? (7 - (e / 8)) : (e / 8);
        int ws = 8 * sr + (s % 8);
        int we = 8 * er + (e % 8);
        sb_printf(sb, ",\"last_move\":{\"from\":%d,\"to\":%d,\"san\":", ws, we);
        sb_json_str(sb, st->mainline_san[game->move_count - 1]);
        sb_append(sb, "}");
    } else {
        sb_append(sb, ",\"last_move\":null");
    }

    // Legal moves
    sb_append(sb, ",\"legal_moves\":[");
    for (int i = 0; game->current_legal_moves[i] != NULL_MOVE; i++) {
        if (i > 0) {
            sb_append(sb, ",");
        }
        u64 m = game->current_legal_moves[i];
        int s = get_start_offset(m);
        int e = get_end_offset(m);
        int sr = black_to_move ? (7 - (s / 8)) : (s / 8);
        int er = black_to_move ? (7 - (e / 8)) : (e / 8);
        int ws = 8 * sr + (s % 8);
        int we = 8 * er + (e % 8);
        char promo[2] = { '\0', '\0' };
        if (get_promotion_piece(m) != get_start_piece(m)) {
            promo[0] = (char) tolower((unsigned char) piece_to_char(get_promotion_piece(m)));
        }
        char uci[8];
        if (promo[0]) {
            snprintf(uci, sizeof(uci), "%c%c%c%c%c", 'a' + (s % 8), '1' + sr, 'a' + (e % 8),
                '1' + er, promo[0]);
        } else {
            snprintf(
                uci, sizeof(uci), "%c%c%c%c", 'a' + (s % 8), '1' + sr, 'a' + (e % 8), '1' + er);
        }
        char san[16];
        static_move_to_san(game->game, game->current_legal_moves, m, san, sizeof(san));
        bool is_cap = (get_end_piece(m) != NONE) || is_en_passant_move(m);
        sb_printf(sb, "{\"from\":%d,\"to\":%d,\"uci\":\"%s\",\"san\":", ws, we, uci);
        sb_json_str(sb, san);
        sb_printf(sb, ",\"promo\":\"%s\",\"capture\":%s}", promo, is_cap ? "true" : "false");
    }
    sb_append(sb, "]");

    // Full mainline move history (so stepping back in analysis keeps future moves visible)
    sb_append(sb, ",\"history\":[");
    for (int p = 0; p < st->mainline_len; p++) {
        if (p > 0) {
            sb_append(sb, ",");
        }
        int abs_ply = p + (game->start_black_to_move ? 1 : 0);
        int mnum = game->start_full_move + (abs_ply / 2);
        const char *side = ((abs_ply % 2) == 0) ? "w" : "b";
        sb_printf(sb, "{\"ply\":%d,\"move_num\":%d,\"side\":\"%s\",\"san\":", p + 1, mnum, side);
        sb_json_str(sb, st->mainline_san[p]);
        sb_append(sb, "}");
    }
    sb_append(sb, "]");

    // Captured pieces & material difference
    const u8 w_types[5]
        = { FRIENDLY_QUEEN, FRIENDLY_ROOK, FRIENDLY_BISHOP, FRIENDLY_KNIGHT, FRIENDLY_PAWN };
    const u8 b_types[5] = { ENEMY_QUEEN, ENEMY_ROOK, ENEMY_BISHOP, ENEMY_KNIGHT, ENEMY_PAWN };
    const int vals[5] = { 9, 5, 3, 3, 1 };
    const int start_cnt[5] = { 1, 2, 2, 2, 8 };
    const char *w_ch[5] = { "Q", "R", "B", "N", "P" };
    const char *b_ch[5] = { "q", "r", "b", "n", "p" };
    int w_cur[5] = { 0 }, b_cur[5] = { 0 };
    int w_mat = 0, b_mat = 0;
    for (int sq = 0; sq < 64; sq++) {
        u8 pc = get_piece_at_offset(&white_view, sq);
        for (int t = 0; t < 5; t++) {
            if (pc == w_types[t]) {
                w_cur[t]++;
                w_mat += vals[t];
            } else if (pc == b_types[t]) {
                b_cur[t]++;
                b_mat += vals[t];
            }
        }
    }
    sb_append(sb, ",\"captured\":{\"white\":[");
    bool first = true;
    for (int t = 0; t < 5; t++) {
        for (int k = b_cur[t]; k < start_cnt[t]; k++) {
            if (!first) {
                sb_append(sb, ",");
            }
            first = false;
            sb_json_str(sb, b_ch[t]);
        }
    }
    sb_append(sb, "],\"black\":[");
    first = true;
    for (int t = 0; t < 5; t++) {
        for (int k = w_cur[t]; k < start_cnt[t]; k++) {
            if (!first) {
                sb_append(sb, ",");
            }
            first = false;
            sb_json_str(sb, w_ch[t]);
        }
    }
    sb_printf(sb, "],\"white_adv\":%d}", w_mat - b_mat);

    // Difficulty presets (Chess-in-C 1..6 + Stockfish 7..11) + saved calibrated Elos
    gauntlet_status gs;
    gauntlet_snapshot(st->gt, &gs);
    sb_printf(sb, ",\"stockfish_available\":%s,\"stockfish_name\":", st->sf ? "true" : "false");
    sb_json_str(sb, st->sf ? uci_get_name(st->sf) : "");
    sb_printf(sb, ",\"cpu_cores\":%d,\"gauntlet_running\":%s,\"presets\":[", gs.cpu_cores,
        gs.running ? "true" : "false");
    int np = engine_num_presets();
    for (int i = 1; i <= np; i++) {
        const difficulty_preset *pr = engine_get_preset(i);
        const level_progress *lp = &gs.levels[i - 1];
        if (i > 1) {
            sb_append(sb, ",");
        }
        sb_printf(sb,
            "{\"level\":%d,\"engine\":\"c\",\"available\":true,\"has_elo\":%s,\"is_relative\":%s,"
            "\"est_elo\":%d,\"name\":",
            pr->level, lp->has_elo ? "true" : "false", lp->is_relative ? "true" : "false",
            lp->est_elo);
        sb_json_str(sb, pr->name);
        sb_append(sb, ",\"desc\":");
        sb_json_str(sb, pr->desc);
        sb_append(sb, "}");
    }
    int nsf = uci_num_presets();
    for (int i = 0; i < nsf; i++) {
        const sf_preset *sp = uci_get_preset(uci_first_level() + i);
        sb_printf(sb,
            ",{\"level\":%d,\"engine\":\"stockfish\",\"available\":%s,\"has_elo\":true,"
            "\"is_relative\":false,\"est_elo\":%d,\"name\":",
            sp->level, st->sf ? "true" : "false", sp->limit_strength ? sp->elo : 3500);
        sb_json_str(sb, sp->name);
        sb_append(sb, ",\"desc\":");
        sb_json_str(sb, sp->desc);
        sb_append(sb, "}");
    }
    const level_progress *az_lp = &gs.levels[GAUNTLET_ZERO_C_LEVEL - 1];
    sb_printf(sb,
        ",{\"level\":%d,\"engine\":\"az\",\"available\":%s,\"has_elo\":%s,"
        "\"is_relative\":%s,\"est_elo\":%d,\"name\":\"Zero-C v2 (Latest)\","
        "\"desc\":\"Active Zero-C AlphaZero self-play MCTS + SE-ResNet neural engine\"}",
        AZ_BOT_LEVEL, az_is_available() ? "true" : "false", az_lp->has_elo ? "true" : "false",
        az_lp->is_relative ? "true" : "false", az_lp->est_elo);
    sb_printf(sb,
        ",{\"level\":%d,\"engine\":\"az\",\"available\":%s,\"has_elo\":true,"
        "\"is_relative\":false,\"est_elo\":1276,\"name\":\"Zero-C v1 Champion (Gen 4320 • Max)\","
        "\"desc\":\"Immortalized v1 6x96 SE-ResNet (554k self-play games, 1000-sim Hybrid MCTS)\"}",
        AZ_V1_BOT_LEVEL, az_v1_is_available() ? "true" : "false");
    bool hyb_ok = hybrid_is_unlocked();
    bool nnue_ok = hybrid_nnue_is_unlocked();
    sb_printf(sb,
        ",{\"level\":%d,\"engine\":\"hybrid\",\"available\":%s,\"locked\":%s,\"has_elo\":false,"
        "\"is_relative\":false,\"est_elo\":2660,\"name\":\"Gemini Arbiter (Dual-Brain)\","
        "\"desc\":\"Parallel CPU Titan-C L6 Multi-PV + GPU Zero-C v2 positional tie-breaker\"}",
        HYBRID_DUAL_LEVEL, hyb_ok ? "true" : "false", hyb_ok ? "false" : "true");
    sb_printf(sb,
        ",{\"level\":%d,\"engine\":\"hybrid\",\"available\":%s,\"locked\":%s,\"has_elo\":false,"
        "\"is_relative\":false,\"est_elo\":2700,\"name\":\"Oracle of LMR (Neural Search)\","
        "\"desc\":\"Zero-C v2 policy-guided move ordering & LMR inside Titan-C 800k-NPS search\"}",
        HYBRID_LMR_LEVEL, hyb_ok ? "true" : "false", hyb_ok ? "false" : "true");
    sb_printf(sb,
        ",{\"level\":%d,\"engine\":\"hybrid\",\"available\":%s,\"locked\":%s,\"has_elo\":false,"
        "\"is_relative\":false,\"est_elo\":2300,\"name\":\"Zero-C Omega (Tactical MCTS)\","
        "\"desc\":\"Zero-C v2 10x128 MCTS armed with Titan-C's tactical refutation verifier\"}",
        HYBRID_OMEGA_LEVEL, hyb_ok ? "true" : "false", hyb_ok ? "false" : "true");
    sb_printf(sb,
        ",{\"level\":%d,\"engine\":\"hybrid\",\"available\":%s,\"locked\":%s,\"needs_distill\":%s,"
        "\"has_elo\":false,\"is_relative\":false,\"est_elo\":2800,\"name\":\"Singularity v5 (Zero-NNUE)\","
        "\"desc\":\"Requires Knowledge Distillation: compresses Zero-C v2 into 200ns AVX2 NNUE v4\"}",
        HYBRID_NNUE_LEVEL, nnue_ok ? "true" : "false", nnue_ok ? "false" : "true",
        nnue_ok ? "false" : "true");
    apex_progress_info ap = apex_get_progress(st);
    sb_printf(sb,
        ",{\"level\":%d,\"engine\":\"apex\",\"available\":true,\"locked\":false,\"has_v4_nnue\":%s,"
        "\"apex_stage\":%d,\"apex_max_stage\":%d,\"apex_pct\":%d,\"apex_tier\":\"%s\","
        "\"apex_tasks\":{\"search\":true,\"datagen\":%s,\"datagen_pos\":%llu,\"spsa\":%s,\"nnue_v4\":%s},"
        "\"has_elo\":true,\"is_relative\":false,\"est_elo\":%d,\"name\":\"Apex-C [Lv %d/%d • %s]\","
        "\"desc\":\"Dynamic Project 3000 Pure C11 Engine (%d/%d forge tasks complete): Full CorrHist + Capture/Cut LMR + %s\"}",
        APEX_PURE_LEVEL, ap.nnue_v4_done ? "true" : "false", ap.pure_stage, ap.pure_max, ap.pure_pct,
        ap.pure_tier, ap.datagen_done ? "true" : "false", ap.datagen_pos,
        ap.spsa_done ? "true" : "false", ap.nnue_v4_done ? "true" : "false", ap.pure_elo,
        ap.pure_stage, ap.pure_max, ap.pure_tier, ap.pure_stage, ap.pure_max,
        ap.nnue_v4_done ? "16x768->1024 Big NNUE v4" : "v3 NNUE Prototype (complete Forge tasks to evolve!)");
    sb_printf(sb,
        ",{\"level\":%d,\"engine\":\"apex\",\"available\":%s,\"locked\":%s,\"needs_syzygy\":%s,"
        "\"syzygy_pieces\":%d,\"has_v4_nnue\":%s,"
        "\"apex_stage\":%d,\"apex_max_stage\":%d,\"apex_pct\":%d,\"apex_tier\":\"%s\","
        "\"apex_tasks\":{\"search\":true,\"syzygy\":%s,\"datagen\":%s,\"datagen_pos\":%llu,\"spsa\":%s,\"nnue_v4\":%s},"
        "\"has_elo\":true,\"is_relative\":false,\"est_elo\":%d,\"name\":\"Apex-C + Syzygy [Lv %d/%d • %s]\","
        "\"desc\":\"Dynamic Project 3000 + 5-Piece Syzygy (%d/%d forge tasks complete): %s\"}",
        APEX_SYZYGY_LEVEL, ap.syzygy_done ? "true" : "false", ap.syzygy_done ? "false" : "true",
        ap.syzygy_done ? "false" : "true", ap.syzygy_pieces, ap.nnue_v4_done ? "true" : "false",
        ap.syz_stage, ap.syz_max, ap.syz_pct, ap.syz_tier, ap.syzygy_done ? "true" : "false",
        ap.datagen_done ? "true" : "false", ap.datagen_pos, ap.spsa_done ? "true" : "false",
        ap.nnue_v4_done ? "true" : "false", ap.syz_elo, ap.syz_stage, ap.syz_max, ap.syz_tier,
        ap.syz_stage, ap.syz_max,
        ap.nnue_v4_done ? "1024 NNUE v4 + 5-Man Syzygy Perfection" : "5-Man Syzygy + v3 NNUE Prototype (complete Forge tasks to evolve!)");
    sb_append(sb, "],\"opening\":");
    sb_json_str(sb, (st->mode == WMODE_PUZZLE)
                        ? "Tactical Puzzle"
                        : history_detect_opening(st->mainline_san, st->mainline_len));
    sb_printf(sb,
        ",\"history_count\":%d,\"puzzle_stats\":{\"rating\":%d,\"streak\":%d,\"best_streak\":%d,"
        "\"solved_count\":%d,\"total\":%d}",
        st->hist.count, st->pstats.rating, st->pstats.streak, st->pstats.best_streak,
        st->pstats.solved_count, puzzle_count());

    if (st->mode == WMODE_PUZZLE && st->active_puzzle_id > 0) {
        const chess_puzzle *pz = puzzle_find_by_id(st->active_puzzle_id);
        if (pz) {
            int total_player_moves = (pz->num_moves + 1) / 2;
            int done_player_moves = (st->puzzle_step + 1) / 2;
            sb_printf(sb,
                ",\"puzzle\":{\"id\":%d,\"rating\":%d,\"difficulty\":%d,\"step\":%d,"
                "\"total_moves\":%d,\"done_moves\":%d,\"solved\":%s,\"failed\":%s,"
                "\"title\":",
                pz->id, pz->rating, pz->difficulty, st->puzzle_step, total_player_moves,
                done_player_moves, st->puzzle_solved ? "true" : "false",
                st->puzzle_failed ? "true" : "false");
            sb_json_str(sb, pz->title);
            sb_append(sb, ",\"theme\":");
            sb_json_str(sb, pz->theme);
            sb_append(sb, ",\"description\":");
            sb_json_str(sb, pz->description);
            sb_append(sb, ",\"hint\":");
            sb_json_str(sb, pz->hint);
            sb_append(sb, "}");
        }
    }
    sb_append(sb, "}");
}

static void serialize_gauntlet_json(const gauntlet_status *gs, strbuf *sb) {
    sb_printf(sb,
        "{\"ok\":true,\"running\":%s,\"calc_mode\":%s,\"vs_stockfish\":%s,\"az_available\":%s,"
        "\"stop_requested\":%s,\"num_threads\":%d,\"cpu_cores\":%d,\"games_per_level\":%d,"
        "\"completed_games\":%d,\"scheduled_games\":%d,\"elapsed_sec\":%.1f,\"opponent\":",
        gs->running ? "true" : "false", gs->calc_mode ? "true" : "false",
        gs->vs_stockfish ? "true" : "false", gs->az_available ? "true" : "false",
        gs->stop_requested ? "true" : "false", gs->num_threads, gs->cpu_cores, gs->games_per_level,
        gs->completed_games_total, gs->scheduled_games_total, gs->elapsed_sec);
    sb_json_str(sb, gs->opponent_name);
    sb_append(sb, ",\"updated_at\":");
    sb_json_str(sb, gs->updated_at);
    sb_append(sb, ",\"elo_file\":");
    sb_json_str(sb, ELO_RATINGS_FILE);

    sb_append(sb, ",\"levels\":[");
    for (int i = 0; i < GAUNTLET_NUM_LEVELS; i++) {
        const level_progress *lp = &gs->levels[i];
        const char *lname = gauntlet_level_name(i + 1);
        const char *eng_tag = (i + 1 == GAUNTLET_ZERO_C_LEVEL) ? "az" : "c";
        const char *st_str
            = (lp->state == 1)
                  ? "running"
                  : ((lp->state == 2) ? "done" : ((lp->state == 3) ? "stopped" : "pending"));
        if (i > 0) {
            sb_append(sb, ",");
        }
        sb_printf(sb, "{\"level\":%d,\"engine\":\"%s\",\"name\":", lp->level, eng_tag);
        sb_json_str(sb, lname);
        sb_printf(sb, ",\"state\":\"%s\",\"opponent\":", st_str);
        sb_json_str(sb, lp->opponent_label);
        sb_printf(sb,
            ",\"done\":%d,\"total\":%d,\"wins\":%d,\"draws\":%d,\"losses\":%d,"
            "\"has_elo\":%s,\"is_relative\":%s,\"est_elo\":%d}",
            lp->done_games, lp->total_games, lp->wins, lp->draws, lp->losses,
            lp->has_elo ? "true" : "false", lp->is_relative ? "true" : "false", lp->est_elo);
    }
    sb_append(sb, "],\"calc\":{");
    const elo_calc_status *cs = &gs->calc;
    sb_printf(sb, "\"active\":%s,\"subject_level\":%d,\"subject_custom_elo\":%d,\"subject_label\":",
        cs->active ? "true" : "false", cs->subject_level, cs->subject_custom_elo);
    sb_json_str(sb, cs->subject_label);
    sb_printf(sb,
        ",\"target_ci\":%d,\"max_games\":%d,\"games_done\":%d,\"wins\":%d,\"draws\":%d,"
        "\"losses\":%d,\"current_anchor\":%d,\"est_elo\":%d,\"ci_half\":%d,\"ci_low\":%d,"
        "\"ci_high\":%d,\"converged\":%s,\"hit_sf_floor\":%s,\"buckets\":[",
        cs->target_ci, cs->max_games, cs->games_done, cs->wins, cs->draws, cs->losses,
        cs->current_anchor, cs->est_elo, cs->ci_half, cs->ci_low, cs->ci_high,
        cs->converged ? "true" : "false", cs->hit_sf_floor ? "true" : "false");
    for (int i = 0; i < cs->num_buckets; i++) {
        if (i > 0) {
            sb_append(sb, ",");
        }
        sb_printf(sb, "{\"anchor\":%d,\"wins\":%d,\"draws\":%d,\"losses\":%d}",
            cs->buckets[i].anchor_elo, cs->buckets[i].wins, cs->buckets[i].draws,
            cs->buckets[i].losses);
    }
    sb_append(sb, "]},\"workers\":[");
    bool first = true;
    for (int t = 0; t < MAX_GAUNTLET_THREADS; t++) {
        const worker_snapshot *ws = &gs->workers[t];
        if (!ws->active) {
            continue;
        }
        if (!first) {
            sb_append(sb, ",");
        }
        first = false;
        char ev_str[16];
        engine_format_eval(ws->eval_white_cp, ev_str, sizeof(ev_str));
        sb_printf(sb,
            "{\"id\":%d,\"level\":%d,\"game_num\":%d,\"total_games\":%d,\"ply\":%d,\"eval_str\":",
            ws->worker_id, ws->level, ws->game_num, ws->total_games, ws->ply_count);
        sb_json_str(sb, ev_str);
        sb_append(sb, ",\"white\":");
        sb_json_str(sb, ws->white_label);
        sb_append(sb, ",\"black\":");
        sb_json_str(sb, ws->black_label);
        sb_append(sb, ",\"last_san\":");
        sb_json_str(sb, ws->last_san);
        sb_append(sb, ",\"fen\":");
        sb_json_str(sb, ws->fen);
        sb_append(sb, ",\"board\":");
        sb_json_str(sb, ws->board);
        sb_append(sb, "}");
    }
    sb_append(sb, "]}");
}

// ============================================================================
// PGN Import Parser (Headers + SAN Movetext -> Analysis Mode)
// ============================================================================

static bool extract_pgn_tag(const char *pgn, const char *tag_name, char *out, size_t out_size) {
    char pat[64];
    snprintf(pat, sizeof(pat), "[%s \"", tag_name);
    const char *p = strstr(pgn, pat);
    if (!p) {
        return false;
    }
    p += strlen(pat);
    size_t i = 0;
    while (*p && *p != '"' && *p != '\n' && i + 1 < out_size) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return i > 0;
}

static bool import_pgn_into_state(app_state *st, const char *pgn, char *err_out, size_t err_size) {
    full_chess *tmp = full_chess_create();
    tmp->is_static = true;

    char fen_tag[MAX_FEN_LENGTH] = "";
    if (extract_pgn_tag(pgn, "FEN", fen_tag, sizeof(fen_tag))) {
        set_from_fen(tmp, fen_tag);
        tmp->is_static = true;
    }

    u64 parsed_moves[MAX_GAME_LENGTH];
    char parsed_san[MAX_GAME_LENGTH][16];
    int parsed_len = 0;

    // Walk PGN skipping header tags [...], comments {...}, ;...\n, and (RAV variations)
    const char *p = pgn;
    int rav_depth = 0;
    while (*p) {
        if (*p == '[') {
            while (*p && *p != ']') {
                p++;
            }
            if (*p == ']') {
                p++;
            }
            continue;
        }
        if (*p == '{') {
            while (*p && *p != '}') {
                p++;
            }
            if (*p == '}') {
                p++;
            }
            continue;
        }
        if (*p == ';') {
            while (*p && *p != '\n') {
                p++;
            }
            continue;
        }
        if (*p == '(') {
            rav_depth++;
            p++;
            continue;
        }
        if (*p == ')') {
            if (rav_depth > 0) {
                rav_depth--;
            }
            p++;
            continue;
        }
        if (rav_depth > 0 || isspace((unsigned char) *p)) {
            p++;
            continue;
        }

        // Read next whitespace-delimited token
        char tok[64];
        size_t ti = 0;
        while (*p && !isspace((unsigned char) *p) && *p != '{' && *p != '(' && *p != ')'
               && *p != ';' && ti + 1 < sizeof(tok)) {
            tok[ti++] = *p++;
        }
        tok[ti] = '\0';
        if (ti == 0) {
            continue;
        }

        // Skip NAGs ($1, $2), result tokens, ellipses, and pure move numbers ("1.", "16...")
        if (tok[0] == '$' || strcmp(tok, "1-0") == 0 || strcmp(tok, "0-1") == 0
            || strcmp(tok, "1/2-1/2") == 0 || strcmp(tok, "*") == 0) {
            continue;
        }

        // Strip leading move number prefix if attached directly to move (e.g. "1.e4" or "16...Nxb8")
        const char *mv_str = tok;
        if (isdigit((unsigned char) mv_str[0])) {
            while (isdigit((unsigned char) *mv_str)) {
                mv_str++;
            }
            if (*mv_str == '.') {
                while (*mv_str == '.') {
                    mv_str++;
                }
                if (*mv_str == '\0') {
                    continue; // pure move number token like "1." or "16..."
                }
            } else if (*mv_str == '\0') {
                continue;
            } else {
                mv_str = tok; // could be "0-0" or "0-0-0"!
            }
        }

        if (tmp->is_game_over) {
            // Stop cleanly if trailing moves exist past automatic draw/mate (e.g. old K vs K PGNs)
            break;
        }

        int matches = 0;
        u64 m = static_encode_san(tmp, mv_str, &matches);
        if (m == NULL_MOVE) {
            m = static_encode_string(tmp, mv_str);
        }
        if (m == NULL_MOVE) {
            snprintf(
                err_out, err_size, "Could not parse move '%s' at ply %d.", mv_str, parsed_len + 1);
            full_chess_delete(&tmp);
            return false;
        }
        if (parsed_len < MAX_GAME_LENGTH) {
            parsed_moves[parsed_len] = m;
            static_move_to_san(tmp->game, tmp->current_legal_moves, m, parsed_san[parsed_len],
                sizeof(parsed_san[0]));
            parsed_len++;
        }
        make_move(tmp, m);
    }

    // Commit parsed game into app_state in Analysis Mode
    full_chess_delete(&st->game);
    st->game = tmp;
    st->mode = WMODE_ANALYSIS;
    st->mainline_len = parsed_len;
    for (int i = 0; i < parsed_len; i++) {
        st->mainline_moves[i] = parsed_moves[i];
        snprintf(st->mainline_san[i], sizeof(st->mainline_san[0]), "%s", parsed_san[i]);
    }

    char w_tag[64] = "", b_tag[64] = "";
    bool has_w = extract_pgn_tag(pgn, "White", w_tag, sizeof(w_tag));
    bool has_b = extract_pgn_tag(pgn, "Black", b_tag, sizeof(b_tag));
    st->custom_names = (has_w || has_b);
    snprintf(st->white_name, sizeof(st->white_name), "%s", has_w ? w_tag : "White");
    snprintf(st->black_name, sizeof(st->black_name), "%s", has_b ? b_tag : "Black");

    engine_clear_tt(st->eng);
    st->eval_white_cp = compute_quick_eval(st);
    return true;
}

// ============================================================================
// Live Position Analysis & Post-Game Review JSON Endpoints
// ============================================================================

static void handle_analyze_json(app_state *st, strbuf *sb, bool use_sf) {
    if (st->game->is_game_over) {
        int ev = st->game->is_draw ? 0 : engine_score_to_white(st->game->game, -MATE_SCORE);
        st->eval_white_cp = ev;
        char ev_str[16];
        engine_format_eval(ev, ev_str, sizeof(ev_str));
        sb_printf(sb, "{\"ok\":true,\"eval_cp\":%d,\"eval_str\":", ev);
        sb_json_str(sb, ev_str);
        sb_printf(sb,
            ",\"win_prob\":%.4f,\"depth\":0,\"nodes\":0,\"nps\":0,\"engine\":\"%s\","
            "\"best\":null,\"pv\":[],\"lines\":[]}",
            engine_win_prob(ev), (use_sf && st->sf) ? "stockfish" : "c");
        return;
    }

    multipv_line lines[MAX_MULTIPV];
    int n_lines = 0;
    u64 total_nodes = 0;
    double elapsed_sec = 0.0;
    const char *eng_tag = "c";

    uci_engine *sf_an = st->sf_ref ? st->sf_ref : st->sf;
    if (use_sf && sf_an) {
        uci_search_result ur = uci_search_depth(sf_an, st->game, 18, 350);
        lines[0].best_move = ur.base.best_move;
        lines[0].score_cp = ur.base.score_cp;
        lines[0].depth_reached = ur.base.depth_reached;
        lines[0].pv_len = (ur.pv_len > 16) ? 16 : ur.pv_len;
        for (int i = 0; i < lines[0].pv_len; i++) {
            lines[0].pv[i] = ur.pv_moves[i];
        }
        total_nodes = ur.base.nodes;
        elapsed_sec = ur.base.elapsed_sec;
        n_lines = 1;
        eng_tag = "stockfish";
    } else {
        n_lines = engine_search_multipv(
            st->eng, st->game, 9, 450, 3, lines, &total_nodes, &elapsed_sec);
    }

    if (n_lines <= 0) {
        sb_append(sb, "{\"ok\":false,\"error\":\"No legal moves.\"}");
        return;
    }

    st->eval_white_cp = engine_score_to_white(st->game->game, lines[0].score_cp);
    char ev_str[16];
    engine_format_eval(st->eval_white_cp, ev_str, sizeof(ev_str));
    double nps = (elapsed_sec > 0.0001) ? ((double) total_nodes / elapsed_sec) : 0.0;

    bool flipped = is_flipped(st->game->game);
    int s0 = get_start_offset(lines[0].best_move);
    int e0 = get_end_offset(lines[0].best_move);
    int ws0 = 8 * (flipped ? (7 - (s0 / 8)) : (s0 / 8)) + (s0 % 8);
    int we0 = 8 * (flipped ? (7 - (e0 / 8)) : (e0 / 8)) + (e0 % 8);
    char best_san[16];
    static_move_to_san(st->game->game, st->game->current_legal_moves, lines[0].best_move, best_san,
        sizeof(best_san));

    sb_printf(sb, "{\"ok\":true,\"eval_cp\":%d,\"eval_str\":", st->eval_white_cp);
    sb_json_str(sb, ev_str);
    sb_printf(sb,
        ",\"win_prob\":%.4f,\"depth\":%d,\"nodes\":%lu,\"nps\":%lu,\"engine\":\"%s\","
        "\"best\":{\"from\":%d,\"to\":%d,\"san\":",
        engine_win_prob(st->eval_white_cp), lines[0].depth_reached, (unsigned long) total_nodes,
        (unsigned long) nps, eng_tag, ws0, we0);
    sb_json_str(sb, best_san);
    sb_append(sb, "},\"pv\":[");

    chess pv_board = *(st->game->game);
    u64 legal[MAX_LEGAL_MOVES];
    int pv0_cap = (lines[0].pv_len > 8) ? 8 : lines[0].pv_len;
    for (int i = 0; i < pv0_cap; i++) {
        if (i > 0) {
            sb_append(sb, ",");
        }
        set_legal_moves(&pv_board, legal);
        char psan[16];
        static_move_to_san(&pv_board, legal, lines[0].pv[i], psan, sizeof(psan));
        sb_json_str(sb, psan);
        chess_make_move(&pv_board, lines[0].pv[i]);
    }
    sb_append(sb, "],\"lines\":[");

    for (int k = 0; k < n_lines; k++) {
        if (k > 0) {
            sb_append(sb, ",");
        }
        int l_ev_w = engine_score_to_white(st->game->game, lines[k].score_cp);
        char l_ev_str[16];
        engine_format_eval(l_ev_w, l_ev_str, sizeof(l_ev_str));

        int sk = get_start_offset(lines[k].best_move);
        int ek = get_end_offset(lines[k].best_move);
        int wsk = 8 * (flipped ? (7 - (sk / 8)) : (sk / 8)) + (sk % 8);
        int wek = 8 * (flipped ? (7 - (ek / 8)) : (ek / 8)) + (ek % 8);
        char l_san[16];
        static_move_to_san(st->game->game, st->game->current_legal_moves, lines[k].best_move, l_san,
            sizeof(l_san));

        sb_printf(sb, "{\"rank\":%d,\"eval_cp\":%d,\"eval_str\":", k + 1, l_ev_w);
        sb_json_str(sb, l_ev_str);
        sb_printf(
            sb, ",\"depth\":%d,\"from\":%d,\"to\":%d,\"san\":", lines[k].depth_reached, wsk, wek);
        sb_json_str(sb, l_san);
        sb_append(sb, ",\"pv\":[");

        chess lb = *(st->game->game);
        int lcap = (lines[k].pv_len > 8) ? 8 : lines[k].pv_len;
        for (int i = 0; i < lcap; i++) {
            if (i > 0) {
                sb_append(sb, ",");
            }
            set_legal_moves(&lb, legal);
            char psan[16];
            static_move_to_san(&lb, legal, lines[k].pv[i], psan, sizeof(psan));
            sb_json_str(sb, psan);
            chess_make_move(&lb, lines[k].pv[i]);
        }
        sb_append(sb, "]}");
    }
    sb_append(sb, "]}");
}

static search_result sf_review_eval_cb(const full_chess *pos, void *ctx) {
    uci_engine *sf = (uci_engine *) ctx;
    uci_search_result ur = uci_search_depth(sf, pos, 15, 85);
    return ur.base;
}

static void handle_review_json(app_state *st, strbuf *sb) {
    if (st->mainline_len == 0) {
        sb_append(sb, "{\"ok\":false,\"error\":\"No moves played yet to review.\"}");
        return;
    }

    uci_engine *sf_rev = st->sf_ref ? st->sf_ref : NULL;
    game_review rev;
    if (!review_analyze_game_ex(st->eng, st->game, st->mainline_moves, st->mainline_len, 9, 100,
            sf_rev ? sf_review_eval_cb : NULL, sf_rev, NULL, NULL, &rev)) {
        sb_append(sb, "{\"ok\":false,\"error\":\"Unable to analyze game.\"}");
        return;
    }

    strbuf moves_json, timeline_json;
    sb_init(&moves_json, 4096);
    sb_init(&timeline_json, 1024);
    sb_printf(&timeline_json, "[%.4f", rev.win_pcts_w[0] / 100.0);
    sb_append(&moves_json, "[");

    for (int p = 0; p < rev.total_plies; p++) {
        const review_ply *rp = &rev.plies[p];
        char eval_after_str[16];
        engine_format_eval(rp->eval_after_w, eval_after_str, sizeof(eval_after_str));

        sb_printf(&timeline_json, ",%.4f", rev.win_pcts_w[p + 1] / 100.0);
        if (p > 0) {
            sb_append(&moves_json, ",");
        }
        sb_printf(&moves_json, "{\"ply\":%d,\"move_num\":%d,\"side\":\"%s\",\"played\":", p + 1,
            rp->move_num, rp->was_black ? "b" : "w");
        sb_json_str(&moves_json, rp->played_san);
        sb_append(&moves_json, ",\"best\":");
        sb_json_str(&moves_json, rp->best_san);
        sb_printf(&moves_json,
            ",\"rating\":\"%s\",\"cpl\":%d,\"win_drop\":%.1f,\"move_acc\":%.1f,\"eval_str\":",
            review_class_name(rp->cls), rp->cpl, rp->win_drop * 100.0, rp->move_acc);
        sb_json_str(&moves_json, eval_after_str);
        sb_append(&moves_json, "}");
    }

    sb_append(&timeline_json, "]");
    sb_append(&moves_json, "]");

    sb_printf(sb, "{\"ok\":true,\"engine\":\"%s\",", sf_rev ? "stockfish" : "titan-c");
    sb_printf(sb,
        "\"white\":{\"accuracy\":%.1f,\"acpl\":%d,\"est_elo\":%d,\"elo_tier\":\"%s\","
        "\"brilliant\":%d,\"best\":%d,\"good\":%d,\"inaccuracy\":%d,\"mistake\":%d,\"blunder\":%d},",
        rev.white.accuracy, rev.white.acpl, rev.white.est_elo, review_elo_tier(rev.white.est_elo),
        rev.white.counts[REV_BRILLIANT], rev.white.counts[REV_BEST], rev.white.counts[REV_GOOD],
        rev.white.counts[REV_INACCURACY], rev.white.counts[REV_MISTAKE],
        rev.white.counts[REV_BLUNDER]);
    sb_printf(sb,
        "\"black\":{\"accuracy\":%.1f,\"acpl\":%d,\"est_elo\":%d,\"elo_tier\":\"%s\","
        "\"brilliant\":%d,\"best\":%d,\"good\":%d,\"inaccuracy\":%d,\"mistake\":%d,\"blunder\":%d},",
        rev.black.accuracy, rev.black.acpl, rev.black.est_elo, review_elo_tier(rev.black.est_elo),
        rev.black.counts[REV_BRILLIANT], rev.black.counts[REV_BEST], rev.black.counts[REV_GOOD],
        rev.black.counts[REV_INACCURACY], rev.black.counts[REV_MISTAKE],
        rev.black.counts[REV_BLUNDER]);

    // Update or persist reviewed Elo into Game History
    if (st->current_hist_id > 0) {
        history_update_elos(&st->hist, st->current_hist_id, rev.white.est_elo, rev.black.est_elo);
        history_save(GAME_HISTORY_FILE, &st->hist);
    }

    if (rev.turning_point_ply >= 0 && rev.turning_point_drop >= 0.08) {
        const review_ply *tp = &rev.plies[rev.turning_point_ply];
        char turn_eb[16], turn_ea[16];
        engine_format_eval(tp->eval_before_w, turn_eb, sizeof(turn_eb));
        engine_format_eval(tp->eval_after_w, turn_ea, sizeof(turn_ea));
        sb_printf(sb, "\"turning_point\":{\"ply\":%d,\"move_num\":%d,\"side\":\"%s\",\"played\":",
            rev.turning_point_ply + 1, tp->move_num, tp->was_black ? "b" : "w");
        sb_json_str(sb, tp->played_san);
        sb_append(sb, ",\"best\":");
        sb_json_str(sb, tp->best_san);
        sb_append(sb, ",\"eval_before\":");
        sb_json_str(sb, turn_eb);
        sb_append(sb, ",\"eval_after\":");
        sb_json_str(sb, turn_ea);
        sb_append(sb, "},");
    } else {
        sb_append(sb, "\"turning_point\":null,");
    }
    sb_append(sb, "\"timeline\":");
    sb_append(sb, timeline_json.data);
    sb_append(sb, ",\"moves\":");
    sb_append(sb, moves_json.data);
    sb_append(sb, "}");

    sb_free(&moves_json);
    sb_free(&timeline_json);
    review_free(&rev);
}

// ============================================================================
// Puzzle Mode & Game History JSON Handlers
// ============================================================================

static void serialize_history_json(const game_history *h, strbuf *sb) {
    int wins = 0, draws = 0, losses = 0;
    for (int i = 0; i < h->count; i++) {
        const history_entry *e = &h->entries[i];
        if (strcmp(e->result, "1/2-1/2") == 0) {
            draws++;
        } else if ((strcmp(e->result, "1-0") == 0 && strcmp(e->white, "Human") == 0)
                   || (strcmp(e->result, "0-1") == 0 && strcmp(e->black, "Human") == 0)) {
            wins++;
        } else if ((strcmp(e->result, "0-1") == 0 && strcmp(e->white, "Human") == 0)
                   || (strcmp(e->result, "1-0") == 0 && strcmp(e->black, "Human") == 0)) {
            losses++;
        }
    }
    sb_printf(sb,
        "{\"ok\":true,\"count\":%d,\"summary\":{\"wins\":%d,\"draws\":%d,\"losses\":%d},"
        "\"games\":[",
        h->count, wins, draws, losses);
    for (int i = 0; i < h->count; i++) {
        const history_entry *e = &h->entries[i];
        if (i > 0) {
            sb_append(sb, ",");
        }
        sb_printf(sb, "{\"id\":%d,\"plies\":%d,\"white_elo\":%d,\"black_elo\":%d,\"date\":", e->id,
            e->plies, e->white_elo, e->black_elo);
        sb_json_str(sb, e->date);
        sb_append(sb, ",\"white\":");
        sb_json_str(sb, e->white);
        sb_append(sb, ",\"black\":");
        sb_json_str(sb, e->black);
        sb_append(sb, ",\"result\":");
        sb_json_str(sb, e->result);
        sb_append(sb, ",\"reason\":");
        sb_json_str(sb, e->reason);
        sb_append(sb, ",\"opening\":");
        sb_json_str(sb, e->opening);
        sb_append(sb, ",\"mode\":");
        sb_json_str(sb, e->mode);
        sb_append(sb, ",\"pgn\":");
        sb_json_str(sb, e->pgn ? e->pgn : "");
        sb_append(sb, "}");
    }
    sb_append(sb, "]}");
}

static void serialize_puzzles_list_json(const puzzle_stats *ps, strbuf *sb) {
    int n = puzzle_count();
    sb_printf(sb,
        "{\"ok\":true,\"stats\":{\"rating\":%d,\"streak\":%d,\"best_streak\":%d,"
        "\"solved_count\":%d,\"attempts\":%d,\"total\":%d},\"puzzles\":[",
        ps->rating, ps->streak, ps->best_streak, ps->solved_count, ps->attempts, n);
    for (int i = 0; i < n; i++) {
        const chess_puzzle *pz = puzzle_get(i);
        if (i > 0) {
            sb_append(sb, ",");
        }
        bool is_solved = (pz->id >= 1 && pz->id < 64) ? ps->solved[pz->id] : false;
        sb_printf(sb,
            "{\"id\":%d,\"rating\":%d,\"difficulty\":%d,\"moves\":%d,\"solved\":%s,\"title\":",
            pz->id, pz->rating, pz->difficulty, (pz->num_moves + 1) / 2,
            is_solved ? "true" : "false");
        sb_json_str(sb, pz->title);
        sb_append(sb, ",\"theme\":");
        sb_json_str(sb, pz->theme);
        sb_append(sb, ",\"description\":");
        sb_json_str(sb, pz->description);
        sb_append(sb, "}");
    }
    sb_append(sb, "]}");
}

static void uci_move_to_white_squares(const char *uci, int *from_sq, int *to_sq) {
    if (!uci || strlen(uci) < 4) {
        *from_sq = -1;
        *to_sq = -1;
        return;
    }
    *from_sq = (uci[1] - '1') * 8 + (uci[0] - 'a');
    *to_sq = (uci[3] - '1') * 8 + (uci[2] - 'a');
}

static void handle_puzzle_load(app_state *st, int puzzle_id, strbuf *sb) {
    const chess_puzzle *pz = puzzle_find_by_id(puzzle_id);
    if (!pz) {
        pz = puzzle_get(0);
    }
    if (!pz) {
        sb_append(sb, "{\"ok\":false,\"error\":\"No puzzles available.\"}");
        return;
    }
    st->mode = WMODE_PUZZLE;
    st->active_puzzle_id = pz->id;
    st->puzzle_step = 0;
    st->puzzle_failed = false;
    st->puzzle_solved = false;
    st->current_hist_id = 0;
    st->auto_saved_game = false;
    st->custom_names = true;
    snprintf(st->white_name, sizeof(st->white_name), "Puzzle #%d", pz->id);
    snprintf(st->black_name, sizeof(st->black_name), "%s", pz->title);

    set_from_fen(st->game, pz->fen);
    st->game->is_static = true;
    st->mainline_len = 0;
    engine_clear_tt(st->eng);
    st->eval_white_cp = compute_quick_eval(st);

    char msg[160];
    snprintf(msg, sizeof(msg), "Puzzle #%d: %s (Rating %d) — Find the best move for %s!", pz->id,
        pz->title, pz->rating, is_flipped(st->game->game) ? "Black" : "White");
    serialize_state_json(st, sb, "move", msg);
}

static void handle_puzzle_move(app_state *st, const char *body, strbuf *sb) {
    const chess_puzzle *pz = puzzle_find_by_id(st->active_puzzle_id);
    if (st->mode != WMODE_PUZZLE || !pz || st->puzzle_solved) {
        sb_append(sb, "{\"ok\":false,\"error\":\"No active puzzle.\"}");
        return;
    }

    int from_sq = json_get_int(body, "from", -1);
    int to_sq = json_get_int(body, "to", -1);
    char promo[8] = "";
    json_get_string(body, "promo", promo, sizeof(promo));
    if (from_sq < 0 || from_sq >= 64 || to_sq < 0 || to_sq >= 64) {
        sb_append(sb, "{\"ok\":false,\"error\":\"Invalid square.\"}");
        return;
    }
    char uci[8];
    if (promo[0] != '\0') {
        snprintf(uci, sizeof(uci), "%c%c%c%c%c", 'a' + (from_sq % 8), '1' + (from_sq / 8),
            'a' + (to_sq % 8), '1' + (to_sq / 8), (char) tolower((unsigned char) promo[0]));
    } else {
        snprintf(uci, sizeof(uci), "%c%c%c%c", 'a' + (from_sq % 8), '1' + (from_sq / 8),
            'a' + (to_sq % 8), '1' + (to_sq / 8));
    }

    u64 played = static_encode_string(st->game, uci);
    if (played == NULL_MOVE) {
        sb_append(sb, "{\"ok\":false,\"sound\":\"illegal\",\"error\":\"Illegal move.\"}");
        return;
    }

    const char *expected_uci = pz->moves[st->puzzle_step];
    bool is_correct = (strcmp(uci, expected_uci) == 0);

    // Also accept ANY alternative legal checkmate if we are on the final move of a mate puzzle!
    if (!is_correct && st->puzzle_step + 1 == pz->num_moves) {
        chess probe = *(st->game->game);
        chess_make_move(&probe, played);
        u64 next_legal[MAX_LEGAL_MOVES];
        if (set_legal_moves(&probe, next_legal) == 0 && in_check(&probe)) {
            is_correct = true;
        }
    }

    if (!is_correct) {
        int delta = 0;
        if (!st->puzzle_failed) {
            st->puzzle_failed = true;
            delta = puzzle_stats_record(&st->pstats, pz->id, pz->rating, false);
            puzzle_stats_save(PUZZLE_STATS_FILE, &st->pstats);
        }
        sb_printf(sb,
            "{\"ok\":true,\"status\":\"wrong\",\"sound\":\"illegal\",\"rating_delta\":%d,"
            "\"rating\":%d,\"streak\":%d,\"best_streak\":%d}",
            delta, st->pstats.rating, st->pstats.streak, st->pstats.best_streak);
        return;
    }

    // Correct move! Apply player's move
    char player_san[16];
    record_and_make_move(st, played, player_san, sizeof(player_san));
    const char *snd = move_sound_name(st->game, played);
    st->puzzle_step++;

    // Check if puzzle is now complete
    if (st->puzzle_step >= pz->num_moves || st->game->is_game_over) {
        st->puzzle_solved = true;
        int delta = 0;
        if (!st->puzzle_failed) {
            delta = puzzle_stats_record(&st->pstats, pz->id, pz->rating, true);
            puzzle_stats_save(PUZZLE_STATS_FILE, &st->pstats);
        } else if (pz->id >= 1 && pz->id < 64 && !st->pstats.solved[pz->id]) {
            st->pstats.solved[pz->id] = true;
            st->pstats.solved_count++;
            puzzle_stats_save(PUZZLE_STATS_FILE, &st->pstats);
        }
        st->eval_white_cp = compute_quick_eval(st);
        strbuf state_sb;
        sb_init(&state_sb, 4096);
        char msg[160];
        snprintf(msg, sizeof(msg), "Puzzle #%d Solved! (%s)", pz->id, player_san);
        serialize_state_json(st, &state_sb, "game_over", msg);
        sb_printf(sb,
            "{\"ok\":true,\"status\":\"solved\",\"sound\":\"game_over\",\"rating_delta\":%d,"
            "\"clean\":%s,\"state\":%s}",
            delta, st->puzzle_failed ? "false" : "true", state_sb.data);
        sb_free(&state_sb);
        return;
    }

    // Multi-move puzzle: play opponent's forced reply too and return both states cleanly!
    const char *reply_uci = pz->moves[st->puzzle_step];
    u64 reply_mv = static_encode_string(st->game, reply_uci);
    char reply_san[16] = "";
    int r_from = -1, r_to = -1;
    if (reply_mv != NULL_MOVE) {
        uci_move_to_white_squares(reply_uci, &r_from, &r_to);
        record_and_make_move(st, reply_mv, reply_san, sizeof(reply_san));
        st->puzzle_step++;
    }
    st->eval_white_cp = compute_quick_eval(st);

    strbuf state_sb;
    sb_init(&state_sb, 4096);
    char msg[160];
    snprintf(msg, sizeof(msg), "Nice! %s ... %s — keep going!", player_san, reply_san);
    serialize_state_json(st, &state_sb, snd, msg);
    sb_printf(sb,
        "{\"ok\":true,\"status\":\"continue\",\"sound\":\"%s\",\"reply_from\":%d,\"reply_to\":%d,"
        "\"reply_san\":",
        snd, r_from, r_to);
    sb_json_str(sb, reply_san);
    sb_printf(sb, ",\"state\":%s}", state_sb.data);
    sb_free(&state_sb);
}

// ============================================================================
// HTTP Handling & Static File Serving
// ============================================================================

static void send_all(int fd, const char *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n <= 0) {
            break;
        }
        sent += (size_t) n;
    }
}

static void send_http(int fd, const char *status, const char *ctype, const char *body, size_t len) {
    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n\r\n",
        status, ctype, len);
    if (hlen > 0) {
        send_all(fd, header, (size_t) hlen);
    }
    if (body && len > 0) {
        send_all(fd, body, len);
    }
}

static void send_file(int fd, const char *filepath, const char *ctype) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        const char *nf = "404 Not Found";
        send_http(fd, "404 Not Found", "text/plain", nf, strlen(nf));
        return;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz < 0) {
        fclose(fp);
        return;
    }
    char *buf = (char *) malloc((size_t) sz + 1);
    if (!buf) {
        fclose(fp);
        return;
    }
    size_t n = fread(buf, 1, (size_t) sz, fp);
    fclose(fp);
    send_http(fd, "200 OK", ctype, buf, n);
    free(buf);
}

static web_mode parse_mode_str(const char *mstr, web_mode fallback) {
    if (strcmp(mstr, "white") == 0) {
        return WMODE_WHITE;
    }
    if (strcmp(mstr, "black") == 0) {
        return WMODE_BLACK;
    }
    if (strcmp(mstr, "2p") == 0) {
        return WMODE_2P;
    }
    if (strcmp(mstr, "watch") == 0) {
        return WMODE_WATCH;
    }
    if (strcmp(mstr, "analysis") == 0) {
        return WMODE_ANALYSIS;
    }
    return fallback;
}

static void handle_client(int fd, app_state *st) {
    char req[131072];
    size_t total = 0;
    size_t header_end = 0;
    size_t content_len = 0;
    bool have_headers = false;

    while (total + 1 < sizeof(req)) {
        ssize_t n = recv(fd, req + total, sizeof(req) - 1 - total, 0);
        if (n <= 0) {
            break;
        }
        total += (size_t) n;
        req[total] = '\0';

        if (!have_headers) {
            char *sep = strstr(req, "\r\n\r\n");
            if (sep) {
                have_headers = true;
                header_end = (size_t) (sep - req) + 4;
                const char *cl = strstr(req, "Content-Length:");
                if (!cl) {
                    cl = strstr(req, "content-length:");
                }
                if (cl && cl < sep) {
                    content_len = (size_t) atoi(cl + 15);
                }
            }
        }
        if (have_headers && total >= header_end + content_len) {
            break;
        }
    }

    if (!have_headers) {
        return;
    }

    char method[16] = "", path[256] = "";
    if (sscanf(req, "%15s %255s", method, path) != 2) {
        return;
    }
    char *qmark = strchr(path, '?');
    if (qmark) {
        *qmark = '\0';
    }
    const char *body = req + header_end;

    // Static assets & SPA client-side routes (so Back/Forward & F5 refresh work on every URL!)
    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/style.css") == 0) {
            send_file(fd, "src/ui/web/static/style.css", "text/css; charset=utf-8");
            return;
        }
        if (strcmp(path, "/app.js") == 0) {
            send_file(fd, "src/ui/web/static/app.js", "application/javascript; charset=utf-8");
            return;
        }
        if (strcmp(path, "/favicon.ico") == 0 || strcmp(path, "/favicon.svg") == 0) {
            static const char kFaviconSvg[]
                = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'>"
                  "<defs><linearGradient id='g' x1='0' y1='0' x2='1' y2='1'>"
                  "<stop offset='0%' stop-color='#a371f7'/><stop offset='100%' stop-color='#58a6ff'/>"
                  "</linearGradient></defs>"
                  "<rect width='64' height='64' rx='14' fill='#0d1117'/>"
                  "<rect x='2' y='2' width='60' height='60' rx='12' fill='none' stroke='url(#g)' stroke-width='3'/>"
                  "<text x='32' y='47' font-size='40' text-anchor='middle' fill='#f0f6fc'>&#9822;</text>"
                  "</svg>";
            send_http(fd, "200 OK", "image/svg+xml", kFaviconSvg, sizeof(kFaviconSvg) - 1);
            return;
        }
        if (strncmp(path, "/api/", 5) != 0) {
            send_file(fd, "src/ui/web/static/index.html", "text/html; charset=utf-8");
            return;
        }
    }

    strbuf sb;
    sb_init(&sb, 4096);

    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/state") == 0) {
        serialize_state_json(st, &sb, "", "");
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/pgn") == 0) {
        char pgn[65536];
        static_get_pgn(st->game, st->white_name, st->black_name, pgn, sizeof(pgn));
        sb_append(&sb, "{\"ok\":true,\"pgn\":");
        sb_json_str(&sb, pgn);
        sb_append(&sb, "}");
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/new") == 0) {
        char mstr[32] = "";
        char fen_in[MAX_FEN_LENGTH] = "";
        if (json_get_string(body, "mode", mstr, sizeof(mstr))) {
            st->mode = parse_mode_str(mstr, st->mode);
        }
        int wl = json_get_int(body, "white_level", st->white_level);
        int bl = json_get_int(body, "black_level", st->black_level);
        int celo = json_get_int(body, "custom_elo", 0);
        int max_l = max_allowed_level(st);
        if (wl >= 1 && wl <= max_l) {
            st->white_level = wl;
        }
        if (bl >= 1 && bl <= max_l) {
            st->black_level = bl;
        }
        st->custom_elo = (celo >= 800 && celo <= 2600) ? celo : 0;
        char az_ck[256] = "";
        if (json_get_string(body, "az_checkpoint", az_ck, sizeof(az_ck))) {
            snprintf(st->az_checkpoint, sizeof(st->az_checkpoint), "%s", az_ck);
        } else {
            st->az_checkpoint[0] = '\0';
        }
        st->custom_names = false;
        update_player_names(st);

        full_chess_initialize(st->game);
        st->game->is_static = true;
        st->mainline_len = 0;
        st->current_hist_id = 0;
        st->auto_saved_game = false;
        st->active_puzzle_id = 0;
        if (json_get_string(body, "fen", fen_in, sizeof(fen_in)) && fen_in[0] != '\0') {
            set_from_fen(st->game, fen_in);
            st->game->is_static = true;
        }
        engine_clear_tt(st->eng);
        st->eval_white_cp = compute_quick_eval(st);
        serialize_state_json(st, &sb, "move", "Started a new game.");
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/mode") == 0) {
        char mstr[32] = "";
        if (json_get_string(body, "mode", mstr, sizeof(mstr))) {
            st->mode = parse_mode_str(mstr, st->mode);
        }
        int wl = json_get_int(body, "white_level", st->white_level);
        int bl = json_get_int(body, "black_level", st->black_level);
        int celo = json_get_int(body, "custom_elo", 0);
        int max_l = max_allowed_level(st);
        if (wl >= 1 && wl <= max_l) {
            st->white_level = wl;
        }
        if (bl >= 1 && bl <= max_l) {
            st->black_level = bl;
        }
        st->custom_elo = (celo >= 800 && celo <= 2600) ? celo : 0;
        char az_ck[256] = "";
        if (json_get_string(body, "az_checkpoint", az_ck, sizeof(az_ck))) {
            snprintf(st->az_checkpoint, sizeof(st->az_checkpoint), "%s", az_ck);
        }
        if (st->mode != WMODE_ANALYSIS) {
            st->custom_names = false;
            // Truncate future mainline if resuming play from an earlier analysis ply
            st->mainline_len = (int) st->game->move_count;
        }
        update_player_names(st);
        char msg[128];
        if (st->mode == WMODE_ANALYSIS) {
            snprintf(msg, sizeof(msg),
                "Entered Analysis Mode (ply %u/%d). Use arrows or click any move to explore.",
                (unsigned) st->game->move_count, st->mainline_len);
        } else {
            snprintf(msg, sizeof(msg), "Switched mode to %s.", mode_to_str(st->mode));
        }
        serialize_state_json(st, &sb, "", msg);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/goto") == 0) {
        int ply = json_get_int(body, "ply", (int) st->game->move_count);
        goto_ply(st, ply);
        st->eval_white_cp = compute_quick_eval(st);
        serialize_state_json(st, &sb, "move", "");
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/import_pgn") == 0) {
        char *pgn_in = (char *) malloc(65536);
        if (!pgn_in || !json_get_string(body, "pgn", pgn_in, 65536) || pgn_in[0] == '\0') {
            free(pgn_in);
            sb_append(&sb, "{\"ok\":false,\"error\":\"Please paste a PGN to import.\"}");
            send_http(fd, "200 OK", "application/json", sb.data, sb.len);
            sb_free(&sb);
            return;
        }
        char err[256] = "";
        if (!import_pgn_into_state(st, pgn_in, err, sizeof(err))) {
            sb_append(&sb, "{\"ok\":false,\"error\":");
            sb_json_str(&sb, err);
            sb_append(&sb, "}");
        } else {
            char msg[160];
            snprintf(msg, sizeof(msg), "Imported PGN into Analysis Mode (%d plies: %s vs %s).",
                st->mainline_len, st->white_name, st->black_name);
            serialize_state_json(st, &sb, "move", msg);
        }
        free(pgn_in);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/analyze") == 0) {
        bool explicit_no_sf = (strstr(body, "\"use_stockfish\":false") != NULL)
                              || (strstr(body, "\"use_stockfish\": false") != NULL);
        bool use_sf = !explicit_no_sf;
        handle_analyze_json(st, &sb, use_sf);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/fen") == 0) {
        char fen_in[MAX_FEN_LENGTH] = "";
        if (!json_get_string(body, "fen", fen_in, sizeof(fen_in)) || fen_in[0] == '\0') {
            sb_append(&sb, "{\"ok\":false,\"error\":\"Missing FEN string.\"}");
        } else {
            set_from_fen(st->game, fen_in);
            st->game->is_static = true;
            st->mainline_len = 0;
            engine_clear_tt(st->eng);
            st->eval_white_cp = compute_quick_eval(st);
            serialize_state_json(st, &sb, "move", "Loaded custom FEN.");
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/move") == 0) {
        if (st->game->is_game_over) {
            sb_append(
                &sb, "{\"ok\":false,\"sound\":\"illegal\",\"error\":\"Game is already over.\"}");
            send_http(fd, "200 OK", "application/json", sb.data, sb.len);
            sb_free(&sb);
            return;
        }

        char input[64] = "";
        int from_sq = json_get_int(body, "from", -1);
        int to_sq = json_get_int(body, "to", -1);
        char promo[8] = "";
        json_get_string(body, "promo", promo, sizeof(promo));

        u64 move = NULL_MOVE;
        int san_matches = 0;

        if (json_get_string(body, "input", input, sizeof(input)) && input[0] != '\0') {
            move = static_encode_san(st->game, input, &san_matches);
            if (san_matches > 1) {
                sb_printf(&sb,
                    "{\"ok\":false,\"sound\":\"illegal\",\"error\":\"Ambiguous move '%s' (%d "
                    "matches) — specify origin file or rank.\"}",
                    input, san_matches);
                send_http(fd, "200 OK", "application/json", sb.data, sb.len);
                sb_free(&sb);
                return;
            }
            if (move == NULL_MOVE) {
                move = static_encode_string(st->game, input);
            }
        } else if (from_sq >= 0 && from_sq < 64 && to_sq >= 0 && to_sq < 64) {
            char uci[8];
            if (promo[0] != '\0') {
                snprintf(uci, sizeof(uci), "%c%c%c%c%c", 'a' + (from_sq % 8), '1' + (from_sq / 8),
                    'a' + (to_sq % 8), '1' + (to_sq / 8), (char) tolower((unsigned char) promo[0]));
            } else {
                snprintf(uci, sizeof(uci), "%c%c%c%c", 'a' + (from_sq % 8), '1' + (from_sq / 8),
                    'a' + (to_sq % 8), '1' + (to_sq / 8));
            }
            move = static_encode_string(st->game, uci);
        }

        if (move == NULL_MOVE) {
            sb_append(&sb, "{\"ok\":false,\"sound\":\"illegal\",\"error\":\"Illegal move.\"}");
            send_http(fd, "200 OK", "application/json", sb.data, sb.len);
            sb_free(&sb);
            return;
        }

        char san[16];
        record_and_make_move(st, move, san, sizeof(san));
        const char *snd = move_sound_name(st->game, move);
        st->eval_white_cp = compute_quick_eval(st);
        char msg[128];
        if (st->game->is_game_over) {
            maybe_auto_save_completed_game(st);
            snprintf(msg, sizeof(msg), "Played %s. %s", san, game_over_reason_str(st->game));
        } else {
            snprintf(msg, sizeof(msg), "Played %s.", san);
        }
        serialize_state_json(st, &sb, snd, msg);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/bot_move") == 0) {
        if (st->game->is_game_over || num_legal_moves(st->game) == 0) {
            serialize_state_json(st, &sb, "", game_over_reason_str(st->game));
            send_http(fd, "200 OK", "application/json", sb.data, sb.len);
            sb_free(&sb);
            return;
        }
        bool black_turn = is_flipped(st->game->game);
        int lvl = black_turn ? st->black_level : st->white_level;
        char bot_label[64];
        search_result sr;
        if (is_apex_level(lvl)) {
            if (lvl == APEX_SYZYGY_LEVEL && !apex_syzygy_ready(st)) {
                sb_append(&sb,
                    "{\"ok\":false,\"error\":\"🔒 Apex-C + Syzygy requires 5-piece tablebases — "
                    "run 'make syzygy-5' to download data/syzygy/ (~939 MB).\"}");
                send_http(fd, "200 OK", "application/json", sb.data, sb.len);
                sb_free(&sb);
                return;
            }
            format_bot_name(st, lvl, bot_label, sizeof(bot_label));
            int syz_p = (lvl == APEX_SYZYGY_LEVEL) ? syzygy_installed_pieces() : 0;
            int pc_cnt = syzygy_piece_count(st->game->game);
            if (syz_p >= 3 && pc_cnt <= syz_p) {
                uci_engine *tb = apex_ensure_tb_oracle(st);
                if (tb) {
                    uci_search_result ur = uci_search_depth(tb, st->game, 6, 180);
                    sr = ur.base;
                } else {
                    memset(&sr, 0, sizeof(sr));
                }
            } else {
                uci_engine *ap = apex_ensure_uci(st);
                if (ap) {
                    uci_search_result ur = uci_search(ap, st->game, false, 0, 2600);
                    sr = ur.base;
                } else {
                    sr = engine_search(st->eng, st->game, 22, 2600);
                }
            }
        } else if (is_hybrid_level(lvl)) {
            uci_engine *hyb = hybrid_ensure_uci(st, lvl);
            if (!hyb) {
                if (lvl == HYBRID_NNUE_LEVEL && hybrid_is_unlocked()) {
                    sb_append(&sb,
                        "{\"ok\":false,\"error\":\"🔒 Singularity v5 requires running Knowledge "
                        "Distillation in Zero-C Mission Control to train data/chess_net_zero_v4.nnue!\"}");
                } else {
                    sb_append(&sb,
                        "{\"ok\":false,\"error\":\"🔒 Mythic Hybrid fighter is locked until Zero-C "
                        "v2 finishes training!\"}");
                }
                send_http(fd, "200 OK", "application/json", sb.data, sb.len);
                sb_free(&sb);
                return;
            }
            format_bot_name(st, lvl, bot_label, sizeof(bot_label));
            uci_search_result ur = uci_search(hyb, st->game, false, 0, 1800);
            sr = ur.base;
        } else if (lvl == AZ_V1_BOT_LEVEL) {
            uci_engine *az1 = az_ensure_v1_uci(st);
            if (!az1) {
                sb_append(&sb,
                    "{\"ok\":false,\"error\":\"Zero-C v1 Champion archive not found at "
                    "data/az_v1_gen4320/latest.pt.\"}");
                send_http(fd, "200 OK", "application/json", sb.data, sb.len);
                sb_free(&sb);
                return;
            }
            format_bot_name(st, lvl, bot_label, sizeof(bot_label));
            uci_search_result ur = uci_search(az1, st->game, false, 0, 2200);
            if (ur.base.nodes == 0) {
                // Auto-respawn if the UCI child process died unexpectedly
                uci_delete(&st->az_v1_uci);
                az1 = az_ensure_v1_uci(st);
                if (az1) {
                    ur = uci_search(az1, st->game, false, 0, 2200);
                }
            }
            sr = ur.base;
        } else if (lvl == AZ_BOT_LEVEL) {
            uci_engine *az = az_ensure_uci(st);
            if (!az) {
                sb_append(&sb,
                    "{\"ok\":false,\"error\":\"Zero-C unavailable — run 'make az-lib az-setup' "
                    "first.\"}");
                send_http(fd, "200 OK", "application/json", sb.data, sb.len);
                sb_free(&sb);
                return;
            }
            format_bot_name(st, lvl, bot_label, sizeof(bot_label));
            uci_search_result ur = uci_search(az, st->game, false, 0, 1800);
            if (ur.base.nodes == 0) {
                // Auto-respawn if the UCI child process died (e.g. during checkpoint update)
                uci_delete(&st->az_uci);
                st->az_last_ckpt[0] = '\0';
                az = az_ensure_uci(st);
                if (az) {
                    ur = uci_search(az, st->game, false, 0, 1800);
                }
            }
            sr = ur.base;
        } else if (st->sf && uci_is_stockfish_level(lvl)) {
            const sf_preset *sp = uci_get_preset(lvl);
            snprintf(bot_label, sizeof(bot_label), "%s", sp->name);
            // Probe curated GM opening book on first 8 plies so bot-vs-bot games stay varied!
            u64 bmove = (st->game->move_count < 8 && num_legal_moves(st->game) > 1)
                            ? book_probe(st->game, NULL, &st->book_rng)
                            : NULL_MOVE;
            if (bmove != NULL_MOVE) {
                memset(&sr, 0, sizeof(sr));
                sr.best_move = bmove;
                sr.depth_reached = 0;
                sr.nodes = 1;
                sr.elapsed_sec = 0.001;
            } else {
                uci_search_result ur = uci_search_level(st->sf, st->game, lvl);
                sr = ur.base;
            }
        } else if (st->custom_elo >= 800) {
            snprintf(bot_label, sizeof(bot_label), "Titan-C ~%d Elo", st->custom_elo);
            sr = engine_search_elo(st->eng, st->game, st->custom_elo);
        } else {
            const difficulty_preset *preset = engine_get_preset(lvl);
            snprintf(bot_label, sizeof(bot_label), "Titan-C %s", preset->name);
            sr = engine_search_level(st->eng, st->game, lvl);
        }
        int bot_eval_white_cp = engine_score_to_white(st->game->game, sr.score_cp);
        char bot_ev_str[16];
        engine_format_eval(bot_eval_white_cp, bot_ev_str, sizeof(bot_ev_str));

        char san[16];
        record_and_make_move(st, sr.best_move, san, sizeof(san));
        // Always update the Eval Bar using the objective Stockfish referee (not the bot's own eval)
        st->eval_white_cp = compute_quick_eval(st);
        if (st->game->is_game_over) {
            maybe_auto_save_completed_game(st);
        }
        const char *snd = move_sound_name(st->game, sr.best_move);
        char msg[160];
        snprintf(msg, sizeof(msg), "%s (%s) played %s  [d=%d, bot eval %s, %lu nodes, %.2fs]",
            black_turn ? "Black" : "White", bot_label, san, sr.depth_reached, bot_ev_str,
            (unsigned long) sr.nodes, sr.elapsed_sec);
        serialize_state_json(st, &sb, snd, msg);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/undo") == 0) {
        if (st->game->move_count == 0) {
            serialize_state_json(st, &sb, "illegal", "Nothing to undo.");
        } else {
            unmake_move(st->game);
            if ((st->mode == WMODE_WHITE || st->mode == WMODE_BLACK) && state_is_bot_turn(st)
                && st->game->move_count > 0) {
                unmake_move(st->game);
            }
            if (st->mode != WMODE_ANALYSIS) {
                st->mainline_len = (int) st->game->move_count;
            }
            st->eval_white_cp = compute_quick_eval(st);
            serialize_state_json(st, &sb, "move", "Move undone.");
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/hint") == 0) {
        if (st->game->is_game_over) {
            sb_append(&sb, "{\"ok\":false,\"error\":\"Game is already over.\"}");
        } else {
            search_result hr;
            if (st->sf_ref) {
                hr = uci_search_depth(st->sf_ref, st->game, 15, 250).base;
            } else {
                hr = engine_search(st->eng, st->game, 6, 300);
            }
            st->eval_white_cp = engine_score_to_white(st->game->game, hr.score_cp);
            char san[16], ev[16];
            static_move_to_san(
                st->game->game, st->game->current_legal_moves, hr.best_move, san, sizeof(san));
            engine_format_eval(st->eval_white_cp, ev, sizeof(ev));
            bool flipped = is_flipped(st->game->game);
            int s = get_start_offset(hr.best_move);
            int e = get_end_offset(hr.best_move);
            int ws = 8 * (flipped ? (7 - (s / 8)) : (s / 8)) + (s % 8);
            int we = 8 * (flipped ? (7 - (e / 8)) : (e / 8)) + (e % 8);
            sb_printf(&sb, "{\"ok\":true,\"from\":%d,\"to\":%d,\"san\":", ws, we);
            sb_json_str(&sb, san);
            sb_printf(&sb, ",\"depth\":%d,\"eval_str\":", hr.depth_reached);
            sb_json_str(&sb, ev);
            sb_printf(&sb, ",\"eval_cp\":%d,\"win_prob\":%.4f}", st->eval_white_cp,
                engine_win_prob(st->eval_white_cp));
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/review") == 0) {
        handle_review_json(st, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/gauntlet/status") == 0) {
        gauntlet_status gs;
        gauntlet_snapshot(st->gt, &gs);
        serialize_gauntlet_json(&gs, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/gauntlet/start") == 0) {
        int games = json_get_int(body, "games", 6);
        int threads = json_get_int(body, "threads", 0);
        int only_level = json_get_int(body, "only_level", 0);
        bool use_sf = (strstr(body, "\"use_stockfish\":false") == NULL)
                      && (strstr(body, "\"use_stockfish\": false") == NULL);
        gauntlet_start(st->gt, games, threads, only_level, use_sf, 500);
        gauntlet_status gs;
        gauntlet_snapshot(st->gt, &gs);
        serialize_gauntlet_json(&gs, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/gauntlet/calc") == 0) {
        int subj_level = json_get_int(body, "subject_level", GAUNTLET_ZERO_C_LEVEL);
        int custom_elo = json_get_int(body, "custom_elo", 0);
        int target_ci = json_get_int(body, "target_ci", 90);
        int max_games = json_get_int(body, "max_games", 40);
        int threads = json_get_int(body, "threads", 0);
        char az_ck[256] = "";
        json_get_string(body, "az_checkpoint", az_ck, sizeof(az_ck));
        if (!gauntlet_start_calculator(
                st->gt, subj_level, custom_elo, az_ck, target_ci, max_games, threads, 450)) {
            sb_append(&sb,
                "{\"ok\":false,\"error\":\"Could not start Elo Calculator (ensure Stockfish is "
                "installed and no gauntlet is currently running).\"}");
        } else {
            gauntlet_status gs;
            gauntlet_snapshot(st->gt, &gs);
            serialize_gauntlet_json(&gs, &sb);
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/gauntlet/stop") == 0) {
        gauntlet_stop(st->gt);
        gauntlet_status gs;
        gauntlet_snapshot(st->gt, &gs);
        serialize_gauntlet_json(&gs, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/az/status") == 0) {
        handle_az_status_json(st, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/az/start") == 0) {
        char arch[32] = "standard";
        json_get_string(body, "arch", arch, sizeof(arch));
        if (strcmp(arch, "tiny") != 0 && strcmp(arch, "standard") != 0 && strcmp(arch, "strong") != 0
            && strcmp(arch, "pro") != 0 && strcmp(arch, "master") != 0) {
            snprintf(arch, sizeof(arch), "pro");
        }
        int games = json_get_int(body, "games", 256);
        int fast_sims = json_get_int(body, "fast_sims", 36);
        int full_sims = json_get_int(body, "full_sims", 200);
        int gpg = json_get_int(body, "games_per_gen", 128);
        int max_gens = json_get_int(body, "max_gens", 0);
        bool reset
            = (strstr(body, "\"reset\":true") != NULL) || (strstr(body, "\"reset\": true") != NULL);
        bool full_throttle = (strstr(body, "\"full_throttle\":true") != NULL)
                             || (strstr(body, "\"full_throttle\": true") != NULL);
        if (!az_start_training(
                st, arch, games, fast_sims, full_sims, gpg, max_gens, reset, full_throttle)) {
            sb_append(&sb,
                "{\"ok\":false,\"error\":\"Could not start Zero-C trainer (run 'make az-lib "
                "az-setup').\"}");
        } else {
            handle_az_status_json(st, &sb);
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/az/stop") == 0) {
        az_stop_training(st);
        handle_az_status_json(st, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/az/complete") == 0) {
        bool unlock = (strstr(body, "\"unlock\":false") == NULL)
                      && (strstr(body, "\"unlock\": false") == NULL);
        if (unlock) {
            FILE *ff = fopen(HYBRID_UNLOCK_FLAG, "wb");
            if (ff) {
                fprintf(ff, "{\"completed_at\":\"manual\",\"reason\":\"Graduated via Mission Control\"}\n");
                fclose(ff);
            }
        } else {
            remove(HYBRID_UNLOCK_FLAG);
            uci_delete(&st->hybrid_uci);
            st->hybrid_last_mode[0] = '\0';
        }
        handle_az_status_json(st, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/az/distill") == 0) {
        if (st->az_running) {
            sb_append(&sb,
                "{\"ok\":false,\"error\":\"Pause Zero-C self-play training first so Distillation "
                "has full RTX GPU & replay.npz access.\"}");
        } else if (!distill_start(st)) {
            sb_append(&sb,
                "{\"ok\":false,\"error\":\"Cannot start Distillation — ensure Zero-C v2 is "
                "complete and data/az/latest.pt + replay.npz exist.\"}");
        } else {
            handle_az_status_json(st, &sb);
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/az/distill/stop") == 0) {
        distill_stop(st);
        handle_az_status_json(st, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/p3k/status") == 0) {
        handle_p3k_status_json(st, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/p3k/datagen/start") == 0) {
        int threads = json_get_int(body, "threads", 2);
        int pos_m = json_get_int(body, "millions", 25);
        int nodes = json_get_int(body, "nodes", 5000);
        unsigned long long target_pos
            = (unsigned long long) ((pos_m < 1) ? 1 : pos_m) * 1000000ull;
        if (!p3k_start_datagen(st, threads, target_pos, nodes)) {
            sb_append(&sb,
                "{\"ok\":false,\"error\":\"build/datagen not found — run 'make datagen' first.\"}");
        } else {
            handle_p3k_status_json(st, &sb);
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/p3k/datagen/stop") == 0) {
        p3k_stop_child(&st->dg_pid, &st->dg_running);
        handle_p3k_status_json(st, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/p3k/spsa/start") == 0) {
        int threads = json_get_int(body, "threads", 2);
        int iters = json_get_int(body, "iters", 150);
        int nodes = json_get_int(body, "nodes", 12000);
        if (!p3k_start_spsa(st, threads, iters, nodes)) {
            sb_append(&sb, "{\"ok\":false,\"error\":\"Could not start SPSA tuner.\"}");
        } else {
            handle_p3k_status_json(st, &sb);
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/p3k/spsa/stop") == 0) {
        p3k_stop_child(&st->spsa_pid, &st->spsa_running);
        handle_p3k_status_json(st, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/p3k/nnue/start") == 0) {
        az_check_alive(st);
        int epochs = json_get_int(body, "epochs", 24);
        if (st->az_running) {
            sb_append(&sb,
                "{\"ok\":false,\"error\":\"Pause Zero-C v2 first so Big NNUE v4 has full RTX GPU "
                "VRAM (~20 min)!\"}");
        } else if (!p3k_start_nnue(st, epochs)) {
            sb_append(&sb,
                "{\"ok\":false,\"error\":\"No training data found at data/datagen_v4.bin — run "
                "Self-Play Datagen first!\"}");
        } else {
            handle_p3k_status_json(st, &sb);
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/p3k/nnue/stop") == 0) {
        p3k_stop_child(&st->nnue_pid, &st->nnue_running);
        handle_p3k_status_json(st, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/p3k/syzygy/start") == 0) {
        if (!p3k_start_syzygy(st)) {
            sb_append(&sb, "{\"ok\":false,\"error\":\"Could not launch Syzygy downloader.\"}");
        } else {
            handle_p3k_status_json(st, &sb);
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/p3k/syzygy/stop") == 0) {
        p3k_stop_child(&st->syz_pid, &st->syz_running);
        handle_p3k_status_json(st, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/toy/status") == 0) {
        handle_toy_status_json(st, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/toy/start") == 0) {
        az_check_alive(st);
        if (st->az_running) {
            sb_append(&sb,
                "{\"ok\":false,\"error\":\"Pause Zero-C v2 first before running the Toy Net on "
                "your GPU!\"}");
        } else {
            char arch[32] = "tiny";
            json_get_string(body, "arch", arch, sizeof(arch));
            if (strcmp(arch, "tiny") != 0 && strcmp(arch, "standard") != 0) {
                snprintf(arch, sizeof(arch), "tiny");
            }
            bool reset = (strstr(body, "\"reset\":true") != NULL)
                         || (strstr(body, "\"reset\": true") != NULL);
            if (!toy_start_training(st, arch, reset)) {
                sb_append(&sb, "{\"ok\":false,\"error\":\"Could not start Toy Net trainer.\"}");
            } else {
                handle_toy_status_json(st, &sb);
            }
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/toy/stop") == 0) {
        p3k_stop_child(&st->toy_pid, &st->toy_running);
        handle_toy_status_json(st, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/az/reset") == 0) {
        az_reset_files(st);
        handle_az_status_json(st, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/history") == 0) {
        serialize_history_json(&st->hist, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/history/save") == 0) {
        if (st->mainline_len <= 0 || st->mode == WMODE_PUZZLE) {
            sb_append(&sb, "{\"ok\":false,\"error\":\"Play at least 1 move before saving.\"}");
        } else {
            st->current_hist_id = 0; // force a fresh snapshot on explicit save
            save_current_game_to_history(st, 0, 0);
            sb_printf(
                &sb, "{\"ok\":true,\"id\":%d,\"count\":%d}", st->current_hist_id, st->hist.count);
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/history/load") == 0) {
        int id = json_get_int(body, "id", 0);
        const history_entry *he = history_find_by_id(&st->hist, id);
        if (!he || !he->pgn || he->pgn[0] == '\0') {
            sb_append(&sb, "{\"ok\":false,\"error\":\"Game not found in history.\"}");
        } else {
            char err[256] = "";
            if (!import_pgn_into_state(st, he->pgn, err, sizeof(err))) {
                sb_append(&sb, "{\"ok\":false,\"error\":");
                sb_json_str(&sb, err);
                sb_append(&sb, "}");
            } else {
                st->current_hist_id = he->id;
                st->auto_saved_game = true;
                char msg[160];
                snprintf(msg, sizeof(msg), "Loaded Game #%d (%s vs %s, %s) into Analysis Mode.",
                    he->id, he->white, he->black, he->result);
                serialize_state_json(st, &sb, "move", msg);
            }
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/history/delete") == 0) {
        int id = json_get_int(body, "id", 0);
        if (history_delete_by_id(&st->hist, id)) {
            if (st->current_hist_id == id) {
                st->current_hist_id = 0;
            }
            history_save(GAME_HISTORY_FILE, &st->hist);
        }
        serialize_history_json(&st->hist, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/history/clear") == 0) {
        history_clear(&st->hist);
        st->current_hist_id = 0;
        history_save(GAME_HISTORY_FILE, &st->hist);
        serialize_history_json(&st->hist, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/puzzles") == 0) {
        serialize_puzzles_list_json(&st->pstats, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/puzzles/load") == 0) {
        int pid = json_get_int(body, "id", 1);
        handle_puzzle_load(st, pid, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/puzzles/move") == 0) {
        handle_puzzle_move(st, body, &sb);
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/puzzles/hint") == 0) {
        const chess_puzzle *pz = puzzle_find_by_id(st->active_puzzle_id);
        if (st->mode != WMODE_PUZZLE || !pz || st->puzzle_solved
            || st->puzzle_step >= pz->num_moves) {
            sb_append(&sb, "{\"ok\":false,\"error\":\"No active puzzle.\"}");
        } else {
            int fsq = -1, tsq = -1;
            uci_move_to_white_squares(pz->moves[st->puzzle_step], &fsq, &tsq);
            sb_printf(&sb, "{\"ok\":true,\"from\":%d,\"to\":%d,\"hint\":", fsq, tsq);
            sb_json_str(&sb, pz->hint);
            sb_append(&sb, "}");
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/puzzles/solve") == 0) {
        const chess_puzzle *pz = puzzle_find_by_id(st->active_puzzle_id);
        if (st->mode != WMODE_PUZZLE || !pz) {
            sb_append(&sb, "{\"ok\":false,\"error\":\"No active puzzle.\"}");
        } else {
            if (!st->puzzle_solved && !st->puzzle_failed) {
                st->puzzle_failed = true;
                puzzle_stats_record(&st->pstats, pz->id, pz->rating, false);
                puzzle_stats_save(PUZZLE_STATS_FILE, &st->pstats);
            }
            sb_append(&sb, "{\"ok\":true,\"solution\":[");
            int first_sol = 1;
            while (st->puzzle_step < pz->num_moves && !st->game->is_game_over) {
                const char *muci = pz->moves[st->puzzle_step];
                u64 mv = static_encode_string(st->game, muci);
                if (mv == NULL_MOVE) {
                    break;
                }
                char san[16];
                record_and_make_move(st, mv, san, sizeof(san));
                if (!first_sol) {
                    sb_append(&sb, ",");
                }
                first_sol = 0;
                sb_json_str(&sb, san);
                st->puzzle_step++;
            }
            st->puzzle_solved = true;
            st->eval_white_cp = compute_quick_eval(st);
            strbuf state_sb;
            sb_init(&state_sb, 4096);
            serialize_state_json(st, &state_sb, "move", "Solution revealed.");
            sb_printf(&sb, "],\"state\":%s}", state_sb.data);
            sb_free(&state_sb);
        }
        send_http(fd, "200 OK", "application/json", sb.data, sb.len);
    } else {
        const char *nf = "{\"ok\":false,\"error\":\"Not found\"}";
        send_http(fd, "404 Not Found", "application/json", nf, strlen(nf));
    }

    sb_free(&sb);
}

int main(int argc, char **argv) {
    int port = 8080;
    const char *sf_path = NULL;
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--stockfish") == 0 && i + 1 < argc) {
            sf_path = argv[++i];
        }
    }

    srand((unsigned) time(NULL));
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

    app_state st;
    memset(&st, 0, sizeof(st));
    {
        struct timespec bts;
        clock_gettime(CLOCK_MONOTONIC, &bts);
        st.book_rng = ((u64) time(NULL) << 32) ^ (u64) bts.tv_nsec ^ 0x9e3779b97f4a7c15ull;
    }
    st.game = full_chess_create();
    st.game->is_static = true;
    st.eng = engine_create(16);
    engine_set_threads(st.eng, 4);
    st.sf = uci_create(sf_path);
    st.sf_ref = uci_create(sf_path);
    if (st.sf_ref) {
        uci_set_option(st.sf_ref, "Threads", "1");
        uci_set_option(st.sf_ref, "Hash", "32");
    }
    st.gt = gauntlet_create(sf_path, ELO_RATINGS_FILE);
    history_init(&st.hist);
    history_load(GAME_HISTORY_FILE, &st.hist);
    puzzle_stats_load(PUZZLE_STATS_FILE, &st.pstats);
    st.mode = WMODE_WHITE;
    st.white_level = 4;
    st.black_level = 4;
    update_player_names(&st);
    st.eval_white_cp = compute_quick_eval(&st);

    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        perror("socket");
        gauntlet_delete(&st.gt);
        uci_delete(&st.sf_ref);
        uci_delete(&st.sf);
        engine_delete(&st.eng);
        full_chess_delete(&st.game);
        return 1;
    }

    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t) port);

    if (bind(g_server_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("bind");
        close(g_server_fd);
        gauntlet_delete(&st.gt);
        uci_delete(&st.sf_ref);
        uci_delete(&st.sf);
        engine_delete(&st.eng);
        full_chess_delete(&st.game);
        return 1;
    }

    if (listen(g_server_fd, 16) < 0) {
        perror("listen");
        close(g_server_fd);
        gauntlet_delete(&st.gt);
        uci_delete(&st.sf_ref);
        uci_delete(&st.sf);
        engine_delete(&st.eng);
        full_chess_delete(&st.game);
        return 1;
    }

    printf("\n  +========================================================+\n");
    printf("  |           CHESS-IN-C  ::  WEB STUDIO SERVER            |\n");
    printf("  +========================================================+\n");
    printf("  Open in your browser:  http://127.0.0.1:%d\n", port);
    if (st.sf) {
        printf("  Stockfish detected:    %s (%s)\n", uci_get_name(st.sf), uci_get_path(st.sf));
    } else {
        printf(
            "  Stockfish:             not installed (install via `sudo apt install stockfish`)\n");
    }
    printf("  Press Ctrl+C to stop.\n\n");
    fflush(stdout);

    while (g_running) {
        int client_fd = accept(g_server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (!g_running) {
                break;
            }
            continue;
        }
        handle_client(client_fd, &st);
        close(client_fd);
    }

    if (g_server_fd >= 0) {
        close(g_server_fd);
    }
    p3k_stop_child(&st.syz_pid, &st.syz_running);
    p3k_stop_child(&st.toy_pid, &st.toy_running);
    p3k_stop_child(&st.dg_pid, &st.dg_running);
    p3k_stop_child(&st.spsa_pid, &st.spsa_running);
    p3k_stop_child(&st.nnue_pid, &st.nnue_running);
    distill_stop(&st);
    az_stop_training(&st);
    uci_delete(&st.sf_tb);
    uci_delete(&st.apex_uci);
    uci_delete(&st.hybrid_uci);
    uci_delete(&st.az_v1_uci);
    uci_delete(&st.az_uci);
    history_free(&st.hist);
    gauntlet_delete(&st.gt);
    uci_delete(&st.sf_ref);
    uci_delete(&st.sf);
    engine_delete(&st.eng);
    full_chess_delete(&st.game);
    printf("\n  Server stopped cleanly.\n");
    return 0;
}
