#include "offset_set.h"
#include <stdlib.h>

offset_set *offset_set_create(bitboard board) {
  offset_set *output = (offset_set *)malloc(sizeof(offset_set));
  output->board = board;
  output->index = 0;
  int ret = 0;
  bitboard temp = board;
  while (temp != 0) {
    temp &= (temp - 1);
    ret++;
  }
  output->size = ret;
  output->remaining = ret;
  return output;
}

void offset_set_delete(offset_set **input) {
  if (*input) {
    free(*input);
    *input = NULL;
  }
  return;
}

bool offset_set_is_empty(offset_set *input) { return input->remaining == 0; }

uint8_t offset_set_size(offset_set *input) { return input->size; }

void offset_set_reset(offset_set *input) {
  input->remaining = input->size;
  input->index = 0;
}

offset offset_set_next(offset_set *input) {
  if (input->remaining == 0) {
    return input->index;
  }
  while (true) {
    input->index += 1;
    if ((((1ull << input->index) & input->board) >> input->index) != 0) {
      input->remaining -= 1;
      return input->index;
    }
  }
}

offset get_next_offset(bitboard board, offset index) {
  index = 63 - index;
  do {
    index -= 1;
  } while (index != -1 && ((1ull << index) & board) == 0);
  return 63 - index;
}
