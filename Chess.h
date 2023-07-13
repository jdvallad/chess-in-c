#ifndef CHESS_H
#define CHESS_H
#include <stdint.h>
#include <stdbool.h>
#include "MoveSet.h"
#include "offset_set.h"
#include "primitives.h"
bitboard VERTICAL_OFFSETS_BITBOARDS[17];
bitboard HORIZONTAL_OFFSETS_BITBOARDS[17];
bitboard offset_to_bitboard(offset input);
offset_set *bitboard_to_offset_set(bitboard board);
Chess *chess_create();
void chess_delete(Chess **input);
void print_bitboard(bitboard board);
bitboard compass(bitboard input, offset north, offset east);
#endif
