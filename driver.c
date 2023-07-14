#include "Chess.h"
#include "MoveSet.h"
#include "offset_set.h"
#include "primitives.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
  Chess *game = chess_create();
  move focus_move = encode_move(4, 63, none);
  bit_move(game, focus_move);
  focus_move = encode_move(63, 4, none);
  bit_move(game, focus_move);
  print_game_state(game);
  chess_delete(&game);
  return 0;
}
