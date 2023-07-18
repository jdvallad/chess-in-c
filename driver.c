#include "chess.h"
#include "full_chess.h"
#include "primitives.h"
#include "static_chess.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
chess past_boards[MAX_GAME_LENGTH];
int game_length = 0;
move legal_moves[MAX_POSSIBLE_MOVES];
move past_legal_moves[MAX_POSSIBLE_MOVES][MAX_GAME_LENGTH];

full_chess game;
int main_1() {
  int depth = 5;
  int batch_count = 1;
  full_chess_initialize(&game);
  clock_t begin = clock();
  for (int i = 0; i < batch_count; i++) {
    perft(&game, depth, true);
  }
  clock_t end = clock();
  double time_spent =
      ((double)(end - begin) / CLOCKS_PER_SEC) / ((double)batch_count);
  printf("perft %d from startpos took %f seconds.\n", depth, time_spent);
  return 0;
}
char fen[MAX_FEN_LENGTH];
int main() {
  full_chess_initialize(&game);
  while (!is_game_over(&game)) {
    make_move(&game, get_random_move(&game));
  }
  print_board(&game);
  print_game_status(&game);
  get_fen(&game, fen);
  printf("%s\n", fen);
}
