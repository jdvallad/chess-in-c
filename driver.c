#include "Chess.h"
#include "MoveSet.h"
#include "offset_set.h"
#include "primitives.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
  bitboard center = offset_to_bitboard(36);
  center |= compass(center, 1, 0);
  center |= compass(center, 0, -1);
  offset_set *set = bitboard_to_offset_set(center);
  printf("%d\n", offset_set_size(set));
  while (!offset_set_is_empty(set)) {
    printf("%d, ", offset_set_next(set));
  }
  printf("\n");
  print_bitboard(center);
  offset_set_reset(set);
  printf("%d\n", offset_set_size(set));
  while (!offset_set_is_empty(set)) {
    printf("%d, ", offset_set_next(set));
  }
  printf("\n");
  print_bitboard(center);
  offset_set_delete(&set);
  return 0;
}
