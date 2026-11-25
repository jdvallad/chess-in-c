#include "engine/nnue.h"

#include "core/board.h"
#include "engine/engine_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

// Dual-format Quantized King-Bucketed + Phase-Bucketed SCReLU NNUE:
//   v3: (4x768)  ->  256x2 -> 8  (Titan-C v4 / Singularity v5, ~1.58 MB active)
//   v4: (16x768) -> 1024x2 -> 8  (Project 3000 Apex-C Big NNUE, ~24 MB active)
// Stored in BSS so untouched v4 pages cost 0 physical RAM until a v4 file is loaded!
typedef struct nnue_network {
    _Alignas(32) int16_t ft_weights[NNUE_MAX_INPUTS][NNUE_MAX_HIDDEN];
    _Alignas(32) int16_t ft_bias[NNUE_MAX_HIDDEN];
    _Alignas(32) int16_t out_weights[NNUE_BUCKETS][NNUE_MAX_HIDDEN * 2];
    int32_t out_bias[NNUE_BUCKETS];
} nnue_network;

static nnue_network g_net;
static bool g_nnue_loaded = false;
static uint32_t g_nnue_gen = 1u;
static uint32_t g_nnue_ver = NNUE_VERSION;
static int g_nnue_hidden = NNUE_HIDDEN;
static int g_nnue_kb = NNUE_KING_BUCKETS;
static nnue_mode g_nnue_mode = NNUE_PURE;

typedef struct nnue_header {
    char magic[8]; // "CINCNNUE"
    uint32_t version; // 3 or 4
    uint32_t inputs; // 3072 (v3) or 12288 (v4)
    uint32_t hidden; // 256 (v3) or 512/1024 (v4)
    uint32_t buckets; // 8
} nnue_header;

uint32_t nnue_get_loaded_version(void) {
    return g_nnue_ver;
}

int nnue_get_loaded_hidden(void) {
    return g_nnue_hidden;
}

int nnue_get_loaded_king_buckets(void) {
    return g_nnue_kb;
}

bool nnue_load_file(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return false;
    }
    nnue_header hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1 || memcmp(hdr.magic, "CINCNNUE", 8) != 0
        || hdr.buckets != NNUE_BUCKETS) {
        fclose(fp);
        return false;
    }

    bool valid_v3 = (hdr.version == NNUE_VERSION && hdr.inputs == NNUE_INPUTS
                     && hdr.hidden == NNUE_HIDDEN);
    bool valid_v4 = (hdr.version == NNUE_VERSION_V4
                     && (hdr.inputs == NNUE_INPUTS || hdr.inputs == NNUE_MAX_INPUTS)
                     && hdr.hidden >= 64 && hdr.hidden <= NNUE_MAX_HIDDEN && (hdr.hidden % 16 == 0));
    if (!valid_v3 && !valid_v4) {
        fclose(fp);
        return false;
    }

    size_t n_in = (size_t) hdr.inputs;
    size_t n_hid = (size_t) hdr.hidden;
    bool ok = true;

    for (size_t i = 0; i < n_in && ok; i++) {
        if (fread(g_net.ft_weights[i], sizeof(int16_t), n_hid, fp) != n_hid) {
            ok = false;
        }
    }
    if (ok && fread(g_net.ft_bias, sizeof(int16_t), n_hid, fp) != n_hid) {
        ok = false;
    }
    for (size_t b = 0; b < NNUE_BUCKETS && ok; b++) {
        if (fread(g_net.out_weights[b], sizeof(int16_t), n_hid * 2, fp) != n_hid * 2) {
            ok = false;
        }
    }
    if (ok && fread(g_net.out_bias, sizeof(int32_t), NNUE_BUCKETS, fp) != NNUE_BUCKETS) {
        ok = false;
    }
    fclose(fp);

    if (ok) {
        g_nnue_ver = hdr.version;
        g_nnue_hidden = (int) hdr.hidden;
        g_nnue_kb = (int) (hdr.inputs / NNUE_BASE_INPUTS);
        g_nnue_loaded = true;
        g_nnue_gen++;
    }
    return ok;
}

void nnue_init(void) {
    if (g_nnue_loaded) {
        return;
    }
    if (!nnue_load_file(NNUE_FILE)) {
        (void) nnue_load_file("chess_net.nnue");
    }
}

bool nnue_is_loaded(void) {
    return g_nnue_loaded;
}

void nnue_set_mode(nnue_mode mode) {
    g_nnue_mode = mode;
}

nnue_mode nnue_get_mode(void) {
    return g_nnue_mode;
}

// =========================================================================
// SIMD Vector Helpers (AVX2 fast path + portable scalar fallback)
// =========================================================================

static inline void vec_add(int16_t *restrict dst, const int16_t *restrict row, int n) {
#if defined(__AVX2__)
    for (int i = 0; i < n; i += 16) {
        __m256i a = _mm256_loadu_si256((const __m256i *) (dst + i));
        __m256i w = _mm256_loadu_si256((const __m256i *) (row + i));
        _mm256_storeu_si256((__m256i *) (dst + i), _mm256_add_epi16(a, w));
    }
#else
    for (int i = 0; i < n; i++) {
        dst[i] = (int16_t) (dst[i] + row[i]);
    }
#endif
}

static inline void vec_sub(int16_t *restrict dst, const int16_t *restrict row, int n) {
#if defined(__AVX2__)
    for (int i = 0; i < n; i += 16) {
        __m256i a = _mm256_loadu_si256((const __m256i *) (dst + i));
        __m256i w = _mm256_loadu_si256((const __m256i *) (row + i));
        _mm256_storeu_si256((__m256i *) (dst + i), _mm256_sub_epi16(a, w));
    }
#else
    for (int i = 0; i < n; i++) {
        dst[i] = (int16_t) (dst[i] - row[i]);
    }
#endif
}

static inline void vec_sub_add(int16_t *restrict dst, const int16_t *restrict src,
    const int16_t *restrict sub_row, const int16_t *restrict add_row, int n) {
#if defined(__AVX2__)
    for (int i = 0; i < n; i += 16) {
        __m256i v = _mm256_loadu_si256((const __m256i *) (src + i));
        __m256i s = _mm256_loadu_si256((const __m256i *) (sub_row + i));
        __m256i a = _mm256_loadu_si256((const __m256i *) (add_row + i));
        v = _mm256_sub_epi16(v, s);
        v = _mm256_add_epi16(v, a);
        _mm256_storeu_si256((__m256i *) (dst + i), v);
    }
#else
    for (int i = 0; i < n; i++) {
        dst[i] = (int16_t) (src[i] - sub_row[i] + add_row[i]);
    }
#endif
}

static inline void vec_sub2_add1(int16_t *restrict dst, const int16_t *restrict src,
    const int16_t *restrict sub1, const int16_t *restrict sub2, const int16_t *restrict add1,
    int n) {
#if defined(__AVX2__)
    for (int i = 0; i < n; i += 16) {
        __m256i v = _mm256_loadu_si256((const __m256i *) (src + i));
        __m256i s1 = _mm256_loadu_si256((const __m256i *) (sub1 + i));
        __m256i s2 = _mm256_loadu_si256((const __m256i *) (sub2 + i));
        __m256i a1 = _mm256_loadu_si256((const __m256i *) (add1 + i));
        v = _mm256_sub_epi16(v, s1);
        v = _mm256_sub_epi16(v, s2);
        v = _mm256_add_epi16(v, a1);
        _mm256_storeu_si256((__m256i *) (dst + i), v);
    }
#else
    for (int i = 0; i < n; i++) {
        dst[i] = (int16_t) (src[i] - sub1[i] - sub2[i] + add1[i]);
    }
#endif
}

static inline void vec_sub2_add2(int16_t *restrict dst, const int16_t *restrict src,
    const int16_t *restrict sub1, const int16_t *restrict sub2, const int16_t *restrict add1,
    const int16_t *restrict add2, int n) {
#if defined(__AVX2__)
    for (int i = 0; i < n; i += 16) {
        __m256i v = _mm256_loadu_si256((const __m256i *) (src + i));
        __m256i s1 = _mm256_loadu_si256((const __m256i *) (sub1 + i));
        __m256i s2 = _mm256_loadu_si256((const __m256i *) (sub2 + i));
        __m256i a1 = _mm256_loadu_si256((const __m256i *) (add1 + i));
        __m256i a2 = _mm256_loadu_si256((const __m256i *) (add2 + i));
        v = _mm256_sub_epi16(v, s1);
        v = _mm256_sub_epi16(v, s2);
        v = _mm256_add_epi16(v, a1);
        v = _mm256_add_epi16(v, a2);
        _mm256_storeu_si256((__m256i *) (dst + i), v);
    }
#else
    for (int i = 0; i < n; i++) {
        dst[i] = (int16_t) (src[i] - sub1[i] - sub2[i] + add1[i] + add2[i]);
    }
#endif
}

static inline u64 bswap_ranks(u64 bb) {
    return __builtin_bswap64(bb);
}

static void extract_canonical_bbs(const chess *b, u64 out[2][6]) {
    bool btm = is_flipped(b);
    int fc = btm ? 1 : 0;
    int ec = 1 - fc;

    u64 friendly = b->friendly_pieces;
    u64 enemy = b->enemy_pieces;
    u64 tp = b->pawns & PAWN_MASK;
    u64 orth = b->orthogonal_pieces;
    u64 diag = b->diagonal_pieces;
    u64 kings = b->kings;

    u64 fp = friendly & tp;
    u64 fn = friendly & ~(tp | orth | diag | kings);
    u64 fb = friendly & diag & ~orth;
    u64 fr = friendly & orth & ~diag;
    u64 fq = friendly & diag & orth;
    u64 fk = friendly & kings;

    u64 ep = enemy & tp;
    u64 en = enemy & ~(tp | orth | diag | kings);
    u64 eb = enemy & diag & ~orth;
    u64 er = enemy & orth & ~diag;
    u64 eq = enemy & diag & orth;
    u64 ek = enemy & kings;

    if (btm) {
        out[fc][0] = bswap_ranks(fp);
        out[fc][1] = bswap_ranks(fn);
        out[fc][2] = bswap_ranks(fb);
        out[fc][3] = bswap_ranks(fr);
        out[fc][4] = bswap_ranks(fq);
        out[fc][5] = bswap_ranks(fk);

        out[ec][0] = bswap_ranks(ep);
        out[ec][1] = bswap_ranks(en);
        out[ec][2] = bswap_ranks(eb);
        out[ec][3] = bswap_ranks(er);
        out[ec][4] = bswap_ranks(eq);
        out[ec][5] = bswap_ranks(ek);
    } else {
        out[fc][0] = fp;
        out[fc][1] = fn;
        out[fc][2] = fb;
        out[fc][3] = fr;
        out[fc][4] = fq;
        out[fc][5] = fk;

        out[ec][0] = ep;
        out[ec][1] = en;
        out[ec][2] = eb;
        out[ec][3] = er;
        out[ec][4] = eq;
        out[ec][5] = ek;
    }
}

static void refresh_persp_scratch(int16_t *dst_v, const u64 bbs[2][6], int persp, int king_wsq) {
    int n = g_nnue_hidden;
    int hm = 0;
    int kb = nnue_king_bucket_for(g_nnue_kb, persp, king_wsq, &hm);
    memcpy(dst_v, g_net.ft_bias, sizeof(int16_t) * (size_t) n);
    for (int c = 0; c < 2; c++) {
        for (int pt = 0; pt < 6; pt++) {
            u64 bb = bbs[c][pt];
            while (bb) {
                int wsq = __builtin_ctzll(bb);
                bb &= bb - 1ull;
                vec_add(dst_v, g_net.ft_weights[nnue_feat_idx(persp, kb, hm, c, pt, wsq)], n);
            }
        }
    }
}

static void refresh_persp_finny(int16_t *dst_v,
    nnue_finny_entry finny[2][NNUE_MAX_KING_BUCKETS][2], const u64 bbs[2][6], int persp,
    int king_wsq) {
    if (!finny) {
        refresh_persp_scratch(dst_v, bbs, persp, king_wsq);
        return;
    }
    int n = g_nnue_hidden;
    int hm = 0;
    int kb = nnue_king_bucket_for(g_nnue_kb, persp, king_wsq, &hm);
    nnue_finny_entry *fe = &finny[persp][kb][hm ? 1 : 0];

    if (!fe->initialized || fe->gen != g_nnue_gen) {
        memcpy(fe->v, g_net.ft_bias, sizeof(int16_t) * (size_t) n);
        memset(fe->by_color_pt, 0, sizeof(fe->by_color_pt));
        fe->gen = g_nnue_gen;
        fe->initialized = 1u;
    }

    for (int c = 0; c < 2; c++) {
        for (int pt = 0; pt < 6; pt++) {
            u64 cur = bbs[c][pt];
            u64 prev = fe->by_color_pt[c][pt];
            if (cur == prev) {
                continue;
            }
            u64 added = cur & ~prev;
            u64 removed = prev & ~cur;
            while (added) {
                int wsq = __builtin_ctzll(added);
                added &= added - 1ull;
                vec_add(fe->v, g_net.ft_weights[nnue_feat_idx(persp, kb, hm, c, pt, wsq)], n);
            }
            while (removed) {
                int wsq = __builtin_ctzll(removed);
                removed &= removed - 1ull;
                vec_sub(fe->v, g_net.ft_weights[nnue_feat_idx(persp, kb, hm, c, pt, wsq)], n);
            }
            fe->by_color_pt[c][pt] = cur;
        }
    }
    memcpy(dst_v, fe->v, sizeof(int16_t) * (size_t) n);
}

void nnue_acc_refresh(nnue_acc *acc, const chess *b) {
    if (!g_nnue_loaded) {
        acc->valid = 0u;
        acc->dirty = 0u;
        return;
    }
    u64 bbs[2][6];
    extract_canonical_bbs(b, bbs);
    int w_ksq = bbs[0][5] ? __builtin_ctzll(bbs[0][5]) : 4;
    int b_ksq = bbs[1][5] ? __builtin_ctzll(bbs[1][5]) : 60;
    acc->ksq[0] = (uint8_t) w_ksq;
    acc->ksq[1] = (uint8_t) b_ksq;

    refresh_persp_scratch(acc->v[0], bbs, 0, w_ksq);
    refresh_persp_scratch(acc->v[1], bbs, 1, b_ksq);
    acc->valid = 1u;
    acc->dirty = 0u;
}

static inline int nnue_pt_idx(u8 pc) {
    switch (pc) {
    case FRIENDLY_PAWN:
    case ENEMY_PAWN: return 0;
    case FRIENDLY_KNIGHT:
    case ENEMY_KNIGHT: return 1;
    case FRIENDLY_BISHOP:
    case ENEMY_BISHOP: return 2;
    case FRIENDLY_ROOK:
    case ENEMY_ROOK: return 3;
    case FRIENDLY_QUEEN:
    case ENEMY_QUEEN: return 4;
    case FRIENDLY_KING:
    case ENEMY_KING: return 5;
    default: return 0;
    }
}

static inline void apply_move_persp(
    int16_t *dst_v, const int16_t *src_v, int persp, int kb, int hm, bool btm, u64 m) {
    int n = g_nnue_hidden;
    int flip_mask = btm ? 56 : 0;
    int mover = btm ? 1 : 0;
    int enemy = 1 - mover;

    int from_wsq = get_start_offset(m) ^ flip_mask;
    int to_wsq = get_end_offset(m) ^ flip_mask;
    int sp_pt = nnue_pt_idx(get_start_piece(m));
    int pr_pt = nnue_pt_idx(get_promotion_piece(m));
    u8 cap = get_end_piece(m);

    if (is_castle_king_move(m)) {
        int r_from = 7 ^ flip_mask;
        int r_to = 5 ^ flip_mask;
        vec_sub2_add2(dst_v, src_v,
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, 5, from_wsq)],
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, 3, r_from)],
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, 5, to_wsq)],
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, 3, r_to)], n);
    } else if (is_castle_queen_move(m)) {
        int r_from = 0 ^ flip_mask;
        int r_to = 3 ^ flip_mask;
        vec_sub2_add2(dst_v, src_v,
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, 5, from_wsq)],
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, 3, r_from)],
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, 5, to_wsq)],
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, 3, r_to)], n);
    } else if (is_en_passant_move(m)) {
        int cap_wsq = (get_end_offset(m) - 8) ^ flip_mask;
        vec_sub2_add1(dst_v, src_v,
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, 0, from_wsq)],
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, enemy, 0, cap_wsq)],
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, 0, to_wsq)], n);
    } else if (cap != NONE) {
        int cap_pt = nnue_pt_idx(cap);
        vec_sub2_add1(dst_v, src_v,
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, sp_pt, from_wsq)],
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, enemy, cap_pt, to_wsq)],
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, pr_pt, to_wsq)], n);
    } else {
        vec_sub_add(dst_v, src_v,
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, sp_pt, from_wsq)],
            g_net.ft_weights[nnue_feat_idx(persp, kb, hm, mover, pr_pt, to_wsq)], n);
    }
}

void nnue_acc_make_move(nnue_acc *dst, const nnue_acc *src, const chess *before, u64 m) {
    if (!g_nnue_loaded || !src || !src->valid) {
        dst->valid = 0u;
        dst->dirty = 0u;
        return;
    }
    bool btm = is_flipped(before);
    int flip_mask = btm ? 56 : 0;
    int mover = btm ? 1 : 0;
    int sp_pt = nnue_pt_idx(get_start_piece(m));
    int to_wsq = get_end_offset(m) ^ flip_mask;

    int old_ksq[2] = { src->ksq[0], src->ksq[1] };
    int new_ksq[2] = { old_ksq[0], old_ksq[1] };
    if (sp_pt == 5) {
        new_ksq[mover] = to_wsq;
    }
    dst->ksq[0] = (uint8_t) new_ksq[0];
    dst->ksq[1] = (uint8_t) new_ksq[1];

    bool need_after = false;
    int kb_arr[2], hm_arr[2];
    bool refresh_p[2] = { false, false };
    for (int p = 0; p < 2; p++) {
        int old_hm = 0, new_hm = 0;
        int old_kb = nnue_king_bucket_for(g_nnue_kb, p, old_ksq[p], &old_hm);
        int new_kb = nnue_king_bucket_for(g_nnue_kb, p, new_ksq[p], &new_hm);
        kb_arr[p] = new_kb;
        hm_arr[p] = new_hm;
        if (old_kb != new_kb || old_hm != new_hm) {
            refresh_p[p] = true;
            need_after = true;
        }
    }

    u64 bbs[2][6];
    if (need_after) {
        chess after = *before;
        chess_make_move(&after, m);
        extract_canonical_bbs(&after, bbs);
    }

    for (int p = 0; p < 2; p++) {
        if (refresh_p[p]) {
            refresh_persp_scratch(dst->v[p], bbs, p, new_ksq[p]);
        } else {
            apply_move_persp(dst->v[p], src->v[p], p, kb_arr[p], hm_arr[p], btm, m);
        }
    }
    dst->valid = 1u;
    dst->dirty = 0u;
}

void nnue_acc_materialize(
    nnue_acc *stack, nnue_finny_entry finny[2][NNUE_MAX_KING_BUCKETS][2], int ply, const chess *b) {
    if (!g_nnue_loaded || !stack || ply < 0) {
        return;
    }
    if (stack[ply].valid) {
        return;
    }

    int p = ply;
    while (p > 0 && !stack[p].valid && stack[p].dirty) {
        p--;
    }

    if (!stack[p].valid || (ply - p) > 6) {
        u64 bbs[2][6];
        extract_canonical_bbs(b, bbs);
        int w_ksq = bbs[0][5] ? __builtin_ctzll(bbs[0][5]) : 4;
        int b_ksq = bbs[1][5] ? __builtin_ctzll(bbs[1][5]) : 60;
        stack[ply].ksq[0] = (uint8_t) w_ksq;
        stack[ply].ksq[1] = (uint8_t) b_ksq;
        refresh_persp_finny(stack[ply].v[0], finny, bbs, 0, w_ksq);
        refresh_persp_finny(stack[ply].v[1], finny, bbs, 1, b_ksq);
        stack[ply].valid = 1u;
        stack[ply].dirty = 0u;
        return;
    }

    int n = g_nnue_hidden;
    int cur_ksq[2] = { stack[p].ksq[0], stack[p].ksq[1] };
    int base_hm[2], base_kb[2];
    base_kb[0] = nnue_king_bucket_for(g_nnue_kb, 0, cur_ksq[0], &base_hm[0]);
    base_kb[1] = nnue_king_bucket_for(g_nnue_kb, 1, cur_ksq[1], &base_hm[1]);
    bool needs_refresh[2] = { false, false };

    for (int k = p + 1; k <= ply; k++) {
        u64 m = stack[k].move;
        if (m != NULL_MOVE) {
            int sp_pt = nnue_pt_idx(get_start_piece(m));
            if (sp_pt == 5) {
                bool btm = stack[k].before_flipped != 0u;
                int mover = btm ? 1 : 0;
                int to_wsq = get_end_offset(m) ^ (btm ? 56 : 0);
                cur_ksq[mover] = to_wsq;
                int nhm = 0;
                int nkb = nnue_king_bucket_for(g_nnue_kb, mover, to_wsq, &nhm);
                if (nkb != base_kb[mover] || nhm != base_hm[mover]) {
                    needs_refresh[mover] = true;
                }
            }
        }
        stack[k].ksq[0] = (uint8_t) cur_ksq[0];
        stack[k].ksq[1] = (uint8_t) cur_ksq[1];
    }

    u64 bbs[2][6];
    if (needs_refresh[0] || needs_refresh[1]) {
        extract_canonical_bbs(b, bbs);
    }

    for (int persp = 0; persp < 2; persp++) {
        if (needs_refresh[persp]) {
            refresh_persp_finny(stack[ply].v[persp], finny, bbs, persp, cur_ksq[persp]);
        } else {
            for (int k = p + 1; k <= ply; k++) {
                if (stack[k].move == NULL_MOVE) {
                    memcpy(stack[k].v[persp], stack[k - 1].v[persp], sizeof(int16_t) * (size_t) n);
                } else {
                    apply_move_persp(stack[k].v[persp], stack[k - 1].v[persp], persp,
                        base_kb[persp], base_hm[persp], stack[k].before_flipped != 0u,
                        stack[k].move);
                }
            }
        }
    }

    if (!needs_refresh[0] && !needs_refresh[1]) {
        for (int k = p + 1; k <= ply; k++) {
            stack[k].valid = 1u;
            stack[k].dirty = 0u;
        }
    } else {
        stack[ply].valid = 1u;
        stack[ply].dirty = 0u;
    }
}

static inline int nnue_phase_bucket(const chess *b) {
    int pieces = __builtin_popcountll(b->friendly_pieces | b->enemy_pieces);
    int bucket = (pieces - 2) / 4;
    if (bucket < 0) {
        bucket = 0;
    } else if (bucket >= NNUE_BUCKETS) {
        bucket = NNUE_BUCKETS - 1;
    }
    return bucket;
}

int nnue_eval_acc(const nnue_acc *acc, const chess *b) {
    if (!g_nnue_loaded) {
        return 0;
    }
    nnue_acc tmp;
    if (!acc || !acc->valid) {
        nnue_acc_refresh(&tmp, b);
        acc = &tmp;
    }

    int n = g_nnue_hidden;
    int stm = is_flipped(b) ? 1 : 0;
    const int16_t *restrict stm_v = acc->v[stm];
    const int16_t *restrict opp_v = acc->v[1 - stm];
    int bucket = nnue_phase_bucket(b);
    const int16_t *restrict ow = g_net.out_weights[bucket];

    int64_t sum = 0;
#if defined(__AVX2__)
    __m256i v_zero = _mm256_setzero_si256();
    __m256i v_qa = _mm256_set1_epi16((int16_t) NNUE_QA);
    __m256i v_sum = _mm256_setzero_si256();

    for (int i = 0; i < n; i += 16) {
        __m256i a0 = _mm256_loadu_si256((const __m256i *) (stm_v + i));
        __m256i c0 = _mm256_min_epi16(_mm256_max_epi16(a0, v_zero), v_qa);
        __m256i w0 = _mm256_loadu_si256((const __m256i *) (ow + i));
        __m256i cw0 = _mm256_mullo_epi16(c0, w0);
        v_sum = _mm256_add_epi32(v_sum, _mm256_madd_epi16(cw0, c0));

        __m256i a1 = _mm256_loadu_si256((const __m256i *) (opp_v + i));
        __m256i c1 = _mm256_min_epi16(_mm256_max_epi16(a1, v_zero), v_qa);
        __m256i w1 = _mm256_loadu_si256((const __m256i *) (ow + n + i));
        __m256i cw1 = _mm256_mullo_epi16(c1, w1);
        v_sum = _mm256_add_epi32(v_sum, _mm256_madd_epi16(cw1, c1));
    }

    __m128i lo = _mm256_castsi256_si128(v_sum);
    __m128i hi = _mm256_extracti128_si256(v_sum, 1);
    __m128i s4 = _mm_add_epi32(lo, hi);
    __m128i s2 = _mm_add_epi32(s4, _mm_shuffle_epi32(s4, _MM_SHUFFLE(1, 0, 3, 2)));
    __m128i s1 = _mm_add_epi32(s2, _mm_shuffle_epi32(s2, _MM_SHUFFLE(2, 3, 0, 1)));
    sum = (int64_t) _mm_cvtsi128_si32(s1);
#else
    for (int i = 0; i < n; i++) {
        int32_t v0 = stm_v[i];
        if (v0 < 0) {
            v0 = 0;
        } else if (v0 > NNUE_QA) {
            v0 = NNUE_QA;
        }
        sum += (int64_t) (v0 * v0) * (int64_t) ow[i];

        int32_t v1 = opp_v[i];
        if (v1 < 0) {
            v1 = 0;
        } else if (v1 > NNUE_QA) {
            v1 = NNUE_QA;
        }
        sum += (int64_t) (v1 * v1) * (int64_t) ow[n + i];
    }
#endif

    int64_t out = (sum / NNUE_QA) + (int64_t) g_net.out_bias[bucket];
    int32_t cp = (int32_t) ((out * NNUE_SCALE) / (NNUE_QA * NNUE_QB));
    return cp;
}

int nnue_evaluate(const chess *b) {
    nnue_acc acc;
    nnue_acc_refresh(&acc, b);
    return nnue_eval_acc(&acc, b);
}
