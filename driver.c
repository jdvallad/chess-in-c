#include "Chess.h"
#include "MoveSet.h"
#include "offset_set.h"
#include "primitives.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
  Chess *game = chess_create();
  print_game_state(game);
  bit_move(game, encode_string_move("e2e5"));
  bit_move(game, encode_string_move("d7d5"));
  game->pawns |= offset_to_bitboard(59);
  // bit_move(game, encode_string_move("e5d6"));
  // bit_move(game, encode_string_move("e1g1"));
  print_offset_table();
  print_game_state(game);
  print_bitboard(game->pawns);
  // printf("%d\n", get_piece_at_offset(game, 51));
  bitboard temp = 0xffffffffffffff00;
  temp = ~compass(temp, 0, -6);
  // print_bitboard(temp);
  printf("%lx", temp);
  chess_delete(&game);
  return 0;
  // bitboard wow = offset_to_bitboard(4);
  // wow |= compass(wow, 0, 7);
  // print_bitboard(wow);
  // printf("%lx", wow);
}
