#include "chess.h"
#include "primitives.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
int main() {
  struct timeval t1;
  gettimeofday(&t1, NULL);
  srand(t1.tv_usec * t1.tv_sec);
  chess *game = chess_create();
  int count = 0;
  for (int i = 0; i < 10; i++) {
    move focus_move = random_bit_move(game);
    bit_move(game, focus_move);
    print_game_state(game);
    print_move(game, focus_move);
    printf("\n");
    print_bitboard(game->friendly_pieces);
    count++;
    printf("this took %d iterations.\n", count);
    // reset(game);
    count = 0;
  }
  chess_delete(&game);
  return 0;
}

int main_1() {
  bitboard board = ~0x0;
  for (offset index = -1; (index = get_next_offset(board, index)) < 64;) {
    printf("%d\n", index);
  }
  return 0;
}
