#include "core/attacks.h"

#include "core/board.h"
#include "core/primitives.h"

#include <pthread.h>
#include <string.h>

u64 g_knight_attacks[64];
u64 g_king_attacks[64];
u64 g_pawn_attacks[2][64];
u64 g_between_bb[64][64];
u64 g_line_bb[64][64];
magic_entry g_rook_magics[64];
magic_entry g_bishop_magics[64];

// Flat backing storage: rooks need 102,400 entries (800 KB), bishops need 5,248 entries (41 KB).
static u64 g_rook_table[102400];
static u64 g_bishop_table[5248];
static pthread_once_t g_attacks_once = PTHREAD_ONCE_INIT;

static u64 xorshift64(u64 *s) {
    u64 x = *s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *s = x;
    return x * 0x2545f4914f6cdd1dull;
}

static u64 sparse_magic(u64 *s) {
    return xorshift64(s) & xorshift64(s) & xorshift64(s);
}

static u64 rook_mask(int sq) {
    int r = sq / 8;
    int f = sq % 8;
    u64 m = 0ull;
    for (int rr = r + 1; rr <= 6; rr++) {
        m |= 1ull << (8 * rr + f);
    }
    for (int rr = r - 1; rr >= 1; rr--) {
        m |= 1ull << (8 * rr + f);
    }
    for (int ff = f + 1; ff <= 6; ff++) {
        m |= 1ull << (8 * r + ff);
    }
    for (int ff = f - 1; ff >= 1; ff--) {
        m |= 1ull << (8 * r + ff);
    }
    return m;
}

static u64 bishop_mask(int sq) {
    int r = sq / 8;
    int f = sq % 8;
    u64 m = 0ull;
    for (int rr = r + 1, ff = f + 1; rr <= 6 && ff <= 6; rr++, ff++) {
        m |= 1ull << (8 * rr + ff);
    }
    for (int rr = r + 1, ff = f - 1; rr <= 6 && ff >= 1; rr++, ff--) {
        m |= 1ull << (8 * rr + ff);
    }
    for (int rr = r - 1, ff = f + 1; rr >= 1 && ff <= 6; rr--, ff++) {
        m |= 1ull << (8 * rr + ff);
    }
    for (int rr = r - 1, ff = f - 1; rr >= 1 && ff >= 1; rr--, ff--) {
        m |= 1ull << (8 * rr + ff);
    }
    return m;
}

u64 rook_attacks_slow(int sq, u64 occ) {
    int r = sq / 8;
    int f = sq % 8;
    u64 atk = 0ull;
    for (int rr = r + 1; rr <= 7; rr++) {
        u64 b = 1ull << (8 * rr + f);
        atk |= b;
        if (occ & b) {
            break;
        }
    }
    for (int rr = r - 1; rr >= 0; rr--) {
        u64 b = 1ull << (8 * rr + f);
        atk |= b;
        if (occ & b) {
            break;
        }
    }
    for (int ff = f + 1; ff <= 7; ff++) {
        u64 b = 1ull << (8 * r + ff);
        atk |= b;
        if (occ & b) {
            break;
        }
    }
    for (int ff = f - 1; ff >= 0; ff--) {
        u64 b = 1ull << (8 * r + ff);
        atk |= b;
        if (occ & b) {
            break;
        }
    }
    return atk;
}

u64 bishop_attacks_slow(int sq, u64 occ) {
    int r = sq / 8;
    int f = sq % 8;
    u64 atk = 0ull;
    for (int rr = r + 1, ff = f + 1; rr <= 7 && ff <= 7; rr++, ff++) {
        u64 b = 1ull << (8 * rr + ff);
        atk |= b;
        if (occ & b) {
            break;
        }
    }
    for (int rr = r + 1, ff = f - 1; rr <= 7 && ff >= 0; rr++, ff--) {
        u64 b = 1ull << (8 * rr + ff);
        atk |= b;
        if (occ & b) {
            break;
        }
    }
    for (int rr = r - 1, ff = f + 1; rr >= 0 && ff <= 7; rr--, ff++) {
        u64 b = 1ull << (8 * rr + ff);
        atk |= b;
        if (occ & b) {
            break;
        }
    }
    for (int rr = r - 1, ff = f - 1; rr >= 0 && ff >= 0; rr--, ff--) {
        u64 b = 1ull << (8 * rr + ff);
        atk |= b;
        if (occ & b) {
            break;
        }
    }
    return atk;
}

static void init_magic_slider(magic_entry *entries, u64 *backing, bool is_bishop, u64 *rng_state) {
    u64 occs[4096];
    u64 refs[4096];
    u64 used[4096];
    size_t offset = 0;

    for (int sq = 0; sq < 64; sq++) {
        u64 mask = is_bishop ? bishop_mask(sq) : rook_mask(sq);
        int bits = pop_count(mask);
        int size = 1 << bits;

        // Carry-Rippler trick enumerates every subset of `mask`
        u64 sub = 0ull;
        int idx = 0;
        do {
            occs[idx] = sub;
            refs[idx] = is_bishop ? bishop_attacks_slow(sq, sub) : rook_attacks_slow(sq, sub);
            idx++;
            sub = (sub - mask) & mask;
        } while (sub != 0ull);

        int shift = 64 - bits;
        u64 magic = 0ull;
        while (true) {
            magic = sparse_magic(rng_state);
            if (pop_count((mask * magic) & 0xff00000000000000ull) < 6) {
                continue;
            }
            memset(used, 0, (size_t) size * sizeof(u64));
            bool fail = false;
            for (int i = 0; i < size; i++) {
                int j = (int) ((occs[i] * magic) >> shift);
                if (used[j] == 0ull) {
                    used[j] = refs[i];
                } else if (used[j] != refs[i]) {
                    fail = true;
                    break;
                }
            }
            if (!fail) {
                break;
            }
        }

        entries[sq].mask = mask;
        entries[sq].magic = magic;
        entries[sq].shift = shift;
        entries[sq].table = backing + offset;
        for (int i = 0; i < size; i++) {
            int j = (int) ((occs[i] * magic) >> shift);
            entries[sq].table[j] = refs[i];
        }
        offset += (size_t) size;
    }
}

static void init_leaper_and_line_tables(void) {
    for (int sq = 0; sq < 64; sq++) {
        u64 b = 1ull << sq;
        g_knight_attacks[sq] = compass(b, 2, 1) | compass(b, 1, 2) | compass(b, 2, -1)
                               | compass(b, -1, 2) | compass(b, -2, 1) | compass(b, 1, -2)
                               | compass(b, -2, -1) | compass(b, -1, -2);

        u64 k = b | compass(b, 1, 0) | compass(b, -1, 0);
        k |= compass(k, 0, 1) | compass(k, 0, -1);
        g_king_attacks[sq] = k & ~b;

        g_pawn_attacks[0][sq] = compass(b, -1, 1) | compass(b, 1, 1);
        g_pawn_attacks[1][sq] = compass(b, -1, -1) | compass(b, 1, -1);
    }

    for (int a = 0; a < 64; a++) {
        u64 ba = 1ull << a;
        u64 ra_empty = rook_attacks_bb(a, 0ull);
        u64 ba_empty = bishop_attacks_bb(a, 0ull);
        for (int b = 0; b < 64; b++) {
            g_between_bb[a][b] = 0ull;
            g_line_bb[a][b] = 0ull;
            if (a == b) {
                continue;
            }
            u64 bb = 1ull << b;
            if (ra_empty & bb) {
                g_between_bb[a][b] = rook_attacks_bb(a, bb) & rook_attacks_bb(b, ba);
                g_line_bb[a][b] = (ra_empty & rook_attacks_bb(b, 0ull)) | ba | bb;
            } else if (ba_empty & bb) {
                g_between_bb[a][b] = bishop_attacks_bb(a, bb) & bishop_attacks_bb(b, ba);
                g_line_bb[a][b] = (ba_empty & bishop_attacks_bb(b, 0ull)) | ba | bb;
            }
        }
    }
}

static void attacks_init_internal(void) {
    u64 seed = 0x123456789abcdef0ull;
    init_magic_slider(g_rook_magics, g_rook_table, false, &seed);
    init_magic_slider(g_bishop_magics, g_bishop_table, true, &seed);
    init_leaper_and_line_tables();
}

void attacks_init(void) {
    pthread_once(&g_attacks_once, attacks_init_internal);
}
