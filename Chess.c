#include "chess.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
 struct Chess {
    char letter;
    uint32_t id;
};

Chess *chess_create(char letter, uint32_t id){
    Chess *output = (Chess *) malloc(sizeof(Chess));
    if(output) {
        output->letter = letter;
        output->id = id;
    }
    return output;
}

void chess_delete(Chess **input){
    if(*input){
        free(*input);
        *input = NULL;
    }
    return;
}
char chess_letter(Chess *input){
    return input->letter;
}
void testFunction() {
  printf("Does this work?\n");
  return;
}
