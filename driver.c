#include "chess.h"
#include "primitives.h"
#include "static_chess.h"
#include <stdio.h>
#include <stdlib.h>
chess past_boards[MAX_GAME_LENGTH];
chess game;
int game_length = 0;
move legal_moves[MAX_POSSIBLE_MOVES];
move past_legal_moves[MAX_POSSIBLE_MOVES][MAX_GAME_LENGTH];

int main1() {
  char *move_list[] = {"f2f4", "e2e4", "g2g4", "d1h5"};
  initialize_game(&game, legal_moves, past_boards);
  int num_moves = (int)(sizeof(move_list) / sizeof(move_list[0]));
  play_move_list(move_list, num_moves, &game, legal_moves, &game_length,
                 past_boards);
  if (game_over(&game)) {
    printf("Game over!\n");
    if (in_check(&game)) {
      printf("Result: %s wins by checkmate!\n",
             game.pawns & 1 ? "white" : "black");
    } else {
      printf("Result: stalemate.\n");
    }
    return 0;
  }
  if (in_check(&game)) {
    printf("In check!\n");
  }
  return 0;
}

int main() {
  initialize_game(&game, legal_moves, past_boards);
  int depth = 3;
  printf("go perft %d\n", depth);
  int perfty = perft(&game, depth, past_legal_moves, true);
  printf("\nNodes searched: %d\n", perfty);
  return 0;
}
