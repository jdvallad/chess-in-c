#include "chess.h"
#include <stdio.h>

int main() {
  printf("Hello World!\n");
  Chess *omg = chess_create('a',2);
  printf("%c\n",chess_letter(omg));
  chess_delete(&omg);
  return 0;
}
