#include "chess.h"
#include "primitives.h"
#include "static_chess.h"
#include <stdio.h>
#include <stdlib.h>
chess past_boards[MAX_GAME_LENGTH];
chess game;
int game_length = 0;
move legal_moves[MAX_GAME_LENGTH];

int main() {
  static_initialize_game(&game, legal_moves, past_boards);
  static_display_move("e2e4", &game, legal_moves, &game_length, past_boards);
  static_display_move("f7f6", &game, legal_moves, &game_length, past_boards);
  static_display_move("d1h5", &game, legal_moves, &game_length, past_boards);
  print_legal_moves(&game, legal_moves, 20);
  return 0;
}
