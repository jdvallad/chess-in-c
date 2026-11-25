#ifndef ENGINE_NNUE_H
#define ENGINE_NNUE_H

#include "core/board.h"
#include "core/primitives.h"

#include <stdbool.h>
#include <stdint.h>

// Version 3 (Titan-C v4 & Singularity v5): (4x768) -> 256x2 -> 8 SCReLU
#define NNUE_VERSION          3u
#define NNUE_VERSION_V4       4u
#define NNUE_KING_BUCKETS     4
#define NNUE_V4_KING_BUCKETS  16
#define NNUE_MAX_KING_BUCKETS 16
#define NNUE_BASE_INPUTS      768
#define NNUE_INPUTS           (NNUE_KING_BUCKETS * NNUE_BASE_INPUTS)
#define NNUE_MAX_INPUTS       (NNUE_MAX_KING_BUCKETS * NNUE_BASE_INPUTS)
#define NNUE_HIDDEN           256
#define NNUE_MAX_HIDDEN       1024
#define NNUE_BUCKETS          8
#define NNUE_QA               255
#define NNUE_QB               64
#define NNUE_SCALE            400
#define NNUE_FILE             "data/chess_net.nnue"
#define NNUE_V4_FILE          "data/titan_v6_1024.nnue"

typedef enum nnue_mode {
    NNUE_OFF = 0, // Pure classical Texel-tuned evaluation
    NNUE_HYBRID = 1, // Blended NNUE + Classical evaluation
    NNUE_PURE = 2, // Pure quantized NNUE evaluation
} nnue_mode;

// Active loaded network metadata (3 = 4x768->256, 4 = 16x768->512/1024)
uint32_t nnue_get_loaded_version(void);
int nnue_get_loaded_hidden(void);
int nnue_get_loaded_king_buckets(void);

// Spatial king bucket with horizontal king mirroring (king always on files e..h):
//   v3 (4 buckets):
//     0: uncastled back-rank (e1/d1/e2/d2), 1: castled wing (r<=1), 2: mid-board (r=2..3), 3: endgame (r>=4)
//   v4 (16 buckets — Stockfish/Viridithas-style fine-grained king map):
//     0..3:  rank 1 files e/f/g/h
//     4..7:  rank 2 files e/f/g/h
//     8..11: ranks 3-4 split into (e/f vs g/h) x (r=2 vs r=3)
//     12..15: ranks 5-8 split into (e/f vs g/h) x (r=4..5 vs r=6..7)
static inline int nnue_king_bucket_for(int num_kb, int persp, int king_wsq, int *hm_out) {
    int ksq = persp ? (king_wsq ^ 56) : king_wsq;
    int hm = ((ksq & 7) < 4) ? 7 : 0;
    if (hm_out) {
        *hm_out = hm;
    }
    int m_ksq = ksq ^ hm;
    int r = m_ksq >> 3;
    int f = m_ksq & 7; // guaranteed 4..7
    if (num_kb >= 16) {
        if (r == 0) {
            return f - 4; // 0..3
        }
        if (r == 1) {
            return 4 + (f - 4); // 4..7
        }
        if (r <= 3) {
            return 8 + ((r - 2) * 2) + ((f >= 6) ? 1 : 0); // 8..11
        }
        return 12 + ((r >= 6) ? 2 : 0) + ((f >= 6) ? 1 : 0); // 12..15
    }
    if (r <= 1) {
        return (f == 4) ? 0 : 1;
    }
    return (r <= 3) ? 2 : 3;
}

static inline int nnue_king_bucket(int persp, int king_wsq, int *hm_out) {
    return nnue_king_bucket_for(nnue_get_loaded_king_buckets(), persp, king_wsq, hm_out);
}

static inline int nnue_feat_idx(int persp, int kb, int hm, int pc_color, int pt, int wsq) {
    int rel_color = (pc_color == persp) ? 0 : 1;
    int sq = (persp ? (wsq ^ 56) : wsq) ^ hm;
    return kb * NNUE_BASE_INPUTS + (rel_color * 6 + pt) * 64 + sq;
}

// Per-worker Finny Table cache entry for fast king-bucket / mirror transitions.
typedef struct nnue_finny_entry {
    int16_t v[NNUE_MAX_HIDDEN];
    u64 by_color_pt[2][6]; // canonical White-at-bottom bitboards [color(0=W,1=B)][pt(0..5)]
    uint32_t gen;
    uint8_t initialized;
} nnue_finny_entry;

// Canonical two-perspective accumulator (supports zero-copy lazy deferred updates):
typedef struct nnue_acc {
    int16_t v[2][NNUE_MAX_HIDDEN];
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

// Load a quantized .nnue weight file (v3 or v4) from disk. Returns true on success.
bool nnue_load_file(const char *path);

// Check whether valid NNUE weights are loaded.
bool nnue_is_loaded(void);

// Compute accumulator from scratch for `b` (used at root & fallback).
void nnue_acc_refresh(nnue_acc *acc, const chess *b);

// Incrementally derive `dst` accumulator from `src` across move `m` played on `before`.
void nnue_acc_make_move(nnue_acc *dst, const nnue_acc *src, const chess *before, u64 m);

// Materialize lazy accumulator at `stack[ply]` using Finny cache when a king crosses buckets.
void nnue_acc_materialize(
    nnue_acc *stack, nnue_finny_entry finny[2][NNUE_MAX_KING_BUCKETS][2], int ply, const chess *b);

// Evaluate `b` (side-to-move perspective) in centipawns using precomputed `acc`.
int nnue_eval_acc(const nnue_acc *acc, const chess *b);

// Evaluate `b` (side-to-move perspective) from scratch (convenience / tests).
int nnue_evaluate(const chess *b);

// Global NNUE evaluation mode (toggleable via UCI UseNNUE / NNUEMode).
void nnue_set_mode(nnue_mode mode);
nnue_mode nnue_get_mode(void);

#endif
