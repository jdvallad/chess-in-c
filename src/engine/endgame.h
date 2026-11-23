#ifndef ENGINE_ENDGAME_H
#define ENGINE_ENDGAME_H

#include "core/primitives.h"

#include <stdbool.h>

// Build the exact 196,608-position KPK retrograde bitbase (called once via engine_global_init).
void endgame_init(void);

// Probe exact KPK bitbase: returns true if position is exact KPK (3 pieces, 1 pawn).
// Sets `*winning` to true if the pawn-holding side wins with best play, false if theoretical draw.
bool endgame_probe_kpk(const chess *b, bool *pawn_holder_wins);

// Specialized endgame evaluator (KPK bitbase, wrong-color rook-pawn fortress, KBNK corner mate,
// K+2N vs K draw). Returns true and writes side-to-move centipawn score to `*score_out` if handled.
bool endgame_eval(const chess *b, int *score_out);

#endif
