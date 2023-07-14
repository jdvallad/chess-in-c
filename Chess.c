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

bitboard offset_to_bitboard(offset input)
{
  return compass(1ull, (input % 8) - 7, (input / 8) - 7);
}

offset_set *bitboard_to_offset_set(bitboard board)
{
  return offset_set_create(board);
}

Chess *chess_create()
{
  Chess *output = (Chess *)malloc(sizeof(Chess));
  output->pawns = 0xff00000000ff00; // contains the is_color_flipped in the
                                    // first bit. (0 is false)
  output->friendly_pieces = 0xffff000000000000;
  output->enemy_pieces = 0xffff;
  output->orthogonal_pieces = 0x9100000000000091;
  output->diagonal_pieces = 0x3400000000000034;
  output->kings[0] = 196;
  output->kings[1] = 252;
  // output->is_color_flipped = false;
  //  output->legal_moves = NULL;
  output->game_over = false;
  output->is_stalemate = false;
  output->in_check = false;
  output->half_move_count = 0;
  output->move_count = 0;
  // output->game_hash_list[MAX_GAME_LENGTH] = NULL;
  return output;
}

void chess_delete(Chess **input)
{
  if (*input)
  {
    free(*input);
    *input = NULL;
  }
  return;
}

void print_bitboard(bitboard board)
{
  for (offset i = 0; i < 8; i++)
  {
    for (offset j = 7; j >= 0; j--)
    {
      printf("%lu ", 1ul & (board >> (8 * i + j)));
      if (j == 0)
      {
        break;
      }
    }
    printf("\n");
  }
  printf("\n");
  return;
}

bitboard compass(bitboard input, offset horizontal, offset vertical)
{
  input = input & VERTICAL_OFFSETS_BITBOARDS[vertical + 8] &
          HORIZONTAL_OFFSETS_BITBOARDS[horizontal + 8];
  input = vertical >= 0 ? input >> (vertical * 8) : input << -(vertical * 8);
  input = horizontal >= 0 ? input >> horizontal : input << -horizontal;
  return input;
}

void print_game_state(Chess *input)
{
  printf("\n ---------------------------------\n");
  for (int i = 56; i != -8; i++)
  {
    printf(" | ");
    piece focus_piece = get_piece_at_offset(input, i);
    switch (focus_piece >> 1)
    {
    case none:
      printf(" ");
      break;
    case rook:
      printf(focus_piece & 1 ? "R" : "r");
      break;
    case knight:
      printf(focus_piece & 1 ? "N" : "n");
      break;
    case bishop:
      printf(focus_piece & 1 ? "B" : "b");
      break;
    case queen:
      printf(focus_piece & 1 ? "Q" : "q");
      break;
    case king:
      printf(focus_piece & 1 ? "K" : "k");
      break;
    case pawn:
      printf(focus_piece & 1 ? "P" : "p");
      break;
    default:
      break;
    }
    if (i % 8 == 7)
    {
      i -= 16;
      printf(" |\n ---------------------------------\n");
    }
  }
  return;
}

piece get_piece_at_offset(Chess *game, offset index)
{
  piece id = 0;
  bitboard board = offset_to_bitboard(index);
  if (board & game->pawns)
  {
    id |= 1;
  }
  id = id << 1;
  if (board & (offset_to_bitboard((game->kings[0]) & 63) |
               offset_to_bitboard((game->kings[1]) & 63)))
  {
    id |= 1;
  }
  id = id << 1;
  if (board & game->orthogonal_pieces)
  {
    id |= 1;
  }
  id = id << 1;
  if (board & game->diagonal_pieces)
  {
    id |= 1;
  }
  id = id << 1;
  if (board & (game->friendly_pieces | game->enemy_pieces))
  {
    id |= 1;
  }
  id = id << 1;
  if (board & game->friendly_pieces)
  {
    id |= 1;
  }
  return id;
}

move encode_move(offset starting_offset, offset ending_offset,
                 piece promotion_piece)
{
  move output = 0;
  switch (promotion_piece)
  {
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
void bit_move(
    Chess *game,
    move the_move)
{ // don't eat the kings off the board, it breaks it
  offset starting_offset = the_move & 63;
  the_move = the_move >> 6;
  offset ending_offset = the_move & 63;
  the_move = the_move >> 6;
  piece promotion_piece;
  switch (the_move & 3)
  {
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
  remove_from_board(game, starting_piece, starting_offset);
  remove_from_board(game, ending_piece, ending_offset);
  printf("%d", starting_piece);
  if (((starting_piece >> 1) == pawn) && ending_offset > 55)
  {
    add_to_board(game, promotion_piece, ending_offset);
  }
  else
  {
    add_to_board(game, starting_piece, ending_offset);
  }
}

void remove_from_board(Chess *game, piece focus_piece, offset square)
{
  if (focus_piece == none)
  {
    return;
  }
  bitboard board = offset_to_bitboard(square);
  if (focus_piece & 1)
  {
    game->friendly_pieces &= ~board;
  }
  else
  {
    game->enemy_pieces &= ~board;
  }
  switch (focus_piece >> 1)
  {
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
    game->kings[1 - (focus_piece & 1)] &= (~63);
    break;
  case pawn:
    game->pawns &= ~board;
    break;
  default:
    break;
  }
}

void add_to_board(Chess *game, piece focus_piece, offset square)
{
  if (focus_piece == none)
  {
    return;
  }
  bitboard board = offset_to_bitboard(square);
  if (focus_piece & 1)
  {
    game->friendly_pieces |= board;
  }
  else
  {
    game->enemy_pieces |= board;
  }

  switch (focus_piece >> 1)
  {
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
    game->kings[1 - (focus_piece & 1)] &= (~63);
    game->kings[1 - (focus_piece & 1)] |= square;
    break;
  case pawn:
    game->pawns |= board;
    break;
  default:
    break;
  }
}
