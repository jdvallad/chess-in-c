#define _POSIX_C_SOURCE 200809L

#include "core/attacks.h"
#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/primitives.h"
#include "engine/book.h"
#include "engine/endgame.h"
#include "engine/engine.h"
#include "engine/engine_internal.h"
#include "engine/eval.h"
#include "engine/nnue.h"
#include "engine/see.h"
#include "engine/tt.h"
#include "engine/zobrist.h"

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double wall_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec * 1e-9;
}

static const difficulty_preset g_presets[] = {
    { 1, "Novice", "Depth 1, frequent slips (~850 Elo)", 1, 50, 28 },
    { 2, "Casual", "Depth 2 + quiescence (~1250 Elo)", 2, 100, 4 },
    { 3, "Club", "Depth 3 tactical search (~1600 Elo)", 3, 200, 0 },
    { 4, "Strong", "Iterative deepening, 0.45s / d6 + Book (~1950 Elo)", 6, 450, 0 },
    { 5, "Master", "Iterative deepening, 1.2s / d11 + Book (~2250 Elo)", 11, 1200, 0 },
    { 6, "Grandmaster", "Unleashed: 3.0s / d20 + Book + SMP (~2550+ Elo)", 20, 3000, 0 },
};

static pthread_once_t g_engine_once = PTHREAD_ONCE_INIT;
static int g_lmr_table[64][64];

typedef struct search_params {
    int rfp_base;
    int rfp_impr;
    int fut_mult;
    int fut_base;
    int probcut_m;
    int asp_delta;
    int se_depth;
    int se_dbl_m;
    int hist_prune;
    int hist_lmr_div;
    int raz_margin;
    int corr_div;
} search_params;

static search_params g_sp = {
    .rfp_base = 75,
    .rfp_impr = 15,
    .fut_mult = 110,
    .fut_base = 70,
    .probcut_m = 190,
    .asp_delta = 25,
    .se_depth = 7,
    .se_dbl_m = 35,
    .hist_prune = 2400,
    .hist_lmr_div = 8192,
    .raz_margin = 240,
    .corr_div = 512,
};

bool engine_set_param(const char *name, int val) {
    if (!name) {
        return false;
    }
    if (strcmp(name, "RFPBase") == 0) {
        g_sp.rfp_base = val;
        return true;
    }
    if (strcmp(name, "RFPImpr") == 0) {
        g_sp.rfp_impr = val;
        return true;
    }
    if (strcmp(name, "FutMult") == 0) {
        g_sp.fut_mult = val;
        return true;
    }
    if (strcmp(name, "FutBase") == 0) {
        g_sp.fut_base = val;
        return true;
    }
    if (strcmp(name, "ProbCutM") == 0) {
        g_sp.probcut_m = val;
        return true;
    }
    if (strcmp(name, "AspDelta") == 0) {
        g_sp.asp_delta = val;
        return true;
    }
    if (strcmp(name, "SEDepth") == 0) {
        g_sp.se_depth = val;
        return true;
    }
    if (strcmp(name, "SEDblM") == 0) {
        g_sp.se_dbl_m = val;
        return true;
    }
    if (strcmp(name, "HistPrune") == 0) {
        g_sp.hist_prune = val;
        return true;
    }
    if (strcmp(name, "HistLMRDiv") == 0) {
        g_sp.hist_lmr_div = (val > 512) ? val : 512;
        return true;
    }
    if (strcmp(name, "RazMargin") == 0) {
        g_sp.raz_margin = val;
        return true;
    }
    if (strcmp(name, "CorrDiv") == 0) {
        g_sp.corr_div = (val > 64) ? val : 64;
        return true;
    }
    return false;
}

static void engine_global_init_impl(void) {
    attacks_init();
    zobrist_init();
    endgame_init();
    nnue_init();
    for (int d = 1; d < 64; d++) {
        for (int m = 1; m < 64; m++) {
            int r = (int) (0.75 + log((double) d) * log((double) m) / 2.25);
            if (r < 0) {
                r = 0;
            }
            g_lmr_table[d][m] = r;
        }
    }
}

void engine_global_init(void) {
    pthread_once(&g_engine_once, engine_global_init_impl);
}

// =========================================================================
// Engine Lifecycle & Configuration
// =========================================================================

static void *smp_helper_loop(void *arg);

static void clear_worker_tables(search_worker *w) {
    if (!w) {
        return;
    }
    memset(w->killers, 0, sizeof(w->killers));
    memset(w->history, 0, sizeof(w->history));
    memset(w->cap_history, 0, sizeof(w->cap_history));
    memset(w->cont_history, 0, sizeof(w->cont_history));
    memset(w->followup_history, 0, sizeof(w->followup_history));
    memset(w->cont4_history, 0, sizeof(w->cont4_history));
    memset(w->corr_hist, 0, sizeof(w->corr_hist));
    memset(w->nonpawn_corr, 0, sizeof(w->nonpawn_corr));
    memset(w->countermove, 0, sizeof(w->countermove));
    memset(w->pawn_tt, 0, sizeof(w->pawn_tt));
    memset(w->finny, 0, sizeof(w->finny));
}

static void smp_ensure_pool(engine *eng, int needed) {
    if (needed >= MAX_THREADS) {
        needed = MAX_THREADS - 1;
    }
    while (eng->pool_size < needed) {
        int idx = eng->pool_size;
        search_worker *hw = (search_worker *) calloc(1, sizeof(search_worker));
        if (!hw) {
            break;
        }
        hw->eng = eng;
        hw->thread_id = idx + 1;
        eng->helpers[idx] = hw;
        if (pthread_create(&eng->helper_threads[idx], NULL, smp_helper_loop, hw) == 0) {
            eng->pool_size++;
        } else {
            free(hw);
            eng->helpers[idx] = NULL;
            break;
        }
    }
}

static void smp_shutdown_pool(engine *eng) {
    if (!eng || eng->pool_size <= 0) {
        return;
    }
    pthread_mutex_lock(&eng->smp_mu);
    eng->smp_shutdown = true;
    pthread_cond_broadcast(&eng->smp_cv);
    pthread_mutex_unlock(&eng->smp_mu);

    for (int t = 0; t < eng->pool_size; t++) {
        pthread_join(eng->helper_threads[t], NULL);
        free(eng->helpers[t]);
        eng->helpers[t] = NULL;
    }
    eng->pool_size = 0;
}

engine *engine_create(size_t tt_mb) {
    engine_global_init();
    engine *eng = (engine *) calloc(1, sizeof(engine));
    if (!eng) {
        return NULL;
    }
    if (!tt_alloc(eng, tt_mb)) {
        free(eng);
        return NULL;
    }
    eng->main_worker = (search_worker *) calloc(1, sizeof(search_worker));
    if (!eng->main_worker) {
        tt_free(eng);
        free(eng);
        return NULL;
    }
    eng->main_worker->eng = eng;
    eng->main_worker->thread_id = 0;
    struct timespec rng_ts;
    clock_gettime(CLOCK_MONOTONIC, &rng_ts);
    eng->rng_state = ((u64) time(NULL) << 32) ^ (u64) rng_ts.tv_nsec ^ (u64) (uintptr_t) eng
                     ^ 0x9e3779b97f4a7c15ull;
    eng->num_threads = 1;
    eng->use_book = true;
    eng->limit_strength = false;
    eng->target_elo = 1800;
    atomic_init(&eng->stop_flag, false);
    atomic_init(&eng->shared_depth, 0);
    pthread_mutex_init(&eng->smp_mu, NULL);
    pthread_cond_init(&eng->smp_cv, NULL);
    pthread_cond_init(&eng->smp_done_cv, NULL);
    return eng;
}

void engine_clear_tt(engine *eng) {
    if (!eng) {
        return;
    }
    tt_clear(eng);
    clear_worker_tables(eng->main_worker);
    for (int t = 0; t < eng->pool_size; t++) {
        clear_worker_tables(eng->helpers[t]);
    }
    eng->tt_gen = 0;
}

void engine_delete(engine **eng) {
    if (eng && *eng) {
        smp_shutdown_pool(*eng);
        pthread_cond_destroy(&(*eng)->smp_done_cv);
        pthread_cond_destroy(&(*eng)->smp_cv);
        pthread_mutex_destroy(&(*eng)->smp_mu);
        tt_free(*eng);
        free((*eng)->main_worker);
        free(*eng);
        *eng = NULL;
    }
}

void engine_set_threads(engine *eng, int num_threads) {
    if (!eng) {
        return;
    }
    if (num_threads < 1) {
        num_threads = 1;
    }
    if (num_threads > MAX_THREADS) {
        num_threads = MAX_THREADS;
    }
    eng->num_threads = num_threads;
}

int engine_get_threads(const engine *eng) {
    return eng ? eng->num_threads : 1;
}

void engine_set_use_book(engine *eng, bool use_book) {
    if (eng) {
        eng->use_book = use_book;
    }
}

bool engine_get_use_book(const engine *eng) {
    return eng ? eng->use_book : false;
}

void engine_set_book_path(engine *eng, const char *path) {
    if (!eng) {
        return;
    }
    if (!path || path[0] == '\0') {
        eng->book_path[0] = '\0';
    } else {
        snprintf(eng->book_path, sizeof(eng->book_path), "%s", path);
    }
}

void engine_set_info_callback(engine *eng, engine_info_cb cb, void *user_data) {
    if (eng) {
        eng->info_cb = cb;
        eng->info_user_data = user_data;
    }
}

void engine_set_multipv_callback(engine *eng, engine_multipv_cb cb, void *user_data) {
    if (eng) {
        eng->multipv_cb = cb;
        eng->multipv_user_data = user_data;
    }
}

void engine_request_stop(engine *eng) {
    if (eng) {
        atomic_store_explicit(&eng->stop_flag, true, memory_order_relaxed);
    }
}

void engine_set_limit_strength(engine *eng, bool enable, int target_elo) {
    if (!eng) {
        return;
    }
    if (target_elo < 800) {
        target_elo = 800;
    } else if (target_elo > 2600) {
        target_elo = 2600;
    }
    eng->limit_strength = enable;
    eng->target_elo = target_elo;
}

bool engine_get_limit_strength(const engine *eng, int *target_elo_out) {
    if (!eng) {
        if (target_elo_out) {
            *target_elo_out = 1800;
        }
        return false;
    }
    if (target_elo_out) {
        *target_elo_out = eng->target_elo;
    }
    return eng->limit_strength;
}

const difficulty_preset *engine_get_preset(int level) {
    int n = (int) (sizeof(g_presets) / sizeof(g_presets[0]));
    if (level < 1) {
        level = 1;
    }
    if (level > n) {
        level = n;
    }
    return &g_presets[level - 1];
}

int engine_num_presets(void) {
    return (int) (sizeof(g_presets) / sizeof(g_presets[0]));
}

// =========================================================================
// History Gravity Helpers & Move Ordering
// =========================================================================

static inline u64 compute_enemy_threats(const chess *b) {
    u64 enemy = b->enemy_pieces;
    u64 occ = b->friendly_pieces | enemy;
    u64 tp = b->pawns & PAWN_MASK;
    u64 e_pawns = enemy & tp;
    u64 th = ((e_pawns & ~COL_1) >> 9) | ((e_pawns & ~COL_8) >> 7);
    u64 e_knights = enemy & ~(tp | b->orthogonal_pieces | b->diagonal_pieces | b->kings);
    while (e_knights) {
        int sq = __builtin_ctzll(e_knights);
        e_knights &= e_knights - 1ull;
        th |= g_knight_attacks[sq];
    }
    u64 e_bishops = enemy & b->diagonal_pieces & ~b->orthogonal_pieces;
    while (e_bishops) {
        int sq = __builtin_ctzll(e_bishops);
        e_bishops &= e_bishops - 1ull;
        th |= bishop_attacks_bb(sq, occ);
    }
    u64 e_rooks = enemy & b->orthogonal_pieces & ~b->diagonal_pieces;
    while (e_rooks) {
        int sq = __builtin_ctzll(e_rooks);
        e_rooks &= e_rooks - 1ull;
        th |= rook_attacks_bb(sq, occ);
    }
    return th;
}

static inline int threat_bucket(u64 threats, int src, int dst) {
    return (int) ((((threats >> src) & 1ull) << 1) | ((threats >> dst) & 1ull));
}

static inline void update_hist_entry(int16_t *slot, int bonus) {
    int clamped = (bonus > HIST_MAX) ? HIST_MAX : ((bonus < -HIST_MAX) ? -HIST_MAX : bonus);
    int cur = *slot;
    cur += clamped - (cur * abs(clamped)) / HIST_MAX;
    if (cur > HIST_MAX) {
        cur = HIST_MAX;
    } else if (cur < -HIST_MAX) {
        cur = -HIST_MAX;
    }
    *slot = (int16_t) cur;
}

static inline bool should_stop(search_worker *w) {
    if (w->local_abort) {
        return true;
    }
    if ((w->nodes & 2047ull) == 0ull) {
        if (atomic_load_explicit(&w->eng->stop_flag, memory_order_relaxed)) {
            w->local_abort = true;
            return true;
        }
        if (w->time_limited && wall_time_sec() >= w->deadline_time) {
            w->local_abort = true;
            atomic_store_explicit(&w->eng->stop_flag, true, memory_order_relaxed);
            return true;
        }
    }
    return false;
}

static inline u32 pawn_corr_index(const chess *b) {
    u64 tp = b->pawns & PAWN_MASK;
    u64 fp = b->friendly_pieces & tp;
    u64 ep = b->enemy_pieces & tp;
    if (is_flipped(b)) {
        u64 wp = __builtin_bswap64(ep);
        u64 bp = __builtin_bswap64(fp);
        u64 h = (wp * 0x9e3779b97f4a7c15ull) ^ (bp * 0xbf58476d1ce4e5b9ull);
        return (u32) ((h >> 32) & CORR_MASK);
    }
    u64 h = (fp * 0x9e3779b97f4a7c15ull) ^ (ep * 0xbf58476d1ce4e5b9ull);
    return (u32) ((h >> 32) & CORR_MASK);
}

static inline void nonpawn_corr_indices(const chess *b, u32 *w_idx_out, u32 *b_idx_out) {
    u64 tp = b->pawns & PAWN_MASK;
    u64 f_np = b->friendly_pieces & ~tp;
    u64 e_np = b->enemy_pieces & ~tp;
    u64 f_o = b->orthogonal_pieces & f_np;
    u64 f_d = b->diagonal_pieces & f_np;
    u64 e_o = b->orthogonal_pieces & e_np;
    u64 e_d = b->diagonal_pieces & e_np;

    if (is_flipped(b)) {
        u64 wh = (__builtin_bswap64(e_np) * 0x9e3779b97f4a7c15ull)
                 ^ (__builtin_bswap64(e_o) * 0xbf58476d1ce4e5b9ull)
                 ^ (__builtin_bswap64(e_d) * 0x94d049bb133111ebull);
        u64 bh = (__builtin_bswap64(f_np) * 0x9e3779b97f4a7c15ull)
                 ^ (__builtin_bswap64(f_o) * 0xbf58476d1ce4e5b9ull)
                 ^ (__builtin_bswap64(f_d) * 0x94d049bb133111ebull);
        *w_idx_out = (u32) ((wh >> 32) & CORR_MASK);
        *b_idx_out = (u32) ((bh >> 32) & CORR_MASK);
    } else {
        u64 wh = (f_np * 0x9e3779b97f4a7c15ull) ^ (f_o * 0xbf58476d1ce4e5b9ull)
                 ^ (f_d * 0x94d049bb133111ebull);
        u64 bh = (e_np * 0x9e3779b97f4a7c15ull) ^ (e_o * 0xbf58476d1ce4e5b9ull)
                 ^ (e_d * 0x94d049bb133111ebull);
        *w_idx_out = (u32) ((wh >> 32) & CORR_MASK);
        *b_idx_out = (u32) ((bh >> 32) & CORR_MASK);
    }
}

static inline void update_corr_hist(int16_t *slot, int diff, int depth) {
    int weight = depth * depth + 2 * depth + 1;
    if (weight > 128) {
        weight = 128;
    }
    int scaled = diff * 256;
    if (scaled > 16384) {
        scaled = 16384;
    } else if (scaled < -16384) {
        scaled = -16384;
    }
    int cur = *slot;
    int nv = (cur * (256 - weight) + scaled * weight) / 256;
    if (nv > 16384) {
        nv = 16384;
    } else if (nv < -16384) {
        nv = -16384;
    }
    *slot = (int16_t) nv;
}

static int score_move(const search_worker *w, const chess *board, int side, u64 m, u16 tt_tag,
    u16 cm_tag, int prev_pt, int prev_to_f, int prev2_pt, int prev2_to, int ply) {
    u16 tag = pack_tt_move(m);
    if (tt_tag != 0u && tag == tt_tag) {
        return 2000000;
    }
    u8 cap = get_end_piece(m);
    u8 sp = get_start_piece(m);
    u8 promo = get_promotion_piece(m);
    int sp_pt = piece_type_idx(sp);
    int dst = get_end_offset(m);

    if (cap != NONE || is_en_passant_move(m)) {
        int vic_pt = is_en_passant_move(m) ? 0 : piece_type_idx(cap);
        int vic_val = g_see_value[vic_pt];
        int att_val = g_see_value[sp_pt];
        int ch = w->cap_history[sp_pt][dst][vic_pt];
        if (vic_val >= att_val) {
            return 1050000 + vic_val * 16 - att_val + ch;
        }
        int see = engine_see(board, m);
        if (see >= 0) {
            return 1000000 + vic_val * 16 - att_val + see + ch;
        }
        return -100000 + see + ch;
    }
    if (promo != sp) {
        return 900000 + g_mg_value[piece_type_idx(promo)];
    }
    if (ply < MAX_PLY) {
        if (m == w->killers[ply][0]) {
            return 800000;
        }
        if (m == w->killers[ply][1]) {
            return 750000;
        }
    }
    if (cm_tag != 0u && tag == cm_tag) {
        return 700000;
    }
    int src = get_start_offset(m);
    u64 th = compute_enemy_threats(board);
    int hist = w->history[side][src][dst][threat_bucket(th, src, dst)];
    if (prev_pt >= 0) {
        hist += w->cont_history[prev_pt][prev_to_f][sp_pt][dst];
    }
    if (prev2_pt >= 0) {
        hist += w->followup_history[prev2_pt][prev2_to][sp_pt][dst];
    }
    if (is_castle_king_move(m) || is_castle_queen_move(m)) {
        hist += 4000;
    }
    return hist;
}

static inline void pick_best_move(u64 *moves, int *scores, int start, int count) {
    int best_idx = start;
    int best_sc = scores[start];
    for (int i = start + 1; i < count; i++) {
        if (scores[i] > best_sc) {
            best_sc = scores[i];
            best_idx = i;
        }
    }
    if (best_idx != start) {
        u64 tm = moves[start];
        moves[start] = moves[best_idx];
        moves[best_idx] = tm;
        int ts = scores[start];
        scores[start] = scores[best_idx];
        scores[best_idx] = ts;
    }
}

// =========================================================================
// Quiescence Search (TT Probe + Delta Pruning + Lazy SEE + Check Evasions)
// =========================================================================

static int quiescence(search_worker *w, chess *board, u64 hash, int alpha, int beta, int ply) {
    w->nodes++;
    if (should_stop(w)) {
        return 0;
    }

    int orig_alpha = alpha;
    bool is_pv = (beta - alpha) > 1;

    u16 tt_tag = 0u;
    int tt_score = 0;
    int tt_eval = TT_EVAL_NONE;
    int tt_depth = 0;
    u8 tt_flag = 0;
    bool tt_hit = tt_probe(w->eng, hash, &tt_tag, &tt_score, &tt_eval, &tt_depth, &tt_flag);
    if (tt_hit) {
        if (!is_pv) {
            if (tt_score > MATE_BOUND) {
                tt_score -= ply;
            } else if (tt_score < -MATE_BOUND) {
                tt_score += ply;
            }
            if (tt_flag == TT_EXACT) {
                return tt_score;
            }
            if (tt_flag == TT_LOWER && tt_score >= beta) {
                return tt_score;
            }
            if (tt_flag == TT_UPPER && tt_score <= alpha) {
                return tt_score;
            }
        }
    }

    u64 f_king_bb = board->kings & board->friendly_pieces;
    int king_sq = f_king_bb ? __builtin_ctzll(f_king_bb) : 64;
    bool checked = (king_sq < 64) && attacked_offset(board, king_sq);

    int raw_eval = TT_EVAL_NONE;
    int stand_pat = -INF_SCORE;
    if (!checked) {
        raw_eval = (tt_hit && tt_eval != TT_EVAL_NONE) ? tt_eval : eval_board_ply(w, board, ply);
        stand_pat = raw_eval;
        if (stand_pat >= beta) {
            return beta;
        }
        if (stand_pat > alpha) {
            alpha = stand_pat;
        }
    }
    if (ply >= MAX_PLY - 1) {
        return checked ? eval_board_ply(w, board, ply) : stand_pat;
    }

    u64 moves[MAX_LEGAL_MOVES];
    int count;
    if (checked) {
        count = set_legal_moves(board, moves);
        if (count == 0) {
            return -MATE_SCORE + ply;
        }
    } else {
        count = set_tactical_moves(board, moves);
    }

    int scores[MAX_LEGAL_MOVES];
    for (int i = 0; i < count; i++) {
        u64 m = moves[i];
        if (tt_tag != 0u && pack_tt_move(m) == tt_tag) {
            scores[i] = 2000000;
            continue;
        }
        u8 cap = get_end_piece(m);
        u8 sp = get_start_piece(m);
        u8 promo = get_promotion_piece(m);
        if (cap != NONE || is_en_passant_move(m)) {
            int vic_pt = is_en_passant_move(m) ? 0 : piece_type_idx(cap);
            scores[i] = 100000 + g_see_value[vic_pt] * 16 - g_see_value[piece_type_idx(sp)];
        } else if (promo != sp) {
            scores[i] = 90000 + g_mg_value[piece_type_idx(promo)];
        } else {
            scores[i] = 0;
        }
    }

    bool has_enemy_pieces
        = (board->enemy_pieces & ~(board->pawns & PAWN_MASK) & ~board->kings) != 0ull;
    int best_score = checked ? -INF_SCORE : stand_pat;
    u64 best_move = NULL_MOVE;

    for (int i = 0; i < count; i++) {
        pick_best_move(moves, scores, i, count);
        u64 m = moves[i];

        if (!checked) {
            u8 sp = get_start_piece(m);
            u8 promo = get_promotion_piece(m);
            bool is_promo = (promo != sp);
            if (!is_promo) {
                u8 cap = get_end_piece(m);
                int vic_pt = is_en_passant_move(m) ? 0 : piece_type_idx(cap);
                int vic_val = (vic_pt >= 0) ? g_see_value[vic_pt] : 0;
                if (has_enemy_pieces && stand_pat + vic_val + 220 < alpha) {
                    continue;
                }
                int att_val = g_see_value[piece_type_idx(sp)];
                if (att_val > vic_val && engine_see(board, m) < 0) {
                    continue;
                }
            }
        }

        u64 child_hash = engine_update_hash(hash, board, m);
        tt_prefetch(w->eng, child_hash);
        if (nnue_get_mode() != NNUE_OFF && nnue_is_loaded()) {
            nnue_acc_mark_lazy(&w->acc_stack[ply + 1], board, m);
        }
        chess child = *board;
        chess_make_move(&child, m);
        int score = -quiescence(w, &child, child_hash, -beta, -alpha, ply + 1);
        if (w->local_abort) {
            return 0;
        }
        if (score > best_score) {
            best_score = score;
            best_move = m;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) {
                    break;
                }
            }
        }
    }

    int store_sc = best_score;
    if (store_sc > MATE_BOUND) {
        store_sc += ply;
    } else if (store_sc < -MATE_BOUND) {
        store_sc -= ply;
    }
    u8 flag = (best_score <= orig_alpha) ? TT_UPPER : ((best_score >= beta) ? TT_LOWER : TT_EXACT);
    tt_store(w->eng, hash, pack_tt_move(best_move), store_sc, raw_eval, 0, flag);

    return best_score;
}

static inline bool has_non_pawn_material(const chess *b) {
    u64 own = b->friendly_pieces;
    u64 own_pawns = own & (b->pawns & PAWN_MASK);
    u64 own_king = own & b->kings;
    return (own & ~own_pawns & ~own_king) != 0ull;
}

static inline bool is_repetition(u64 hash, const u64 *path, int path_len) {
    for (int i = path_len - 2; i >= 0; i -= 2) {
        if (path[i] == hash) {
            return true;
        }
    }
    return false;
}

// =========================================================================
// Staged (Lazy) Move Picker for Interior Nodes
// =========================================================================

typedef enum mp_stage {
    MP_TT = 0,
    MP_GEN_TACTICAL,
    MP_GOOD_TACTICAL,
    MP_KILLER1,
    MP_KILLER2,
    MP_COUNTERMOVE,
    MP_GEN_QUIETS,
    MP_QUIETS,
    MP_BAD_TACTICAL,
    MP_DONE,
} mp_stage;

typedef struct move_picker {
    mp_stage stage;
    u16 tt_tag;
    u16 excluded_tag;
    u16 k1_tag;
    u16 k2_tag;
    u16 cm_tag;
    int side;
    int prev_pt;
    int prev_to_f;
    int prev2_pt;
    int prev2_to;
    int prev4_pt;
    int prev4_to;
    int ply;
    u64 threats;
    bool threats_ready;

    u64 caps[64];
    int cap_scores[64];
    int cap_idx;
    int cap_count;

    u64 bad_caps[64];
    int bad_cap_idx;
    int bad_cap_count;

    u64 quiets[MAX_LEGAL_MOVES];
    int quiet_scores[MAX_LEGAL_MOVES];
    int quiet_idx;
    int quiet_count;
} move_picker;

static inline bool is_move_quiet_fast(u64 m) {
    return (get_end_piece(m) == NONE) && !is_en_passant_move(m)
           && (get_promotion_piece(m) == get_start_piece(m));
}

static void mp_init(move_picker *mp, const search_worker *w, int side, u16 tt_tag, u16 excluded_tag,
    u16 cm_tag, int prev_pt, int prev_to_f, int prev2_pt, int prev2_to, int prev4_pt, int prev4_to,
    int ply) {
    mp->stage = (tt_tag != 0u && tt_tag != excluded_tag) ? MP_TT : MP_GEN_TACTICAL;
    mp->tt_tag = tt_tag;
    mp->excluded_tag = excluded_tag;
    mp->k1_tag = (ply < MAX_PLY) ? pack_tt_move(w->killers[ply][0]) : 0u;
    mp->k2_tag = (ply < MAX_PLY) ? pack_tt_move(w->killers[ply][1]) : 0u;
    mp->cm_tag = cm_tag;
    mp->side = side;
    mp->prev_pt = prev_pt;
    mp->prev_to_f = prev_to_f;
    mp->prev2_pt = prev2_pt;
    mp->prev2_to = prev2_to;
    mp->prev4_pt = prev4_pt;
    mp->prev4_to = prev4_to;
    mp->ply = ply;
    mp->threats = 0ull;
    mp->threats_ready = false;
    mp->cap_idx = 0;
    mp->cap_count = 0;
    mp->bad_cap_idx = 0;
    mp->bad_cap_count = 0;
    mp->quiet_idx = 0;
    mp->quiet_count = 0;
}

static u64 mp_next(
    move_picker *mp, const search_worker *w, chess *board, int *score_out, bool *is_tt_out) {
    *is_tt_out = false;
    *score_out = 0;

    for (;;) {
        switch (mp->stage) {
        case MP_TT: {
            mp->stage = MP_GEN_TACTICAL;
            u64 m = validate_packed_move(board, mp->tt_tag);
            if (m != NULL_MOVE) {
                *score_out = 2000000;
                *is_tt_out = true;
                return m;
            }
            mp->tt_tag = 0u; // TT tag wasn't legal here (collision)
            break;
        }

        case MP_GEN_TACTICAL: {
            u64 raw[MAX_LEGAL_MOVES];
            int n = set_tactical_moves(board, raw);
            int gc = 0, bc = 0;
            for (int i = 0; i < n; i++) {
                u64 m = raw[i];
                u16 tag = pack_tt_move(m);
                if (tag == mp->tt_tag || (mp->excluded_tag != 0u && tag == mp->excluded_tag)) {
                    continue;
                }
                u8 sp = get_start_piece(m);
                u8 cap = get_end_piece(m);
                u8 promo = get_promotion_piece(m);
                bool is_ep = is_en_passant_move(m);
                bool is_promo = (promo != sp);
                int sc;
                if (is_promo) {
                    sc = 950000 + g_mg_value[piece_type_idx(promo)];
                    mp->caps[gc] = m;
                    mp->cap_scores[gc++] = sc;
                } else {
                    int att_pt = piece_type_idx(sp);
                    int vic_pt = is_ep ? 0 : piece_type_idx(cap);
                    int dst = get_end_offset(m);
                    int see = engine_see(board, m);
                    if (see >= 0) {
                        sc = 1000000 + g_see_value[vic_pt] * 16 - g_see_value[att_pt]
                             + w->cap_history[att_pt][dst][vic_pt];
                        mp->caps[gc] = m;
                        mp->cap_scores[gc++] = sc;
                    } else if (bc < 64) {
                        mp->bad_caps[bc++] = m;
                    }
                }
            }
            mp->cap_count = gc;
            mp->cap_idx = 0;
            mp->bad_cap_count = bc;
            mp->bad_cap_idx = 0;
            mp->stage = MP_GOOD_TACTICAL;
            break;
        }

        case MP_GOOD_TACTICAL: {
            if (mp->cap_idx < mp->cap_count) {
                pick_best_move(mp->caps, mp->cap_scores, mp->cap_idx, mp->cap_count);
                *score_out = mp->cap_scores[mp->cap_idx];
                return mp->caps[mp->cap_idx++];
            }
            mp->stage = MP_KILLER1;
            break;
        }

        case MP_KILLER1: {
            mp->stage = MP_KILLER2;
            u16 tag = mp->k1_tag;
            if (tag != 0u && tag != mp->tt_tag && tag != mp->excluded_tag) {
                u64 m = validate_packed_move(board, tag);
                if (m != NULL_MOVE && is_move_quiet_fast(m)) {
                    *score_out = 900000;
                    return m;
                }
            }
            mp->k1_tag = 0u;
            break;
        }

        case MP_KILLER2: {
            mp->stage = MP_COUNTERMOVE;
            u16 tag = mp->k2_tag;
            if (tag != 0u && tag != mp->tt_tag && tag != mp->excluded_tag && tag != mp->k1_tag) {
                u64 m = validate_packed_move(board, tag);
                if (m != NULL_MOVE && is_move_quiet_fast(m)) {
                    *score_out = 850000;
                    return m;
                }
            }
            mp->k2_tag = 0u;
            break;
        }

        case MP_COUNTERMOVE: {
            mp->stage = MP_GEN_QUIETS;
            u16 tag = mp->cm_tag;
            if (tag != 0u && tag != mp->tt_tag && tag != mp->excluded_tag && tag != mp->k1_tag
                && tag != mp->k2_tag) {
                u64 m = validate_packed_move(board, tag);
                if (m != NULL_MOVE && is_move_quiet_fast(m)) {
                    *score_out = 820000;
                    return m;
                }
            }
            mp->cm_tag = 0u;
            break;
        }

        case MP_GEN_QUIETS: {
            if (!mp->threats_ready) {
                mp->threats = compute_enemy_threats(board);
                mp->threats_ready = true;
            }
            u64 raw[MAX_LEGAL_MOVES];
            int n = set_quiet_moves(board, raw);
            int qc = 0;
            for (int i = 0; i < n; i++) {
                u64 m = raw[i];
                u16 tag = pack_tt_move(m);
                if (tag == mp->tt_tag || tag == mp->k1_tag || tag == mp->k2_tag || tag == mp->cm_tag
                    || (mp->excluded_tag != 0u && tag == mp->excluded_tag)) {
                    continue;
                }
                int src = get_start_offset(m);
                int dst = get_end_offset(m);
                int pt = piece_type_idx(get_start_piece(m));
                int tb = threat_bucket(mp->threats, src, dst);
                int sc = (int) w->history[mp->side][src][dst][tb];
                if (mp->prev_pt >= 0) {
                    sc += (int) w->cont_history[mp->prev_pt][mp->prev_to_f][pt][dst];
                }
                if (mp->prev2_pt >= 0) {
                    sc += (int) w->followup_history[mp->prev2_pt][mp->prev2_to][pt][dst];
                }
                if (mp->prev4_pt >= 0) {
                    sc += (int) w->cont4_history[mp->prev4_pt][mp->prev4_to][pt][dst] / 2;
                }
                mp->quiets[qc] = m;
                mp->quiet_scores[qc++] = sc;
            }
            mp->quiet_count = qc;
            mp->quiet_idx = 0;
            mp->stage = MP_QUIETS;
            break;
        }

        case MP_QUIETS: {
            if (mp->quiet_idx < mp->quiet_count) {
                pick_best_move(mp->quiets, mp->quiet_scores, mp->quiet_idx, mp->quiet_count);
                *score_out = mp->quiet_scores[mp->quiet_idx];
                return mp->quiets[mp->quiet_idx++];
            }
            mp->stage = MP_BAD_TACTICAL;
            break;
        }

        case MP_BAD_TACTICAL: {
            if (mp->bad_cap_idx < mp->bad_cap_count) {
                *score_out = -100000;
                return mp->bad_caps[mp->bad_cap_idx++];
            }
            mp->stage = MP_DONE;
            return NULL_MOVE;
        }

        default: return NULL_MOVE;
        }
    }
}

// =========================================================================
// Main Alpha-Beta Search
// =========================================================================

static int alphabeta(search_worker *w, chess *board, u64 hash, int depth, int alpha, int beta,
    int ply, int path_len, int half_move, bool allow_null, bool cut_node, u64 prev_move,
    u16 excluded_tag) {
    w->nodes++;
    if (should_stop(w)) {
        return 0;
    }

    if (ply > 0 && excluded_tag == 0u
        && (is_repetition(hash, w->path, path_len) || is_insufficient_material(board))) {
        return 0;
    }

    if (ply > 0) {
        int m_alpha = -MATE_SCORE + ply;
        int m_beta = MATE_SCORE - ply - 1;
        if (m_alpha > alpha) {
            alpha = m_alpha;
        }
        if (m_beta < beta) {
            beta = m_beta;
        }
        if (alpha >= beta) {
            return alpha;
        }
    }

    u64 f_king_bb = board->kings & board->friendly_pieces;
    int king_sq = f_king_bb ? __builtin_ctzll(f_king_bb) : 64;
    bool checked = (king_sq < 64) && attacked_offset(board, king_sq);

    if (ply > 0 && half_move >= 100 && !checked) {
        return 0;
    }

    if (checked && ply < MAX_PLY - 2) {
        depth++;
    }

    if (depth <= 0) {
        return quiescence(w, board, hash, alpha, beta, ply);
    }

    bool is_pv = (beta - alpha) > 1;
    int orig_alpha = alpha;

    u16 tt_tag = 0u;
    int tt_score = 0;
    int tt_eval = TT_EVAL_NONE;
    int tt_depth = 0;
    u8 tt_flag = 0;
    bool tt_hit = false;
    if (excluded_tag == 0u) {
        tt_hit = tt_probe(w->eng, hash, &tt_tag, &tt_score, &tt_eval, &tt_depth, &tt_flag);
        if (tt_hit) {
            if (tt_score > MATE_BOUND) {
                tt_score -= ply;
            } else if (tt_score < -MATE_BOUND) {
                tt_score += ply;
            }
            if (tt_depth >= depth && ply > 0 && !is_pv) {
                if (tt_flag == TT_EXACT) {
                    return tt_score;
                }
                if (tt_flag == TT_LOWER && tt_score > alpha) {
                    alpha = tt_score;
                } else if (tt_flag == TT_UPPER && tt_score < beta) {
                    beta = tt_score;
                }
                if (alpha >= beta) {
                    return tt_score;
                }
            }
        }
    }

    if (ply + 1 < MAX_PLY) {
        w->killers[ply + 1][0] = NULL_MOVE;
        w->killers[ply + 1][1] = NULL_MOVE;
    }

    // Internal Iterative Reductions (IIR)
    if (depth >= 4 && tt_tag == 0u && !checked && excluded_tag == 0u) {
        depth--;
    }

    int side = is_flipped(board) ? 1 : 0;
    u32 corr_idx = pawn_corr_index(board);
    u32 w_np_idx = 0u, b_np_idx = 0u;
    nonpawn_corr_indices(board, &w_np_idx, &b_np_idx);
    int raw_static_eval = -INF_SCORE;
    int static_eval = -INF_SCORE;
    int eval = -INF_SCORE;
    bool improving = false;
    if (!checked) {
        raw_static_eval
            = (tt_hit && tt_eval != TT_EVAL_NONE) ? tt_eval : eval_board_ply(w, board, ply);
        // Apply multi-source Static Eval Correction History (pawn + White/Black non-pawn)
        int corr_delta = (w->corr_hist[side][corr_idx] * 2 + w->nonpawn_corr[side][0][w_np_idx]
                             + w->nonpawn_corr[side][1][b_np_idx])
                         / g_sp.corr_div;
        static_eval = raw_static_eval + corr_delta;
        if (half_move >= 44) {
            static_eval = (static_eval * (200 - half_move)) / 200;
        }
        if (static_eval > MATE_BOUND - 100) {
            static_eval = MATE_BOUND - 100;
        } else if (static_eval < -MATE_BOUND + 100) {
            static_eval = -MATE_BOUND + 100;
        }
        eval = static_eval;
        if (tt_hit && tt_depth >= 1 && abs(tt_score) < MATE_BOUND) {
            if (tt_flag == TT_EXACT || (tt_flag == TT_LOWER && tt_score > eval)
                || (tt_flag == TT_UPPER && tt_score < eval)) {
                eval = tt_score;
            }
        }
        if (ply < MAX_PLY) {
            w->eval_stack[ply] = static_eval;
            improving = (ply >= 2 && w->eval_stack[ply - 2] != -INF_SCORE
                         && static_eval > w->eval_stack[ply - 2]);
        }
    } else if (ply < MAX_PLY) {
        w->eval_stack[ply] = -INF_SCORE;
    }

    // Pre-movegen pruning (non-PV, not in check, not singular search)
    if (!is_pv && !checked && ply > 0 && excluded_tag == 0u) {
        if (depth <= 6 && abs(beta) < MATE_BOUND) {
            int rfp_margin = (g_sp.rfp_base - g_sp.rfp_impr * (int) improving) * depth;
            if (eval - rfp_margin >= beta) {
                return eval;
            }
        }

        if (depth <= 2 && abs(alpha) < MATE_BOUND && eval + g_sp.raz_margin * depth <= alpha) {
            int q = quiescence(w, board, hash, alpha, alpha + 1, ply);
            if (w->local_abort) {
                return 0;
            }
            if (q <= alpha) {
                return q;
            }
        }

        if (allow_null && depth >= 3 && has_non_pawn_material(board) && eval >= beta) {
            chess null_b = *board;
            int old_ep = has_en_passant(board) ? (get_en_passant_offset(board) - 40) : 8;
            u64 null_hash = hash ^ g_zobrist_ep[old_ep] ^ g_zobrist_ep[8] ^ g_zobrist_side;
            tt_prefetch(w->eng, null_hash);
            null_b.pawns
                &= ~((FRIENDLY | ENEMY) & (EN_PASSANT_TOGGLED | EN_PASSANT_OFFSET | IN_CHECK));
            flip_perspective(&null_b);

            if (ply < MAX_PLY) {
                w->move_stack[ply] = NULL_MOVE;
                w->double_exts[ply + 1] = w->double_exts[ply];
            }
            if (nnue_get_mode() != NNUE_OFF && nnue_is_loaded()) {
                nnue_acc_mark_null(&w->acc_stack[ply + 1]);
            }
            int eval_bonus = (eval - beta) / 200;
            if (eval_bonus > 3) {
                eval_bonus = 3;
            }
            int R = 3 + depth / 6 + eval_bonus;
            int null_score = -alphabeta(w, &null_b, null_hash, depth - 1 - R, -beta, -beta + 1,
                ply + 1, path_len, 0, false, !cut_node, NULL_MOVE, 0u);
            if (w->local_abort) {
                return 0;
            }
            if (null_score >= beta && null_score < MATE_BOUND) {
                return beta;
            }
        }

        // ProbCut: shallow tactical verification pruning on high-depth non-PV nodes
        if (depth >= 5 && abs(beta) < MATE_BOUND) {
            int prob_beta = beta + g_sp.probcut_m;
            int prob_depth = depth - 4;
            if (!(tt_hit && tt_depth >= prob_depth && tt_score < prob_beta)) {
                u64 pc_moves[MAX_LEGAL_MOVES];
                int pc_cnt = set_tactical_moves(board, pc_moves);
                int see_thresh = prob_beta - static_eval;
                for (int i = 0; i < pc_cnt; i++) {
                    u64 pm = pc_moves[i];
                    if (engine_see(board, pm) < see_thresh) {
                        continue;
                    }
                    u64 ch = engine_update_hash(hash, board, pm);
                    tt_prefetch(w->eng, ch);
                    if (nnue_get_mode() != NNUE_OFF && nnue_is_loaded()) {
                        nnue_acc_mark_lazy(&w->acc_stack[ply + 1], board, pm);
                    }
                    chess child = *board;
                    chess_make_move(&child, pm);
                    int psc = -quiescence(w, &child, ch, -prob_beta, -prob_beta + 1, ply + 1);
                    if (w->local_abort) {
                        return 0;
                    }
                    if (psc >= prob_beta) {
                        if (ply < MAX_PLY) {
                            w->double_exts[ply + 1] = w->double_exts[ply];
                        }
                        psc = -alphabeta(w, &child, ch, prob_depth, -prob_beta, -prob_beta + 1,
                            ply + 1, path_len + 1, 0, true, !cut_node, pm, 0u);
                        if (w->local_abort) {
                            return 0;
                        }
                    }
                    if (psc >= prob_beta) {
                        tt_store(w->eng, hash, pack_tt_move(pm), psc, raw_static_eval,
                            prob_depth + 1, TT_LOWER);
                        return psc;
                    }
                }
            }
        }
    }

    u16 cm_tag = 0u;
    int prev_from_f = 0, prev_to_f = 0, prev_pt = -1;
    if (prev_move != NULL_MOVE) {
        prev_from_f = get_start_offset(prev_move) ^ 56;
        prev_to_f = get_end_offset(prev_move) ^ 56;
        prev_pt = piece_type_idx(get_promotion_piece(prev_move));
        cm_tag = w->countermove[prev_from_f][prev_to_f];
    }

    int prev2_pt = -1, prev2_to = 0;
    if (ply >= 2 && w->move_stack[ply - 2] != NULL_MOVE) {
        u64 pm2 = w->move_stack[ply - 2];
        prev2_pt = piece_type_idx(get_promotion_piece(pm2));
        prev2_to = get_end_offset(pm2);
    }

    int prev4_pt = -1, prev4_to = 0;
    if (ply >= 4 && w->move_stack[ply - 4] != NULL_MOVE) {
        u64 pm4 = w->move_stack[ply - 4];
        prev4_pt = piece_type_idx(get_promotion_piece(pm4));
        prev4_to = get_end_offset(pm4);
    }

    move_picker mp;
    mp_init(&mp, w, side, tt_tag, excluded_tag, cm_tag, prev_pt, prev_to_f, prev2_pt, prev2_to,
        prev4_pt, prev4_to, ply);

    w->path[path_len] = hash;
    u64 best_move = NULL_MOVE;
    int best_score = -INF_SCORE;

    u64 quiets_tried[64];
    int q_tried = 0;
    u64 caps_tried[32];
    int c_tried = 0;

    int lmp_limit
        = (!is_pv && !checked && depth <= 4) ? ((3 + depth * depth) / (improving ? 1 : 2)) : 9999;
    bool can_futility = (!is_pv && !checked && depth <= 5 && ply > 0 && abs(alpha) < MATE_BOUND
                         && static_eval != -INF_SCORE
                         && static_eval + g_sp.fut_mult * depth + g_sp.fut_base <= alpha);

    int legal_searched = 0;
    u64 m;
    int move_score = 0;
    bool is_tt_move = false;

    while ((m = mp_next(&mp, w, board, &move_score, &is_tt_move)) != NULL_MOVE) {
        u16 m_tag = pack_tt_move(m);
        if (best_move == NULL_MOVE) {
            best_move = m;
        }

        u8 sp = get_start_piece(m);
        u8 cap = get_end_piece(m);
        u8 promo = get_promotion_piece(m);
        bool is_ep = is_en_passant_move(m);
        bool is_cap = (cap != NONE) || is_ep;
        bool is_promo = (promo != sp);
        bool is_quiet = !is_cap && !is_promo;
        bool is_special_quiet
            = (ply < MAX_PLY && (m == w->killers[ply][0] || m == w->killers[ply][1]))
              || (cm_tag != 0u && m_tag == cm_tag);

        // Singular Extensions v2: Lower trigger depth + Double Extension budget + Multi-Cut + Negative Extensions
        int ext = 0;
        if (is_tt_move && depth >= g_sp.se_depth && ply > 0 && ply < w->cur_iter_depth * 2
            && excluded_tag == 0u && tt_hit && tt_depth >= depth - 3
            && (tt_flag == TT_LOWER || tt_flag == TT_EXACT) && abs(tt_score) < MATE_BOUND
            && ply < MAX_PLY - 4) {
            int s_beta = tt_score - 2 * depth;
            int s_depth = (depth - 1) / 2;
            int s_score = alphabeta(w, board, hash, s_depth, s_beta - 1, s_beta, ply, path_len,
                half_move, false, cut_node, prev_move, tt_tag);
            if (w->local_abort) {
                return 0;
            }
            if (s_score < s_beta) {
                ext = 1;
                if (!is_pv && s_score < s_beta - g_sp.se_dbl_m && w->double_exts[ply] < 6) {
                    ext = 2;
                }
            } else if (s_beta >= beta) {
                return s_beta;
            } else if (tt_score >= beta) {
                ext = -1;
            } else if (cut_node) {
                ext = -1;
            }
        } else if (sp == FRIENDLY_PAWN && get_end_offset(m) >= 48 && ply < MAX_PLY - 4) {
            ext = 1;
        }

        int sp_pt = piece_type_idx(sp);
        int dst = get_end_offset(m);
        int quiet_hval = 0;
        if (is_quiet) {
            if (!mp.threats_ready) {
                mp.threats = compute_enemy_threats(board);
                mp.threats_ready = true;
            }
            int m_src = get_start_offset(m);
            int tb = threat_bucket(mp.threats, m_src, dst);
            quiet_hval
                = w->history[side][m_src][dst][tb]
                  + (prev_pt >= 0 ? w->cont_history[prev_pt][prev_to_f][sp_pt][dst] : 0)
                  + (prev2_pt >= 0 ? w->followup_history[prev2_pt][prev2_to][sp_pt][dst] : 0)
                  + (prev4_pt >= 0 ? w->cont4_history[prev4_pt][prev4_to][sp_pt][dst] / 2 : 0);
        }

        u64 child_hash = engine_update_hash(hash, board, m);
        tt_prefetch(w->eng, child_hash);
        int next_hm = (sp == FRIENDLY_PAWN || is_cap) ? 0 : (half_move + 1);
        chess child = *board;
        chess_make_move(&child, m);

        u64 opp_kbb = child.kings & child.friendly_pieces;
        int opp_ksq = opp_kbb ? __builtin_ctzll(opp_kbb) : 64;
        bool gives_check = (opp_ksq < 64) && attacked_offset(&child, opp_ksq);

        if (!is_pv && !checked && legal_searched > 0 && !gives_check && !is_promo
            && best_score > -MATE_BOUND) {
            if (is_quiet) {
                if (q_tried >= lmp_limit && !is_special_quiet) {
                    continue;
                }
                if (can_futility && !is_special_quiet) {
                    continue;
                }
                if (depth <= 4 && !is_special_quiet && quiet_hval < -g_sp.hist_prune * depth) {
                    continue;
                }
                if (depth <= 4 && legal_searched >= 2 && !is_special_quiet
                    && engine_see(board, m) < -25 * depth * depth) {
                    continue;
                }
            } else if (depth <= 5 && move_score < 0 && engine_see(board, m) < -90 * depth) {
                continue;
            }
        }

        if (ply < MAX_PLY) {
            w->move_stack[ply] = m;
            w->double_exts[ply + 1] = (uint8_t) (w->double_exts[ply] + (ext >= 2 ? 1u : 0u));
        }
        if (nnue_get_mode() != NNUE_OFF && nnue_is_loaded()) {
            nnue_acc_mark_lazy(&w->acc_stack[ply + 1], board, m);
        }

        int new_depth = depth - 1 + ext;
        int score;

        if (legal_searched == 0) {
            score = -alphabeta(w, &child, child_hash, new_depth, -beta, -alpha, ply + 1,
                path_len + 1, next_hm, true, false, m, 0u);
        } else {
            int r = 0;
            if (depth >= 3 && legal_searched >= 2 && is_quiet && !checked && !gives_check) {
                int di = (depth < 63) ? depth : 63;
                int mi = (legal_searched < 63) ? legal_searched : 63;
                r = g_lmr_table[di][mi];
                if (is_pv) {
                    r--;
                }
                if (!improving) {
                    r++;
                }
                if (is_special_quiet) {
                    r--;
                }
                r -= quiet_hval / g_sp.hist_lmr_div;
                if (r < 0) {
                    r = 0;
                }
                if (r >= new_depth) {
                    r = new_depth - 1;
                }
            }

            if (r > 0) {
                score = -alphabeta(w, &child, child_hash, new_depth - r, -alpha - 1, -alpha,
                    ply + 1, path_len + 1, next_hm, true, true, m, 0u);
            } else {
                score = alpha + 1;
            }

            if (score > alpha && !w->local_abort) {
                score = -alphabeta(w, &child, child_hash, new_depth, -alpha - 1, -alpha, ply + 1,
                    path_len + 1, next_hm, true, !cut_node, m, 0u);
                if (score > alpha && score < beta && !w->local_abort) {
                    score = -alphabeta(w, &child, child_hash, new_depth, -beta, -alpha, ply + 1,
                        path_len + 1, next_hm, true, false, m, 0u);
                }
            }
        }
        legal_searched++;

        if (w->local_abort) {
            return 0;
        }

        if (score > best_score) {
            best_score = score;
            best_move = m;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) {
                    int bonus = depth * depth * 16;
                    if (bonus > 1600) {
                        bonus = 1600;
                    }
                    if (is_quiet) {
                        if (ply < MAX_PLY && w->killers[ply][0] != m) {
                            w->killers[ply][1] = w->killers[ply][0];
                            w->killers[ply][0] = m;
                        }
                        if (prev_move != NULL_MOVE) {
                            w->countermove[prev_from_f][prev_to_f] = m_tag;
                        }
                        int src = get_start_offset(m);
                        int tb = threat_bucket(mp.threats, src, dst);
                        update_hist_entry(&w->history[side][src][dst][tb], bonus);
                        if (prev_pt >= 0) {
                            update_hist_entry(
                                &w->cont_history[prev_pt][prev_to_f][sp_pt][dst], bonus);
                        }
                        if (prev2_pt >= 0) {
                            update_hist_entry(
                                &w->followup_history[prev2_pt][prev2_to][sp_pt][dst], bonus);
                        }
                        if (prev4_pt >= 0) {
                            update_hist_entry(
                                &w->cont4_history[prev4_pt][prev4_to][sp_pt][dst], bonus);
                        }
                        for (int q = 0; q < q_tried; q++) {
                            u64 qm = quiets_tried[q];
                            int qs = get_start_offset(qm);
                            int qd = get_end_offset(qm);
                            int qpt = piece_type_idx(get_start_piece(qm));
                            int qtb = threat_bucket(mp.threats, qs, qd);
                            update_hist_entry(&w->history[side][qs][qd][qtb], -bonus);
                            if (prev_pt >= 0) {
                                update_hist_entry(
                                    &w->cont_history[prev_pt][prev_to_f][qpt][qd], -bonus);
                            }
                            if (prev2_pt >= 0) {
                                update_hist_entry(
                                    &w->followup_history[prev2_pt][prev2_to][qpt][qd], -bonus);
                            }
                            if (prev4_pt >= 0) {
                                update_hist_entry(
                                    &w->cont4_history[prev4_pt][prev4_to][qpt][qd], -bonus);
                            }
                        }
                    } else if (is_cap) {
                        int vic_pt = is_ep ? 0 : piece_type_idx(cap);
                        update_hist_entry(&w->cap_history[sp_pt][dst][vic_pt], bonus);
                        for (int c = 0; c < c_tried; c++) {
                            u64 cm = caps_tried[c];
                            int csp = piece_type_idx(get_start_piece(cm));
                            int cvp
                                = is_en_passant_move(cm) ? 0 : piece_type_idx(get_end_piece(cm));
                            update_hist_entry(
                                &w->cap_history[csp][get_end_offset(cm)][cvp], -bonus);
                        }
                    }
                    break;
                }
            }
        }

        if (is_quiet && q_tried < 64) {
            quiets_tried[q_tried++] = m;
        } else if (is_cap && c_tried < 32) {
            caps_tried[c_tried++] = m;
        }
    }

    if (legal_searched == 0) {
        if (excluded_tag != 0u) {
            return alpha;
        }
        return checked ? (-MATE_SCORE + ply) : 0;
    }

    if (excluded_tag == 0u) {
        u8 flag
            = (best_score <= orig_alpha) ? TT_UPPER : ((best_score >= beta) ? TT_LOWER : TT_EXACT);

        // Update Static Eval Correction History when best move is quiet and bound is informative
        bool best_is_quiet = (get_end_piece(best_move) == NONE) && !is_en_passant_move(best_move)
                             && (get_promotion_piece(best_move) == get_start_piece(best_move));
        if (!checked && best_is_quiet && abs(best_score) < MATE_BOUND
            && raw_static_eval != -INF_SCORE) {
            if (flag == TT_EXACT || (flag == TT_LOWER && best_score > static_eval)
                || (flag == TT_UPPER && best_score < static_eval)) {
                int diff = best_score - raw_static_eval;
                update_corr_hist(&w->corr_hist[side][corr_idx], diff, depth);
                update_corr_hist(&w->nonpawn_corr[side][0][w_np_idx], diff, depth);
                update_corr_hist(&w->nonpawn_corr[side][1][b_np_idx], diff, depth);
            }
        }

        int store_score = best_score;
        if (store_score > MATE_BOUND) {
            store_score += ply;
        } else if (store_score < -MATE_BOUND) {
            store_score -= ply;
        }
        int store_ev = (raw_static_eval != -INF_SCORE) ? raw_static_eval : TT_EVAL_NONE;
        tt_store(w->eng, hash, pack_tt_move(best_move), store_score, store_ev, depth, flag);
    }

    return best_score;
}

static int root_search_excluded(search_worker *w, u64 root_hash, int depth, int alpha, int beta,
    u64 *best_move_io, int *scores, const u64 *excluded, int num_excluded) {
    u64 iter_best = NULL_MOVE;
    int iter_score = -INF_SCORE;
    chess *board = &w->root_board;
    if (nnue_get_mode() != NNUE_OFF && nnue_is_loaded()) {
        nnue_acc_refresh(&w->acc_stack[0], board);
    }
    w->eval_stack[0] = eval_board_ply(w, board, 0);
    w->double_exts[0] = 0u;
    w->double_exts[1] = 0u;
    int searched = 0;

    for (int i = 0; i < w->root_count; i++) {
        pick_best_move(w->root_moves, scores, i, w->root_count);
        u64 m = w->root_moves[i];

        bool skip = false;
        for (int e = 0; e < num_excluded; e++) {
            if (excluded[e] == m) {
                skip = true;
                break;
            }
        }
        if (skip) {
            continue;
        }
        if (iter_best == NULL_MOVE) {
            iter_best = m;
        }

        u8 sp = get_start_piece(m);
        bool is_cap = (get_end_piece(m) != NONE) || is_en_passant_move(m);
        int next_hm = (sp == FRIENDLY_PAWN || is_cap) ? 0 : (w->root_half_move + 1);
        u64 child_hash = engine_update_hash(root_hash, board, m);
        tt_prefetch(w->eng, child_hash);
        chess child = *board;
        chess_make_move(&child, m);
        w->move_stack[0] = m;
        if (nnue_get_mode() != NNUE_OFF && nnue_is_loaded()) {
            nnue_acc_mark_lazy(&w->acc_stack[1], board, m);
        }

        int sc;
        if (searched == 0) {
            sc = -alphabeta(w, &child, child_hash, depth - 1, -beta, -alpha, 1,
                w->base_path_len + 1, next_hm, true, false, m, 0u);
        } else {
            sc = -alphabeta(w, &child, child_hash, depth - 1, -alpha - 1, -alpha, 1,
                w->base_path_len + 1, next_hm, true, true, m, 0u);
            if (sc > alpha && !w->local_abort) {
                sc = -alphabeta(w, &child, child_hash, depth - 1, -beta, -alpha, 1,
                    w->base_path_len + 1, next_hm, true, false, m, 0u);
            }
        }
        searched++;

        if (w->local_abort) {
            return 0;
        }
        if (sc > iter_score) {
            iter_score = sc;
            iter_best = m;
            if (sc > alpha) {
                alpha = sc;
                if (alpha >= beta) {
                    break;
                }
            }
        }
    }
    if (iter_best != NULL_MOVE) {
        *best_move_io = iter_best;
    }
    return iter_score;
}

static int root_search(search_worker *w, u64 root_hash, int depth, int alpha, int beta,
    u64 *best_move_io, int *scores) {
    return root_search_excluded(w, root_hash, depth, alpha, beta, best_move_io, scores, NULL, 0);
}

static void smp_run_worker_search(search_worker *w) {
    u64 root_hash = engine_hash_board(&w->root_board);
    int side = is_flipped(&w->root_board) ? 1 : 0;
    w->path[w->base_path_len] = root_hash;
    u64 best = w->root_moves[0];
    int last_score = 0;

    int d = 1 + (w->thread_id % 2);
    while (d <= w->max_depth && !w->local_abort) {
        if (atomic_load_explicit(&w->eng->stop_flag, memory_order_relaxed)) {
            break;
        }
        int md = atomic_load_explicit(&w->eng->shared_depth, memory_order_relaxed);
        int target = md + 1 + (w->thread_id % 2);
        if (d < target && target <= w->max_depth) {
            d = target;
        }
        w->cur_iter_depth = d;

        int scores[MAX_LEGAL_MOVES];
        int alpha = -INF_SCORE;
        int beta = INF_SCORE;
        int delta = 25 + w->thread_id * 5;
        if (d >= 4 && abs(last_score) < MATE_BOUND) {
            alpha = last_score - delta;
            beta = last_score + delta;
        }

        u64 iter_best = best;
        int sc = 0;
        for (;;) {
            u16 cur_tag = pack_tt_move(iter_best);
            for (int i = 0; i < w->root_count; i++) {
                int s = score_move(
                    w, &w->root_board, side, w->root_moves[i], cur_tag, 0u, -1, 0, -1, 0, 0);
                if (s < 1900000) {
                    s += ((w->thread_id * 41 + i * 17 + d * 7) & 63) - 32;
                }
                scores[i] = s;
            }
            sc = root_search(w, root_hash, d, alpha, beta, &iter_best, scores);
            if (w->local_abort) {
                break;
            }
            if (sc <= alpha) {
                alpha = (delta > 500) ? -INF_SCORE : (alpha - delta);
                delta *= 2;
            } else if (sc >= beta) {
                beta = (delta > 500) ? INF_SCORE : (beta + delta);
                delta *= 2;
            } else {
                break;
            }
        }
        if (!w->local_abort) {
            best = iter_best;
            last_score = sc;
            tt_store(w->eng, root_hash, pack_tt_move(best), sc, TT_EVAL_NONE, d, TT_EXACT);
        }
        d++;
    }
}

static void *smp_helper_loop(void *arg) {
    search_worker *w = (search_worker *) arg;
    engine *eng = w->eng;
    u64 my_epoch = 0;

    for (;;) {
        pthread_mutex_lock(&eng->smp_mu);
        while (!eng->smp_shutdown && eng->smp_epoch == my_epoch) {
            pthread_cond_wait(&eng->smp_cv, &eng->smp_mu);
        }
        if (eng->smp_shutdown) {
            pthread_mutex_unlock(&eng->smp_mu);
            break;
        }
        my_epoch = eng->smp_epoch;
        bool participate = (w->thread_id <= eng->target_helpers);
        pthread_mutex_unlock(&eng->smp_mu);

        if (participate) {
            smp_run_worker_search(w);
            pthread_mutex_lock(&eng->smp_mu);
            eng->active_helpers--;
            if (eng->active_helpers == 0) {
                pthread_cond_signal(&eng->smp_done_cv);
            }
            pthread_mutex_unlock(&eng->smp_mu);
        }
    }
    return NULL;
}

search_result engine_search_timed(
    engine *eng, const full_chess *game, int max_depth, int soft_ms, int hard_ms) {
    search_result res;
    memset(&res, 0, sizeof(res));

    int count = num_legal_moves(game);
    if (count == 0) {
        res.best_move = NULL_MOVE;
        res.score_cp = in_check(game->game) ? -MATE_SCORE : 0;
        return res;
    }

    if (max_depth < 1) {
        max_depth = 1;
    }
    if (max_depth > MAX_PLY - 4) {
        max_depth = MAX_PLY - 4;
    }

    if (hard_ms > 0 && (soft_ms <= 0 || soft_ms > hard_ms)) {
        soft_ms = hard_ms;
    }
    int time_ms = hard_ms;

    eng->tt_gen = (u8) ((eng->tt_gen + 1u) & 63u);
    atomic_store_explicit(&eng->stop_flag, false, memory_order_relaxed);
    atomic_store_explicit(&eng->shared_depth, 0, memory_order_relaxed);

    search_worker *mw = eng->main_worker;
    for (int c = 0; c < 2; c++) {
        for (int s = 0; s < 64; s++) {
            for (int e = 0; e < 64; e++) {
                for (int tb = 0; tb < 4; tb++) {
                    mw->history[c][s][e][tb] = (int16_t) (mw->history[c][s][e][tb] / 2);
                }
            }
        }
    }
    memset(mw->killers, 0, sizeof(mw->killers));
    memset(mw->countermove, 0, sizeof(mw->countermove));

    int path_len = 0;
    int hist_start = (game->move_count > 120) ? (game->move_count - 120) : 0;
    for (int i = hist_start; i < game->move_count; i++) {
        mw->path[path_len++] = engine_hash_board(&game->game_history[i]);
    }
    mw->base_path_len = path_len;
    mw->root_board = *(game->game);
    mw->root_count = count;
    mw->root_half_move = (int) game->half_move_count;
    for (int i = 0; i < count; i++) {
        mw->root_moves[i] = game->current_legal_moves[i];
    }

    double start_time = wall_time_sec();
    mw->nodes = 0ull;
    mw->max_depth = max_depth;
    mw->time_limited = (time_ms > 0);
    mw->deadline_time = start_time + ((double) time_ms / 1000.0);
    mw->local_abort = false;

    u64 root_hash = engine_hash_board(&mw->root_board);
    mw->path[path_len] = root_hash;
    int root_side = is_flipped(&mw->root_board) ? 1 : 0;

    bool use_smp
        = (eng->num_threads > 1) && (max_depth >= 5) && (!mw->time_limited || time_ms >= 50);
    int n_helpers = use_smp ? (eng->num_threads - 1) : 0;
    if (n_helpers > 0) {
        smp_ensure_pool(eng, n_helpers);
        if (n_helpers > eng->pool_size) {
            n_helpers = eng->pool_size;
        }
    }

    if (n_helpers > 0) {
        pthread_mutex_lock(&eng->smp_mu);
        for (int t = 0; t < n_helpers; t++) {
            search_worker *hw = eng->helpers[t];
            for (int c = 0; c < 2; c++) {
                for (int s = 0; s < 64; s++) {
                    for (int e = 0; e < 64; e++) {
                        for (int tb = 0; tb < 4; tb++) {
                            hw->history[c][s][e][tb] = (int16_t) (hw->history[c][s][e][tb] / 2);
                        }
                    }
                }
            }
            memset(hw->killers, 0, sizeof(hw->killers));
            memset(hw->countermove, 0, sizeof(hw->countermove));
            hw->nodes = 0ull;
            hw->local_abort = false;
            hw->root_board = mw->root_board;
            hw->root_count = count;
            hw->root_half_move = mw->root_half_move;
            memcpy(hw->root_moves, mw->root_moves, (size_t) count * sizeof(u64));
            memcpy(hw->path, mw->path, (size_t) (path_len + 1) * sizeof(u64));
            hw->base_path_len = path_len;
            hw->max_depth = max_depth;
            hw->time_limited = mw->time_limited;
            hw->deadline_time = mw->deadline_time;
        }
        eng->target_helpers = n_helpers;
        eng->active_helpers = n_helpers;
        eng->smp_epoch++;
        pthread_cond_broadcast(&eng->smp_cv);
        pthread_mutex_unlock(&eng->smp_mu);
    }

    u64 completed_best = mw->root_moves[0];
    int completed_score = 0;
    int completed_depth = 0;
    int stable_depths = 0;
    int prev_iter_score = 0;
    double soft_deadline = start_time + ((double) soft_ms / 1000.0);

    for (int d = 1; d <= max_depth; d++) {
        mw->cur_iter_depth = d;
        bool prev_limit = mw->time_limited;
        if (d == 1) {
            mw->time_limited = false;
        }

        int scores[MAX_LEGAL_MOVES];
        int alpha = -INF_SCORE;
        int beta = INF_SCORE;
        int delta = g_sp.asp_delta;
        if (d >= 4 && abs(completed_score) < MATE_BOUND) {
            alpha = completed_score - delta;
            beta = completed_score + delta;
        }

        u64 iter_best = completed_best;
        int iter_score = 0;

        for (;;) {
            u16 cur_tag = pack_tt_move(iter_best);
            for (int i = 0; i < count; i++) {
                scores[i] = score_move(mw, &mw->root_board, root_side, mw->root_moves[i], cur_tag,
                    0u, -1, 0, -1, 0, 0);
            }
            iter_score = root_search(mw, root_hash, d, alpha, beta, &iter_best, scores);
            if (mw->local_abort) {
                break;
            }
            if (iter_score <= alpha) {
                alpha = (delta > 500) ? -INF_SCORE : (alpha - delta);
                delta *= 2;
            } else if (iter_score >= beta) {
                beta = (delta > 500) ? INF_SCORE : (beta + delta);
                delta *= 2;
            } else {
                break;
            }
        }

        mw->time_limited = prev_limit;
        if (mw->local_abort) {
            break;
        }

        if (d > 1 && iter_best == completed_best) {
            stable_depths++;
        } else {
            stable_depths = 0;
        }
        prev_iter_score = completed_score;
        completed_best = iter_best;
        completed_score = iter_score;
        completed_depth = d;
        atomic_store_explicit(&eng->shared_depth, d, memory_order_relaxed);

        tt_store(eng, root_hash, pack_tt_move(completed_best), completed_score, TT_EVAL_NONE, d,
            TT_EXACT);

        if (eng->info_cb) {
            u64 pv[32];
            int pv_len = engine_extract_pv(eng, game, completed_best, pv, 16);
            u64 total_nodes = mw->nodes;
            for (int t = 0; t < n_helpers; t++) {
                total_nodes += eng->helpers[t]->nodes;
            }
            eng->info_cb(d, completed_score, total_nodes, wall_time_sec() - start_time, pv, pv_len,
                eng->info_user_data);
        }

        if (completed_score > MATE_BOUND || completed_score < -MATE_BOUND) {
            break;
        }
        if (count == 1 && mw->time_limited && d >= 4) {
            break;
        }
        if (mw->time_limited) {
            double now_t = wall_time_sec();
            if (now_t >= mw->deadline_time) {
                break;
            }
            if (soft_ms > 0 && soft_ms < hard_ms && d >= 6) {
                double factor = 1.0;
                if (stable_depths >= 4 && abs(completed_score - prev_iter_score) <= 20) {
                    factor = 0.65;
                } else if (stable_depths == 0 || (prev_iter_score - completed_score) >= 35) {
                    factor = 1.50;
                }
                double eff_soft = start_time + (soft_deadline - start_time) * factor;
                if (now_t >= eff_soft) {
                    break;
                }
            }
        }
    }

    atomic_store_explicit(&eng->stop_flag, true, memory_order_relaxed);
    u64 total_nodes = mw->nodes;
    if (n_helpers > 0) {
        pthread_mutex_lock(&eng->smp_mu);
        while (eng->active_helpers > 0) {
            pthread_cond_wait(&eng->smp_done_cv, &eng->smp_mu);
        }
        for (int t = 0; t < n_helpers; t++) {
            total_nodes += eng->helpers[t]->nodes;
        }
        pthread_mutex_unlock(&eng->smp_mu);
    }

    res.best_move = completed_best;
    res.score_cp = completed_score;
    res.depth_reached = completed_depth;
    res.nodes = total_nodes;
    res.elapsed_sec = wall_time_sec() - start_time;
    return res;
}

search_result engine_search(engine *eng, const full_chess *game, int max_depth, int time_ms) {
    return engine_search_timed(eng, game, max_depth, time_ms, time_ms);
}

int engine_search_multipv(engine *eng, const full_chess *game, int max_depth, int time_ms,
    int num_pvs, multipv_line *out_lines, u64 *total_nodes_out, double *elapsed_out) {
    if (total_nodes_out) {
        *total_nodes_out = 0;
    }
    if (elapsed_out) {
        *elapsed_out = 0.0;
    }
    if (!eng || !game || !out_lines || num_pvs <= 0) {
        return 0;
    }

    int count = num_legal_moves(game);
    if (count == 0) {
        return 0;
    }
    if (num_pvs > MAX_MULTIPV) {
        num_pvs = MAX_MULTIPV;
    }
    if (num_pvs > count) {
        num_pvs = count;
    }

    if (num_pvs == 1) {
        search_result sr = engine_search(eng, game, max_depth, time_ms);
        out_lines[0].best_move = sr.best_move;
        out_lines[0].score_cp = sr.score_cp;
        out_lines[0].depth_reached = sr.depth_reached;
        out_lines[0].pv_len = engine_extract_pv(eng, game, sr.best_move, out_lines[0].pv, 16);
        if (total_nodes_out) {
            *total_nodes_out = sr.nodes;
        }
        if (elapsed_out) {
            *elapsed_out = sr.elapsed_sec;
        }
        if (eng->multipv_cb) {
            eng->multipv_cb(sr.depth_reached, 1, sr.score_cp, sr.nodes, sr.elapsed_sec,
                out_lines[0].pv, out_lines[0].pv_len, eng->multipv_user_data);
        }
        return 1;
    }

    if (max_depth < 1) {
        max_depth = 1;
    }
    if (max_depth > MAX_PLY - 4) {
        max_depth = MAX_PLY - 4;
    }

    eng->tt_gen = (u8) ((eng->tt_gen + 1u) & 63u);
    atomic_store_explicit(&eng->stop_flag, false, memory_order_relaxed);
    atomic_store_explicit(&eng->shared_depth, 0, memory_order_relaxed);

    search_worker *mw = eng->main_worker;
    for (int c = 0; c < 2; c++) {
        for (int s = 0; s < 64; s++) {
            for (int e = 0; e < 64; e++) {
                for (int tb = 0; tb < 4; tb++) {
                    mw->history[c][s][e][tb] = (int16_t) (mw->history[c][s][e][tb] / 2);
                }
            }
        }
    }
    memset(mw->killers, 0, sizeof(mw->killers));
    memset(mw->countermove, 0, sizeof(mw->countermove));

    int path_len = 0;
    int hist_start = (game->move_count > 120) ? (game->move_count - 120) : 0;
    for (int i = hist_start; i < game->move_count; i++) {
        mw->path[path_len++] = engine_hash_board(&game->game_history[i]);
    }
    mw->base_path_len = path_len;
    mw->root_board = *(game->game);
    mw->root_count = count;
    mw->root_half_move = (int) game->half_move_count;
    for (int i = 0; i < count; i++) {
        mw->root_moves[i] = game->current_legal_moves[i];
    }

    double start_time = wall_time_sec();
    mw->nodes = 0ull;
    mw->max_depth = max_depth;
    mw->time_limited = (time_ms > 0);
    mw->deadline_time = start_time + ((double) time_ms / 1000.0);
    mw->local_abort = false;

    u64 root_hash = engine_hash_board(&mw->root_board);
    mw->path[path_len] = root_hash;
    int root_side = is_flipped(&mw->root_board) ? 1 : 0;

    multipv_line completed[MAX_MULTIPV];
    memset(completed, 0, sizeof(completed));
    int completed_n = 0;

    for (int d = 1; d <= max_depth; d++) {
        mw->cur_iter_depth = d;
        bool prev_limit = mw->time_limited;
        if (d == 1) {
            mw->time_limited = false;
        }

        u64 excluded[MAX_MULTIPV];
        multipv_line iter_lines[MAX_MULTIPV];
        int n_done = 0;

        for (int k = 0; k < num_pvs; k++) {
            int scores[MAX_LEGAL_MOVES];
            u16 hint_tag = (k < completed_n) ? pack_tt_move(completed[k].best_move) : 0u;
            for (int i = 0; i < count; i++) {
                scores[i] = score_move(mw, &mw->root_board, root_side, mw->root_moves[i], hint_tag,
                    0u, -1, 0, -1, 0, 0);
            }

            u64 iter_best = NULL_MOVE;
            int sc = root_search_excluded(
                mw, root_hash, d, -INF_SCORE, INF_SCORE, &iter_best, scores, excluded, n_done);
            if (mw->local_abort || iter_best == NULL_MOVE) {
                break;
            }

            excluded[n_done] = iter_best;
            iter_lines[n_done].best_move = iter_best;
            iter_lines[n_done].score_cp = sc;
            iter_lines[n_done].depth_reached = d;
            iter_lines[n_done].pv_len
                = engine_extract_pv(eng, game, iter_best, iter_lines[n_done].pv, 16);
            n_done++;
        }

        mw->time_limited = prev_limit;
        if (mw->local_abort || n_done < num_pvs) {
            break;
        }

        // Sort candidate lines descending by score
        for (int i = 0; i < n_done; i++) {
            for (int j = i + 1; j < n_done; j++) {
                if (iter_lines[j].score_cp > iter_lines[i].score_cp) {
                    multipv_line tmp = iter_lines[i];
                    iter_lines[i] = iter_lines[j];
                    iter_lines[j] = tmp;
                }
            }
        }

        for (int k = 0; k < n_done; k++) {
            completed[k] = iter_lines[k];
        }
        completed_n = n_done;
        tt_store(eng, root_hash, pack_tt_move(completed[0].best_move), completed[0].score_cp,
            TT_EVAL_NONE, d, TT_EXACT);

        if (eng->multipv_cb) {
            double el = wall_time_sec() - start_time;
            for (int k = 0; k < completed_n; k++) {
                eng->multipv_cb(d, k + 1, completed[k].score_cp, mw->nodes, el, completed[k].pv,
                    completed[k].pv_len, eng->multipv_user_data);
            }
        }

        if (mw->time_limited && wall_time_sec() >= mw->deadline_time) {
            break;
        }
    }

    for (int k = 0; k < completed_n; k++) {
        out_lines[k] = completed[k];
    }
    if (total_nodes_out) {
        *total_nodes_out = mw->nodes;
    }
    if (elapsed_out) {
        *elapsed_out = wall_time_sec() - start_time;
    }
    return completed_n;
}

search_result engine_search_level(engine *eng, const full_chess *game, int level) {
    const difficulty_preset *p = engine_get_preset(level);
    int count = num_legal_moves(game);

    if (count > 1 && p->blunder_pct > 0
        && ((int) (engine_splitmix64(&eng->rng_state) % 100ull)) < p->blunder_pct) {
        search_result res;
        memset(&res, 0, sizeof(res));
        res.best_move
            = game->current_legal_moves[(int) (engine_splitmix64(&eng->rng_state) % (u64) count)];
        res.depth_reached = 1;
        res.nodes = 1;
        chess next = *(game->game);
        chess_make_move(&next, res.best_move);
        res.score_cp = -eval_board(eng->main_worker, &next);
        return res;
    }

    if (eng->use_book && level >= 2 && count > 1) {
        const char *bpath = eng->book_path[0] ? eng->book_path : NULL;
        u64 bmove = book_probe(game, bpath, &eng->rng_state);
        if (bmove != NULL_MOVE) {
            search_result res;
            memset(&res, 0, sizeof(res));
            res.best_move = bmove;
            res.depth_reached = 0;
            res.nodes = 1;
            chess next = *(game->game);
            chess_make_move(&next, bmove);
            res.score_cp = -eval_board(eng->main_worker, &next);
            res.elapsed_sec = 0.001;
            return res;
        }
        // Out-of-book opening variety (first 8 plies): randomly pick among top moves within 18 cp
        // so bot-vs-bot games don't repeat identically even from custom FENs or sidelines.
        if (game->move_count < 8 && level >= 3) {
            multipv_line lines[3];
            u64 tot_n = 0;
            double el = 0.0;
            int got = engine_search_multipv(
                eng, game, p->max_depth, p->time_ms, 3, lines, &tot_n, &el);
            if (got > 0) {
                int best_sc = lines[0].score_cp;
                int viable = 1;
                while (viable < got && (best_sc - lines[viable].score_cp) <= 18
                       && lines[viable].score_cp > -MATE_BOUND) {
                    viable++;
                }
                int pick = (viable > 1)
                               ? (int) (engine_splitmix64(&eng->rng_state) % (u64) viable)
                               : 0;
                search_result res;
                memset(&res, 0, sizeof(res));
                res.best_move = lines[pick].best_move;
                res.score_cp = lines[pick].score_cp;
                res.depth_reached = lines[pick].depth_reached;
                res.nodes = tot_n;
                res.elapsed_sec = el;
                return res;
            }
        }
    }

    return engine_search(eng, game, p->max_depth, p->time_ms);
}

search_result engine_search_elo(engine *eng, const full_chess *game, int target_elo) {
    if (target_elo < 800) {
        target_elo = 800;
    } else if (target_elo > 2600) {
        target_elo = 2600;
    }

    int count = num_legal_moves(game);
    if (count <= 0) {
        search_result empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }

    // Use opening book naturally from 1100+ Elo onward for varied openings
    if (eng->use_book && target_elo >= 1100 && count > 1) {
        const char *bpath = eng->book_path[0] ? eng->book_path : NULL;
        u64 bmove = book_probe(game, bpath, &eng->rng_state);
        if (bmove != NULL_MOVE) {
            search_result res;
            memset(&res, 0, sizeof(res));
            res.best_move = bmove;
            res.depth_reached = 0;
            res.nodes = 1;
            chess next = *(game->game);
            chess_make_move(&next, bmove);
            res.score_cp = -eval_board(eng->main_worker, &next);
            res.elapsed_sec = 0.001;
            return res;
        }
    }

    // Beginner tier (800..1150): shallow search + controlled blunder chance
    if (target_elo < 1150) {
        int blunder_pct = 30 - ((target_elo - 800) * 24) / 350;
        if (count > 1 && blunder_pct > 0
            && ((int) (engine_splitmix64(&eng->rng_state) % 100ull)) < blunder_pct) {
            search_result res;
            memset(&res, 0, sizeof(res));
            res.best_move = game->current_legal_moves[(
                int) (engine_splitmix64(&eng->rng_state) % (u64) count)];
            res.depth_reached = 1;
            res.nodes = 1;
            chess next = *(game->game);
            chess_make_move(&next, res.best_move);
            res.score_cp = -eval_board(eng->main_worker, &next);
            return res;
        }
        int d = (target_elo < 1000) ? 1 : 2;
        return engine_search(eng, game, d, 80);
    }

    // Club / Intermediate tier (1150..1850): Multi-PV human-like inaccuracy sampling
    if (target_elo < 1850) {
        int depth = 2 + ((target_elo - 1150) * 4) / 700; // 2..5
        int time_ms = 100 + ((target_elo - 1150) * 200) / 700;
        int k_lines = (target_elo < 1550) ? 3 : 2;
        int max_gap = 180 - ((target_elo - 1150) * 145) / 700; // 180cp -> 35cp

        multipv_line lines[MAX_MULTIPV];
        u64 nodes = 0;
        double elapsed = 0.0;
        int n = engine_search_multipv(eng, game, depth, time_ms, k_lines, lines, &nodes, &elapsed);
        if (n <= 0) {
            return engine_search(eng, game, depth, time_ms);
        }
        int pick = 0;
        if (n > 1 && abs(lines[0].score_cp) < MATE_BOUND) {
            int r = (int) (engine_splitmix64(&eng->rng_state) % 100ull);
            int sub_chance = 42 - ((target_elo - 1150) * 30) / 700; // 42% -> 12%
            if (r < sub_chance) {
                int cand = (n >= 3 && (r < sub_chance / 3)) ? 2 : 1;
                if (lines[0].score_cp - lines[cand].score_cp <= max_gap) {
                    pick = cand;
                }
            }
        }
        search_result res;
        memset(&res, 0, sizeof(res));
        res.best_move = lines[pick].best_move;
        res.score_cp = lines[pick].score_cp;
        res.depth_reached = lines[pick].depth_reached;
        res.nodes = nodes;
        res.elapsed_sec = elapsed;
        return res;
    }

    // Expert / Master / Grandmaster tier (1850..2600): smooth depth + time scaling
    int depth = 6 + ((target_elo - 1850) * 14) / 750; // 6..20
    int time_ms = 350 + ((target_elo - 1850) * 2650) / 750; // 350ms..3000ms
    return engine_search(eng, game, depth, time_ms);
}

// =========================================================================
// Score Formatting & PV Extraction
// =========================================================================

int engine_score_to_white(const chess *b, int stm_score) {
    return is_flipped(b) ? -stm_score : stm_score;
}

void engine_format_eval(int white_cp, char *buf, size_t buf_size) {
    if (white_cp >= MATE_SCORE) {
        snprintf(buf, buf_size, " 1-0");
    } else if (white_cp <= -MATE_SCORE) {
        snprintf(buf, buf_size, " 0-1");
    } else if (white_cp > MATE_BOUND) {
        int plies = MATE_SCORE - white_cp;
        int moves = (plies + 1) / 2;
        if (moves < 1) {
            moves = 1;
        }
        snprintf(buf, buf_size, "+M%d", moves);
    } else if (white_cp < -MATE_BOUND) {
        int plies = MATE_SCORE + white_cp;
        int moves = (plies + 1) / 2;
        if (moves < 1) {
            moves = 1;
        }
        snprintf(buf, buf_size, "-M%d", moves);
    } else {
        double pawns = (double) white_cp / 100.0;
        if (fabs(pawns) < 0.05) {
            snprintf(buf, buf_size, " 0.0");
        } else {
            snprintf(buf, buf_size, "%+.1f", pawns);
        }
    }
}

double engine_win_prob(int white_cp) {
    if (white_cp > MATE_BOUND) {
        return 1.0;
    }
    if (white_cp < -MATE_BOUND) {
        return 0.0;
    }
    return 1.0 / (1.0 + exp(-0.00368208 * (double) white_cp));
}

int engine_extract_pv(
    engine *eng, const full_chess *game, u64 first_move, u64 *pv_out, int max_len) {
    if (!eng || !game || !pv_out || max_len <= 0 || first_move == NULL_MOVE) {
        return 0;
    }
    chess b = *(game->game);
    u64 seen[32];
    int len = 0;
    u64 cur = first_move;
    u64 h = engine_hash_board(&b);

    while (len < max_len && len < 32 && cur != NULL_MOVE) {
        u64 legal[MAX_LEGAL_MOVES];
        int n = set_legal_moves(&b, legal);
        if (!in_set(legal, cur)) {
            break;
        }
        bool loop = false;
        for (int i = 0; i < len; i++) {
            if (seen[i] == h) {
                loop = true;
                break;
            }
        }
        if (loop) {
            break;
        }
        seen[len] = h;
        pv_out[len++] = cur;
        h = engine_update_hash(h, &b, cur);
        chess_make_move(&b, cur);

        u16 tag = 0u;
        int sc = 0, dp = 0;
        u8 fl = 0;
        cur = NULL_MOVE;
        if (tt_probe(eng, h, &tag, &sc, NULL, &dp, &fl) && tag != 0u) {
            n = set_legal_moves(&b, legal);
            for (int i = 0; i < n; i++) {
                if (pack_tt_move(legal[i]) == tag) {
                    cur = legal[i];
                    break;
                }
            }
        }
    }
    return len;
}
