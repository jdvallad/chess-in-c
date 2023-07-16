#include "Chess.h"
#include "MoveSet.h"
#include "offset_set.h"
#include "primitives.h"
#include <stdio.h>
#include <stdlib.h>
Chess past_boards[MAX_GAME_LENGTH];
Chess game;
int game_length = 0;

void do_move(char *focus_move) {
  make_move(&game, focus_move);
  game_length += 1;
  set_from_source(&past_boards[game_length], &game);
}

void undo_move() {
  if (game_length == 0) {
    return;
  }
  game_length -= 1;
  set_from_source(&game, &past_boards[game_length]);
}
int main() {
  reset(&game);
  set_from_source(&past_boards[0], &game);
  draw(&game);
  do_move("e2f8n");
  draw(&game);
  do_move("e8f8");
  draw(&game);
  undo_move();
  draw(&game);
  return 0;
}
