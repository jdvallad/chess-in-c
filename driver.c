#include "chess.h"
#include "full_chess.h"
#include "primitives.h"
#include "static_chess.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main_1() {
  int depth = 4;
  int batch_count = 10;
  full_chess *game = full_chess_create();
  clock_t begin = clock();
  for (int i = 0; i < batch_count; i++) {
    perft(game, depth, false);
  }
  clock_t end = clock();
  double time_spent =
      ((double)(end - begin) / CLOCKS_PER_SEC) / ((double)batch_count);
  printf("perft %d from startpos took %f seconds.\n", depth, time_spent);
  full_chess_delete(&game);
  return 0;
}

char fen[MAX_FEN_LENGTH];
int main() {
  full_chess *game = full_chess_create();
  while (true) {
    while (!is_game_over(game)) {
      make_move(game, get_random_move(game));
    }
    // print_game_status(game);
    if (game->current_board->pawns & 2 & (~1)) {
      printf("%s\n", get_fen(game, fen));
    }
    reset(game);
  }
  full_chess_delete(&game);
  return 0;
}
