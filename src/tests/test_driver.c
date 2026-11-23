#include "core/board.h"
#include "core/primitives.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    chess *game = chess_create();
    chess_reset(game);
    print_game_state(game);

    // Verify piece lookups on startpos
    assert(get_piece_at_offset(game, 0) == FRIENDLY_ROOK);
    assert(get_piece_at_offset(game, 1) == FRIENDLY_KNIGHT);
    assert(get_piece_at_offset(game, 2) == FRIENDLY_BISHOP);
    assert(get_piece_at_offset(game, 3) == FRIENDLY_QUEEN);
    assert(get_piece_at_offset(game, 4) == FRIENDLY_KING);
    assert(get_piece_at_offset(game, 12) == FRIENDLY_PAWN);
    assert(get_piece_at_offset(game, 28) == NONE);
    assert(get_piece_at_offset(game, 60) == ENEMY_KING);

    // Verify reversible move make + unmake round-trip (e2e4)
    chess original = *game;
    u64 e2e4 = build_move(game, 12, 28, false, false, false, FRIENDLY_PAWN);
    chess_make_move(game, e2e4);
    assert(is_flipped(game));
    assert(has_en_passant(game));
    assert(get_en_passant_offset(game) == 44); // e6 in Black's flipped perspective (e3 in static)
    chess_unmake_move(game, e2e4);

    assert(game->pawns == original.pawns);
    assert(game->friendly_pieces == original.friendly_pieces);
    assert(game->enemy_pieces == original.enemy_pieces);
    assert(game->orthogonal_pieces == original.orthogonal_pieces);
    assert(game->diagonal_pieces == original.diagonal_pieces);
    assert(game->kings == original.kings);

    printf("prototyper checks passed!\n");
    chess_delete(&game);
    return 0;
}
