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
  initialize_game(&game, legal_moves, past_boards);
  // display_move("e2e4", &game, legal_moves, &game_length, past_boards);
  // display_move("e2e4", &game, legal_moves, &game_length, past_boards);
  return 0;
}
