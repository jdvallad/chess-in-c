#include "chess.h"
#include "primitives.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
int main() {
    chess *game = chess_create();
    reset(game);
    print_bitboard(game->pawns);
    print_game_state(game);
    printf("%d\n", get_piece_at_offset(game, 1));
    chess_delete(&game);
    return 0;
}
