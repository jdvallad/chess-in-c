#include "Chess.h"
#include "primitives.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
Chess past_boards[MAX_GAME_LENGTH];
Chess game;
int game_length = 0;
move legal_moves[MAX_GAME_LENGTH];

int main() {
  initialize_game(&game, legal_moves, past_boards);
  terminal_move("e2e4", &game, legal_moves, &game_length, past_boards);
  terminal_move("e7e5", &game, legal_moves, &game_length, past_boards);
  return 0;
}
