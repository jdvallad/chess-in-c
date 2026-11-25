#ifndef ENGINE_INTERNAL_H
#define ENGINE_INTERNAL_H

#include "core/board.h"
#include "core/primitives.h"
#include "engine/engine.h"
#include "engine/nnue.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_PLY      64
#define INF_SCORE    1000000
#define TT_EXACT     0
#define TT_LOWER     1
#define TT_UPPER     2
#define TT_EVAL_NONE (-32000)
#define TT_MATE_BASE 30000
#define TT_MATE_LIM  29000
#define TT_WAYS      4
#define PAWN_TT_BITS 14
#define PAWN_TT_SIZE (1u << PAWN_TT_BITS)
#define PAWN_TT_MASK (PAWN_TT_SIZE - 1u)
#define CORR_SIZE    16384u
#define CORR_MASK    (CORR_SIZE - 1u)
#define MAX_THREADS  16
#define HIST_MAX     16384

// Lock-free 16-byte XOR-verified transposition table entry.
typedef struct tt_entry {
    _Atomic u64 key_xor;
    _Atomic u64 data;
} tt_entry;

// 4-way set-associative TT bucket (64 bytes = exactly 1 CPU cache line).
typedef struct tt_bucket {
    tt_entry e[TT_WAYS];
} tt_bucket;

// Per-worker Pawn Hash Table entry caching pawn structure AND passed-pawn bitboards.
typedef struct pawn_tt_entry {
    u64 f_pawns;
    u64 e_pawns;
    u64 f_passed;
    u64 e_passed;
    int16_t mg;
    int16_t eg;
} pawn_tt_entry;

typedef struct search_worker {
    engine *eng;
    int thread_id;
    u64 killers[MAX_PLY][2];
    int16_t history[2][64][64][4]; // [side(0=W,1=B)][from][to][threat_bucket(0..3)]
    int16_t cap_history[6][64][6]; // [attacker_pt][to][victim_pt]
    int16_t cont_history[6][64][6][64]; // 1-ply opponent response history
    int16_t followup_history[6][64][6][64]; // 2-ply own follow-up history
    int16_t cont4_history[6][64][6][64]; // 4-ply own continuation history
    int16_t low_ply_hist[4][64][64]; // low-ply (plies 0..3) quiet move ordering history
    int16_t corr_hist[2][CORR_SIZE]; // pawn-structure correction history
    int16_t nonpawn_corr[2][2]
                        [CORR_SIZE]; // [stm][color(0=W,1=B)] non-pawn piece correction history
    int16_t minor_corr[2][CORR_SIZE]; // minor-piece (K+N+B) correction history
    int16_t major_corr[2][CORR_SIZE]; // major-piece (K+R+Q) correction history
    int16_t cont_corr[2][6][64]; // 1-ply continuation correction history [stm][prev_pt][prev_to]
    u16 countermove[64][64];
    int eval_stack[MAX_PLY];
    u64 move_stack[MAX_PLY];
    uint8_t double_exts[MAX_PLY + 4];
    nnue_acc acc_stack[MAX_PLY + 4];
    nnue_finny_entry finny[2][NNUE_MAX_KING_BUCKETS][2];
    pawn_tt_entry pawn_tt[PAWN_TT_SIZE];
    u64 nodes;
    u64 node_limit; // 0 = unlimited; >0 = stop once worker nodes >= node_limit
    chess root_board;
    u64 root_moves[MAX_LEGAL_MOVES];
    int root_count;
    int root_half_move;
    u64 path[MAX_GAME_LENGTH + MAX_PLY];
    int base_path_len;
    int max_depth;
    int cur_iter_depth;
    bool time_limited;
    double deadline_time;
    bool local_abort;
} search_worker;

struct engine {
    tt_bucket *tt;
    size_t tt_mask;
    size_t tt_count;
    search_worker *main_worker;
    u64 rng_state;
    int num_threads;
    bool use_book;
    bool use_syzygy;
    char book_path[256];
    bool limit_strength;
    int target_elo;
    engine_info_cb info_cb;
    void *info_user_data;
    engine_multipv_cb multipv_cb;
    void *multipv_user_data;
    _Atomic bool stop_flag;
    _Atomic int shared_depth;
    u8 tt_gen;

    // Persistent Lazy SMP helper thread pool (avoids per-move spawn & keeps warm histories!)
    pthread_mutex_t smp_mu;
    pthread_cond_t smp_cv;
    pthread_cond_t smp_done_cv;
    pthread_t helper_threads[MAX_THREADS];
    search_worker *helpers[MAX_THREADS];
    int pool_size; // number of live helper threads (0..MAX_THREADS-1)
    int active_helpers; // currently searching helpers in this epoch
    int target_helpers; // how many helpers should participate in this epoch
    u64 smp_epoch;
    bool smp_shutdown;
};

// Shared piece values, file masks, and fast piece-type lookup table (defined in eval.c)
extern const int g_mg_value[6];
extern const int g_eg_value[6];
extern const int g_see_value[6];
extern const u64 g_file_masks[8];
extern const u64 g_adj_file_masks[8];
extern const int8_t g_pt_lookup[64];

// Thread-safe one-time engine initialization (attacks + Zobrist + LMR tables)
void engine_global_init(void);

static inline u64 engine_splitmix64(u64 *state) {
    u64 z = (*state += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static inline int piece_type_idx(u8 pc) {
    return (int) g_pt_lookup[pc & 63u];
}

// Compact 16-bit move tag for TT / countermove matching (unique among legal moves from a board).
static inline u16 pack_tt_move(u64 m) {
    if (m == NULL_MOVE) {
        return 0u;
    }
    int s = get_start_offset(m);
    int e = get_end_offset(m);
    int pt = piece_type_idx(get_promotion_piece(m));
    if (pt < 0) {
        pt = 0;
    }
    return (u16) (0x8000u | (unsigned) (s & 63) | ((unsigned) (e & 63) << 6)
                  | ((unsigned) (pt & 7) << 12));
}

#endif
