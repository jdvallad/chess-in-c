#ifndef ENGINE_NNUE_H
#define ENGINE_NNUE_H

#include "core/board.h"
#include "core/primitives.h"

#include <stdbool.h>
#include <stdint.h>

#define NNUE_VERSION      3u
#define NNUE_KING_BUCKETS 4
#define NNUE_BASE_INPUTS  768
#define NNUE_INPUTS       (NNUE_KING_BUCKETS * NNUE_BASE_INPUTS)
#define NNUE_HIDDEN       256
#define NNUE_BUCKETS      8
#define NNUE_QA           255
#define NNUE_QB           64
#define NNUE_SCALE        400
#define NNUE_FILE         "data/chess_net.nnue"

typedef enum nnue_mode {
    NNUE_OFF = 0, // Pure classical Texel-tuned evaluation
    NNUE_HYBRID = 1, // Blended NNUE + Classical evaluation
    NNUE_PURE = 2, // Pure quantized NNUE evaluation
} nnue_mode;

// Spatial king bucket (0..3) with horizontal king mirroring (king always on files e..h):
//   Bucket 0: Uncastled / central back-rank king (e1/d1, e2/d2)
//   Bucket 1: Castled wing king (f1/g1/h1, c1/b1/a1, ranks 1-2)
//   Bucket 2: Mid-board king (ranks 3-4)
//   Bucket 3: Active endgame king (ranks 5-8)
static inline int nnue_king_bucket(int persp, int king_wsq, int *hm_out) {
    int ksq = persp ? (king_wsq ^ 56) : king_wsq;
    int hm = ((ksq & 7) < 4) ? 7 : 0;
    if (hm_out) {
        *hm_out = hm;
    }
    int m_ksq = ksq ^ hm;
    int r = m_ksq >> 3;
    int f = m_ksq & 7; // guaranteed 4..7
    if (r <= 1) {
        return (f == 4) ? 0 : 1;
    }
    return (r <= 3) ? 2 : 3;
}

static inline int nnue_feat_idx(int persp, int kb, int hm, int pc_color, int pt, int wsq) {
    int rel_color = (pc_color == persp) ? 0 : 1;
    int sq = (persp ? (wsq ^ 56) : wsq) ^ hm;
    return kb * NNUE_BASE_INPUTS + (rel_color * 6 + pt) * 64 + sq;
}

// Per-worker Finny Table cache entry for fast king-bucket / mirror transitions.
typedef struct nnue_finny_entry {
    int16_t v[NNUE_HIDDEN];
    u64 by_color_pt[2][6]; // canonical White-at-bottom bitboards [color(0=W,1=B)][pt(0..5)]
    uint32_t gen;
    uint8_t initialized;
} nnue_finny_entry;

// Canonical two-perspective accumulator (supports zero-copy lazy deferred updates):
//   v[0] = White's perspective (White pieces = 0..5, Black = 6..11, White-at-bottom squares)
//   v[1] = Black's perspective (Black pieces = 0..5, White = 6..11, vertically flipped squares)
typedef struct nnue_acc {
    int16_t v[2][NNUE_HIDDEN];
    u64 move;
    uint8_t ksq[2]; // White-at-bottom king squares: ksq[0]=White king, ksq[1]=Black king
    uint8_t before_flipped;
    uint8_t valid;
    uint8_t dirty;
} nnue_acc;

static inline void nnue_acc_mark_lazy(nnue_acc *dst, const chess *before, u64 m) {
    dst->move = m;
    dst->before_flipped = is_flipped(before) ? 1u : 0u;
    dst->valid = 0u;
    dst->dirty = 1u;
}

static inline void nnue_acc_mark_null(nnue_acc *dst) {
    dst->move = NULL_MOVE;
    dst->valid = 0u;
    dst->dirty = 1u;
}

// Load default network (data/chess_net.nnue if present). Safe to call multiple times.
void nnue_init(void);

// Load a quantized .nnue weight file from disk. Returns true on success.
bool nnue_load_file(const char *path);

// Check whether valid NNUE weights are loaded.
bool nnue_is_loaded(void);

// Compute accumulator from scratch for `b` (used at root & fallback).
void nnue_acc_refresh(nnue_acc *acc, const chess *b);

// Incrementally derive `dst` accumulator from `src` across move `m` played on `before`.
void nnue_acc_make_move(nnue_acc *dst, const nnue_acc *src, const chess *before, u64 m);

// Materialize lazy accumulator at `stack[ply]` using Finny cache when a king crosses buckets.
void nnue_acc_materialize(
    nnue_acc *stack, nnue_finny_entry finny[2][NNUE_KING_BUCKETS][2], int ply, const chess *b);

// Evaluate `b` (side-to-move perspective) in centipawns using precomputed `acc`.
int nnue_eval_acc(const nnue_acc *acc, const chess *b);

// Evaluate `b` (side-to-move perspective) from scratch (convenience / tests).
int nnue_evaluate(const chess *b);

// Global NNUE evaluation mode (toggleable via UCI UseNNUE / NNUEMode).
void nnue_set_mode(nnue_mode mode);
nnue_mode nnue_get_mode(void);

#endif
