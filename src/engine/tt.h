#ifndef ENGINE_TT_H
#define ENGINE_TT_H

#include "engine/engine_internal.h"

#include <stdbool.h>
#include <stddef.h>

bool tt_alloc(engine *eng, size_t tt_mb);
void tt_clear(engine *eng);
void tt_free(engine *eng);

static inline void tt_prefetch(const engine *eng, u64 hash) {
    __builtin_prefetch(&eng->tt[hash & eng->tt_mask], 0, 1);
}

static inline int16_t tt_encode_score(int score) {
    if (score > MATE_BOUND) {
        int plies = MATE_SCORE - score;
        if (plies < 0) {
            plies = 0;
        }
        if (plies > 500) {
            plies = 500;
        }
        return (int16_t) (TT_MATE_BASE - plies);
    }
    if (score < -MATE_BOUND) {
        int plies = MATE_SCORE + score;
        if (plies < 0) {
            plies = 0;
        }
        if (plies > 500) {
            plies = 500;
        }
        return (int16_t) (-TT_MATE_BASE + plies);
    }
    if (score > TT_MATE_LIM - 1) {
        score = TT_MATE_LIM - 1;
    } else if (score < -TT_MATE_LIM + 1) {
        score = -TT_MATE_LIM + 1;
    }
    return (int16_t) score;
}

static inline int tt_decode_score(int16_t s) {
    int v = (int) s;
    if (v >= TT_MATE_LIM) {
        return MATE_SCORE - (TT_MATE_BASE - v);
    }
    if (v <= -TT_MATE_LIM) {
        return -MATE_SCORE + (TT_MATE_BASE + v);
    }
    return v;
}

static inline int16_t tt_encode_eval(int eval) {
    if (eval == TT_EVAL_NONE || eval <= -INF_SCORE / 2 || eval >= INF_SCORE / 2) {
        return (int16_t) TT_EVAL_NONE;
    }
    if (eval > 28000) {
        eval = 28000;
    } else if (eval < -28000) {
        eval = -28000;
    }
    return (int16_t) eval;
}

// 64-bit packed TT payload:
// [0..15]  u16 move_tag
// [16..31] int16_t score (mate-mapped)
// [32..47] int16_t static_eval (TT_EVAL_NONE if absent)
// [48..55] u8 depth
// [56..57] u8 flag (TT_EXACT/LOWER/UPPER)
// [58..63] u8 gen (0..63)
static inline u64 tt_pack_data(
    u16 move_tag, int score, int static_eval, int depth, u8 flag, u8 gen) {
    u64 d = (u64) move_tag;
    d |= ((u64) (u16) tt_encode_score(score)) << 16;
    d |= ((u64) (u16) tt_encode_eval(static_eval)) << 32;
    d |= ((u64) (u8) (depth < 0 ? 0 : (depth > 255 ? 255 : depth))) << 48;
    d |= ((u64) (flag & 3u)) << 56;
    d |= ((u64) (gen & 63u)) << 58;
    return d;
}

static inline bool tt_probe(const engine *eng, u64 hash, u16 *move_out, int *score_out,
    int *eval_out, int *depth_out, u8 *flag_out) {
    const tt_bucket *b = &eng->tt[hash & eng->tt_mask];
    for (int s = 0; s < TT_WAYS; s++) {
        u64 data = atomic_load_explicit(&b->e[s].data, memory_order_relaxed);
        u64 key_xor = atomic_load_explicit(&b->e[s].key_xor, memory_order_relaxed);
        if ((key_xor ^ data) == hash) {
            *move_out = (u16) (data & 0xffffull);
            *score_out = tt_decode_score((int16_t) ((data >> 16) & 0xffffull));
            if (eval_out) {
                *eval_out = (int) (int16_t) ((data >> 32) & 0xffffull);
            }
            *depth_out = (int) ((data >> 48) & 0xffull);
            *flag_out = (u8) ((data >> 56) & 3ull);
            return true;
        }
    }
    return false;
}

static inline void tt_store(
    engine *eng, u64 hash, u16 move_tag, int score, int static_eval, int depth, u8 flag) {
    tt_bucket *b = &eng->tt[hash & eng->tt_mask];
    int replace_idx = 0;
    int worst_prio = 1000000;

    for (int s = 0; s < TT_WAYS; s++) {
        u64 d = atomic_load_explicit(&b->e[s].data, memory_order_relaxed);
        u64 x = atomic_load_explicit(&b->e[s].key_xor, memory_order_relaxed);
        if ((x ^ d) == hash) {
            int old_depth = (int) ((d >> 48) & 0xffull);
            u8 old_gen = (u8) ((d >> 58) & 63ull);
            if (move_tag == 0u) {
                move_tag = (u16) (d & 0xffffull);
            }
            if (static_eval == TT_EVAL_NONE) {
                static_eval = (int) (int16_t) ((d >> 32) & 0xffffull);
            }
            // Don't overwrite a deeper exact entry from the same generation with a shallower bound
            if (flag != TT_EXACT && old_gen == eng->tt_gen && depth < old_depth - 2) {
                return;
            }
            u64 nd = tt_pack_data(move_tag, score, static_eval, depth, flag, eng->tt_gen);
            atomic_store_explicit(&b->e[s].data, nd, memory_order_relaxed);
            atomic_store_explicit(&b->e[s].key_xor, hash ^ nd, memory_order_relaxed);
            return;
        }
        if (x == 0ull && d == 0ull) {
            replace_idx = s;
            break;
        }
        int s_depth = (int) ((d >> 48) & 0xffull);
        u8 s_gen = (u8) ((d >> 58) & 63ull);
        int age = (int) ((eng->tt_gen - s_gen) & 63u);
        int prio = s_depth - 8 * age;
        if (prio < worst_prio) {
            worst_prio = prio;
            replace_idx = s;
        }
    }

    u64 nd = tt_pack_data(move_tag, score, static_eval, depth, flag, eng->tt_gen);
    atomic_store_explicit(&b->e[replace_idx].data, nd, memory_order_relaxed);
    atomic_store_explicit(&b->e[replace_idx].key_xor, hash ^ nd, memory_order_relaxed);
}

#endif
