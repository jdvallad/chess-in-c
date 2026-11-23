#ifndef ATTACKS_H
#define ATTACKS_H

#include "core/primitives.h"

#include <stdbool.h>

typedef struct magic_entry {
    u64 mask;
    u64 magic;
    u64 *table;
    int shift;
} magic_entry;

extern u64 g_knight_attacks[64];
extern u64 g_king_attacks[64];
extern u64 g_pawn_attacks[2][64]; // [0] = friendly upward, [1] = enemy downward
extern u64 g_between_bb[64][64]; // squares strictly between a and b on same rank/file/diag
extern u64 g_line_bb[64][64]; // full ray line containing a and b (0 if not collinear)
extern magic_entry g_rook_magics[64];
extern magic_entry g_bishop_magics[64];

// Thread-safe one-time initialization (automatically called via pthread_once).
void attacks_init(void);

static inline u64 rook_attacks_bb(int sq, u64 occ) {
    const magic_entry *m = &g_rook_magics[sq];
    return m->table[((occ & m->mask) * m->magic) >> m->shift];
}

static inline u64 bishop_attacks_bb(int sq, u64 occ) {
    const magic_entry *m = &g_bishop_magics[sq];
    return m->table[((occ & m->mask) * m->magic) >> m->shift];
}

static inline u64 queen_attacks_bb(int sq, u64 occ) {
    return rook_attacks_bb(sq, occ) | bishop_attacks_bb(sq, occ);
}

// Reference slow raycasters (exported for magic table verification tests).
u64 rook_attacks_slow(int sq, u64 occ);
u64 bishop_attacks_slow(int sq, u64 occ);

#endif
