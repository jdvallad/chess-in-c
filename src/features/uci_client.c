#define _POSIX_C_SOURCE 200809L

#include "features/uci_client.h"

#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/notation.h"
#include "core/primitives.h"

#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct uci_engine {
    pid_t pid;
    FILE *to_child;
    FILE *from_child;
    char path[512];
    char name[128];
    bool has_configured;
    bool last_limit_strength;
    int last_elo;
    int uci_elo_min;
    int uci_elo_max;
};

static const sf_preset g_sf_presets[] = {
    { 7, "Stockfish 1350", "Stockfish UCI_Elo 1350 (0.2s)", true, 1350, 200 },
    { 8, "Stockfish 1600", "Stockfish UCI_Elo 1600 (0.25s)", true, 1600, 250 },
    { 9, "Stockfish 1900", "Stockfish UCI_Elo 1900 (0.3s)", true, 1900, 300 },
    { 10, "Stockfish 2200", "Stockfish UCI_Elo 2200 (0.4s)", true, 2200, 400 },
    { 11, "Stockfish Max", "Full-strength Stockfish (~3500+ Elo, 0.5s)", false, 3190, 500 },
};

int uci_first_level(void) {
    return 7;
}

int uci_num_presets(void) {
    return (int) (sizeof(g_sf_presets) / sizeof(g_sf_presets[0]));
}

const sf_preset *uci_get_preset(int level) {
    int idx = level - uci_first_level();
    int n = uci_num_presets();
    if (idx < 0) {
        idx = 0;
    }
    if (idx >= n) {
        idx = n - 1;
    }
    return &g_sf_presets[idx];
}

bool uci_is_stockfish_level(int level) {
    return level >= uci_first_level() && level < uci_first_level() + uci_num_presets();
}

static bool is_executable_file(const char *path) {
    return path && path[0] != '\0' && access(path, X_OK) == 0;
}

bool uci_find_stockfish(const char *explicit_path, char *out_path, size_t out_size) {
    if (!out_path || out_size == 0) {
        return false;
    }
    out_path[0] = '\0';

    if (is_executable_file(explicit_path)) {
        snprintf(out_path, out_size, "%s", explicit_path);
        return true;
    }

    const char *env_path = getenv("STOCKFISH_PATH");
    if (is_executable_file(env_path)) {
        snprintf(out_path, out_size, "%s", env_path);
        return true;
    }

    const char *candidates[] = {
        "./stockfish",
        "/usr/games/stockfish",
        "/usr/bin/stockfish",
        "/usr/local/bin/stockfish",
        "/snap/bin/stockfish",
        "/opt/homebrew/bin/stockfish",
    };
    int nc = (int) (sizeof(candidates) / sizeof(candidates[0]));
    for (int i = 0; i < nc; i++) {
        if (is_executable_file(candidates[i])) {
            snprintf(out_path, out_size, "%s", candidates[i]);
            return true;
        }
    }

    const char *path_env = getenv("PATH");
    if (path_env) {
        const char *p = path_env;
        while (*p) {
            const char *colon = strchr(p, ':');
            size_t len = colon ? (size_t) (colon - p) : strlen(p);
            if (len > 0 && len + 12 < out_size) {
                char probe[512];
                snprintf(probe, sizeof(probe), "%.*s/stockfish", (int) len, p);
                if (is_executable_file(probe)) {
                    snprintf(out_path, out_size, "%s", probe);
                    return true;
                }
            }
            if (!colon) {
                break;
            }
            p = colon + 1;
        }
    }
    return false;
}

static void uci_send(uci_engine *eng, const char *cmd) {
    if (!eng || !eng->to_child) {
        return;
    }
    fputs(cmd, eng->to_child);
    fflush(eng->to_child);
}

static bool uci_wait_token(uci_engine *eng, const char *token) {
    char line[4096];
    while (fgets(line, (int) sizeof(line), eng->from_child)) {
        if (strncmp(line, "id name ", 8) == 0) {
            char *nl = strpbrk(line + 8, "\r\n");
            if (nl) {
                *nl = '\0';
            }
            snprintf(eng->name, sizeof(eng->name), "%s", line + 8);
        } else if (strncmp(line, "option name UCI_Elo type spin", 29) == 0) {
            const char *pmin = strstr(line, " min ");
            const char *pmax = strstr(line, " max ");
            if (pmin) {
                int mn = atoi(pmin + 5);
                if (mn >= 400 && mn <= 3000) {
                    eng->uci_elo_min = mn;
                }
            }
            if (pmax) {
                int mx = atoi(pmax + 5);
                if (mx >= eng->uci_elo_min && mx <= 4000) {
                    eng->uci_elo_max = mx;
                }
            }
        }
        if (strncmp(line, token, strlen(token)) == 0) {
            return true;
        }
    }
    return false;
}

uci_engine *uci_create(const char *binary_path) {
    char resolved[512];
    if (!uci_find_stockfish(binary_path, resolved, sizeof(resolved))) {
        return NULL;
    }

    int in_pipe[2];
    int out_pipe[2];
    if (pipe(in_pipe) != 0) {
        return NULL;
    }
    if (pipe(out_pipe) != 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        return NULL;
    }

    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        char *argv[2];
        argv[0] = resolved;
        argv[1] = NULL;
        execv(resolved, argv);
        _exit(127);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);

    uci_engine *eng = (uci_engine *) calloc(1, sizeof(uci_engine));
    if (!eng) {
        close(in_pipe[1]);
        close(out_pipe[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return NULL;
    }
    eng->pid = pid;
    eng->to_child = fdopen(in_pipe[1], "w");
    eng->from_child = fdopen(out_pipe[0], "r");
    eng->uci_elo_min = 1350;
    eng->uci_elo_max = 2850;
    snprintf(eng->path, sizeof(eng->path), "%s", resolved);
    snprintf(eng->name, sizeof(eng->name), "Stockfish");

    if (!eng->to_child || !eng->from_child) {
        uci_delete(&eng);
        return NULL;
    }

    uci_send(eng, "uci\n");
    if (!uci_wait_token(eng, "uciok")) {
        uci_delete(&eng);
        return NULL;
    }
    uci_send(eng, "isready\n");
    if (!uci_wait_token(eng, "readyok")) {
        uci_delete(&eng);
        return NULL;
    }
    return eng;
}

void uci_new_game(uci_engine *eng) {
    if (!eng) {
        return;
    }
    uci_send(eng, "ucinewgame\nisready\n");
    (void) uci_wait_token(eng, "readyok");
}

void uci_set_option(uci_engine *eng, const char *name, const char *value) {
    if (!eng || !name || !value) {
        return;
    }
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "setoption name %s value %s\nisready\n", name, value);
    uci_send(eng, cmd);
    (void) uci_wait_token(eng, "readyok");
}

void uci_delete(uci_engine **eng) {
    if (!eng || !*eng) {
        return;
    }
    uci_engine *e = *eng;
    if (e->to_child) {
        uci_send(e, "quit\n");
        fclose(e->to_child);
        e->to_child = NULL;
    }
    if (e->from_child) {
        fclose(e->from_child);
        e->from_child = NULL;
    }
    if (e->pid > 0) {
        int status = 0;
        if (waitpid(e->pid, &status, WNOHANG) == 0) {
            kill(e->pid, SIGTERM);
            waitpid(e->pid, &status, 0);
        }
        e->pid = 0;
    }
    free(e);
    *eng = NULL;
}

const char *uci_get_name(const uci_engine *eng) {
    return eng ? eng->name : "";
}

const char *uci_get_path(const uci_engine *eng) {
    return eng ? eng->path : "";
}

int uci_get_min_elo(const uci_engine *eng) {
    return eng ? eng->uci_elo_min : 1350;
}

int uci_get_max_elo(const uci_engine *eng) {
    return eng ? eng->uci_elo_max : 2850;
}

int uci_apply_spsa_file(uci_engine *eng, const char *json_path) {
    if (!eng || !json_path || !json_path[0]) {
        return 0;
    }
    FILE *fp = fopen(json_path, "rb");
    if (!fp) {
        return 0;
    }
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';

    const char *p = strstr(buf, "\"params\"");
    p = p ? strchr(p, '{') : strchr(buf, '{');
    if (!p) {
        return 0;
    }
    const char *end = strchr(p, '}');
    if (!end) {
        return 0;
    }

    int applied = 0;
    while (p < end) {
        const char *q1 = strchr(p, '"');
        if (!q1 || q1 >= end) {
            break;
        }
        const char *q2 = strchr(q1 + 1, '"');
        if (!q2 || q2 >= end) {
            break;
        }
        size_t klen = (size_t) (q2 - (q1 + 1));
        char key[64];
        if (klen > 0 && klen < sizeof(key)) {
            memcpy(key, q1 + 1, klen);
            key[klen] = '\0';
            const char *colon = strchr(q2 + 1, ':');
            if (colon && colon < end) {
                const char *vp = colon + 1;
                while (*vp == ' ' || *vp == '\t' || *vp == '\r' || *vp == '\n') {
                    vp++;
                }
                if (*vp == '-' || (*vp >= '0' && *vp <= '9')) {
                    int val = atoi(vp);
                    char vstr[24];
                    snprintf(vstr, sizeof(vstr), "%d", val);
                    uci_set_option(eng, key, vstr);
                    applied++;
                }
            }
        }
        p = q2 + 1;
    }
    return applied;
}

static void parse_info_line(const char *line, int *depth, int *score_cp, u64 *nodes,
    char pv_tokens[UCI_MAX_PV][8], int *pv_count) {
    const char *p = line;
    const char *tok;

    tok = strstr(p, " depth ");
    if (tok) {
        *depth = atoi(tok + 7);
    }

    tok = strstr(p, " nodes ");
    if (tok) {
        *nodes = (u64) strtoull(tok + 7, NULL, 10);
    }

    tok = strstr(p, " score cp ");
    if (tok) {
        *score_cp = atoi(tok + 10);
    } else {
        tok = strstr(p, " score mate ");
        if (tok) {
            int m = atoi(tok + 12);
            if (m > 0) {
                *score_cp = MATE_SCORE - (2 * m - 1);
            } else if (m < 0) {
                *score_cp = -MATE_SCORE - (2 * m);
            } else {
                *score_cp = -MATE_SCORE;
            }
        }
    }

    tok = strstr(p, " pv ");
    if (tok) {
        tok += 4;
        int n = 0;
        while (*tok && *tok != '\r' && *tok != '\n' && n < UCI_MAX_PV) {
            while (*tok == ' ') {
                tok++;
            }
            if (*tok == '\0' || *tok == '\r' || *tok == '\n') {
                break;
            }
            int k = 0;
            while (tok[k] && tok[k] != ' ' && tok[k] != '\r' && tok[k] != '\n' && k < 7) {
                pv_tokens[n][k] = tok[k];
                k++;
            }
            pv_tokens[n][k] = '\0';
            if (k >= 4) {
                n++;
            }
            tok += k;
        }
        if (n > 0) {
            *pv_count = n;
        }
    }
}

static void chess_board_to_fen(const chess *b, u8 half_moves, u16 full_moves, char *fen) {
    chess wv = *b;
    bool btm = is_flipped(&wv);
    if (btm) {
        flip_perspective(&wv);
    }

    int idx = 0;
    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            u8 pc = get_piece_at_offset(&wv, rank * 8 + file);
            if (pc == NONE) {
                empty++;
            } else {
                if (empty > 0) {
                    fen[idx++] = (char) ('0' + empty);
                    empty = 0;
                }
                fen[idx++] = piece_to_char(pc);
            }
        }
        if (empty > 0) {
            fen[idx++] = (char) ('0' + empty);
        }
        if (rank > 0) {
            fen[idx++] = '/';
        }
    }

    fen[idx++] = ' ';
    fen[idx++] = btm ? 'b' : 'w';
    fen[idx++] = ' ';

    int cstart = idx;
    if (wv.pawns & FRIENDLY & KINGSIDE) {
        fen[idx++] = 'K';
    }
    if (wv.pawns & FRIENDLY & QUEENSIDE) {
        fen[idx++] = 'Q';
    }
    if (wv.pawns & ENEMY & KINGSIDE) {
        fen[idx++] = 'k';
    }
    if (wv.pawns & ENEMY & QUEENSIDE) {
        fen[idx++] = 'q';
    }
    if (idx == cstart) {
        fen[idx++] = '-';
    }

    fen[idx++] = ' ';
    if (has_en_passant(b)) {
        int ep_file = get_en_passant_offset(b) - 40;
        fen[idx++] = (char) ('a' + ep_file);
        fen[idx++] = btm ? '3' : '6';
    } else {
        fen[idx++] = '-';
    }

    snprintf(fen + idx, MAX_FEN_LENGTH - (size_t) idx, " %u %u", (unsigned) half_moves,
        (unsigned) (full_moves > 0 ? full_moves : 1));
}

static void format_uci_move(const chess *board, u64 m, char out[8]) {
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

static uci_search_result uci_search_internal(uci_engine *eng, const full_chess *game,
    bool limit_strength, int elo, int depth_cap, int time_ms) {
    uci_search_result res;
    memset(&res, 0, sizeof(res));

    if (!eng || !game || num_legal_moves(game) == 0) {
        return res;
    }

    int elo_min = (eng->uci_elo_min > 0) ? eng->uci_elo_min : 1350;
    int elo_max = (eng->uci_elo_max >= elo_min) ? eng->uci_elo_max : 2850;
    if (elo < elo_min) {
        elo = elo_min;
    }
    if (elo > elo_max) {
        elo = elo_max;
    }

    if (!eng->has_configured || eng->last_limit_strength != limit_strength
        || (limit_strength && eng->last_elo != elo)) {
        char opt[128];
        snprintf(opt, sizeof(opt), "setoption name UCI_LimitStrength value %s\n",
            limit_strength ? "true" : "false");
        uci_send(eng, opt);
        if (limit_strength) {
            snprintf(opt, sizeof(opt), "setoption name UCI_Elo value %d\n", elo);
            uci_send(eng, opt);
        }
        uci_send(eng, "isready\n");
        if (!uci_wait_token(eng, "readyok")) {
            return res;
        }
        eng->has_configured = true;
        eng->last_limit_strength = limit_strength;
        eng->last_elo = elo;
    }

    char fen[MAX_FEN_LENGTH];
    get_fen(game, fen);

    // Send full game history (`position fen <init_fen> moves ...`) so Stockfish sees repetitions!
    char start_fen[MAX_FEN_LENGTH];
    chess_board_to_fen(
        &game->game_history[0], game->half_move_history[0], game->start_full_move, start_fen);

    size_t cmd_cap = 256 + (size_t) game->move_count * 7;
    char *cmd = (char *) malloc(cmd_cap);
    if (!cmd) {
        return res;
    }
    size_t pos = (size_t) snprintf(cmd, cmd_cap, "position fen %s", start_fen);
    if (game->move_count > 0) {
        pos += (size_t) snprintf(cmd + pos, cmd_cap - pos, " moves");
        for (int i = 0; i < game->move_count; i++) {
            char mv[8];
            format_uci_move(&game->game_history[i], game->move_history[i], mv);
            pos += (size_t) snprintf(cmd + pos, cmd_cap - pos, " %s", mv);
        }
    }
    int t_ms = (time_ms > 10) ? time_ms : 50;
    if (depth_cap > 0) {
        snprintf(cmd + pos, cmd_cap - pos, "\ngo depth %d movetime %d\n", depth_cap, t_ms);
    } else {
        snprintf(cmd + pos, cmd_cap - pos, "\ngo movetime %d\n", t_ms);
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    uci_send(eng, cmd);
    free(cmd);

    char line[4096];
    char best_uci[16] = "";
    char pv_tokens[UCI_MAX_PV][8];
    int pv_tok_count = 0;
    int depth = 0;
    int score_cp = 0;
    u64 nodes = 0;

    while (fgets(line, (int) sizeof(line), eng->from_child)) {
        if (strncmp(line, "info ", 5) == 0) {
            parse_info_line(line, &depth, &score_cp, &nodes, pv_tokens, &pv_tok_count);
        } else if (strncmp(line, "bestmove ", 9) == 0) {
            sscanf(line + 9, "%15s", best_uci);
            break;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    res.base.elapsed_sec
        = (double) (t1.tv_sec - t0.tv_sec) + (double) (t1.tv_nsec - t0.tv_nsec) * 1e-9;
    res.base.depth_reached = depth;
    res.base.score_cp = score_cp;
    res.base.nodes = nodes;

    u64 best_move = static_encode_string(game, best_uci);
    if (best_move == NULL_MOVE) {
        best_move = game->current_legal_moves[0];
    } else {
        res.ok = true;
    }
    res.base.best_move = best_move;

    // Convert UCI PV tokens into native u64 moves
    full_chess *tmp = full_chess_create();
    if (tmp && pv_tok_count > 0) {
        set_from_fen(tmp, fen);
        tmp->is_static = true;
        for (int i = 0; i < pv_tok_count && !tmp->is_game_over; i++) {
            u64 m = static_encode_string(tmp, pv_tokens[i]);
            if (m == NULL_MOVE) {
                break;
            }
            res.pv_moves[res.pv_len++] = m;
            make_move(tmp, m);
        }
    }
    full_chess_delete(&tmp);
    return res;
}

uci_search_result uci_search(
    uci_engine *eng, const full_chess *game, bool limit_strength, int elo, int time_ms) {
    return uci_search_internal(eng, game, limit_strength, elo, 0, time_ms);
}

uci_search_result uci_search_depth(
    uci_engine *eng, const full_chess *game, int depth, int time_ms) {
    return uci_search_internal(eng, game, false, 3190, depth, time_ms);
}

uci_search_result uci_search_level(uci_engine *eng, const full_chess *game, int level) {
    const sf_preset *p = uci_get_preset(level);
    return uci_search(eng, game, p->limit_strength, p->elo, p->time_ms);
}
