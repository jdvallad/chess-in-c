#include <stdint.h>

typedef struct Chess Chess;
void testFunction();
Chess *chess_create(char letter, uint32_t id);
void chess_delete(Chess **input);
char chess_letter(Chess *input);
