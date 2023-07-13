#include "Chess.h"
#include <stdio.h>
#include <stdlib.h>
bitboard VERTICAL_OFFSETS_BITBOARDS[17] = {0ull,
                                           255ull,
                                           65535ull,
                                           16777215ull,
                                           4294967295ull,
                                           1099511627775ull,
                                           281474976710655ull,
                                           72057594037927935ull,
                                           18446744073709551615ull,
                                           18446744073709551360ull,
                                           18446744073709486080ull,
                                           18446744073692774400ull,
                                           18446744069414584320ull,
                                           18446742974197923840ull,
                                           18446462598732840960ull,
                                           18374686479671623680ull,
                                           0ull};

bitboard HORIZONTAL_OFFSETS_BITBOARDS[17] = {0ull,
                                             72340172838076673ull,
                                             217020518514230019ull,
                                             506381209866536711ull,
                                             1085102592571150095ull,
                                             2242545357980376863ull,
                                             4557430888798830399ull,
                                             9187201950435737471ull,
                                             18446744073709551615ull,
                                             18374403900871474942ull,
                                             18229723555195321596ull,
                                             17940362863843014904ull,
                                             17361641481138401520ull,
                                             16204198715729174752ull,
                                             13889313184910721216ull,
                                             9259542123273814144ull,
                                             0ull};

bitboard offset_to_bitboard(offset input) {
  return compass(1ull, (input % 8) - 7, (input / 8) - 7);
}

offset_set *bitboard_to_offset_set(bitboard board) {
  return offset_set_create(board);
}

Chess *chess_create() {
  Chess *output = (Chess *)malloc(sizeof(Chess));
  return output;
}

void chess_delete(Chess **input) {
  if (*input) {
    free(*input);
    *input = NULL;
  }
  return;
}

void print_bitboard(bitboard board) {
  for (offset i = 0; i < 8; i++) {
    for (offset j = 7; j >= 0; j--) {
      printf("%lu ", 1ul & (board >> (8 * i + j)));
      if (j == 0) {
        break;
      }
    }
    printf("\n");
  }
  printf("\n");
  return;
}

bitboard compass(bitboard input, offset horizontal, offset vertical) {
  input = input & VERTICAL_OFFSETS_BITBOARDS[vertical + 8] &
          HORIZONTAL_OFFSETS_BITBOARDS[horizontal + 8];
  input = vertical >= 0 ? input >> (vertical * 8) : input << -(vertical * 8);
  input = horizontal >= 0 ? input >> horizontal : input << -horizontal;
  return input;
}
