#include "Chess.h"
#include <stdio.h>
#include <stdlib.h>
bitboard VERTICAL_OFFSETS_BITBOARDS[17] = {0ull,
                                           255ull,
                                           65535ull,
                                           16777215ull,
                                           4294967295ull,
                                           1099511627775ull,
                                           281474976710655ull,
                                           72057594037927935ull,
                                           18446744073709551615ull,
                                           18446744073709551360ull,
                                           18446744073709486080ull,
                                           18446744073692774400ull,
                                           18446744069414584320ull,
                                           18446742974197923840ull,
                                           18446462598732840960ull,
                                           18374686479671623680ull,
                                           0ull};

bitboard HORIZONTAL_OFFSETS_BITBOARDS[17] = {0ull,
                                             72340172838076673ull,
                                             217020518514230019ull,
                                             506381209866536711ull,
                                             1085102592571150095ull,
                                             2242545357980376863ull,
                                             4557430888798830399ull,
                                             9187201950435737471ull,
                                             18446744073709551615ull,
                                             18374403900871474942ull,
                                             18229723555195321596ull,
                                             17940362863843014904ull,
                                             17361641481138401520ull,
                                             16204198715729174752ull,
                                             13889313184910721216ull,
                                             9259542123273814144ull,
                                             0ull};

bitboard offset_to_bitboard(offset input) {
  return compass(1ull, (input % 8) - 7, (input / 8) - 7);
}

offset_set *bitboard_to_offset_set(bitboard board) {
  return offset_set_create(board);
}

Chess *chess_create() {
  Chess *output = (Chess *)malloc(sizeof(Chess));
  output->pawns = 0xff00000000ff00; // contains the is_color_flipped in the
                                    // first bit. (0 is false)
  output->friendly_pieces = 0xffff000000000000;
  output->enemy_pieces = 0xffff;
  output->orthogonal_pieces = 0x9100000000000091;
  output->diagonal_pieces = 0x3400000000000034;
  output->kings = 0x8900000000000089; // contains castle rights in corners
  //  output->legal_moves = NULL;
  output->game_over = false;
  output->is_stalemate = false;
  output->in_check = false;
  output->half_move_count = 0;
  output->move_count = 0;
  // output->game_hash_list[MAX_GAME_LENGTH] = NULL;
  return output;
}

void chess_delete(Chess **input) {
  if (*input) {
    free(*input);
    *input = NULL;
  }
  return;
}

void print_bitboard(bitboard board) {
  for (offset i = 0; i < 8; i++) {
    for (offset j = 7; j >= 0; j--) {
      printf("%lu ", 1ul & (board >> (8 * i + j)));
      if (j == 0) {
        break;
      }
    }
    printf("\n");
  }
  printf("\n");
  return;
}

bitboard compass(bitboard input, offset horizontal, offset vertical) {
  input = input & VERTICAL_OFFSETS_BITBOARDS[vertical + 8] &
          HORIZONTAL_OFFSETS_BITBOARDS[horizontal + 8];
  input = vertical >= 0 ? input >> (vertical * 8) : input << -(vertical * 8);
  input = horizontal >= 0 ? input >> horizontal : input << -horizontal;
  return input;
}

void print_game_state(Chess *game) {
  printf("\n ---------------------------------\n");
  for (int i = 56; i != -8; i++) {
    printf(" | ");
    piece focus_piece = get_piece_at_offset(game, i);
    bitboard board = offset_to_bitboard(i);
    char upset = game->friendly_pieces & board ? 0 : 32;
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
      i -= 16;
      printf(" |\n ---------------------------------\n");
    }
  }
  return;
}

move encode_string_move(char *focus_move) {
  offset starting_offset = (focus_move[0] - 'a') + 8 * (focus_move[1] - '1');
  offset ending_offset = (focus_move[2] - 'a') + 8 * (focus_move[3] - '1');
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
void print_offset_table() {
  printf("\n -----------------------------------------\n");
  for (int i = 56; i != -8; i++) {
    printf(" | ");
    if (i < 10) {
      printf(" ");
    }
    printf("%d", i);

    if (i % 8 == 7) {
      i -= 16;
      printf(" |\n -----------------------------------------\n");
    }
  }
  return;
}

bitboard castle_rights(Chess *game) {
  return game->kings & (compass(game->kings & 0x800000000000008, -4, 0) |
                        compass(game->kings & 0x800000000000008, 3, 0));
}

piece get_piece_at_offset(Chess *game, offset index) {
  piece id = 0;
  bitboard board = offset_to_bitboard(index);
  if (board & game->pawns & 0xffffffffffff00) {
    id |= 1;
  }
  id = id << 1;
  if (board & game->kings & ~castle_rights(game)) {
    id |= 1;
  }
  id = id << 1;
  if (board & game->orthogonal_pieces) {
    id |= 1;
  }
  id = id << 1;
  if (board & game->diagonal_pieces) {
    id |= 1;
  }
  id = id << 1;
  if (board & (game->friendly_pieces | game->enemy_pieces)) {
    id |= 1;
  }
  return id;
}

move encode_move(offset starting_offset, offset ending_offset,
                 piece promotion_piece) {
  move output = 0;
  switch (promotion_piece) {
  case queen:
    output |= 0;
    break;
  case rook:
    output |= 1;
    break;
  case knight:
    output |= 2;
    break;
  case bishop:
    output |= 3;
    break;
  default:
    output |= 0;
    break;
  }
  output = output << 6;
  output |= ending_offset;
  output = output << 6;
  output |= starting_offset;
  return output;
}

void flip_perspective(Chess *game) {
  game->legal_moves[0]++;
  return;
}

void bit_move(
    Chess *game,
    move the_move) { // don't eat the kings off the board, it breaks it
  offset starting_offset = the_move & 63;
  the_move = the_move >> 6;
  offset ending_offset = the_move & 63;
  the_move = the_move >> 6;
  piece promotion_piece;
  switch (the_move & 3) {
  case 0:
    promotion_piece = queen;
    break;
  case 1:
    promotion_piece = rook;
    break;
  case 2:
    promotion_piece = knight;
    break;
  case 3:
    promotion_piece = bishop;
    break;
  default:
    promotion_piece = none;
    break;
  }
  piece starting_piece = get_piece_at_offset(game, starting_offset);
  piece ending_piece = get_piece_at_offset(game, ending_offset);
  if (starting_piece == king) {
    if ((castle_rights(game) & 0x1) && ending_offset == 2) {
      remove_from_board(game, rook, 0);
      remove_from_board(game, get_piece_at_offset(game, 3), 3);
      add_to_board(game, rook, 3);
    }
    if ((castle_rights(game) & offset_to_bitboard(7)) && ending_offset == 6) {
      remove_from_board(game, rook, 7);
      remove_from_board(game, get_piece_at_offset(game, 5), 5);
      add_to_board(game, rook, 5);
    }
    game->kings &= ~offset_to_bitboard(0);
    game->kings &= ~offset_to_bitboard(7);
  }
  if (ending_piece == king) {
    game->kings &= ~offset_to_bitboard(56);
    game->kings &= ~offset_to_bitboard(63);
  }
  if (starting_offset == 0 | starting_offset == 7) {
    game->kings &= ~offset_to_bitboard(starting_offset);
  }
  if (ending_offset == 56 | ending_offset == 63) {
    game->kings &= ~offset_to_bitboard(ending_offset);
  }
  remove_from_board(game, starting_piece, starting_offset);
  remove_from_board(game, ending_piece, ending_offset);
  if ((starting_piece == pawn) && starting_offset > 31 &&
      starting_offset < 40 &&
      ((ending_offset - starting_offset == 7) ||
       (ending_offset - starting_offset == 9)) &&
      (compass(game->pawns, 0, 5) & offset_to_bitboard(ending_offset))) {
    remove_from_board(game, get_piece_at_offset(game, ending_offset - 8),
                      ending_offset - 8);
  }
  if ((starting_piece == pawn) && ending_offset > 55) {
    add_to_board(game, promotion_piece, ending_offset);
  } else {
    add_to_board(game, starting_piece, ending_offset);
  }
  game->pawns &= (0xffffffffffffff);
  game->pawns |= compass((game->pawns & 0xff00000000000000), 0, -7);
  if ((starting_piece == pawn) && starting_offset > 7 && starting_offset < 16 &&
      ending_offset > 23 && ending_offset < 32) {
    game->pawns |= offset_to_bitboard(starting_offset + 48);
  }
  game->pawns ^= offset_to_bitboard(7);
  flip_perspective(game);
}

bool is_same_rank(offset a, offset b) { return (a / 8) == (b / 8); }

bool is_same_file(offset a, offset b) { return (a % 8) == (b % 8); }

void remove_from_board(Chess *game, piece focus_piece, offset square) {
  if (focus_piece == none) {
    return;
  }
  bitboard board = offset_to_bitboard(square);
  if (game->friendly_pieces & board) {
    game->friendly_pieces &= ~board;
  } else {
    game->enemy_pieces &= ~board;
  }
  switch (focus_piece) {
  case rook:
    game->orthogonal_pieces &= ~board;
    break;
  case knight:
    break;
  case bishop:
    game->diagonal_pieces &= ~board;
    break;
  case queen:
    game->orthogonal_pieces &= ~board;
    game->diagonal_pieces &= ~board;
    break;
  case king:
    game->kings &= ~board;
    break;
  case pawn:
    game->pawns &= ~board;
    break;
  default:
    break;
  }
}

void add_to_board(Chess *game, piece focus_piece, offset square) {
  if (focus_piece == none) {
    return;
  }
  bitboard board = offset_to_bitboard(square);
  game->friendly_pieces |= board;
  switch (focus_piece) {
  case rook:
    game->orthogonal_pieces |= board;
    break;
  case knight:
    break;
  case bishop:
    game->diagonal_pieces |= board;
    break;
  case queen:
    game->orthogonal_pieces |= board;
    game->diagonal_pieces |= board;
    break;
  case king:
    game->kings |= board;
    break;
  case pawn:
    game->pawns |= board;
    break;
  default:
    break;
  }
}
