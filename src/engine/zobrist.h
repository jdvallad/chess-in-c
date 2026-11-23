#ifndef ENGINE_ZOBRIST_H
#define ENGINE_ZOBRIST_H

#include "core/primitives.h"

extern u64 g_zobrist_piece[12][64];
extern u64 g_zobrist_castle[16];
extern u64 g_zobrist_ep[9];
extern u64 g_zobrist_side;

void zobrist_init(void);

#endif
