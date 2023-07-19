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
    double time_spent = ((double) (end - begin) / CLOCKS_PER_SEC) / ((double) batch_count);
    printf("perft %d from startpos took %f seconds.\n", depth, time_spent);
    full_chess_delete(&game);
    return 0;
}

char fen[MAX_FEN_LENGTH];
int main() {
    full_chess *game = full_chess_create();
    set_from_fen(game, "r3k2r/pppbbppp/2n2q1P/1P2p3/3p4/BN2PNP1/P1PnQPB1/R3K2R b KQkq - 0 1");
    printf("%s\n", get_fen(game, fen));
    static_perft(game, 1, true);
    static_print_board(game);

    print_bitboard(get_attacked_board(game->current_board));
    full_chess_delete(&game);
    return 0;
}
