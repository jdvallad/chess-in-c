#define _POSIX_C_SOURCE 200809L
#include "az/az_mcts.h"

#include "core/attacks.h"
#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/notation.h"

#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// =========================================================================
// Fast Deterministic PRNG + Gamma / Dirichlet Sampler (Zero Dependencies)
// =========================================================================

static inline u64 az_rng_next(u64 *s) {
    u64 z = (*s += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static inline double az_rng_uniform(u64 *s) {
    return (double) (az_rng_next(s) >> 11) * (1.0 / 9007199254740992.0);
}

static double az_rng_normal(u64 *s) {
    double u1 = az_rng_uniform(s);
    double u2 = az_rng_uniform(s);
    if (u1 < 1e-12) {
        u1 = 1e-12;
    }
    return sqrt(-2.0 * log(u1)) * cos(6.28318530717958647692 * u2);
}

// Marsaglia & Tsang gamma sampler (handles alpha < 1 via boost trick)
static double az_rng_gamma(u64 *s, double alpha) {
    if (alpha <= 0.0) {
        return 0.0;
    }
    if (alpha < 1.0) {
        double u = az_rng_uniform(s);
        if (u < 1e-12) {
            u = 1e-12;
        }
        return az_rng_gamma(s, alpha + 1.0) * pow(u, 1.0 / alpha);
    }
    double d = alpha - (1.0 / 3.0);
    double c = 1.0 / sqrt(9.0 * d);
    for (;;) {
        double x, v;
        do {
            x = az_rng_normal(s);
            v = 1.0 + c * x;
        } while (v <= 0.0);
        v = v * v * v;
        double u = az_rng_uniform(s);
        if (u < 1.0 - 0.0331 * (x * x) * (x * x)) {
            return d * v;
        }
        if (log(u) < 0.5 * x * x + d * (1.0 - v + log(v))) {
            return d * v;
        }
    }
}

void az_global_init(void) {
    attacks_init();
}

// =========================================================================
// Canonical 20-Plane Encoding & Exact AlphaZero 4,672-Action Mapping
// =========================================================================

// Queen slide directions: N, NE, E, SE, S, SW, W, NW
static const int g_queen_df[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const int g_queen_dr[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

// Knight jumps
static const int g_knight_df[8] = { 1, 2, 2, 1, -1, -2, -2, -1 };
static const int g_knight_dr[8] = { 2, 1, -1, -2, -2, -1, 1, 2 };

int az_move_to_action(u64 move) {
    int from = get_start_offset(move);
    int to = get_end_offset(move);
    int df = (to & 7) - (from & 7);
    int dr = (to >> 3) - (from >> 3);

    u8 sp = get_start_piece(move);
    u8 pr = get_promotion_piece(move);

    // Underpromotions (Knight, Bishop, Rook) -> planes 64..72
    if (pr != sp && pr != FRIENDLY_QUEEN) {
        int dir_idx = (df < 0) ? 0 : ((df == 0) ? 1 : 2);
        int pc_idx = (pr == FRIENDLY_KNIGHT) ? 0 : ((pr == FRIENDLY_BISHOP) ? 1 : 2);
        int plane = 64 + dir_idx * 3 + pc_idx;
        return from * 73 + plane;
    }

    // Knight moves -> planes 56..63
    int adf = abs(df);
    int adr = abs(dr);
    if ((adf == 1 && adr == 2) || (adf == 2 && adr == 1)) {
        for (int k = 0; k < 8; k++) {
            if (g_knight_df[k] == df && g_knight_dr[k] == dr) {
                return from * 73 + (56 + k);
            }
        }
    }

    // Queen-like slides (includes pawns, king, castling, queen promos) -> planes 0..55
    int dist = (adf > adr) ? adf : adr;
    int sgn_f = (df > 0) ? 1 : ((df < 0) ? -1 : 0);
    int sgn_r = (dr > 0) ? 1 : ((dr < 0) ? -1 : 0);
    for (int d = 0; d < 8; d++) {
        if (g_queen_df[d] == sgn_f && g_queen_dr[d] == sgn_r) {
            int plane = d * 7 + (dist - 1);
            return from * 73 + plane;
        }
    }
    return 0;
}

static inline void fill_bb_plane(float *plane, u64 bb) {
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1ull;
        plane[sq] = 1.0f;
    }
}

static inline void fill_const_plane(float *plane, float val) {
    for (int i = 0; i < 64; i++) {
        plane[i] = val;
    }
}

void az_encode_board_ctx(
    const chess *b, int half_move, int is_rep, float out_planes[AZ_BOARD_FLOATS]) {
    memset(out_planes, 0, sizeof(float) * AZ_BOARD_FLOATS);

    u64 f = b->friendly_pieces;
    u64 e = b->enemy_pieces;
    u64 tp = b->pawns & PAWN_MASK;
    u64 orth = b->orthogonal_pieces;
    u64 diag = b->diagonal_pieces;
    u64 kings = b->kings;

    fill_bb_plane(out_planes + 0 * 64, f & tp);
    fill_bb_plane(out_planes + 1 * 64, f & ~(tp | orth | diag | kings));
    fill_bb_plane(out_planes + 2 * 64, f & diag & ~orth);
    fill_bb_plane(out_planes + 3 * 64, f & orth & ~diag);
    fill_bb_plane(out_planes + 4 * 64, f & diag & orth);
    fill_bb_plane(out_planes + 5 * 64, f & kings);

    fill_bb_plane(out_planes + 6 * 64, e & tp);
    fill_bb_plane(out_planes + 7 * 64, e & ~(tp | orth | diag | kings));
    fill_bb_plane(out_planes + 8 * 64, e & diag & ~orth);
    fill_bb_plane(out_planes + 9 * 64, e & orth & ~diag);
    fill_bb_plane(out_planes + 10 * 64, e & diag & orth);
    fill_bb_plane(out_planes + 11 * 64, e & kings);

    if (has_en_passant(b)) {
        int ep_sq = get_en_passant_offset(b);
        if (ep_sq >= 0 && ep_sq < 64) {
            out_planes[12 * 64 + ep_sq] = 1.0f;
        }
    }

    if (b->pawns & (FRIENDLY & KINGSIDE)) {
        fill_const_plane(out_planes + 13 * 64, 1.0f);
    }
    if (b->pawns & (FRIENDLY & QUEENSIDE)) {
        fill_const_plane(out_planes + 14 * 64, 1.0f);
    }
    if (b->pawns & (ENEMY & KINGSIDE)) {
        fill_const_plane(out_planes + 15 * 64, 1.0f);
    }
    if (b->pawns & (ENEMY & QUEENSIDE)) {
        fill_const_plane(out_planes + 16 * 64, 1.0f);
    }

    float hm_norm = (float) half_move / 100.0f;
    if (hm_norm > 1.0f) {
        hm_norm = 1.0f;
    } else if (hm_norm < 0.0f) {
        hm_norm = 0.0f;
    }
    fill_const_plane(out_planes + 17 * 64, hm_norm);

    // Plane 18: repetition flag (position already seen in game history -> 3-fold risk!)
    if (is_rep) {
        fill_const_plane(out_planes + 18 * 64, 1.0f);
    }
    // Plane 19: side-to-move color (1.0 = White, 0.0 = Black)
    if (!is_flipped(b)) {
        fill_const_plane(out_planes + 19 * 64, 1.0f);
    }
}

void az_encode_board(const chess *b, int half_move, float out_planes[AZ_BOARD_FLOATS]) {
    az_encode_board_ctx(b, half_move, 0, out_planes);
}

int az_legal_actions(
    const chess *b, u64 out_moves[MAX_LEGAL_MOVES], int32_t out_actions[MAX_LEGAL_MOVES]) {
    chess tmp = *b;
    int n = set_legal_moves(&tmp, out_moves);
    for (int i = 0; i < n; i++) {
        out_actions[i] = az_move_to_action(out_moves[i]);
    }
    return n;
}

void az_move_to_uci(const chess *b, u64 m, char out[8]) {
    if (m == NULL_MOVE) {
        snprintf(out, 8, "0000");
        return;
    }
    bool flip = is_flipped(b);
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

// =========================================================================
// Pure-Rules Mate-in-1 / Mate-in-2 Probe + Path-C Tactical Quiescence Delta
// =========================================================================

static inline bool az_is_in_check(const chess *b) {
    u64 fk = b->kings & b->friendly_pieces;
    if (!fk) {
        return false;
    }
    return attacked_offset(b, __builtin_ctzll(fk));
}

static bool az_has_mate_in_1(const chess *b, u64 *out_mate_mv) {
    chess tmp = *b;
    u64 moves[MAX_LEGAL_MOVES];
    int n = set_legal_moves(&tmp, moves);
    for (int i = 0; i < n; i++) {
        chess nxt = *b;
        chess_make_move(&nxt, moves[i]);
        if (!az_is_in_check(&nxt)) {
            continue;
        }
        u64 rep[MAX_LEGAL_MOVES];
        if (set_legal_moves(&nxt, rep) == 0) {
            if (out_mate_mv) {
                *out_mate_mv = moves[i];
            }
            return true;
        }
    }
    return false;
}

// Pure rules-only forced Mate-in-2 check after candidate move `m1` produced `after_m1`.
// Checks that `after_m1` is in check and EVERY opponent reply allows immediate Mate-in-1.
static bool az_is_forced_mate_in_2(const chess *after_m1) {
    if (!az_is_in_check(after_m1)) {
        return false;
    }
    chess tmp = *after_m1;
    u64 rep[MAX_LEGAL_MOVES];
    int nr = set_legal_moves(&tmp, rep);
    if (nr == 0) {
        return true; // already Mate-in-1!
    }
    if (nr > 10) {
        return false; // cap work to keep root expansion sub-millisecond
    }
    for (int i = 0; i < nr; i++) {
        chess nxt2 = *after_m1;
        chess_make_move(&nxt2, rep[i]);
        if (!az_has_mate_in_1(&nxt2, NULL)) {
            return false;
        }
    }
    return true;
}

static inline int az_piece_cp(u8 p) {
    switch (p) {
        case FRIENDLY_PAWN:
        case ENEMY_PAWN:
            return 100;
        case FRIENDLY_KNIGHT:
        case ENEMY_KNIGHT:
            return 320;
        case FRIENDLY_BISHOP:
        case ENEMY_BISHOP:
            return 330;
        case FRIENDLY_ROOK:
        case ENEMY_ROOK:
            return 500;
        case FRIENDLY_QUEEN:
        case ENEMY_QUEEN:
            return 900;
        default:
            return 0;
    }
}

static inline int az_fast_material(const chess *b) {
    u64 f = b->friendly_pieces;
    u64 e = b->enemy_pieces;
    u64 tp = b->pawns & PAWN_MASK;
    u64 orth = b->orthogonal_pieces;
    u64 diag = b->diagonal_pieces;
    u64 kings = b->kings;

    int fp = __builtin_popcountll(f & tp);
    int fn = __builtin_popcountll(f & ~(tp | orth | diag | kings));
    int fb = __builtin_popcountll(f & diag & ~orth);
    int fr = __builtin_popcountll(f & orth & ~diag);
    int fq = __builtin_popcountll(f & diag & orth);

    int ep = __builtin_popcountll(e & tp);
    int en = __builtin_popcountll(e & ~(tp | orth | diag | kings));
    int eb = __builtin_popcountll(e & diag & ~orth);
    int er = __builtin_popcountll(e & orth & ~diag);
    int eq = __builtin_popcountll(e & diag & orth);

    return (fp - ep) * 100 + (fn - en) * 320 + (fb - eb) * 330 + (fr - er) * 500 + (fq - eq) * 900;
}

// Fast capture-only quiescence search (Path-C Tactical Booster; NEVER used during Path-A RL).
static int az_qsearch(const chess *b, int alpha, int beta, int depth, int *nodes_left) {
    if (*nodes_left <= 0) {
        return az_fast_material(b);
    }
    (*nodes_left)--;

    bool in_chk = az_is_in_check(b);
    int stand_pat = az_fast_material(b);

    if (!in_chk) {
        if (stand_pat >= beta) {
            return beta;
        }
        if (stand_pat > alpha) {
            alpha = stand_pat;
        }
        if (depth <= 0) {
            return stand_pat;
        }
    } else if (depth <= 0) {
        return stand_pat;
    }

    chess tmp = *b;
    u64 moves[MAX_LEGAL_MOVES];
    int n = set_legal_moves(&tmp, moves);
    if (n == 0) {
        return in_chk ? (-20000 - depth) : 0;
    }

    // Collect tactical moves (all evasions if in check, otherwise captures & promotions)
    u64 t_moves[MAX_LEGAL_MOVES];
    int t_scores[MAX_LEGAL_MOVES];
    int tn = 0;

    for (int i = 0; i < n; i++) {
        u64 m = moves[i];
        u8 sp = get_start_piece(m);
        u8 ep = get_end_piece(m);
        u8 pr = get_promotion_piece(m);
        bool is_cap = (ep != NONE) || is_en_passant_move(m);
        bool is_promo = (pr != sp);
        if (!in_chk && !is_cap && !is_promo) {
            continue;
        }
        int vic = is_en_passant_move(m) ? 100 : az_piece_cp(ep);
        if (is_promo) {
            vic += az_piece_cp(pr) - 100;
        }
        // Delta pruning on hopeless captures when not in check
        if (!in_chk && !is_promo && stand_pat + vic + 150 < alpha) {
            continue;
        }
        t_moves[tn] = m;
        t_scores[tn] = vic * 16 - (az_piece_cp(sp) >> 4);
        tn++;
    }

    // Selection sort MVV-LVA (tn is tiny: typically 2..12 captures)
    for (int i = 0; i < tn; i++) {
        int best_j = i;
        for (int j = i + 1; j < tn; j++) {
            if (t_scores[j] > t_scores[best_j]) {
                best_j = j;
            }
        }
        if (best_j != i) {
            u64 tm = t_moves[i];
            t_moves[i] = t_moves[best_j];
            t_moves[best_j] = tm;
            int ts = t_scores[i];
            t_scores[i] = t_scores[best_j];
            t_scores[best_j] = ts;
        }
        chess nxt = *b;
        chess_make_move(&nxt, t_moves[i]);
        int sc = -az_qsearch(&nxt, -beta, -alpha, depth - 1, nodes_left);
        if (sc >= beta) {
            return beta;
        }
        if (sc > alpha) {
            alpha = sc;
        }
        if (*nodes_left <= 0) {
            break;
        }
    }
    return alpha;
}

// Zero-bias tactical delta: 0 on quiet positions, non-zero ONLY when pieces are tactically hanging!
static int az_tactical_delta(const chess *b, int *out_mate_flag) {
    int budget = 96;
    int base = az_fast_material(b);
    int qs = az_qsearch(b, -25000, 25000, 6, &budget);
    if (out_mate_flag) {
        *out_mate_flag = 0;
        if (qs > 15000) {
            *out_mate_flag = 1;
        } else if (qs < -15000) {
            *out_mate_flag = -1;
        }
    }
    return qs - base;
}

// Blend tactical quiescence delta into neural WDL (leaves quiet positions 100% untouched!).
static void az_apply_tactical_wdl_delta(
    const chess *b, float *w, float *d, float *l) {
    int mate_flag = 0;
    int delta = az_tactical_delta(b, &mate_flag);
    if (mate_flag > 0) {
        *w = 0.992f;
        *d = 0.004f;
        *l = 0.004f;
        return;
    }
    if (mate_flag < 0) {
        *w = 0.004f;
        *d = 0.004f;
        *l = 0.992f;
        return;
    }
    if (delta > -65 && delta < 65) {
        return; // Quiet leaf: preserve 100% pure Zero-C neural evaluation!
    }
    if (delta > 950) {
        delta = 950;
    } else if (delta < -950) {
        delta = -950;
    }
    float q = *w - *l;
    if (q > 0.992f) {
        q = 0.992f;
    } else if (q < -0.992f) {
        q = -0.992f;
    }
    float nn_cp = 290.0f * atanhf(q);
    float adj_cp = nn_cp + 0.75f * (float) delta;
    float q_new = tanhf(adj_cp / 290.0f);
    float denom = 1.0f - fabsf(q);
    if (denom < 0.05f) {
        denom = 0.05f;
    }
    float d_new = (*d) * (1.0f - fabsf(q_new)) / denom;
    float max_d = 1.0f - fabsf(q_new);
    if (d_new > max_d) {
        d_new = max_d;
    }
    if (d_new < 0.005f) {
        d_new = 0.005f;
    }
    float w_new = 0.5f * (1.0f - d_new + q_new);
    float l_new = 0.5f * (1.0f - d_new - q_new);
    if (w_new < 0.0f) {
        w_new = 0.0f;
    }
    if (l_new < 0.0f) {
        l_new = 0.0f;
    }
    float s = w_new + d_new + l_new;
    if (s > 1e-6f) {
        *w = w_new / s;
        *d = d_new / s;
        *l = l_new / s;
    }
}

// =========================================================================
// Batched Multi-Game MCTS Structures (Flat Pool per Game, Virtual Loss)
// =========================================================================

#define AZ_MAX_NODES_PER_GAME 32768
#define AZ_MAX_PENDING_LEAVES 4096
#define AZ_MAX_SAMPLE_QUEUE   32768
#define AZ_MAX_GAME_SAMPLES   512
#define AZ_MAX_WORKERS        16

enum {
    AZ_JOB_NONE = 0,
    AZ_JOB_COLLECT = 1,
    AZ_JOB_APPLY = 2,
    AZ_JOB_STEP = 3,
};

enum {
    AZ_PROVEN_NONE = 0,
    AZ_PROVEN_WIN = 1, // side to move has a forced checkmate win!
    AZ_PROVEN_LOSS = 2, // side to move is forcibly checkmated!
};

typedef struct az_node {
    chess board;
    u64 move_from_parent;
    int32_t action_from_parent;
    int32_t parent;
    int32_t first_child;
    int16_t num_children;
    int16_t half_move;
    int32_t visits;
    int32_t vloss;
    float prior;
    float raw_prior;
    float w_sum;
    float d_sum;
    float l_sum;
    float ml_sum;
    uint8_t expanded;
    uint8_t terminal; // 0=no, 1=stm loss (mate), 2=draw
    uint8_t proven; // 0=none, 1=PROVEN_WIN, 2=PROVEN_LOSS
    uint8_t pad0;
} az_node;

typedef struct az_pending_leaf {
    int32_t game_idx;
    int32_t node_idx;
    int32_t num_legal;
    u64 legal_moves[MAX_LEGAL_MOVES];
    int32_t legal_actions[MAX_LEGAL_MOVES];
} az_pending_leaf;

typedef struct az_game_slot {
    full_chess *game;
    az_node *pool;
    int32_t pool_used;
    int32_t sims_target;
    int32_t is_full_search;
    int32_t dirichlet_applied;
    u64 rng;
    int32_t slot_pending[16];
    int32_t slot_pending_n;
    int32_t game_seq;
    char last_san[16];
    char last_uci[8];

    // Smart resignation state (15% of self-play games disable resign for calibration)
    uint8_t no_resign_game;
    int8_t resign_sign;
    int16_t resign_streak;

    // Trajectory buffer for current in-progress game (committed when game ends)
    az_sample *traj;
    float (*traj_search_wdl)[3];
    int32_t traj_len;
    int32_t traj_cap;
} az_game_slot;

typedef struct az_worker_arg {
    az_arena *arena;
    int worker_id;
} az_worker_arg;

struct az_arena {
    az_config cfg;
    u64 rng;
    az_game_slot *slots;
    az_pending_leaf *pending;
    atomic_int pending_count;
    int32_t pending_cap;

    az_sample *sample_queue;
    int32_t q_head;
    int32_t q_count;
    int32_t q_cap;
    pthread_mutex_t queue_mtx;

    // Per-worker scratch MCTS pool for zero-malloc thread-safe subtree reuse!
    az_node *worker_tmp[AZ_MAX_WORKERS];

    // Persistent pthread worker pool (zero thread-spawn overhead per GPU batch)
    int num_threads;
    int num_workers;
    bool pool_init;
    pthread_t workers[AZ_MAX_WORKERS];
    az_worker_arg worker_args[AZ_MAX_WORKERS];
    pthread_mutex_t pool_mtx;
    pthread_cond_t job_cv;
    pthread_cond_t done_cv;
    uint64_t job_epoch;
    int workers_left;
    int pool_stop;
    int job_kind;

    // Active parallel job parameters (read-only by workers during an epoch)
    int cur_max_batch;
    float *cur_out_planes;
    int32_t *cur_out_lc;
    int32_t *cur_out_la;

    int cur_apply_n;
    const float *cur_priors;
    const float *cur_wdl;
    const float *cur_ml;

    int cur_auto_reset;
    int32_t t_finished[AZ_MAX_WORKERS];
    int32_t t_ww[AZ_MAX_WORKERS];
    int32_t t_dr[AZ_MAX_WORKERS];
    int32_t t_bw[AZ_MAX_WORKERS];
    int32_t t_plies[AZ_MAX_WORKERS];
};

static inline bool is_board_repetition(const full_chess *g, const chess *b) {
    int min_i = (int) g->move_count - (int) g->half_move_count;
    if (min_i < 0) {
        min_i = 0;
    }
    for (int i = (int) g->move_count - 2; i >= min_i; i -= 2) {
        if (chess_equal(&g->game_history[i], b)) {
            return true;
        }
    }
    return false;
}

static void roll_playout_cap(az_arena *arena, az_game_slot *sl, int reused_visits) {
    double r = az_rng_uniform(&sl->rng);
    int budget;
    if (r < (double) arena->cfg.full_search_prob || arena->cfg.fast_sims >= arena->cfg.full_sims) {
        sl->is_full_search = 1;
        budget = arena->cfg.full_sims;
    } else {
        sl->is_full_search = 0;
        budget = arena->cfg.fast_sims;
    }
    if (budget < 1) {
        budget = 1;
    }
    if (reused_visits > 0) {
        int min_fresh = (arena->cfg.dirichlet_eps > 0.0f) ? (budget / 2) : ((budget * 3) / 4);
        if (min_fresh < 1) {
            min_fresh = 1;
        }
        int target = reused_visits + min_fresh;
        if (target < budget) {
            target = budget;
        }
        sl->sims_target = target;
    } else {
        sl->sims_target = budget;
    }
}

static void init_root_node(az_arena *arena, az_game_slot *sl) {
    sl->pool_used = 1;
    sl->dirichlet_applied = 0;
    sl->slot_pending_n = 0;
    az_node *root = &sl->pool[0];
    memset(root, 0, sizeof(*root));
    root->board = *(sl->game->game);
    root->parent = -1;
    root->first_child = -1;
    root->half_move = (int16_t) sl->game->half_move_count;
    root->prior = 1.0f;
    root->raw_prior = 1.0f;
    roll_playout_cap(arena, sl, 0);
}

static void apply_root_dirichlet(az_arena *arena, az_game_slot *sl);
static void propagate_proven(az_game_slot *sl, int node_idx);
static void probe_root_mates(az_arena *arena, az_game_slot *sl);

// True MCTS Subtree Reuse: BFS-compact `child_idx`'s subtree into pool[0..new_used-1].
static bool reroot_slot_to_child(az_arena *arena, az_game_slot *sl, int wid, int child_idx) {
    if (child_idx <= 0 || child_idx >= sl->pool_used || !sl->pool[child_idx].expanded
        || sl->pool[child_idx].terminal != 0 || sl->pool[child_idx].num_children <= 0) {
        init_root_node(arena, sl);
        return false;
    }
    if (wid < 0 || wid >= AZ_MAX_WORKERS || !arena->worker_tmp[wid]) {
        init_root_node(arena, sl);
        return false;
    }
    az_node *tmp = arena->worker_tmp[wid];
    tmp[0] = sl->pool[child_idx];
    tmp[0].parent = -1;
    tmp[0].move_from_parent = 0;
    tmp[0].action_from_parent = 0;
    tmp[0].prior = 1.0f;
    tmp[0].raw_prior = 1.0f;
    tmp[0].vloss = 0;
    tmp[0].board = *(sl->game->game);
    tmp[0].half_move = (int16_t) sl->game->half_move_count;

    int new_used = 1;
    int max_keep = (AZ_MAX_NODES_PER_GAME * 3) / 4;

    for (int q = 0; q < new_used; q++) {
        int old_fc = tmp[q].first_child;
        int old_nc = tmp[q].num_children;
        if (tmp[q].expanded && old_fc >= 0 && old_nc > 0 && new_used + old_nc <= max_keep) {
            tmp[q].first_child = new_used;
            for (int c = 0; c < old_nc; c++) {
                az_node *dst = &tmp[new_used++];
                *dst = sl->pool[old_fc + c];
                dst->parent = q;
                dst->vloss = 0;
            }
        } else {
            tmp[q].first_child = -1;
            tmp[q].num_children = 0;
            if (tmp[q].terminal == 0) {
                tmp[q].expanded = 0;
            }
        }
    }

    if (!tmp[0].expanded || tmp[0].num_children <= 0) {
        init_root_node(arena, sl);
        return false;
    }

    memcpy(sl->pool, tmp, (size_t) new_used * sizeof(az_node));
    sl->pool_used = new_used;
    sl->dirichlet_applied = 0;
    sl->slot_pending_n = 0;

    // Restore clean raw priors at new root before applying fresh Dirichlet noise
    az_node *root = &sl->pool[0];
    for (int c = 0; c < root->num_children; c++) {
        az_node *ch = &sl->pool[root->first_child + c];
        ch->prior = ch->raw_prior;
    }

    roll_playout_cap(arena, sl, root->visits);
    probe_root_mates(arena, sl);
    apply_root_dirichlet(arena, sl);
    return true;
}

static void reset_slot_internal(az_arena *arena, az_game_slot *sl, const char *fen) {
    if (fen && fen[0] != '\0') {
        set_from_fen(sl->game, fen);
    } else {
        full_chess_initialize(sl->game);
    }
    sl->game->is_static = true;
    sl->game_seq++;
    sl->last_san[0] = '\0';
    sl->last_uci[0] = '\0';
    sl->traj_len = 0;
    sl->resign_sign = 0;
    sl->resign_streak = 0;
    // 15% of self-play games disable resignation so Zero-C always practices full mates
    sl->no_resign_game = (az_rng_uniform(&sl->rng) < 0.15) ? 1 : 0;
    init_root_node(arena, sl);
}

static void az_run_worker_slice(az_arena *arena, int wid);

static void *az_worker_main(void *arg) {
    az_worker_arg *wa = (az_worker_arg *) arg;
    az_arena *a = wa->arena;
    int wid = wa->worker_id;
    uint64_t seen_epoch = 0;

    for (;;) {
        pthread_mutex_lock(&a->pool_mtx);
        while (!a->pool_stop && a->job_epoch == seen_epoch) {
            pthread_cond_wait(&a->job_cv, &a->pool_mtx);
        }
        if (a->pool_stop) {
            pthread_mutex_unlock(&a->pool_mtx);
            break;
        }
        seen_epoch = a->job_epoch;
        pthread_mutex_unlock(&a->pool_mtx);

        az_run_worker_slice(a, wid);

        pthread_mutex_lock(&a->pool_mtx);
        a->workers_left--;
        if (a->workers_left == 0) {
            pthread_cond_signal(&a->done_cv);
        }
        pthread_mutex_unlock(&a->pool_mtx);
    }
    return NULL;
}

static void az_dispatch_job(az_arena *arena, int kind) {
    if (arena->num_workers <= 0) {
        arena->job_kind = kind;
        az_run_worker_slice(arena, 0);
        return;
    }
    pthread_mutex_lock(&arena->pool_mtx);
    arena->job_kind = kind;
    arena->workers_left = arena->num_workers;
    arena->job_epoch++;
    pthread_cond_broadcast(&arena->job_cv);
    pthread_mutex_unlock(&arena->pool_mtx);

    az_run_worker_slice(arena, 0);

    pthread_mutex_lock(&arena->pool_mtx);
    while (arena->workers_left > 0) {
        pthread_cond_wait(&arena->done_cv, &arena->pool_mtx);
    }
    pthread_mutex_unlock(&arena->pool_mtx);
}

az_arena *az_arena_create(const az_config *cfg) {
    az_global_init();
    if (!cfg || cfg->num_games < 1 || cfg->num_games > AZ_MAX_GAMES) {
        return NULL;
    }
    az_arena *a = (az_arena *) calloc(1, sizeof(az_arena));
    if (!a) {
        return NULL;
    }
    a->cfg = *cfg;
    if (a->cfg.c_puct <= 0.0f) {
        a->cfg.c_puct = 1.75f;
    }
    if (a->cfg.batch_per_game < 1) {
        a->cfg.batch_per_game = 1;
    }
    if (a->cfg.batch_per_game > 16) {
        a->cfg.batch_per_game = 16;
    }
    if (a->cfg.max_plies <= 0) {
        a->cfg.max_plies = 220;
    }
    a->rng = cfg->seed ? cfg->seed : 0x517cc1b727220a95ull;
    atomic_init(&a->pending_count, 0);
    pthread_mutex_init(&a->queue_mtx, NULL);

    a->slots = (az_game_slot *) calloc((size_t) cfg->num_games, sizeof(az_game_slot));
    a->pending_cap = AZ_MAX_PENDING_LEAVES;
    a->pending = (az_pending_leaf *) calloc((size_t) a->pending_cap, sizeof(az_pending_leaf));
    a->q_cap = AZ_MAX_SAMPLE_QUEUE;
    a->sample_queue = (az_sample *) calloc((size_t) a->q_cap, sizeof(az_sample));

    if (!a->slots || !a->pending || !a->sample_queue) {
        az_arena_destroy(a);
        return NULL;
    }

    for (int i = 0; i < cfg->num_games; i++) {
        az_game_slot *sl = &a->slots[i];
        sl->rng = az_rng_next(&a->rng) ^ ((u64) (i + 1) * 0x9e3779b97f4a7c15ull);
        sl->game = full_chess_create();
        sl->pool = (az_node *) calloc(AZ_MAX_NODES_PER_GAME, sizeof(az_node));
        sl->traj_cap = AZ_MAX_GAME_SAMPLES;
        sl->traj = (az_sample *) calloc((size_t) sl->traj_cap, sizeof(az_sample));
        sl->traj_search_wdl = (float (*)[3]) calloc((size_t) sl->traj_cap, sizeof(float[3]));
        if (!sl->game || !sl->pool || !sl->traj || !sl->traj_search_wdl) {
            az_arena_destroy(a);
            return NULL;
        }
        reset_slot_internal(a, sl, NULL);
    }

    int req_threads = cfg->num_threads;
    if (req_threads <= 0) {
        long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
        if (ncpu < 1) {
            ncpu = 1;
        }
        req_threads = (int) (ncpu > 8 ? 8 : ncpu);
    }
    if (cfg->num_games < 8) {
        req_threads = 1;
    }
    if (req_threads > cfg->num_games) {
        req_threads = cfg->num_games;
    }
    if (req_threads > AZ_MAX_WORKERS) {
        req_threads = AZ_MAX_WORKERS;
    }
    if (req_threads < 1) {
        req_threads = 1;
    }
    a->num_threads = req_threads;
    a->num_workers = req_threads - 1;

    for (int t = 0; t < a->num_threads; t++) {
        a->worker_tmp[t] = (az_node *) malloc(AZ_MAX_NODES_PER_GAME * sizeof(az_node));
        if (!a->worker_tmp[t]) {
            az_arena_destroy(a);
            return NULL;
        }
    }

    if (a->num_workers > 0) {
        pthread_mutex_init(&a->pool_mtx, NULL);
        pthread_cond_init(&a->job_cv, NULL);
        pthread_cond_init(&a->done_cv, NULL);
        a->pool_init = true;
        for (int w = 0; w < a->num_workers; w++) {
            a->worker_args[w].arena = a;
            a->worker_args[w].worker_id = w + 1;
            if (pthread_create(&a->workers[w], NULL, az_worker_main, &a->worker_args[w]) != 0) {
                a->num_workers = w;
                a->num_threads = w + 1;
                break;
            }
        }
    }
    return a;
}

int az_arena_num_threads(const az_arena *arena) {
    return arena ? arena->num_threads : 0;
}

void az_arena_destroy(az_arena *arena) {
    if (!arena) {
        return;
    }
    if (arena->pool_init) {
        pthread_mutex_lock(&arena->pool_mtx);
        arena->pool_stop = 1;
        pthread_cond_broadcast(&arena->job_cv);
        pthread_mutex_unlock(&arena->pool_mtx);
        for (int w = 0; w < arena->num_workers; w++) {
            pthread_join(arena->workers[w], NULL);
        }
        pthread_cond_destroy(&arena->done_cv);
        pthread_cond_destroy(&arena->job_cv);
        pthread_mutex_destroy(&arena->pool_mtx);
        arena->pool_init = false;
    }
    pthread_mutex_destroy(&arena->queue_mtx);
    for (int t = 0; t < AZ_MAX_WORKERS; t++) {
        free(arena->worker_tmp[t]);
    }
    if (arena->slots) {
        for (int i = 0; i < arena->cfg.num_games; i++) {
            if (arena->slots[i].game) {
                full_chess_delete(&arena->slots[i].game);
            }
            free(arena->slots[i].pool);
            free(arena->slots[i].traj);
            free(arena->slots[i].traj_search_wdl);
        }
        free(arena->slots);
    }
    free(arena->pending);
    free(arena->sample_queue);
    free(arena);
}

void az_arena_reset_game(az_arena *arena, int game_idx, const char *fen_or_null) {
    if (!arena || game_idx < 0 || game_idx >= arena->cfg.num_games) {
        return;
    }
    reset_slot_internal(arena, &arena->slots[game_idx], fen_or_null);
}

void az_arena_set_position_uci(
    az_arena *arena, int game_idx, const char *fen_or_null, const char *moves_space_sep) {
    if (!arena || game_idx < 0 || game_idx >= arena->cfg.num_games) {
        return;
    }
    az_game_slot *sl = &arena->slots[game_idx];

    // Check if the previous root tree can be reused (1-ply or 2-ply advancement in UCI!)
    bool had_tree = (sl->pool_used > 1 && sl->pool[0].expanded && sl->pool[0].num_children > 0);
    int prev_ply = (int) sl->game->move_count;

    if (fen_or_null && fen_or_null[0] != '\0') {
        set_from_fen(sl->game, fen_or_null);
    } else {
        full_chess_initialize(sl->game);
    }
    sl->game->is_static = true;
    sl->game_seq++;
    sl->last_san[0] = '\0';
    sl->last_uci[0] = '\0';
    sl->traj_len = 0;

    if (moves_space_sep && moves_space_sep[0] != '\0') {
        const char *p = moves_space_sep;
        char tok[16];
        while (*p) {
            while (*p && isspace((unsigned char) *p)) {
                p++;
            }
            if (!*p) {
                break;
            }
            size_t ti = 0;
            while (*p && !isspace((unsigned char) *p) && ti + 1 < sizeof(tok)) {
                tok[ti++] = *p++;
            }
            tok[ti] = '\0';
            u64 m = static_encode_string(sl->game, tok);
            if (m != NULL_MOVE) {
                static_move_to_san(sl->game->game, sl->game->current_legal_moves, m, sl->last_san,
                    sizeof(sl->last_san));
                make_move(sl->game, m);
            }
        }
    }

    int new_ply = (int) sl->game->move_count;
    int matched_node = -1;

    if (had_tree && (new_ply == prev_ply + 1 || new_ply == prev_ply + 2)) {
        const chess *target_b = sl->game->game;
        const az_node *root = &sl->pool[0];
        if (new_ply == prev_ply + 1) {
            for (int i = 0; i < root->num_children; i++) {
                int cidx = root->first_child + i;
                if (chess_equal(&sl->pool[cidx].board, target_b)) {
                    matched_node = cidx;
                    break;
                }
            }
        } else {
            // 2 plies ahead: our move + opponent's reply -> check grandchildren!
            for (int i = 0; i < root->num_children && matched_node < 0; i++) {
                int cidx = root->first_child + i;
                const az_node *ch = &sl->pool[cidx];
                if (!ch->expanded || ch->num_children <= 0) {
                    continue;
                }
                for (int j = 0; j < ch->num_children; j++) {
                    int gidx = ch->first_child + j;
                    if (chess_equal(&sl->pool[gidx].board, target_b)) {
                        matched_node = gidx;
                        break;
                    }
                }
            }
        }
    }

    if (matched_node > 0) {
        reroot_slot_to_child(arena, sl, 0, matched_node);
    } else {
        init_root_node(arena, sl);
    }
}

// Exact MCTS Solver: bottom-up proven win/loss propagation (pure chess rules!).
static void propagate_proven(az_game_slot *sl, int node_idx) {
    int cur = node_idx;
    while (cur > 0) {
        uint8_t child_p = sl->pool[cur].proven;
        if (child_p == AZ_PROVEN_NONE) {
            break;
        }
        int p = sl->pool[cur].parent;
        if (p < 0) {
            break;
        }
        az_node *pn = &sl->pool[p];
        if (pn->proven != AZ_PROVEN_NONE) {
            break;
        }
        if (child_p == AZ_PROVEN_LOSS) {
            // Child is a forced loss for opponent -> parent has a forced WIN!
            pn->proven = AZ_PROVEN_WIN;
            cur = p;
        } else if (child_p == AZ_PROVEN_WIN && pn->expanded && pn->num_children > 0) {
            bool all_win = true;
            for (int i = 0; i < pn->num_children; i++) {
                if (sl->pool[pn->first_child + i].proven != AZ_PROVEN_WIN) {
                    all_win = false;
                    break;
                }
            }
            if (all_win) {
                pn->proven = AZ_PROVEN_LOSS;
                cur = p;
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

// Fast pure-rules Mate-in-1 (always) and Mate-in-2 / Opponent Mate-in-1 guard (play mode) at root.
static void probe_root_mates(az_arena *arena, az_game_slot *sl) {
    az_node *root = &sl->pool[0];
    if (!root->expanded || root->num_children <= 0) {
        return;
    }
    bool full_mate2 = (arena->cfg.tactical_mode != 0 || arena->cfg.dirichlet_eps <= 0.0f);

    for (int i = 0; i < root->num_children; i++) {
        int cidx = root->first_child + i;
        az_node *ch = &sl->pool[cidx];
        if (ch->terminal == 1 || ch->proven == AZ_PROVEN_LOSS) {
            root->proven = AZ_PROVEN_WIN;
            continue;
        }
        if (ch->terminal != 0) {
            continue;
        }
        if (az_is_in_check(&ch->board)) {
            chess tmp = ch->board;
            u64 rep[MAX_LEGAL_MOVES];
            int nr = set_legal_moves(&tmp, rep);
            if (nr == 0) {
                ch->terminal = 1;
                ch->expanded = 1;
                ch->proven = AZ_PROVEN_LOSS;
                root->proven = AZ_PROVEN_WIN;
                continue;
            }
            if (full_mate2 && nr <= 10 && az_is_forced_mate_in_2(&ch->board)) {
                ch->proven = AZ_PROVEN_LOSS;
                root->proven = AZ_PROVEN_WIN;
                continue;
            }
        }
        // In play/match mode, flag any root move that allows an immediate opponent Mate-in-1!
        if (full_mate2 && ch->proven == AZ_PROVEN_NONE && az_has_mate_in_1(&ch->board, NULL)) {
            ch->proven = AZ_PROVEN_WIN;
        }
    }
}

// Backpropagate WDL (from leaf's side-to-move perspective) & Moves-Left up to root, clearing vloss.
static void backprop_wdl(
    az_game_slot *sl, int node_idx, float w, float d, float l, float ml, int remove_vloss) {
    int cur = node_idx;
    while (cur >= 0) {
        az_node *nd = &sl->pool[cur];
        if (remove_vloss && nd->vloss > 0) {
            nd->vloss--;
        }
        nd->visits++;
        nd->w_sum += w;
        nd->d_sum += d;
        nd->l_sum += l;
        nd->ml_sum += ml;

        // Flip perspective going up one ply: parent's win == child's loss!
        float tmp = w;
        w = l;
        l = tmp;
        ml += 0.01f; // +1 ply remaining (normalized by 100)
        cur = nd->parent;
    }
}

static void apply_root_dirichlet(az_arena *arena, az_game_slot *sl) {
    az_node *root = &sl->pool[0];
    if (!sl->is_full_search || sl->dirichlet_applied || !root->expanded || root->num_children <= 1
        || arena->cfg.dirichlet_eps <= 0.0f) {
        return;
    }
    double noise[MAX_LEGAL_MOVES];
    double sum = 0.0;
    for (int i = 0; i < root->num_children; i++) {
        noise[i] = az_rng_gamma(&sl->rng, (double) arena->cfg.dirichlet_alpha);
        sum += noise[i];
    }
    if (sum > 1e-12) {
        float eps = arena->cfg.dirichlet_eps;
        float keep = 1.0f - eps;
        for (int i = 0; i < root->num_children; i++) {
            az_node *ch = &sl->pool[root->first_child + i];
            ch->prior = keep * ch->raw_prior + eps * (float) (noise[i] / sum);
        }
    }
    sl->dirichlet_applied = 1;
}

static int select_child_puct(const az_arena *arena, const az_game_slot *sl, int node_idx) {
    const az_node *parent = &sl->pool[node_idx];
    int total_n = parent->visits + parent->vloss;
    if (total_n < 1) {
        total_n = 1;
    }
    float sqrt_n = sqrtf((float) total_n);

    // Compute visited policy mass for FPU reduction
    float visited_policy = 0.0f;
    for (int i = 0; i < parent->num_children; i++) {
        const az_node *ch = &sl->pool[parent->first_child + i];
        if (ch->visits + ch->vloss > 0) {
            visited_policy += ch->prior;
        }
    }
    float parent_q
        = (parent->visits > 0) ? ((parent->w_sum - parent->l_sum) / (float) parent->visits) : 0.0f;
    float fpu_q = parent_q - arena->cfg.fpu_reduction * sqrtf(visited_policy);

    bool use_forced = (node_idx == 0 && sl->is_full_search && arena->cfg.forced_playouts
                       && arena->cfg.dirichlet_eps > 0.0f && total_n >= 4);

    int best_c = parent->first_child;
    float best_score = -1e30f;

    for (int i = 0; i < parent->num_children; i++) {
        int cidx = parent->first_child + i;
        const az_node *ch = &sl->pool[cidx];
        int cn = ch->visits + ch->vloss;

        // Exact MCTS Solver overrides:
        if (ch->proven == AZ_PROVEN_LOSS) {
            // Forced checkmate for parent! Prefer immediate mate & shortest ML.
            float ch_ml = (ch->visits > 0) ? (ch->ml_sum / (float) ch->visits) : 0.0f;
            float sc = (ch->terminal == 1 ? 2000.0f : 1000.0f) - ch_ml;
            if (sc > best_score) {
                best_score = sc;
                best_c = cidx;
            }
            continue;
        }
        if (ch->proven == AZ_PROVEN_WIN) {
            // Child is a forced win for opponent (blunder into mate!) -> avoid unless all lose.
            float ch_ml = (ch->visits > 0) ? (ch->ml_sum / (float) ch->visits) : 0.0f;
            float sc = -1000.0f + ch_ml;
            if (sc > best_score) {
                best_score = sc;
                best_c = cidx;
            }
            continue;
        }

        // KataGo Forced Playouts at root during full-search exploration
        if (use_forced && cn > 0) {
            float forced = sqrtf(2.0f * ch->prior * (float) total_n);
            if ((float) cn < forced) {
                return cidx;
            }
        }

        float q;
        if (cn == 0) {
            q = fpu_q;
        } else {
            float q_raw = (ch->l_sum - ch->w_sum) - (float) ch->vloss;
            q = q_raw / (float) cn;
            // Leela/KataGo Moves-Left Utility: break Q-saturation ties in winning/losing endgames
            if (ch->visits > 0 && parent->visits > 0 && fabsf(q) > 0.50f) {
                float parent_ml = parent->ml_sum / (float) parent->visits;
                float child_ml = ch->ml_sum / (float) ch->visits;
                float ml_diff = parent_ml - child_ml;
                if (ml_diff > 0.25f) {
                    ml_diff = 0.25f;
                } else if (ml_diff < -0.25f) {
                    ml_diff = -0.25f;
                }
                q += (q > 0.0f ? 1.0f : -1.0f) * 0.24f * ml_diff;
            }
        }
        float u = arena->cfg.c_puct * ch->prior * sqrt_n / (1.0f + (float) cn);
        float sc = q + u;
        if (sc > best_score) {
            best_score = sc;
            best_c = cidx;
        }
    }
    return best_c;
}

static void collect_leaves_range(az_arena *arena, int g_start, int g_end) {
    int max_batch = arena->cur_max_batch;
    float *out_planes = arena->cur_out_planes;
    int32_t *out_legal_counts = arena->cur_out_lc;
    int32_t *out_legal_actions = arena->cur_out_la;

    for (int gi = g_start; gi < g_end; gi++) {
        az_game_slot *sl = &arena->slots[gi];
        sl->slot_pending_n = 0;
        if (sl->game->is_game_over || (int) sl->game->move_count >= arena->cfg.max_plies) {
            continue;
        }
        az_node *root = &sl->pool[0];
        if (root->visits >= sl->sims_target) {
            continue;
        }
        // Early stop in play/match mode when root already has a proven forced mate!
        if (arena->cfg.dirichlet_eps <= 0.0f && root->proven == AZ_PROVEN_WIN
            && root->visits >= 16) {
            continue;
        }
        if (atomic_load_explicit(&arena->pending_count, memory_order_relaxed) >= max_batch) {
            break;
        }

        int tries = arena->cfg.batch_per_game;
        if (tries > 16) {
            tries = 16;
        }
        int collected_here = 0;
        int seen_nodes[16];
        int guard = sl->sims_target - root->visits + 8;

        for (int iter = 0; iter < guard && collected_here < tries && root->visits < sl->sims_target;
             iter++) {
            if (atomic_load_explicit(&arena->pending_count, memory_order_relaxed) >= max_batch) {
                break;
            }

            int cur = 0;
            sl->pool[cur].vloss++;

            while (sl->pool[cur].expanded && !sl->pool[cur].terminal
                   && sl->pool[cur].num_children > 0) {
                cur = select_child_puct(arena, sl, cur);
                sl->pool[cur].vloss++;
            }

            az_node *leaf = &sl->pool[cur];

            // Already resolved terminal? Backpropagate directly in C!
            if (leaf->terminal) {
                if (leaf->terminal == 1) {
                    if (leaf->proven == AZ_PROVEN_NONE) {
                        leaf->proven = AZ_PROVEN_LOSS;
                        propagate_proven(sl, cur);
                    }
                    backprop_wdl(sl, cur, 0.0f, 0.0f, 1.0f, 0.0f, 1);
                } else {
                    backprop_wdl(sl, cur, 0.0f, 1.0f, 0.0f, 0.0f, 1);
                }
                continue;
            }

            // O(tries) L1-local duplicate leaf check within this game slot
            bool dup = false;
            for (int p = 0; p < collected_here; p++) {
                if (seen_nodes[p] == cur) {
                    dup = true;
                    break;
                }
            }
            if (dup) {
                int u = cur;
                while (u >= 0) {
                    if (sl->pool[u].vloss > 0) {
                        sl->pool[u].vloss--;
                    }
                    u = sl->pool[u].parent;
                }
                break;
            }

            // Check exact pure chess rules for terminal conditions at this leaf
            u64 legal[MAX_LEGAL_MOVES];
            int32_t actions[MAX_LEGAL_MOVES];
            int n_legal = az_legal_actions(&leaf->board, legal, actions);
            if (n_legal == 0) {
                leaf->expanded = 1;
                if (az_is_in_check(&leaf->board)) {
                    leaf->terminal = 1;
                    leaf->proven = AZ_PROVEN_LOSS;
                    propagate_proven(sl, cur);
                    backprop_wdl(sl, cur, 0.0f, 0.0f, 1.0f, 0.0f, 1);
                } else {
                    leaf->terminal = 2;
                    backprop_wdl(sl, cur, 0.0f, 1.0f, 0.0f, 0.0f, 1);
                }
                continue;
            }
            bool leaf_rep = (cur > 0 && is_board_repetition(sl->game, &leaf->board));
            if (leaf->half_move >= 100 || is_insufficient_material(&leaf->board) || leaf_rep) {
                leaf->expanded = 1;
                leaf->terminal = 2;
                backprop_wdl(sl, cur, 0.0f, 1.0f, 0.0f, 0.0f, 1);
                continue;
            }

            // Atomically claim a batch slot for GPU neural evaluation
            int bi = atomic_fetch_add_explicit(&arena->pending_count, 1, memory_order_relaxed);
            if (bi >= max_batch) {
                int u = cur;
                while (u >= 0) {
                    if (sl->pool[u].vloss > 0) {
                        sl->pool[u].vloss--;
                    }
                    u = sl->pool[u].parent;
                }
                break;
            }

            az_pending_leaf *pl = &arena->pending[bi];
            pl->game_idx = gi;
            pl->node_idx = cur;
            pl->num_legal = n_legal;
            memcpy(pl->legal_moves, legal, (size_t) n_legal * sizeof(u64));
            memcpy(pl->legal_actions, actions, (size_t) n_legal * sizeof(int32_t));

            int root_rep = (cur == 0 && position_frequency(sl->game) >= 2) ? 1 : 0;
            az_encode_board_ctx(&leaf->board, (int) leaf->half_move, root_rep,
                out_planes + (size_t) bi * AZ_BOARD_FLOATS);
            out_legal_counts[bi] = n_legal;
            memcpy(out_legal_actions + (size_t) bi * MAX_LEGAL_MOVES, actions,
                (size_t) n_legal * sizeof(int32_t));

            seen_nodes[collected_here] = cur;
            sl->slot_pending[sl->slot_pending_n++] = bi;
            collected_here++;
        }
    }
}

static void apply_evals_range(az_arena *arena, int g_start, int g_end) {
    int n = arena->cur_apply_n;
    const float *policy_priors = arena->cur_priors;
    const float *wdl = arena->cur_wdl;
    const float *moves_left = arena->cur_ml;
    bool use_tactical = (arena->cfg.tactical_mode != 0);

    for (int gi = g_start; gi < g_end; gi++) {
        az_game_slot *sl = &arena->slots[gi];
        int pn = sl->slot_pending_n;
        for (int k = 0; k < pn; k++) {
            int bi = sl->slot_pending[k];
            if (bi < 0 || bi >= n) {
                continue;
            }
            az_pending_leaf *pl = &arena->pending[bi];
            az_node *leaf = &sl->pool[pl->node_idx];

            const float *priors = policy_priors + (size_t) bi * MAX_LEGAL_MOVES;
            float w = wdl[bi * 3 + 0];
            float d = wdl[bi * 3 + 1];
            float l = wdl[bi * 3 + 2];
            float ml = moves_left[bi];
            if (ml < 0.0f) {
                ml = 0.0f;
            }

            // Path-C Tactical Hybrid Booster (ONLY when tactical_mode != 0; OFF in pure Path-A RL!)
            if (use_tactical) {
                az_apply_tactical_wdl_delta(&leaf->board, &w, &d, &l);
            }

            if (!leaf->expanded && sl->pool_used + pl->num_legal <= AZ_MAX_NODES_PER_GAME) {
                leaf->first_child = sl->pool_used;
                leaf->num_children = (int16_t) pl->num_legal;
                for (int c = 0; c < pl->num_legal; c++) {
                    az_node *ch = &sl->pool[sl->pool_used++];
                    memset(ch, 0, sizeof(*ch));
                    ch->board = leaf->board;
                    u64 m = pl->legal_moves[c];
                    u8 sp = get_start_piece(m);
                    bool is_cap = (get_end_piece(m) != NONE) || is_en_passant_move(m);
                    chess_make_move(&ch->board, m);
                    ch->move_from_parent = m;
                    ch->action_from_parent = pl->legal_actions[c];
                    ch->parent = pl->node_idx;
                    ch->first_child = -1;
                    ch->half_move
                        = (sp == FRIENDLY_PAWN || is_cap) ? 0 : (int16_t) (leaf->half_move + 1);
                    ch->prior = priors[c];
                    ch->raw_prior = priors[c];
                }
                leaf->expanded = 1;
                if (pl->node_idx == 0) {
                    probe_root_mates(arena, sl);
                    apply_root_dirichlet(arena, sl);
                }
            }

            backprop_wdl(sl, pl->node_idx, w, d, l, ml, 1);
        }
        sl->slot_pending_n = 0;
    }
}

int az_arena_collect_leaves(az_arena *arena, int max_batch, float *out_planes,
    int32_t *out_legal_counts, int32_t *out_legal_actions) {
    if (!arena || max_batch <= 0) {
        return 0;
    }
    if (max_batch > arena->pending_cap) {
        max_batch = arena->pending_cap;
    }
    atomic_store_explicit(&arena->pending_count, 0, memory_order_relaxed);
    arena->cur_max_batch = max_batch;
    arena->cur_out_planes = out_planes;
    arena->cur_out_lc = out_legal_counts;
    arena->cur_out_la = out_legal_actions;

    az_dispatch_job(arena, AZ_JOB_COLLECT);

    int got = atomic_load_explicit(&arena->pending_count, memory_order_relaxed);
    if (got > max_batch) {
        got = max_batch;
        atomic_store_explicit(&arena->pending_count, got, memory_order_relaxed);
    }
    return got;
}

void az_arena_apply_evals(az_arena *arena, int batch_size, const float *policy_priors,
    const float *wdl, const float *moves_left) {
    if (!arena || batch_size <= 0) {
        return;
    }
    int pc = atomic_load_explicit(&arena->pending_count, memory_order_relaxed);
    int n = (batch_size < pc) ? batch_size : pc;
    if (n <= 0) {
        return;
    }
    arena->cur_apply_n = n;
    arena->cur_priors = policy_priors;
    arena->cur_wdl = wdl;
    arena->cur_ml = moves_left;

    az_dispatch_job(arena, AZ_JOB_APPLY);

    atomic_store_explicit(&arena->pending_count, 0, memory_order_relaxed);
}

// Compute training policy distribution at root (with KataGo Forced-Playout Policy Pruning!).
static void compute_root_policy(
    const az_arena *arena, const az_game_slot *sl, float out_probs[MAX_LEGAL_MOVES]) {
    const az_node *root = &sl->pool[0];
    int nc = root->num_children;
    if (nc <= 0) {
        return;
    }

    // If a forced checkmate child exists, put 100% policy target on it!
    for (int i = 0; i < nc; i++) {
        const az_node *ch = &sl->pool[root->first_child + i];
        if (ch->terminal == 1 || ch->proven == AZ_PROVEN_LOSS) {
            for (int j = 0; j < nc; j++) {
                out_probs[j] = (j == i) ? 1.0f : 0.0f;
            }
            return;
        }
    }

    int best_i = 0;
    int best_v = -1;
    for (int i = 0; i < nc; i++) {
        int v = sl->pool[root->first_child + i].visits;
        if (v > best_v) {
            best_v = v;
            best_i = i;
        }
    }

    const az_node *best_ch = &sl->pool[root->first_child + best_i];
    float sqrt_n = sqrtf((float) (root->visits > 0 ? root->visits : 1));
    float best_puct
        = (best_ch->visits > 0 ? ((best_ch->l_sum - best_ch->w_sum) / (float) best_ch->visits)
                               : 0.0f)
          + arena->cfg.c_puct * best_ch->prior * sqrt_n / (1.0f + (float) best_ch->visits);

    bool prune
        = (sl->is_full_search && arena->cfg.forced_playouts && arena->cfg.dirichlet_eps > 0.0f);
    double sum = 0.0;
    for (int i = 0; i < nc; i++) {
        const az_node *ch = &sl->pool[root->first_child + i];
        int eff_v = ch->visits;
        if (prune && i != best_i && eff_v > 0) {
            int max_forced = (int) sqrtf(2.0f * ch->prior * (float) root->visits);
            float cq = (ch->l_sum - ch->w_sum) / (float) ch->visits;
            while (eff_v > 0 && max_forced > 0) {
                float puct_at = cq + arena->cfg.c_puct * ch->prior * sqrt_n / (float) eff_v;
                if (puct_at < best_puct) {
                    eff_v--;
                    max_forced--;
                } else {
                    break;
                }
            }
            if (eff_v == 1) {
                eff_v = 0;
            }
        }
        out_probs[i] = (float) eff_v;
        sum += (double) eff_v;
    }

    if (sum <= 1e-9) {
        for (int i = 0; i < nc; i++) {
            out_probs[i] = (i == best_i) ? 1.0f : 0.0f;
        }
    } else {
        for (int i = 0; i < nc; i++) {
            out_probs[i] = (float) ((double) out_probs[i] / sum);
        }
    }
}

static void push_sample_queue(az_arena *arena, const az_sample *s) {
    if (arena->q_count >= arena->q_cap) {
        arena->q_head = (arena->q_head + 1) % arena->q_cap;
        arena->q_count--;
    }
    int tail = (arena->q_head + arena->q_count) % arena->q_cap;
    arena->sample_queue[tail] = *s;
    arena->q_count++;
}

static void step_moves_range(az_arena *arena, int wid, int g_start, int g_end) {
    int finished = 0;
    int ww = 0, dr = 0, bw = 0, plies_sum = 0;
    int auto_reset = arena->cur_auto_reset;

    for (int gi = g_start; gi < g_end; gi++) {
        az_game_slot *sl = &arena->slots[gi];
        az_node *root = &sl->pool[0];

        bool resigned = false;
        int resigned_white_outcome = 0;

        bool game_done = sl->game->is_game_over
                         || (int) sl->game->move_count >= arena->cfg.max_plies
                         || position_frequency(sl->game) >= 3 || sl->game->half_move_count >= 100;

        if (!game_done && root->visits >= sl->sims_target && root->expanded
            && root->num_children > 0) {
            float probs[MAX_LEGAL_MOVES];
            compute_root_policy(arena, sl, probs);

            // Compute MCTS root WDL for Q-blended targets & smart resignation
            float rw = 0.33f, rd = 0.34f, rl = 0.33f;
            if (root->visits > 0) {
                float inv = 1.0f / (float) root->visits;
                rw = root->w_sum * inv;
                rd = root->d_sum * inv;
                rl = root->l_sum * inv;
            }

            // Record training sample in trajectory buffer
            if (sl->traj_len < sl->traj_cap) {
                int k = sl->traj_len++;
                az_sample *sp = &sl->traj[k];
                memset(sp, 0, sizeof(*sp));
                int is_rep = (position_frequency(sl->game) >= 2) ? 1 : 0;
                az_encode_board_ctx(
                    sl->game->game, (int) sl->game->half_move_count, is_rep, sp->planes);
                sp->num_legal = root->num_children;
                for (int i = 0; i < root->num_children; i++) {
                    sp->actions[i] = sl->pool[root->first_child + i].action_from_parent;
                    sp->probs[i] = probs[i];
                }
                sp->wdl[0] = is_flipped(sl->game->game) ? 1.0f : 0.0f;
                sp->moves_left = (float) sl->game->move_count;
                sp->is_full_search = sl->is_full_search;
                sl->traj_search_wdl[k][0] = rw;
                sl->traj_search_wdl[k][1] = rd;
                sl->traj_search_wdl[k][2] = rl;
            }

            // Smart Resignation check (85% of self-play games; requires 4 consecutive plies beyond threshold)
            if (arena->cfg.resign_threshold < -0.5f && !sl->no_resign_game
                && (int) sl->game->move_count >= 30 && root->visits >= 16) {
                float rq = rw - rl;
                float thr = fabsf(arena->cfg.resign_threshold);
                if (fabsf(rq) >= thr) {
                    int w_sign = (rq > 0.0f) ? (is_flipped(sl->game->game) ? -1 : 1)
                                             : (is_flipped(sl->game->game) ? 1 : -1);
                    if (w_sign == (int) sl->resign_sign) {
                        sl->resign_streak++;
                    } else {
                        sl->resign_sign = (int8_t) w_sign;
                        sl->resign_streak = 1;
                    }
                    if (sl->resign_streak >= 4) {
                        resigned = true;
                        resigned_white_outcome = w_sign;
                        game_done = true;
                    }
                } else {
                    sl->resign_streak = 0;
                }
            }

            if (!game_done) {
                // Smooth opening temperature schedule:
                //   Plies 0..7: T=1.0 on full search, T=0.5 on fast search
                //   Plies 8..temp_plies-1: T=0.5 (v^2) sharpened sampling
                //   Plies >= temp_plies: greedy (always prefers proven mate!)
                int chosen_i = 0;
                int forced_mate_i = -1;
                for (int i = 0; i < root->num_children; i++) {
                    const az_node *ch = &sl->pool[root->first_child + i];
                    if (ch->terminal == 1 || ch->proven == AZ_PROVEN_LOSS) {
                        forced_mate_i = i;
                        break;
                    }
                }

                if (forced_mate_i >= 0) {
                    chosen_i = forced_mate_i;
                } else if ((int) sl->game->move_count < arena->cfg.temp_plies
                           && arena->cfg.dirichlet_eps > 0.0f) {
                    bool linear_t = (sl->is_full_search && (int) sl->game->move_count < 8);
                    double vsum = 0.0;
                    double weights[MAX_LEGAL_MOVES];
                    for (int i = 0; i < root->num_children; i++) {
                        const az_node *ch = &sl->pool[root->first_child + i];
                        double v = (ch->proven == AZ_PROVEN_WIN) ? 0.0 : (double) ch->visits;
                        weights[i] = linear_t ? v : (v * v);
                        vsum += weights[i];
                    }
                    if (vsum > 1e-12) {
                        double r = az_rng_uniform(&sl->rng) * vsum;
                        double acc = 0.0;
                        for (int i = 0; i < root->num_children; i++) {
                            acc += weights[i];
                            if (acc >= r) {
                                chosen_i = i;
                                break;
                            }
                        }
                    }
                } else {
                    int best_v = -1;
                    for (int i = 0; i < root->num_children; i++) {
                        const az_node *ch = &sl->pool[root->first_child + i];
                        if (ch->proven == AZ_PROVEN_WIN && best_v >= 0) {
                            continue;
                        }
                        if (ch->visits > best_v) {
                            best_v = ch->visits;
                            chosen_i = i;
                        }
                    }
                }

                int chosen_node_idx = root->first_child + chosen_i;
                u64 chosen_mv = sl->pool[chosen_node_idx].move_from_parent;
                static_move_to_san(sl->game->game, sl->game->current_legal_moves, chosen_mv,
                    sl->last_san, sizeof(sl->last_san));
                az_move_to_uci(sl->game->game, chosen_mv, sl->last_uci);
                make_move(sl->game, chosen_mv);

                game_done = sl->game->is_game_over
                            || (int) sl->game->move_count >= arena->cfg.max_plies
                            || position_frequency(sl->game) >= 3
                            || sl->game->half_move_count >= 100;
                if (!game_done) {
                    // TRUE MCTS SUBTREE REUSE! Carry over chosen child's tree into next ply!
                    reroot_slot_to_child(arena, sl, wid, chosen_node_idx);
                }
            }
        }

        if (game_done) {
            int white_outcome = 0;
            if (resigned) {
                white_outcome = resigned_white_outcome;
            } else if (sl->game->is_game_over && !sl->game->is_draw) {
                white_outcome = is_flipped(sl->game->game) ? 1 : -1;
            }
            if (white_outcome > 0) {
                ww++;
            } else if (white_outcome < 0) {
                bw++;
            } else {
                dr++;
            }
            int final_plies = (int) sl->game->move_count;
            plies_sum += final_plies;
            finished++;

            // Finalize Q-Blended WDL & Moves-Left targets on all trajectory samples!
            for (int k = 0; k < sl->traj_len; k++) {
                az_sample *sp = &sl->traj[k];
                bool was_black = (sp->wdl[0] > 0.5f);
                int stm_res = was_black ? -white_outcome : white_outcome;
                float ow = (stm_res > 0) ? 1.0f : 0.0f;
                float od = (stm_res == 0) ? 1.0f : 0.0f;
                float ol = (stm_res < 0) ? 1.0f : 0.0f;

                // KataGo Q-Blending: mix final outcome with MCTS root search WDL to cut variance
                float q_blend = sp->is_full_search ? 0.30f : 0.12f;
                float keep = 1.0f - q_blend;
                sp->wdl[0] = keep * ow + q_blend * sl->traj_search_wdl[k][0];
                sp->wdl[1] = keep * od + q_blend * sl->traj_search_wdl[k][1];
                sp->wdl[2] = keep * ol + q_blend * sl->traj_search_wdl[k][2];

                int rem = final_plies - (int) sp->moves_left;
                if (rem < 0) {
                    rem = 0;
                }
                sp->moves_left = (float) rem / 100.0f;
            }
            if (sl->traj_len > 0) {
                pthread_mutex_lock(&arena->queue_mtx);
                for (int k = 0; k < sl->traj_len; k++) {
                    push_sample_queue(arena, &sl->traj[k]);
                }
                pthread_mutex_unlock(&arena->queue_mtx);
            }
            sl->traj_len = 0;

            if (auto_reset) {
                reset_slot_internal(arena, sl, NULL);
            }
        }
    }

    arena->t_finished[wid] = finished;
    arena->t_ww[wid] = ww;
    arena->t_dr[wid] = dr;
    arena->t_bw[wid] = bw;
    arena->t_plies[wid] = plies_sum;
}

static void az_run_worker_slice(az_arena *arena, int wid) {
    int nt = arena->num_threads;
    int ng = arena->cfg.num_games;
    int g_start = (ng * wid) / nt;
    int g_end = (ng * (wid + 1)) / nt;
    if (g_start >= g_end) {
        if (arena->job_kind == AZ_JOB_STEP) {
            arena->t_finished[wid] = 0;
            arena->t_ww[wid] = 0;
            arena->t_dr[wid] = 0;
            arena->t_bw[wid] = 0;
            arena->t_plies[wid] = 0;
        }
        return;
    }
    if (arena->job_kind == AZ_JOB_COLLECT) {
        collect_leaves_range(arena, g_start, g_end);
    } else if (arena->job_kind == AZ_JOB_APPLY) {
        apply_evals_range(arena, g_start, g_end);
    } else if (arena->job_kind == AZ_JOB_STEP) {
        step_moves_range(arena, wid, g_start, g_end);
    }
}

int az_arena_step_moves(az_arena *arena, int auto_reset, int32_t *out_white_wins,
    int32_t *out_draws, int32_t *out_black_wins, int32_t *out_total_plies_finished) {
    if (!arena) {
        return 0;
    }
    arena->cur_auto_reset = auto_reset;
    az_dispatch_job(arena, AZ_JOB_STEP);

    int finished = 0, ww = 0, dr = 0, bw = 0, plies_sum = 0;
    for (int t = 0; t < arena->num_threads; t++) {
        finished += arena->t_finished[t];
        ww += arena->t_ww[t];
        dr += arena->t_dr[t];
        bw += arena->t_bw[t];
        plies_sum += arena->t_plies[t];
    }

    if (out_white_wins) {
        *out_white_wins = ww;
    }
    if (out_draws) {
        *out_draws = dr;
    }
    if (out_black_wins) {
        *out_black_wins = bw;
    }
    if (out_total_plies_finished) {
        *out_total_plies_finished = plies_sum;
    }
    return finished;
}

int az_arena_pop_samples(az_arena *arena, az_sample *out_buf, int max_samples) {
    if (!arena || !out_buf || max_samples <= 0) {
        return 0;
    }
    int n = (max_samples < arena->q_count) ? max_samples : arena->q_count;
    for (int i = 0; i < n; i++) {
        out_buf[i] = arena->sample_queue[arena->q_head];
        arena->q_head = (arena->q_head + 1) % arena->q_cap;
        arena->q_count--;
    }
    return n;
}

int az_arena_pending_samples(const az_arena *arena) {
    return arena ? arena->q_count : 0;
}

bool az_arena_get_snapshot(const az_arena *arena, int game_idx, az_game_snapshot *out) {
    if (!arena || !out || game_idx < 0 || game_idx >= arena->cfg.num_games) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    const az_game_slot *sl = &arena->slots[game_idx];
    const az_node *root = &sl->pool[0];

    out->game_id = game_idx;
    out->game_seq = sl->game_seq;
    out->ply = (int32_t) sl->game->move_count;
    out->sims_done = root->visits;
    out->sims_target = sl->sims_target;
    out->is_full_search = sl->is_full_search;

    if (root->visits > 0) {
        float inv = 1.0f / (float) root->visits;
        float w = root->w_sum * inv;
        float d = root->d_sum * inv;
        float l = root->l_sum * inv;
        if (is_flipped(sl->game->game)) {
            out->root_w = l;
            out->root_d = d;
            out->root_l = w;
        } else {
            out->root_w = w;
            out->root_d = d;
            out->root_l = l;
        }
        out->root_ml = root->ml_sum * inv * 100.0f;
    } else {
        out->root_w = 0.33f;
        out->root_d = 0.34f;
        out->root_l = 0.33f;
        out->root_ml = 60.0f;
    }

    get_fen(sl->game, out->fen);
    snprintf(out->last_san, sizeof(out->last_san), "%s", sl->last_san);
    snprintf(out->last_uci, sizeof(out->last_uci), "%s", sl->last_uci);

    chess white_view = *(sl->game->game);
    if (is_flipped(&white_view)) {
        flip_perspective(&white_view);
    }
    for (int sq = 0; sq < 64; sq++) {
        out->board[sq] = piece_to_char(get_piece_at_offset(&white_view, sq));
    }
    out->board[64] = '\0';

    if (root->expanded && root->num_children > 0) {
        int picked[3] = { -1, -1, -1 };
        int kmax = (root->num_children < 3) ? root->num_children : 3;
        for (int k = 0; k < kmax; k++) {
            int bi = -1;
            int bv = -1;
            float bp = -1.0f;
            for (int i = 0; i < root->num_children; i++) {
                bool used = false;
                for (int prev = 0; prev < k; prev++) {
                    if (picked[prev] == i) {
                        used = true;
                        break;
                    }
                }
                if (used) {
                    continue;
                }
                const az_node *ch = &sl->pool[root->first_child + i];
                if (ch->visits > bv || (ch->visits == bv && ch->prior > bp)) {
                    bv = ch->visits;
                    bp = ch->prior;
                    bi = i;
                }
            }
            if (bi < 0) {
                break;
            }
            picked[k] = bi;
            const az_node *ch = &sl->pool[root->first_child + bi];
            static_move_to_san(sl->game->game, sl->game->current_legal_moves, ch->move_from_parent,
                out->top_san[k], sizeof(out->top_san[k]));
            az_move_to_uci(sl->game->game, ch->move_from_parent, out->top_uci[k]);
            out->top_visits[k] = ch->visits;
            out->top_prior[k] = ch->prior;
            out->top_q[k]
                = (ch->visits > 0) ? ((ch->l_sum - ch->w_sum) / (float) ch->visits) : 0.0f;
            out->top_count++;
        }
    }
    return true;
}

bool az_arena_best_move(const az_arena *arena, int game_idx, float temperature, char out_uci[8],
    float out_wdl[3], float *out_ml) {
    if (!arena || game_idx < 0 || game_idx >= arena->cfg.num_games) {
        return false;
    }
    const az_game_slot *sl = &arena->slots[game_idx];
    const az_node *root = &sl->pool[0];
    if (!root->expanded || root->num_children <= 0) {
        if (num_legal_moves(sl->game) > 0) {
            az_move_to_uci(sl->game->game, sl->game->current_legal_moves[0], out_uci);
            if (out_wdl) {
                out_wdl[0] = 0.33f;
                out_wdl[1] = 0.34f;
                out_wdl[2] = 0.33f;
            }
            if (out_ml) {
                *out_ml = 60.0f;
            }
            return true;
        }
        snprintf(out_uci, 8, "0000");
        return false;
    }

    int chosen = 0;

    // 1. Always deliver an immediate Mate-in-1 if present!
    int mate1_idx = -1;
    int proven_mate_idx = -1;
    float best_mate_ml = 1e9f;
    bool has_safe_move = false;

    for (int i = 0; i < root->num_children; i++) {
        const az_node *ch = &sl->pool[root->first_child + i];
        if (ch->terminal == 1) {
            mate1_idx = i;
            break;
        }
        if (ch->proven == AZ_PROVEN_LOSS) {
            float ml_est = (ch->visits > 0) ? (ch->ml_sum / (float) ch->visits) : 0.02f;
            if (proven_mate_idx < 0 || ml_est < best_mate_ml) {
                proven_mate_idx = i;
                best_mate_ml = ml_est;
            }
        }
        if (ch->proven != AZ_PROVEN_WIN) {
            has_safe_move = true;
        }
    }

    if (mate1_idx >= 0) {
        chosen = mate1_idx;
    } else if (proven_mate_idx >= 0) {
        chosen = proven_mate_idx;
    } else if (temperature > 0.05f) {
        int best_v = 0;
        for (int i = 0; i < root->num_children; i++) {
            const az_node *ch = &sl->pool[root->first_child + i];
            if (has_safe_move && ch->proven == AZ_PROVEN_WIN) {
                continue;
            }
            if (ch->visits > best_v) {
                best_v = ch->visits;
            }
        }
        int min_v = (int) ((double) best_v * 0.35);
        if (min_v < 1) {
            min_v = 1;
        }
        double weights[MAX_LEGAL_MOVES];
        double sum = 0.0;
        double inv_t = 1.0 / (double) temperature;
        for (int i = 0; i < root->num_children; i++) {
            const az_node *ch = &sl->pool[root->first_child + i];
            int vi = (has_safe_move && ch->proven == AZ_PROVEN_WIN) ? 0 : ch->visits;
            weights[i] = (vi >= min_v) ? pow((double) vi, inv_t) : 0.0;
            sum += weights[i];
        }
        if (sum > 1e-12) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            u64 tmp_rng = arena->rng ^ (u64) ts.tv_nsec ^ ((u64) ts.tv_sec << 32)
                          ^ (u64) (root->visits * 2654435761u + 17u);
            double r = az_rng_uniform(&tmp_rng) * sum;
            double acc = 0.0;
            for (int i = 0; i < root->num_children; i++) {
                acc += weights[i];
                if (acc >= r) {
                    chosen = i;
                    break;
                }
            }
        }
    } else {
        int best_v = -1;
        float best_p = -1.0f;
        for (int i = 0; i < root->num_children; i++) {
            const az_node *ch = &sl->pool[root->first_child + i];
            if (has_safe_move && ch->proven == AZ_PROVEN_WIN) {
                continue;
            }
            if (ch->visits > best_v || (ch->visits == best_v && ch->prior > best_p)) {
                best_v = ch->visits;
                best_p = ch->prior;
                chosen = i;
            }
        }
    }

    // Path-C Root Tactical Safety Guard (play mode only, no forced mate, greedy):
    // If the top MCTS move drops >= 240 cp to an immediate tactical sequence (hanging piece / fork),
    // rescue to the best visited non-blundering move!
    if (arena->cfg.tactical_mode != 0 && mate1_idx < 0 && proven_mate_idx < 0
        && temperature <= 0.05f) {
        const az_node *ch_pick = &sl->pool[root->first_child + chosen];
        int mf = 0;
        // Opponent's tactical delta after our move (positive = opponent wins material!)
        int opp_gain = az_tactical_delta(&ch_pick->board, &mf);
        if (mf > 0 || opp_gain >= 240) {
            int best_alt = chosen;
            float best_alt_score = -1e9f;
            for (int i = 0; i < root->num_children; i++) {
                const az_node *ch = &sl->pool[root->first_child + i];
                if (has_safe_move && ch->proven == AZ_PROVEN_WIN) {
                    continue;
                }
                int alt_mf = 0;
                int alt_opp_gain = az_tactical_delta(&ch->board, &alt_mf);
                if (alt_mf > 0) {
                    continue;
                }
                float cq = (ch->visits > 0) ? ((ch->l_sum - ch->w_sum) / (float) ch->visits) : -0.5f;
                float sc = cq * 300.0f - (float) alt_opp_gain + 15.0f * logf(1.0f + (float) ch->visits);
                if (alt_mf < 0) {
                    sc += 5000.0f;
                }
                if (sc > best_alt_score) {
                    best_alt_score = sc;
                    best_alt = i;
                }
            }
            chosen = best_alt;
        }
    }

    const az_node *ch = &sl->pool[root->first_child + chosen];
    az_move_to_uci(sl->game->game, ch->move_from_parent, out_uci);
    if (out_wdl) {
        if (ch->terminal == 1 || ch->proven == AZ_PROVEN_LOSS) {
            out_wdl[0] = 0.995f;
            out_wdl[1] = 0.002f;
            out_wdl[2] = 0.003f;
        } else if (ch->visits > 0) {
            float inv = 1.0f / (float) ch->visits;
            out_wdl[0] = ch->l_sum * inv;
            out_wdl[1] = ch->d_sum * inv;
            out_wdl[2] = ch->w_sum * inv;
        } else if (root->visits > 0) {
            float inv = 1.0f / (float) root->visits;
            out_wdl[0] = root->w_sum * inv;
            out_wdl[1] = root->d_sum * inv;
            out_wdl[2] = root->l_sum * inv;
        } else {
            out_wdl[0] = 0.33f;
            out_wdl[1] = 0.34f;
            out_wdl[2] = 0.33f;
        }
    }
    if (out_ml) {
        *out_ml = (root->visits > 0) ? (root->ml_sum / (float) root->visits * 100.0f) : 60.0f;
    }
    return true;
}
