#include <MoveSet.h>
#include <stdlib.h>
MoveSet *moveset_create() {
  MoveSet *output = (MoveSet *)malloc(sizeof(MoveSet));
  return output;
}

void moveset_delete(MoveSet **input) {
  if (*input) {
    free(*input);
    *input = NULL;
  }
  return;
}
