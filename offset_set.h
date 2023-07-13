#ifndef OFFSET_SET_H
#define OFFSET_SET_H
#include "primitives.h"
#include <stdbool.h>
offset_set *offset_set_create(bitboard board);

void offset_set_delete(offset_set **input);

void offset_set_reset(offset_set *input);

uint8_t offset_set_size(offset_set *input);

offset offset_set_next(offset_set *input);

bool offset_set_is_empty(offset_set *input);
#endif
