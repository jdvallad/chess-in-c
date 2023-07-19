#include "chess.h"
#include "primitives.h"
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

bitboard offset_to_bitboard[64] = {0x8000000000000000,
                                   0x4000000000000000,
                                   0x2000000000000000,
                                   0x1000000000000000,
                                   0x800000000000000,
                                   0x400000000000000,
                                   0x200000000000000,
                                   0x100000000000000,
                                   0x80000000000000,
                                   0x40000000000000,
                                   0x20000000000000,
                                   0x10000000000000,
                                   0x8000000000000,
                                   0x4000000000000,
                                   0x2000000000000,
                                   0x1000000000000,
                                   0x800000000000,
                                   0x400000000000,
                                   0x200000000000,
                                   0x100000000000,
                                   0x80000000000,
                                   0x40000000000,
                                   0x20000000000,
                                   0x10000000000,
                                   0x8000000000,
                                   0x4000000000,
                                   0x2000000000,
                                   0x1000000000,
                                   0x800000000,
                                   0x400000000,
                                   0x200000000,
                                   0x100000000,
                                   0x80000000,
                                   0x40000000,
                                   0x20000000,
                                   0x10000000,
                                   0x8000000,
                                   0x4000000,
                                   0x2000000,
                                   0x1000000,
                                   0x800000,
                                   0x400000,
                                   0x200000,
                                   0x100000,
                                   0x80000,
                                   0x40000,
                                   0x20000,
                                   0x10000,
                                   0x8000,
                                   0x4000,
                                   0x2000,
                                   0x1000,
                                   0x800,
                                   0x400,
                                   0x200,
                                   0x100,
                                   0x80,
                                   0x40,
                                   0x20,
                                   0x10,
                                   0x8,
                                   0x4,
                                   0x2,
                                   0x1};

chess *chess_create() {
  chess *game = (chess *)malloc(sizeof(chess));
  game->pawns = 0xff00000000ff00;
  game->friendly_pieces = 0xffff000000000000;
  game->enemy_pieces = 0xffff;
  game->orthogonal_pieces = 0x9100000000000091;
  game->diagonal_pieces = 0x3400000000000034;
  game->kings = 0x8900000000000089; // contains castle rights in corners
  game->kings &= (~offset_to_bitboard[0] & (~offset_to_bitboard[7]) &
                  (~offset_to_bitboard[56]) &
                  (~offset_to_bitboard[63])); // remove relic castle rights
  game->pawns &= ~1;                          // board isn't color flipped
  game->pawns &= ~(ROW_1);                    // no en-passant is possible
  game->pawns |= 2 | 4 | 8 | 16; // all 4 castling flags are enabled
  return game;
}

void reset(chess *game) {
  game->pawns = 0xff00000000ff00;
  game->friendly_pieces = 0xffff000000000000;
  game->enemy_pieces = 0xffff;
  game->orthogonal_pieces = 0x9100000000000091;
  game->diagonal_pieces = 0x3400000000000034;
  game->kings = 0x8900000000000089; // contains castle rights in corners
  game->kings &= (~offset_to_bitboard[0]) & (~offset_to_bitboard[7]) &
                 (~offset_to_bitboard[56]) &
                 (~offset_to_bitboard[63]); // remove relic castle rights
  game->pawns &= ~1;                        // board isn't color flipped
  game->pawns &= ~(ROW_1);                  // no en-passant is possible
  game->pawns |= 2 | 4 | 8 | 16;            // all 4 castling flags are enabled
  return;
}

void chess_delete(chess **input) {
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

void print_game_state(chess *game) {
  printf("\n\n     a   b   c   d   e   f   g   h  ");
  printf("\n   ---------------------------------\n");
  for (int i = 56; i != -8; i++) {
    if (i % 8 == 0) {
      printf(" %d", 1 + (i / 8));
    }
    printf(" | ");
    piece focus_piece = get_piece_at_offset(game, i);
    bitboard board = offset_to_bitboard[i];
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
      printf(" | %d\n   ---------------------------------\n",
             2 + ((i + 1) / 8));
    }
  }
  printf("     a   b   c   d   e   f   g   h  \n\n");
  return;
}

bool chess_equal(chess *a, chess *b) {
  return (a->pawns == b->pawns) && (a->kings == b->kings) &&
         (a->diagonal_pieces == b->diagonal_pieces) &&
         (a->orthogonal_pieces == b->orthogonal_pieces) &&
         (a->enemy_pieces == b->enemy_pieces) &&
         (a->friendly_pieces == b->friendly_pieces);
}

move encode_string(char *focus_move) {
  if (focus_move[0] == 'i' && focus_move[1] == 'n' && focus_move[2] == 'i' &&
      focus_move[3] == 't' && focus_move[4] == '\0') {
    return NULL_MOVE; // rejects moves aren't long enough
  }
  if (focus_move[0] == '\0' || focus_move[1] == '\0' || focus_move[2] == '\0' ||
      focus_move[3] == '\0') {
    return NULL_MOVE; // rejects moves aren't long enough
  }
  offset starting_offset = (focus_move[0] - 'a') + 8 * (focus_move[1] - '1');
  offset ending_offset = (focus_move[2] - 'a') + 8 * (focus_move[3] - '1');
  piece promotion_piece = none;
  switch (focus_move[4]) {
  case '\0':
    promotion_piece = none;
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

piece get_piece_at_offset(chess *game, offset index) {
  piece id = 0;
  bitboard board = offset_to_bitboard[index];
  if (board & (~PAWN_MASK) & game->pawns) {
    id |= 1;
  }
  id = id << 1;
  if (board & game->kings) {
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
  case none:
    output |= 0;
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

bitboard flip_bitboard(bitboard board) {
  bitboard scan = 0xff00000000000000;
  bitboard output = 0x0;
  for (int i = 0; i < 8; i++) {
    output |= compass(compass(scan, 0, i) & board, 0, 7 - 2 * i);
  }
  return output;
}

void flip_perspective(
    chess *game) // switches perspective of game to your opponents perspective
// if you could en-passant capture, the flipped board will be able to as well
// this function as its own inverse
{
  bitboard information_row = compass(game->pawns, 0, -7) & ROW_1;
  bitboard en_passant_row = compass(game->pawns & ROW_1, 0, 7);
  game->pawns &= ~(ROW_1 | compass(ROW_1, 0, 7));
  game->pawns |= information_row | en_passant_row;
  game->pawns = flip_bitboard(game->pawns);
  game->orthogonal_pieces = flip_bitboard(game->orthogonal_pieces);
  game->diagonal_pieces = flip_bitboard(game->diagonal_pieces);
  game->friendly_pieces = flip_bitboard(game->friendly_pieces);
  game->enemy_pieces = flip_bitboard(game->enemy_pieces);
  game->kings = flip_bitboard(game->kings);
  bitboard temp = game->friendly_pieces;
  game->friendly_pieces = game->enemy_pieces;
  game->enemy_pieces = temp;
  game->pawns ^= 1;
  return;
}

offset get_starting_offset(move focus_move) { return focus_move & 63; }

offset get_ending_offset(move focus_move) { return (focus_move >> 6) & 63; }

piece get_starting_piece(chess *game, move focus_move) {
  return get_piece_at_offset(game, get_starting_offset(focus_move));
}

piece get_ending_piece(chess *game, move focus_move) {
  return get_piece_at_offset(game, get_ending_offset(focus_move));
}

piece get_promotion_piece(move focus_move) {
  switch ((focus_move >> 12) & 3) {
  case 0:
    return queen;
  case 1:
    return rook;
  case 2:
    return knight;
  case 3:
    return bishop;
  default:
    return none;
  }
}

bool is_castle(chess *game, move focus_move) {
  offset starting_offset = get_starting_offset(focus_move);
  offset ending_offset = get_ending_offset(focus_move);
  piece starting_piece = get_piece_at_offset(game, starting_offset);
  return (starting_piece == king) && (starting_offset == 4) &&
         (((game->pawns & 2) && ending_offset == 6) ||
          ((game->pawns & 4) && ending_offset == 2));
}

bool is_promotion(chess *game, move focus_move) {
  return (get_starting_piece(game, focus_move) == pawn) &&
         (get_ending_offset(focus_move) > 55);
}

bool is_en_passant(chess *game, move focus_move) {
  offset starting_offset = get_starting_offset(focus_move);
  piece starting_piece = get_piece_at_offset(game, starting_offset);
  bitboard starting_board = offset_to_bitboard[starting_offset];
  bitboard en_passant = game->pawns & ROW_1;
  return (starting_piece == pawn) && (starting_board & ROW_5) &&
         ((compass(starting_board, 1, 1) & compass(en_passant, 0, 5)) ||
          (compass(starting_board, -1, 1) & compass(en_passant, 0, 5)));
}

void castle_move(chess *game, move focus_move) {
  offset starting_offset = get_starting_offset(focus_move);
  offset ending_offset = get_ending_offset(focus_move);
  move_piece(game, starting_offset, ending_offset);
  if (ending_offset == 2) {
    move_piece(game, 0, 3);
  } else {
    move_piece(game, 7, 5);
  }
  game->pawns &= ~6;     // remove friendly castling rights
  game->pawns &= ~ROW_1; // castle_moves aren't pawn pushes
}

void en_passant_move(chess *game,
                     move focus_move) // makes en-passant move, updating
                                      // en_passant in the process
{
  offset starting_offset = get_starting_offset(focus_move);
  offset ending_offset = get_ending_offset(focus_move);
  move_piece(game, starting_offset, ending_offset);
  remove_piece(game, ending_offset - 8);
  game->pawns &= ~ROW_1; // en-passant move isn't double pawn push
}

void promotion_move(chess *game, move focus_move) {
  printf("promotion\n");
  offset starting_offset = get_starting_offset(focus_move);
  offset ending_offset = get_ending_offset(focus_move);
  piece promotion_piece = get_promotion_piece(focus_move);
  piece ending_piece = get_piece_at_offset(game, ending_offset);
  remove_piece(game, starting_offset);
  add_piece(game, promotion_piece, true, starting_offset);
  move_piece(game, starting_offset, ending_offset);
  game->pawns &= ~ROW_1; // promotions aren't double pushes
  if (ending_offset == 56) {
    game->pawns &= ~16; // capturing enemy queenside rook (maybe)
  }
  if (ending_offset == 63) {
    game->pawns &= ~8;
  }
  if (ending_piece == king) {
    game->pawns &= ~24; // removing enemy castle rights
  }
}

void standard_move(chess *game, move focus_move) {
  offset starting_offset = get_starting_offset(focus_move);
  offset ending_offset = get_ending_offset(focus_move);
  piece starting_piece = get_piece_at_offset(game, starting_offset);
  piece ending_piece = get_piece_at_offset(game, ending_offset);
  move_piece(game, get_starting_offset(focus_move),
             get_ending_offset(focus_move));
  if (ending_offset == 56) {
    game->pawns &= ~16; // capturing enemy queenside rook (maybe)
  }
  if (ending_offset == 63) {
    game->pawns &= ~8; // capturing enemy kingside rook (maybe)
  }
  if (starting_offset == 0) {
    game->pawns &= ~4; // moving friendly queenside rook (maybe)
  }
  if (ending_offset == 7) {
    game->pawns &= ~2; // moving friendly kingside rook (maybe)
  }
  if (starting_piece == king) {
    game->pawns &= ~6; // removing friendly castle rights
  }
  if (ending_piece == king) {
    game->pawns &= ~24; // removing enemy castle rights
  }
  game->pawns &= ~ROW_1;
  if ((starting_piece == pawn) && (starting_offset > 7) &&
      (starting_offset < 16) && (ending_offset > 23) && (ending_offset < 32)) {
    game->pawns |= offset_to_bitboard[starting_offset - 8];
  }
  return;
}

void bit_move(chess *game, move focus_move) {
  if ((offset_to_bitboard[get_starting_offset(focus_move)] &
       game->friendly_pieces) &&
      (!(offset_to_bitboard[get_ending_offset(focus_move)] &
         game->friendly_pieces))) {
    if (is_castle(game, focus_move)) {
      castle_move(game, focus_move);
    } else if (is_promotion(game, focus_move)) {
      promotion_move(game, focus_move);
    } else if (is_en_passant(game, focus_move)) {
      en_passant_move(game, focus_move);
    } else {
      standard_move(game, focus_move);
    }
    flip_perspective(game);
  }
}

bool is_same_rank(offset a, offset b) { return (a / 8) == (b / 8); }

bool is_same_file(offset a, offset b) { return (a % 8) == (b % 8); }

void remove_piece(chess *game, offset focus_offset) {
  bitboard focus_board = offset_to_bitboard[focus_offset];
  game->pawns &= ~(focus_board & (~PAWN_MASK));
  game->kings &= ~focus_board;
  game->friendly_pieces &= ~focus_board;
  game->enemy_pieces &= ~focus_board;
  game->orthogonal_pieces &= ~focus_board;
  game->diagonal_pieces &= ~focus_board;
  return;
}

void add_piece(chess *game, piece focus_piece, bool is_friendly,
               offset focus_offset) {
  bitboard board = offset_to_bitboard[focus_offset];
  remove_piece(game, focus_offset);
  switch (focus_piece) {
  case none:
    remove_piece(game, focus_offset);
  case pawn:
    game->pawns |= (board & (~PAWN_MASK));
    if (is_friendly) {
      game->friendly_pieces |= (board & (~PAWN_MASK));
    } else {
      game->enemy_pieces |= (board & (~PAWN_MASK));
    }
    return;
  case rook:
    game->orthogonal_pieces |= board;
    if (is_friendly) {
      game->friendly_pieces |= board;
    } else {
      game->enemy_pieces |= board;
    }
    return;
  case knight:
    return;
  case bishop:
    game->diagonal_pieces |= board;
    if (is_friendly) {
      game->friendly_pieces |= board;
    } else {
      game->enemy_pieces |= board;
    }
    return;
  case queen:
    game->orthogonal_pieces |= board;
    game->diagonal_pieces |= board;
    if (is_friendly) {
      game->friendly_pieces |= board;
    } else {
      game->enemy_pieces |= board;
    }
    return;
  case king:
    game->kings |= board;
    if (is_friendly) {
      game->friendly_pieces |= board;
    } else {
      game->enemy_pieces |= board;
    }
    return;
  default:
    return;
  }
}

void move_piece(chess *game, offset start_offset, offset end_offset) {
  piece starting_piece = get_piece_at_offset(game, start_offset);
  if (starting_piece == none) {
    printf("You can't move an empty square.\n");
    return;
  }
  bool is_friendly = game->friendly_pieces & offset_to_bitboard[start_offset];
  remove_piece(game, start_offset);
  remove_piece(game, end_offset);
  add_piece(game, starting_piece, is_friendly, end_offset);
  return;
}

offset get_next_offset(bitboard board, offset index) {
  index = 63 - index;
  do {
    index -= 1;
  } while (index != -1 && ((1ull << index) & board) == 0);
  return 63 - index;
}

move random_bit_move(chess *game) {
  bitboard start_board = game->friendly_pieces;
  bitboard end_board = ~start_board;
  offset starting_offset = -1;
  offset ending_offset = -1;
  int temp = random_in_range(0, pop_count(start_board));
  for (int i = 0; i < temp; i++) {
    starting_offset = get_next_offset(start_board, starting_offset);
  }
  temp = random_in_range(0, pop_count(end_board));
  for (int i = 0; i < temp; i++) {
    ending_offset = get_next_offset(end_board, ending_offset);
  }
  move focus_move = encode_move(starting_offset, ending_offset, none);
  if (is_promotion(game, focus_move)) {
    return focus_move | random_in_range(0, 3); // add random promotion piece
  }
  return focus_move;
}

int random_in_range(int min, int max) {
  return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

int pop_count(bitboard board) {
  int ret = 0;
  while (board != 0) {
    board &= (board - 1);
    ret++;
  }
  return ret;
}

void print_move(chess *game, move focus_move) {
  offset starting_offset = get_starting_offset(focus_move);
  offset ending_offset = get_ending_offset(focus_move);
  printf("%c", 'a' + (starting_offset % 8));
  printf("%c", '1' + (starting_offset / 8));
  printf("%c", 'a' + (ending_offset % 8));
  printf("%c", '1' + (ending_offset / 8));
  piece starting_piece = get_piece_at_offset(game, starting_offset);
  switch ((focus_move >> 12) & 3) {
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
  return;
}
