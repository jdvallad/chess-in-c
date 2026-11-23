#define _POSIX_C_SOURCE 200809L

#include "core/board.h"
#include "core/primitives.h"
#include "engine/nnue.h"

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct sample {
    chess board;
    int16_t search_cp;
    float result;
} sample;

typedef struct nnue_header {
    char magic[8];
    uint32_t version;
    uint32_t inputs;
    uint32_t hidden;
    uint32_t buckets;
} nnue_header;

// Virtual Feature Factorization:
// Effective weight for feature (kb * 768 + base_f) is W_BASE[base_f] + W1[kb * 768 + base_f].
// At export time, W_BASE is fused directly into W1 -> ZERO runtime cost, 4x faster convergence!
static float W_BASE[NNUE_BASE_INPUTS][NNUE_HIDDEN];
static float W1[NNUE_INPUTS][NNUE_HIDDEN];
static float B1[NNUE_HIDDEN];
static float W2[NNUE_BUCKETS][NNUE_HIDDEN * 2];
static float B2[NNUE_BUCKETS];

static float M_WB[NNUE_BASE_INPUTS][NNUE_HIDDEN], V_WB[NNUE_BASE_INPUTS][NNUE_HIDDEN];
static float M_W1[NNUE_INPUTS][NNUE_HIDDEN], V_W1[NNUE_INPUTS][NNUE_HIDDEN];
static float M_B1[NNUE_HIDDEN], V_B1[NNUE_HIDDEN];
static float M_W2[NNUE_BUCKETS][NNUE_HIDDEN * 2], V_W2[NNUE_BUCKETS][NNUE_HIDDEN * 2];
static float M_B2[NNUE_BUCKETS], V_B2[NNUE_BUCKETS];

typedef struct grad_buf {
    float g_wb[NNUE_BASE_INPUTS][NNUE_HIDDEN];
    float g_w1[NNUE_INPUTS][NNUE_HIDDEN];
    float g_b1[NNUE_HIDDEN];
    float g_w2[NNUE_BUCKETS][NNUE_HIDDEN * 2];
    float g_b2[NNUE_BUCKETS];
    double loss_sum;
} grad_buf;

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static float rand_uniform(uint64_t *state) {
    return (float) ((double) (splitmix64(state) >> 11) * (1.0 / 9007199254740992.0));
}

static inline int sample_bucket(const chess *b) {
    int pieces = __builtin_popcountll(b->friendly_pieces | b->enemy_pieces);
    int bk = (pieces - 2) / 4;
    if (bk < 0) {
        bk = 0;
    } else if (bk >= NNUE_BUCKETS) {
        bk = NNUE_BUCKETS - 1;
    }
    return bk;
}

// Extracts canonical king-bucketed + horizontally-mirrored features matching nnue.c BIT-FOR-BIT!
static int extract_features(const chess *b, int stm_feats[32], int opp_feats[32]) {
    bool btm = is_flipped(b);
    int flip_mask = btm ? 56 : 0;
    int stm = btm ? 1 : 0;
    int opp = 1 - stm;

    u64 friendly = b->friendly_pieces;
    u64 enemy = b->enemy_pieces;
    u64 tp = b->pawns & PAWN_MASK;
    u64 orth = b->orthogonal_pieces;
    u64 diag = b->diagonal_pieces;
    u64 kings = b->kings;

    u64 fk_bb = friendly & kings;
    u64 ek_bb = enemy & kings;
    int stm_kwsq = (fk_bb ? __builtin_ctzll(fk_bb) : 4) ^ flip_mask;
    int opp_kwsq = (ek_bb ? __builtin_ctzll(ek_bb) : 60) ^ flip_mask;

    int stm_hm = 0, opp_hm = 0;
    int stm_kb = nnue_king_bucket(stm, stm_kwsq, &stm_hm);
    int opp_kb = nnue_king_bucket(opp, opp_kwsq, &opp_hm);

    u64 f_bbs[6] = {
        friendly & tp,
        friendly & ~(tp | orth | diag | kings),
        friendly & diag & ~orth,
        friendly & orth & ~diag,
        friendly & diag & orth,
        fk_bb,
    };
    u64 e_bbs[6] = {
        enemy & tp,
        enemy & ~(tp | orth | diag | kings),
        enemy & diag & ~orth,
        enemy & orth & ~diag,
        enemy & diag & orth,
        ek_bb,
    };

    int n = 0;
    for (int pt = 0; pt < 6; pt++) {
        u64 bb = f_bbs[pt];
        while (bb && n < 32) {
            int wsq = __builtin_ctzll(bb) ^ flip_mask;
            bb &= bb - 1ull;
            stm_feats[n] = nnue_feat_idx(stm, stm_kb, stm_hm, stm, pt, wsq);
            opp_feats[n] = nnue_feat_idx(opp, opp_kb, opp_hm, stm, pt, wsq);
            n++;
        }
    }
    for (int pt = 0; pt < 6; pt++) {
        u64 bb = e_bbs[pt];
        while (bb && n < 32) {
            int wsq = __builtin_ctzll(bb) ^ flip_mask;
            bb &= bb - 1ull;
            stm_feats[n] = nnue_feat_idx(stm, stm_kb, stm_hm, opp, pt, wsq);
            opp_feats[n] = nnue_feat_idx(opp, opp_kb, opp_hm, opp, pt, wsq);
            n++;
        }
    }
    return n;
}

static inline float sigmoidf_safe(float x) {
    if (x > 16.0f) {
        return 1.0f;
    }
    if (x < -16.0f) {
        return 0.0f;
    }
    return 1.0f / (1.0f + expf(-x));
}

static float forward_sample(const sample *s, int *bucket_out, int stm_f[32], int opp_f[32],
    int *nf_out, float a0[NNUE_HIDDEN], float a1[NNUE_HIDDEN]) {
    int nf = extract_features(&s->board, stm_f, opp_f);
    int bk = sample_bucket(&s->board);
    *nf_out = nf;
    *bucket_out = bk;

    memcpy(a0, B1, sizeof(float) * NNUE_HIDDEN);
    memcpy(a1, B1, sizeof(float) * NNUE_HIDDEN);
    for (int k = 0; k < nf; k++) {
        int f0 = stm_f[k];
        int f1 = opp_f[k];
        const float *restrict r0 = W1[f0];
        const float *restrict b0 = W_BASE[f0 % NNUE_BASE_INPUTS];
        const float *restrict r1 = W1[f1];
        const float *restrict b1 = W_BASE[f1 % NNUE_BASE_INPUTS];
        for (int j = 0; j < NNUE_HIDDEN; j++) {
            a0[j] += r0[j] + b0[j];
            a1[j] += r1[j] + b1[j];
        }
    }
    const float *restrict w2 = W2[bk];
    float out = B2[bk];
    for (int j = 0; j < NNUE_HIDDEN; j++) {
        float c0 = (a0[j] < 0.0f) ? 0.0f : ((a0[j] > 1.0f) ? 1.0f : a0[j]);
        float c1 = (a1[j] < 0.0f) ? 0.0f : ((a1[j] > 1.0f) ? 1.0f : a1[j]);
        out += (c0 * c0) * w2[j] + (c1 * c1) * w2[NNUE_HIDDEN + j];
    }
    return out;
}

typedef struct batch_job {
    const sample *data;
    const size_t *order;
    size_t start;
    size_t end;
    float eval_blend;
    float k_cp;
    grad_buf *gb;
} batch_job;

static void *batch_worker(void *arg) {
    batch_job *job = (batch_job *) arg;
    grad_buf *gb = job->gb;
    memset(gb, 0, sizeof(*gb));

    for (size_t idx = job->start; idx < job->end; idx++) {
        const sample *s = &job->data[job->order[idx]];
        int stm_f[32], opp_f[32], nf = 0, bk = 0;
        float a0[NNUE_HIDDEN], a1[NNUE_HIDDEN];

        float out = forward_sample(s, &bk, stm_f, opp_f, &nf, a0, a1);
        float pred_p = sigmoidf_safe(out * (float) NNUE_SCALE * job->k_cp);

        bool btm = is_flipped(&s->board);
        float stm_cp = btm ? -(float) s->search_cp : (float) s->search_cp;
        float stm_res = btm ? (1.0f - s->result) : s->result;
        float target_p = job->eval_blend * sigmoidf_safe(stm_cp * job->k_cp)
                         + (1.0f - job->eval_blend) * stm_res;

        float err = pred_p - target_p;
        gb->loss_sum += (double) (err * err);

        float d_out = 2.0f * err * pred_p * (1.0f - pred_p) * ((float) NNUE_SCALE * job->k_cp);
        gb->g_b2[bk] += d_out;

        const float *restrict w2 = W2[bk];
        float *restrict gw2 = gb->g_w2[bk];
        float da0[NNUE_HIDDEN], da1[NNUE_HIDDEN];

        for (int j = 0; j < NNUE_HIDDEN; j++) {
            float c0 = (a0[j] < 0.0f) ? 0.0f : ((a0[j] > 1.0f) ? 1.0f : a0[j]);
            float c1 = (a1[j] < 0.0f) ? 0.0f : ((a1[j] > 1.0f) ? 1.0f : a1[j]);
            gw2[j] += d_out * (c0 * c0);
            gw2[NNUE_HIDDEN + j] += d_out * (c1 * c1);
            da0[j] = (a0[j] > 0.0f && a0[j] < 1.0f) ? (d_out * w2[j] * 2.0f * c0) : 0.0f;
            da1[j]
                = (a1[j] > 0.0f && a1[j] < 1.0f) ? (d_out * w2[NNUE_HIDDEN + j] * 2.0f * c1) : 0.0f;
            gb->g_b1[j] += da0[j] + da1[j];
        }

        for (int k = 0; k < nf; k++) {
            int f0 = stm_f[k];
            int f1 = opp_f[k];
            float *restrict gw0 = gb->g_w1[f0];
            float *restrict gwb0 = gb->g_wb[f0 % NNUE_BASE_INPUTS];
            float *restrict gw1 = gb->g_w1[f1];
            float *restrict gwb1 = gb->g_wb[f1 % NNUE_BASE_INPUTS];
            for (int j = 0; j < NNUE_HIDDEN; j++) {
                gw0[j] += da0[j];
                gwb0[j] += da0[j];
                gw1[j] += da1[j];
                gwb1[j] += da1[j];
            }
        }
    }
    return NULL;
}

typedef struct adam_w1_job {
    grad_buf *gbufs;
    int n_workers;
    int row_start;
    int row_end;
    float lr;
    float b1_corr;
    float b2_corr;
    float inv_batch;
    float wd;
} adam_w1_job;

static void *adam_w1_worker(void *arg) {
    adam_w1_job *job = (adam_w1_job *) arg;
    const float beta1 = 0.9f;
    const float beta2 = 0.999f;
    const float eps = 1e-8f;
    for (int i = job->row_start; i < job->row_end; i++) {
        float *restrict w = W1[i];
        float *restrict m = M_W1[i];
        float *restrict v = V_W1[i];
        float *restrict g0 = job->gbufs[0].g_w1[i];
        for (int t = 1; t < job->n_workers; t++) {
            const float *restrict gt = job->gbufs[t].g_w1[i];
            for (int j = 0; j < NNUE_HIDDEN; j++) {
                g0[j] += gt[j];
            }
        }
        for (int j = 0; j < NNUE_HIDDEN; j++) {
            float grad = g0[j] * job->inv_batch + job->wd * w[j];
            m[j] = beta1 * m[j] + (1.0f - beta1) * grad;
            v[j] = beta2 * v[j] + (1.0f - beta2) * grad * grad;
            float mh = m[j] * job->b1_corr;
            float vh = v[j] * job->b2_corr;
            w[j] -= job->lr * mh / (sqrtf(vh) + eps);
            g0[j] = 0.0f;
        }
    }
    return NULL;
}

static void adam_step(float *w, float *g, float *m, float *v, size_t n, float lr, float b1_corr,
    float b2_corr, float inv_batch, float wd, float clamp_lim) {
    const float beta1 = 0.9f;
    const float beta2 = 0.999f;
    const float eps = 1e-8f;
    for (size_t i = 0; i < n; i++) {
        float grad = g[i] * inv_batch + wd * w[i];
        m[i] = beta1 * m[i] + (1.0f - beta1) * grad;
        v[i] = beta2 * v[i] + (1.0f - beta2) * grad * grad;
        float mh = m[i] * b1_corr;
        float vh = v[i] * b2_corr;
        w[i] -= lr * mh / (sqrtf(vh) + eps);
        if (clamp_lim > 0.0f) {
            if (w[i] > clamp_lim) {
                w[i] = clamp_lim;
            } else if (w[i] < -clamp_lim) {
                w[i] = -clamp_lim;
            }
        }
        g[i] = 0.0f;
    }
}

static int16_t clamp_i16(double x, int lo, int hi) {
    long v = lround(x);
    if (v > hi) {
        return (int16_t) hi;
    }
    if (v < lo) {
        return (int16_t) lo;
    }
    return (int16_t) v;
}

static bool save_quantized(const char *path) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        return false;
    }
    nnue_header hdr;
    memcpy(hdr.magic, "CINCNNUE", 8);
    hdr.version = NNUE_VERSION;
    hdr.inputs = NNUE_INPUTS;
    hdr.hidden = NNUE_HIDDEN;
    hdr.buckets = NNUE_BUCKETS;

    int16_t(*qw1)[NNUE_HIDDEN] = calloc(NNUE_INPUTS, sizeof(*qw1));
    int16_t qb1[NNUE_HIDDEN];
    int16_t qw2[NNUE_BUCKETS][NNUE_HIDDEN * 2];
    int32_t qb2[NNUE_BUCKETS];

    // Fuse virtual base weights W_BASE into each king-bucket row W1!
    for (int i = 0; i < NNUE_INPUTS; i++) {
        int bf = i % NNUE_BASE_INPUTS;
        for (int j = 0; j < NNUE_HIDDEN; j++) {
            double fused = (double) W_BASE[bf][j] + (double) W1[i][j];
            qw1[i][j] = clamp_i16(fused * (double) NNUE_QA, -32767, 32767);
        }
    }
    for (int j = 0; j < NNUE_HIDDEN; j++) {
        qb1[j] = clamp_i16((double) B1[j] * (double) NNUE_QA, -32767, 32767);
    }
    // Clamp qw2 to [-127, 127] so AVX2 int16 mullo(clamp(a,0,255), qw2) NEVER overflows!
    for (int bk = 0; bk < NNUE_BUCKETS; bk++) {
        for (int j = 0; j < NNUE_HIDDEN * 2; j++) {
            qw2[bk][j] = clamp_i16((double) W2[bk][j] * (double) NNUE_QB, -127, 127);
        }
        qb2[bk] = (int32_t) lround((double) B2[bk] * (double) (NNUE_QA * NNUE_QB));
    }

    bool ok = (fwrite(&hdr, sizeof(hdr), 1, fp) == 1)
              && (fwrite(qw1, sizeof(*qw1), NNUE_INPUTS, fp) == NNUE_INPUTS)
              && (fwrite(qb1, sizeof(qb1), 1, fp) == 1) && (fwrite(qw2, sizeof(qw2), 1, fp) == 1)
              && (fwrite(qb2, sizeof(qb2), 1, fp) == 1);
    free(qw1);
    fclose(fp);
    return ok;
}

int main(int argc, char **argv) {
    const char *dataset_path = "/tmp/chess_dataset_gen3.bin";
    const char *out_path = NNUE_FILE;
    int epochs = 36;
    int batch_size = 4096;
    int nthreads = 10;
    float base_lr = 0.005f;
    float eval_blend = 0.65f;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dataset") == 0 && i + 1 < argc) {
            dataset_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(argv[i], "--epochs") == 0 && i + 1 < argc) {
            epochs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--batch") == 0 && i + 1 < argc) {
            batch_size = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            nthreads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--lr") == 0 && i + 1 < argc) {
            base_lr = (float) atof(argv[++i]);
        } else if (strcmp(argv[i], "--blend") == 0 && i + 1 < argc) {
            eval_blend = (float) atof(argv[++i]);
        }
    }
    if (nthreads < 1) {
        nthreads = 1;
    }
    if (nthreads > 12) {
        nthreads = 12;
    }

    FILE *fp = fopen(dataset_path, "rb");
    if (!fp) {
        fprintf(stderr, "Cannot open dataset %s\n", dataset_path);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long fsz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    size_t total = (size_t) fsz / sizeof(sample);
    sample *data = (sample *) malloc(total * sizeof(sample));
    if (!data) {
        fclose(fp);
        return 1;
    }
    total = fread(data, sizeof(sample), total, fp);
    fclose(fp);
    printf("[NNUE-v3] Loaded %zu positions from %s (Factorized (%dx768)->%dx2->%d, batch=%d, "
           "threads=%d)\n",
        total, dataset_path, NNUE_KING_BUCKETS, NNUE_HIDDEN, NNUE_BUCKETS, batch_size, nthreads);
    fflush(stdout);

    uint64_t rng = 0xDEADBEEFCAFEBABEull;
    size_t *order = (size_t *) malloc(total * sizeof(size_t));
    for (size_t i = 0; i < total; i++) {
        order[i] = i;
    }
    for (size_t i = total - 1; i > 0; i--) {
        size_t j = (size_t) (splitmix64(&rng) % (uint64_t) (i + 1));
        size_t tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }
    size_t n_val = total / 20;
    if (n_val > 60000) {
        n_val = 60000;
    }
    size_t n_train = total - n_val;

    // Initialize shared base weights with Xavier scale; king-bucket residuals start at 0!
    float s1 = 1.0f / sqrtf(32.0f);
    for (int i = 0; i < NNUE_BASE_INPUTS; i++) {
        for (int j = 0; j < NNUE_HIDDEN; j++) {
            W_BASE[i][j] = (rand_uniform(&rng) * 2.0f - 1.0f) * s1 * 0.25f;
        }
    }
    memset(W1, 0, sizeof(W1));
    for (int j = 0; j < NNUE_HIDDEN; j++) {
        B1[j] = 0.02f;
    }
    float s2 = 1.0f / sqrtf((float) (NNUE_HIDDEN * 2));
    for (int bk = 0; bk < NNUE_BUCKETS; bk++) {
        for (int j = 0; j < NNUE_HIDDEN * 2; j++) {
            W2[bk][j] = (rand_uniform(&rng) * 2.0f - 1.0f) * s2;
        }
        B2[bk] = 0.0f;
    }

    grad_buf *gbufs = (grad_buf *) calloc((size_t) nthreads, sizeof(grad_buf));
    const float k_cp = 1.0f / 320.0f;
    int step = 0;

    for (int ep = 1; ep <= epochs; ep++) {
        for (size_t i = n_train - 1; i > 0; i--) {
            size_t j = (size_t) (splitmix64(&rng) % (uint64_t) (i + 1));
            size_t tmp = order[i];
            order[i] = order[j];
            order[j] = tmp;
        }

        float prog = (float) (ep - 1) / (float) (epochs > 1 ? epochs - 1 : 1);
        float lr = base_lr * (0.04f + 0.96f * 0.5f * (1.0f + cosf(3.14159265f * prog)));
        double train_loss = 0.0;

        for (size_t bstart = 0; bstart < n_train; bstart += (size_t) batch_size) {
            size_t bend = bstart + (size_t) batch_size;
            if (bend > n_train) {
                bend = n_train;
            }
            size_t blen = bend - bstart;
            size_t chunk = (blen + (size_t) nthreads - 1) / (size_t) nthreads;

            pthread_t th[12];
            batch_job jobs[12];
            int launched = 0;
            for (int t = 0; t < nthreads; t++) {
                size_t s = bstart + (size_t) t * chunk;
                if (s >= bend) {
                    break;
                }
                size_t e = s + chunk;
                if (e > bend) {
                    e = bend;
                }
                jobs[t].data = data;
                jobs[t].order = order;
                jobs[t].start = s;
                jobs[t].end = e;
                jobs[t].eval_blend = eval_blend;
                jobs[t].k_cp = k_cp;
                jobs[t].gb = &gbufs[t];
                pthread_create(&th[t], NULL, batch_worker, &jobs[t]);
                launched++;
            }
            for (int t = 0; t < launched; t++) {
                pthread_join(th[t], NULL);
                train_loss += gbufs[t].loss_sum;
            }

            step++;
            float b1c = 1.0f / (1.0f - powf(0.9f, (float) step));
            float b2c = 1.0f / (1.0f - powf(0.999f, (float) step));
            float inv_b = 1.0f / (float) blen;

            adam_w1_job w1_jobs[12];
            int rows_per = (NNUE_INPUTS + launched - 1) / launched;
            int w1_launched = 0;
            for (int t = 0; t < launched; t++) {
                int rs = t * rows_per;
                if (rs >= NNUE_INPUTS) {
                    break;
                }
                int re = rs + rows_per;
                if (re > NNUE_INPUTS) {
                    re = NNUE_INPUTS;
                }
                w1_jobs[t].gbufs = gbufs;
                w1_jobs[t].n_workers = launched;
                w1_jobs[t].row_start = rs;
                w1_jobs[t].row_end = re;
                w1_jobs[t].lr = lr;
                w1_jobs[t].b1_corr = b1c;
                w1_jobs[t].b2_corr = b2c;
                w1_jobs[t].inv_batch = inv_b;
                w1_jobs[t].wd
                    = 6e-5f; // gentle L2 pulls rare king-bucket residuals cleanly toward W_BASE!
                pthread_create(&th[t], NULL, adam_w1_worker, &w1_jobs[t]);
                w1_launched++;
            }

            // Reduce W_BASE, B1, W2, B2 on main thread while W1 workers run
            for (int t = 1; t < launched; t++) {
                for (int i = 0; i < NNUE_BASE_INPUTS; i++) {
                    for (int j = 0; j < NNUE_HIDDEN; j++) {
                        gbufs[0].g_wb[i][j] += gbufs[t].g_wb[i][j];
                    }
                }
                for (int j = 0; j < NNUE_HIDDEN; j++) {
                    gbufs[0].g_b1[j] += gbufs[t].g_b1[j];
                }
                for (int bk = 0; bk < NNUE_BUCKETS; bk++) {
                    for (int j = 0; j < NNUE_HIDDEN * 2; j++) {
                        gbufs[0].g_w2[bk][j] += gbufs[t].g_w2[bk][j];
                    }
                    gbufs[0].g_b2[bk] += gbufs[t].g_b2[bk];
                }
            }
            adam_step(&W_BASE[0][0], &gbufs[0].g_wb[0][0], &M_WB[0][0], &V_WB[0][0],
                (size_t) NNUE_BASE_INPUTS * NNUE_HIDDEN, lr, b1c, b2c, inv_b, 3e-5f, 0.0f);
            adam_step(B1, gbufs[0].g_b1, M_B1, V_B1, NNUE_HIDDEN, lr, b1c, b2c, inv_b, 0.0f, 0.0f);
            adam_step(&W2[0][0], &gbufs[0].g_w2[0][0], &M_W2[0][0], &V_W2[0][0],
                (size_t) NNUE_BUCKETS * NNUE_HIDDEN * 2, lr, b1c, b2c, inv_b, 4e-5f, 1.98f);
            adam_step(B2, gbufs[0].g_b2, M_B2, V_B2, NNUE_BUCKETS, lr, b1c, b2c, inv_b, 0.0f, 0.0f);

            for (int t = 0; t < w1_launched; t++) {
                pthread_join(th[t], NULL);
            }
        }

        if (ep == 1 || ep % 4 == 0 || ep == epochs) {
            double val_loss = 0.0;
            for (size_t idx = n_train; idx < total; idx++) {
                const sample *s = &data[order[idx]];
                int stm_f[32], opp_f[32], nf = 0, bk = 0;
                float a0[NNUE_HIDDEN], a1[NNUE_HIDDEN];
                float out = forward_sample(s, &bk, stm_f, opp_f, &nf, a0, a1);
                float pred_p = sigmoidf_safe(out * (float) NNUE_SCALE * k_cp);
                bool btm = is_flipped(&s->board);
                float stm_cp = btm ? -(float) s->search_cp : (float) s->search_cp;
                float stm_res = btm ? (1.0f - s->result) : s->result;
                float target_p
                    = eval_blend * sigmoidf_safe(stm_cp * k_cp) + (1.0f - eval_blend) * stm_res;
                float err = pred_p - target_p;
                val_loss += (double) (err * err);
            }
            printf("[NNUE-v3] Epoch %2d/%d | lr=%.5f | train_mse=%.6f | val_mse=%.6f\n", ep, epochs,
                lr, train_loss / (double) n_train, val_loss / (double) n_val);
            fflush(stdout);
        }
    }

    if (save_quantized(out_path)) {
        printf("[NNUE-v3] Saved quantized (%dx768)->%dx2->%d network to %s\n", NNUE_KING_BUCKETS,
            NNUE_HIDDEN, NNUE_BUCKETS, out_path);
        if (nnue_load_file(out_path)) {
            double max_qerr = 0.0, sum_qerr = 0.0;
            size_t check_n = (n_val < 500) ? n_val : 500;
            for (size_t k = 0; k < check_n; k++) {
                const sample *s = &data[order[n_train + k]];
                int stm_f[32], opp_f[32], nf = 0, bk = 0;
                float a0[NNUE_HIDDEN], a1[NNUE_HIDDEN];
                float f_cp = forward_sample(s, &bk, stm_f, opp_f, &nf, a0, a1) * (float) NNUE_SCALE;
                int q_cp = nnue_evaluate(&s->board);
                double diff = fabs((double) f_cp - (double) q_cp);
                sum_qerr += diff;
                if (diff > max_qerr) {
                    max_qerr = diff;
                }
            }
            printf(
                "[NNUE-v3] Quantization verification (%zu pos): mean_err=%.2fcp, max_err=%.2fcp\n",
                check_n, sum_qerr / (double) check_n, max_qerr);
        }
    } else {
        fprintf(stderr, "Failed to save %s\n", out_path);
    }

    free(gbufs);
    free(order);
    free(data);
    return 0;
}
