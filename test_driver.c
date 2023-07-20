#include "chess.h"
#include "primitives.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
int main() {
    bitboard temp = (1ull) | (1ull << 8) | (1ull << 16) | (1ull << 24);
    temp |= temp << 32;
    for (int i = 0; i < 8; i++) {
        printf("#define COL_%d 0x%lx\n", (i + 1), temp);
        //print_bitboard(temp);
        temp <<= 1;
    }
    return 0;
}
