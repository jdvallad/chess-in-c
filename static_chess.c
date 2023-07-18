#include "static_chess.h"
#include "chess.h"
#include "full_chess.h"
#include "primitives.h"
#include <stdio.h>
#include <stdlib.h>

move static_encode_string(full_chess *game, char *focus_move) {
  if (!(game->current_board->pawns & 1)) {
    return encode_string(focus_move);
  }
  if (focus_move[0] == 'i' && focus_move[1] == 'n' && focus_move[2] == 'i' &&
      focus_move[3] == 't' && focus_move[4] == '\0') {
    return NULL_MOVE; // rejects moves aren't long enough
  }
  if (focus_move[0] == '\0' || focus_move[1] == '\0' || focus_move[2] == '\0' ||
      focus_move[3] == '\0') {
    return NULL_MOVE; // rejects moves aren't long enough
  }
  offset starting_offset =
      (focus_move[0] - 'a') + 8 * (('9' - (focus_move[1] - '0')) - '1');
  offset ending_offset =
      (focus_move[2] - 'a') + 8 * (('9' - (focus_move[3] - '0')) - '1');
  piece promotion_piece = none;
  switch (focus_move[4]) {
  case '\0':
    break;
  case 'q':
    promotion_piece = queen;
    break;
  case 'r':
    promotion_piece = rook;
    break;
  case 'b':
    promotion_piece = bishop;
    break;
  case 'n':
    promotion_piece = knight;
    break;
  default:
    promotion_piece = none;
    break;
  }
  return encode_move(starting_offset, ending_offset, promotion_piece);
}

void static_string_move(full_chess *game, char *focus_move) {
  make_move(game, static_encode_string(game, focus_move));
  return;
}

void static_print_move(full_chess *game, move the_move) {
  if (!(game->current_board->pawns & 1)) {
    offset starting_offset = the_move & 63;
    printf("%c", 'a' + (starting_offset % 8));
    printf("%c", '1' + (starting_offset / 8));
    the_move = the_move >> 6;
    offset ending_offset = the_move & 63;
    printf("%c", 'a' + (ending_offset % 8));
    printf("%c", '1' + (ending_offset / 8));
    the_move = the_move >> 6;
    piece starting_piece =
        get_piece_at_offset(game->current_board, starting_offset);
    switch (the_move & 3) {
    case 0:
      if ((starting_piece == pawn) && ending_offset > 55) {
        printf("q");
      }
      break;
    case 1:
      printf("r");
      break;
    case 2:
      printf("k");
      break;
    case 3:
      printf("b");
      break;
    default:
      break;
    }
  } else {
    offset starting_offset = the_move & 63;
    printf("%c", 'a' + (starting_offset % 8));
    printf("%c", '8' - (starting_offset / 8));
    the_move = the_move >> 6;
    offset ending_offset = the_move & 63;
    printf("%c", 'a' + (ending_offset % 8));
    printf("%c", '8' - (ending_offset / 8));
    the_move = the_move >> 6;
    piece starting_piece =
        get_piece_at_offset(game->current_board, starting_offset);
    switch (the_move & 3) {
    case 0:
      if ((starting_piece == pawn) && ending_offset > 55) {
        printf("q");
      }
      break;
    case 1:
      printf("r");
      break;
    case 2:
      printf("k");
      break;
    case 3:
      printf("b");
      break;
    default:
      break;
    }
  }
  return;
}

void static_print_legal_moves(full_chess *game) {
  printf("{");
  for (int i = 0; game->current_legal_moves[i] != NULL_MOVE; i++) {
    if (i > 0) {
      printf(",");
    }
    static_print_move(game, game->current_legal_moves[i]);
  }
  printf("}\n");
  return;
}

void static_print_board(full_chess *full_game) {
  chess *game = full_game->current_board;
  if (!(game->pawns & 1)) {
    print_game_state(game);
    return;
  }
  printf("\n\n     a   b   c   d   e   f   g   h  ");
  printf("\n   ---------------------------------\n");
  for (int i = 0; i != 64; i++) {
    if (i % 8 == 0) {
      printf(" %d", 8 - (i / 8));
    }
    printf(" | ");
    piece focus_piece = get_piece_at_offset(game, i);
    bitboard board = offset_to_bitboard[i];
    char upset = game->friendly_pieces & board ? 32 : 0;
    switch (focus_piece) {
    case none:
      printf("%c", ' ');
      break;
    case rook:
      printf("%c", 'R' + upset);
      break;
    case knight:
      printf("%c", 'N' + upset);
      break;
    case bishop:
      printf("%c", 'B' + upset);
      break;
    case queen:
      printf("%c", 'Q' + upset);
      break;
    case king:
      printf("%c", 'K' + upset);
      break;
    case pawn:
      printf("%c", 'P' + upset);
      break;
    default:
      printf("%c", '?');
      break;
    }
    if (i % 8 == 7) {
      printf(" | %d\n   ---------------------------------\n",
             9 - ((i + 1) / 8));
    }
  }
  printf("     a   b   c   d   e   f   g   h  \n\n");
  return;
}

int static_perft(full_chess *game, int depth, bool top_call) {
  if (depth < 1) {
    return 0;
  }
  if (top_call) {
    printf("go perft %d\n", depth);
  }
  if (depth == 1) {
    int index = 0;
    while (game->current_legal_moves[index] != NULL_MOVE) {
      if (top_call) {
        static_print_move(game, game->current_legal_moves[index]);
        printf(": 1\n");
      }
      index++;
    }
    if (top_call) {
      printf("\nNodes searched: %d\n\n", index);
    }
    return index;
  }
  int output = 0;
  int index = 0;
  while (game->current_legal_moves[index] != NULL_MOVE) {
    chess temp = *(game->current_board);
    make_move(game, game->current_legal_moves[index]);
    int perft_result = perft(game, depth - 1, false);
    unmake_move(game);
    output += perft_result;
    if (top_call) {
      static_print_move(game, game->current_legal_moves[index]);
      printf(": %d\n", perft_result);
    }
    *(game->current_board) = temp;
    index++;
  }
  if (top_call) {
    printf("\nNodes searched: %d\n\n", output);
  }
  return output;
}

void static_print_game_status(full_chess *game) {
  if (!is_game_over(game)) {
    printf("game status: ongoing, %s to move.\n",
           game->current_board->pawns & 1 ? "white" : "black");
    return;
  }
  if (is_stalemate(game)) {
    printf("game status: stalemate, %s to move.\n",
           game->current_board->pawns & 1 ? "white" : "black");
    return;
  }
  printf("game status: %s won by checkmate\n",
         game->current_board->pawns & 1 ? "white" : "black");
}
