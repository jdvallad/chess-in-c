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
    bit_move(game, encode_string("e2e4"));
    print_game_state(game);
    print_bitboard(game->pawns);
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
