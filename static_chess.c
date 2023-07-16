#include "static_chess.h"
#include "chess.h"
#include "primitives.h"
#include <stdio.h>
#include <stdlib.h>

void static_bit_move(Chess *game, char *focus_move) {
  bit_move(game, static_encode_string_move(game, focus_move));
  return;
}

void static_display_move(char *focus_move, Chess *game, move *legal_moves,
                          int *game_length, Chess *past_boards) {
  move real_move = static_encode_string_move(game, focus_move);
  if (in_set(legal_moves, real_move)) {
    bit_move(game, real_move);
    *game_length += 1;
    set_from_source(&past_boards[*game_length], game);
    set_legal_moves(game, legal_moves);
    static_print_game_state(game);
    static_print_legal_moves(game, legal_moves);
  } else {
    printf("not a legal move!\n");
  }
  return;
}

void static_undo_move(Chess *game, move *legal_moves, int *game_length,
                      Chess *past_boards) {
  if (*game_length == 0) {
    return;
  }
  *game_length -= 1;
  set_from_source(game, &past_boards[*game_length]);
  set_legal_moves(game, legal_moves);
  static_print_game_state(game);
  static_print_legal_moves(game, legal_moves);
}

void static_initialize_game(Chess *game, move *legal_moves,
                            Chess *past_boards) {
  reset(game);
  set_from_source(&past_boards[0], game);
  set_legal_moves(game, legal_moves);
  static_print_game_state(game);
  static_print_legal_moves(game, legal_moves);
}

void static_print_game_state(Chess *game) {
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
    bitboard board = offset_to_bitboard(i);
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

move static_encode_string_move(Chess *game, char *focus_move) {
  if (!(game->pawns & 1)) {
    return encode_string_move(focus_move);
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

void static_print_legal_moves(Chess *game, move *legal_moves) {
  printf("{");
  for (int i = 0; legal_moves[i] != NULL_MOVE; i++) {
    if (i > 0) {
      printf(",");
    }
    if (!(game->pawns & 1)) {
      move the_move = legal_moves[i];
      offset starting_offset = the_move & 63;
      printf("%c", 'a' + (starting_offset % 8));
      printf("%c", '1' + (starting_offset / 8));
      the_move = the_move >> 6;
      offset ending_offset = the_move & 63;
      printf("%c", 'a' + (ending_offset % 8));
      printf("%c", '1' + (ending_offset / 8));
      the_move = the_move >> 6;
      piece starting_piece = get_piece_at_offset(game, starting_offset);
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
      move the_move = legal_moves[i];
      offset starting_offset = the_move & 63;
      printf("%c", 'a' + (starting_offset % 8));
      printf("%c", '8' - (starting_offset / 8));
      the_move = the_move >> 6;
      offset ending_offset = the_move & 63;
      printf("%c", 'a' + (ending_offset % 8));
      printf("%c", '8' - (ending_offset / 8));
      the_move = the_move >> 6;
      piece starting_piece = get_piece_at_offset(game, starting_offset);
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
  }
  printf("}\n");
  return;
}
