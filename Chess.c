#include "chess.h"
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

chess *chess_create()
{
  chess *output = (chess *)malloc(sizeof(chess));
  output->pawns = 0xff00000000ff00;
  output->friendly_pieces = 0xffff000000000000;
  output->enemy_pieces = 0xffff;
  output->orthogonal_pieces = 0x9100000000000091;
  output->diagonal_pieces = 0x3400000000000034;
  output->kings = 0x8900000000000089; // contains castle rights in corners
  output->pawns &=
      ~((0x0) & offset_to_bitboard(63)); // board isn't color-flipped
  output->pawns &=
      ~((0x0) & offset_to_bitboard(62));              // player to move isn't in check
  output->pawns &= ~((0x0) & offset_to_bitboard(61)); // game isn't over
  output->pawns &=
      ~compass(0xffff000000000000, 0, -1); // no en-passant is possible
  return output;
}

void reset(chess *output)
{
  output->pawns = 0xff00000000ff00;
  output->friendly_pieces = 0xffff000000000000;
  output->enemy_pieces = 0xffff;
  output->orthogonal_pieces = 0x9100000000000091;
  output->diagonal_pieces = 0x3400000000000034;
  output->kings = 0x8900000000000089; // contains castle rights in corners
  output->pawns &=
      ~((0x0) & offset_to_bitboard(63)); // board isn't color-flipped
  output->pawns &=
      ~((0x0) & offset_to_bitboard(62));              // player to move isn't in check
  output->pawns &= ~((0x0) & offset_to_bitboard(61)); // game isn't a stalemate
  output->pawns &= ~((0x0) & offset_to_bitboard(60)); // gam isn't over
  output->pawns &=
      ~compass(0xffff000000000000, 0, -1); // no enpassant is possible
  return;
}

void chess_delete(chess **input)
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

void print_game_state(chess *game)
{
  printf("\n\n     a   b   c   d   e   f   g   h  ");
  printf("\n   ---------------------------------\n");
  for (int i = 56; i != -8; i++)
  {
    if (i % 8 == 0)
    {
      printf(" %d", 1 + (i / 8));
    }
    printf(" | ");
    piece focus_piece = get_piece_at_offset(game, i);
    bitboard board = offset_to_bitboard(i);
    char upset = game->friendly_pieces & board ? 0 : 32;
    switch (focus_piece)
    {
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
    if (i % 8 == 7)
    {
      i -= 16;
      printf(" | %d\n   ---------------------------------\n",
             2 + ((i + 1) / 8));
    }
  }
  printf("     a   b   c   d   e   f   g   h  \n\n");
  return;
}

// fills up legal_moves with all possible legal moves
// returns the number of legal moves
int set_pseudo_legal_moves(chess *game, move *legal_moves, int index_offset)
{
  int legal_moves_count = index_offset;
  legal_moves[legal_moves_count] = NULL_MOVE;
  legal_moves_count =
      add_en_passant_capture(game, legal_moves, legal_moves_count);
  legal_moves_count = add_castling_moves(game, legal_moves, legal_moves_count);
  legal_moves_count =
      add_double_pawn_pushes(game, legal_moves, legal_moves_count);
  legal_moves_count =
      add_single_pawn_pushes(game, legal_moves, legal_moves_count);
  legal_moves_count =
      add_left_pawn_captures(game, legal_moves, legal_moves_count);
  legal_moves_count =
      add_right_pawn_captures(game, legal_moves, legal_moves_count);
  legal_moves_count = add_king_moves(game, legal_moves, legal_moves_count);
  legal_moves_count = add_diagonal_moves(game, legal_moves, legal_moves_count);
  legal_moves_count =
      add_orthogonal_moves(game, legal_moves, legal_moves_count);
  legal_moves_count = add_knight_moves(game, legal_moves, legal_moves_count);
  return legal_moves_count;
}

int set_legal_moves(chess *game, move *legal_moves, int index_offset)
{
  int index = 0;
  int legal_index = 0;
  while (legal_moves[index] != NULL_MOVE)
  {
    if (is_legal_move(game, legal_moves[index], legal_moves, index_offset))
    {
      legal_moves[legal_index] = legal_moves[index];
      legal_index++;
    }
    index++;
  }
  legal_moves[legal_index] = NULL_MOVE;
  return legal_index;
}

bool is_castle_move(chess *game, move the_move)
{
  offset starting_offset = the_move & 63;
  piece focus_piece = get_piece_at_offset(starting_offset);
  the_move = the_move >> 6;
  offset ending_offset = the_move & 63;
  return focus_piece == king & starting_offset == 4 && (ending_offset == 1 || ending_offset == 6);
}

bool is_legal_move(chess *game, move focus_move, move *legal_moves,
                   int index_offset)
{
  bitboard pawns = game->pawns;
  bitboard kings = game->kings;
  bitboard friendly_pieces = game->friendly_pieces;
  bitboard enemy_pieces = game->enemy_pieces;
  bitboard diagonal_pieces = game->diagonal_pieces;
  bitboard orthogonal_pieces = game->orthogonal_pieces;
  offset starting_offset = focus_move & 63;
  piece focus_piece = get_piece_at_offset(starting_offset);
  offset ending_offset = (focus_move >> 6) & 63;
  if (is_castle_move(game,focus_move))
  {
     piece promotion_piece;
  switch ((focus_move >> 12) & 3)
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
    return is_legal_move(game,encode_move(starting_offset,ending_offset+1,promotion_piece)) || ...
    // need to finish here!!
  }
  else
  {
    bit_move(game, focus_move);
    set_pseudo_legal_moves(game, legal_moves, index_offset + 1);
    int index = index_offset + 1;
    while (legal_moves[index] != NULL_MOVE)
    {
      if (offset_to_bitboard((legal_moves[index] >> 6) & 63) & true_kings(game) &
          game->enemy_pieces)
      {
        game->pawns = pawns;
        game->kings = kings;
        game->friendly_pieces = friendly_pieces;
        game->enemy_pieces = enemy_pieces;
        game->diagonal_pieces = diagonal_pieces;
        game->orthogonal_pieces = orthogonal_pieces;
        return false;
      }
      index++;
    }
    game->pawns = pawns;
    game->kings = kings;
    game->friendly_pieces = friendly_pieces;
    game->enemy_pieces = enemy_pieces;
    game->diagonal_pieces = diagonal_pieces;
    game->orthogonal_pieces = orthogonal_pieces;
    return true;
  }
}

int add_en_passant_capture(chess *game, move *legal_moves,
                           int legal_moves_count)
{
  offset en_passant = get_next_offset(ROW_1 & game->pawns, -1);
  if (offset_to_bitboard(33 + en_passant) & game->pawns &
      game->friendly_pieces)
  {
    legal_moves[legal_moves_count] =
        encode_move(33 + en_passant, 40 + en_passant, none);
    legal_moves_count++;
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  if (offset_to_bitboard(31 + en_passant) & game->pawns &
      game->friendly_pieces)
  {
    legal_moves[legal_moves_count] =
        encode_move(31 + en_passant, 40 + en_passant, none);
    legal_moves_count++;
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

int add_castling_moves(chess *game, move *legal_moves, int legal_moves_count)
{
  if ((game->kings & offset_to_bitboard(0)) &&
      !((game->friendly_pieces | game->enemy_pieces) &
        (offset_to_bitboard(1) | offset_to_bitboard(2) |
         offset_to_bitboard(3))))
  {
    legal_moves[legal_moves_count] = encode_move(4, 2, none);
    legal_moves_count++;
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  if ((game->kings & offset_to_bitboard(7)) &&
      !((game->friendly_pieces | game->enemy_pieces) &
        (offset_to_bitboard(6) | offset_to_bitboard(5))))
  {
    legal_moves[legal_moves_count] = encode_move(4, 6, none);
    legal_moves_count++;
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

int add_double_pawn_pushes(chess *game, move *legal_moves,
                           int legal_moves_count)
{
  bitboard output_squares = game->friendly_pieces & true_pawns(game);
  bitboard entire_board = game->friendly_pieces | game->enemy_pieces;
  output_squares = output_squares & compass(ROW_1, 0, 1);
  output_squares = (~entire_board) & compass(output_squares, 0, 1);
  output_squares = (~entire_board) & compass(output_squares, 0, 1);
  for (offset end_index = -1;
       (end_index = get_next_offset(output_squares, end_index)) < 64;)
  {
    legal_moves[legal_moves_count] =
        encode_move(end_index - 16, end_index, none);
    legal_moves_count++;
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

int add_single_pawn_pushes(chess *game, move *legal_moves,
                           int legal_moves_count)
{
  bitboard output_squares = game->friendly_pieces & true_pawns(game);
  bitboard entire_board = game->friendly_pieces | game->enemy_pieces;
  output_squares = (~entire_board) & compass(output_squares, 0, 1);
  for (offset end_index = -1;
       (end_index = get_next_offset(output_squares, end_index)) < 64;)
  {
    legal_moves[legal_moves_count] =
        encode_move(end_index - 8, end_index, none);
    legal_moves_count++;
    if (end_index > 55)
    {
      legal_moves[legal_moves_count] =
          encode_move(end_index - 8, end_index, rook);
      legal_moves_count++;
      encode_move(end_index - 8, end_index, bishop);
      legal_moves_count++;
      encode_move(end_index - 8, end_index, knight);
      legal_moves_count++;
    }
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

int add_left_pawn_captures(chess *game, move *legal_moves,
                           int legal_moves_count)
{
  bitboard output_squares = game->friendly_pieces & true_pawns(game);
  output_squares = (game->enemy_pieces) & compass(output_squares, -1, 1);
  for (offset end_index = -1;
       (end_index = get_next_offset(output_squares, end_index)) < 64;)
  {
    legal_moves[legal_moves_count] =
        encode_move(end_index - 7, end_index, none);
    legal_moves_count++;
    if (end_index > 55)
    {
      legal_moves[legal_moves_count] =
          encode_move(end_index - 7, end_index, rook);
      legal_moves_count++;
      encode_move(end_index - 7, end_index, bishop);
      legal_moves_count++;
      encode_move(end_index - 7, end_index, knight);
      legal_moves_count++;
    }
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

int add_right_pawn_captures(chess *game, move *legal_moves,
                            int legal_moves_count)
{
  bitboard output_squares = game->friendly_pieces & true_pawns(game);
  output_squares = (game->enemy_pieces) & compass(output_squares, 1, 1);
  for (offset end_index = -1;
       (end_index = get_next_offset(output_squares, end_index)) < 64;)
  {
    legal_moves[legal_moves_count] =
        encode_move(end_index - 9, end_index, none);
    legal_moves_count++;
    if (end_index > 55)
    {
      legal_moves[legal_moves_count] =
          encode_move(end_index - 9, end_index, rook);
      legal_moves_count++;
      encode_move(end_index - 9, end_index, bishop);
      legal_moves_count++;
      encode_move(end_index - 9, end_index, knight);
      legal_moves_count++;
    }
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

int add_king_moves(chess *game, move *legal_moves, int legal_moves_count)
{
  bitboard king = game->friendly_pieces & true_kings(game);
  bitboard output_squares = compass(king, 1, 0);
  output_squares |= compass(output_squares, -1, 0);
  output_squares |= compass(output_squares, 0, 1);
  output_squares |= compass(output_squares, 0, -1);
  output_squares &= (~game->friendly_pieces);
  for (offset end_index = -1;
       (end_index = get_next_offset(output_squares, end_index)) < 64;)
  {
    legal_moves[legal_moves_count] =
        encode_move(get_next_offset(king, -1), end_index, none);
    legal_moves_count++;
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

int add_diagonal_moves(chess *game, move *legal_moves, int legal_moves_count)
{
  bitboard diagonals = game->friendly_pieces & game->diagonal_pieces;
  for (offset start_index = -1;
       (start_index = get_next_offset(diagonals, start_index)) < 64;)
  {
    bitboard output_squares =
        get_diagonal_destination_bitboard(game, start_index);
    for (offset end_index = -1;
         (end_index = get_next_offset(output_squares, end_index)) < 64;)
    {
      legal_moves[legal_moves_count] =
          encode_move(start_index, end_index, none);
      legal_moves_count++;
      legal_moves[legal_moves_count] = NULL_MOVE;
    }
  }
  return legal_moves_count;
}

bitboard get_diagonal_destination_bitboard(chess *game, offset start_index)
{
  bitboard start_board = offset_to_bitboard(start_index);
  bitboard output_board = 0x0;
  bitboard bullet = compass(start_board, -1, 1);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces))
  {
    output_board |= bullet;
    bullet = compass(bullet, -1, 1);
  }
  if (bullet & game->enemy_pieces)
  {
    output_board |= bullet;
  }
  bullet = compass(start_board, 1, 1);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces))
  {
    output_board |= bullet;
    bullet = compass(bullet, 1, 1);
  }
  if (bullet & game->enemy_pieces)
  {
    output_board |= bullet;
  }
  bullet = compass(start_board, 1, -1);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces))
  {
    output_board |= bullet;
    bullet = compass(bullet, 1, -1);
  }
  if (bullet & game->enemy_pieces)
  {
    output_board |= bullet;
  }
  bullet = compass(start_board, -1, -1);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces))
  {
    output_board |= bullet;
    bullet = compass(bullet, -1, -1);
  }
  if (bullet & game->enemy_pieces)
  {
    output_board |= bullet;
  }
  return output_board;
}

int add_orthogonal_moves(chess *game, move *legal_moves,
                         int legal_moves_count)
{
  bitboard orthogonals = game->friendly_pieces & game->diagonal_pieces;
  for (offset start_index = -1;
       (start_index = get_next_offset(orthogonals, start_index)) < 64;)
  {
    bitboard output_squares =
        get_orthogonal_destination_bitboard(game, start_index);
    for (offset end_index = -1;
         (end_index = get_next_offset(output_squares, end_index)) < 64;)
    {
      legal_moves[legal_moves_count] =
          encode_move(start_index, end_index, none);
      legal_moves_count++;
      legal_moves[legal_moves_count] = NULL_MOVE;
    }
  }
  return legal_moves_count;
}

bitboard get_orthogonal_destination_bitboard(chess *game, offset start_index)
{
  bitboard start_board = offset_to_bitboard(start_index);
  bitboard output_board = 0x0;
  bitboard bullet = compass(start_board, 0, 1);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces))
  {
    output_board |= bullet;
    bullet = compass(bullet, 0, 1);
  }
  if (bullet & game->enemy_pieces)
  {
    output_board |= bullet;
  }
  bullet = compass(start_board, 1, 0);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces))
  {
    output_board |= bullet;
    bullet = compass(bullet, 1, 0);
  }
  if (bullet & game->enemy_pieces)
  {
    output_board |= bullet;
  }
  bullet = compass(start_board, 0, -1);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces))
  {
    output_board |= bullet;
    bullet = compass(bullet, 0, -1);
  }
  if (bullet & game->enemy_pieces)
  {
    output_board |= bullet;
  }
  bullet = compass(start_board, -1, 0);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces))
  {
    output_board |= bullet;
    bullet = compass(bullet, -1, 0);
  }
  if (bullet & game->enemy_pieces)
  {
    output_board |= bullet;
  }
  return output_board;
}

int add_knight_moves(chess *game, move *legal_moves, int legal_moves_count)
{
  bitboard knights = game->friendly_pieces & (~true_pawns(game)) &
                     (~game->orthogonal_pieces) & ~(game->diagonal_pieces) &
                     (~true_kings(game));
  for (offset start_index = -1;
       (start_index = get_next_offset(knights, start_index)) < 64;)
  {
    bitboard output_squares =
        get_knight_destination_bitboard(game, start_index);
    for (offset end_index = -1;
         (end_index = get_next_offset(output_squares, end_index)) < 64;)
    {
      legal_moves[legal_moves_count] =
          encode_move(start_index, end_index, none);
      legal_moves_count++;
      legal_moves[legal_moves_count] = NULL_MOVE;
    }
  }
  return legal_moves_count;
}

bitboard get_knight_destination_bitboard(chess *game, offset start_index)
{
  bitboard start_board = offset_to_bitboard(start_index);
  return (~game->friendly_pieces) &
         (compass(start_board, 2, 1) | compass(start_board, 1, 2) |
          compass(start_board, 2, -1) | compass(start_board, -1, 2) |
          compass(start_board, -2, 1) | compass(start_board, 1, -2) |
          compass(start_board, -2, -1) | compass(start_board, -1, -2));
}

move encode_string_move(char *focus_move)
{
  offset starting_offset = (focus_move[0] - 'a') + 8 * (focus_move[1] - '1');
  offset ending_offset = (focus_move[2] - 'a') + 8 * (focus_move[3] - '1');
  piece promotion_piece = none;
  switch (focus_move[4])
  {
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

void print_offset_table()
{
  printf("\n -----------------------------------------\n");
  for (int i = 56; i != -8; i++)
  {
    printf(" | ");
    if (i < 10)
    {
      printf(" ");
    }
    printf("%d", i);

    if (i % 8 == 7)
    {
      i -= 16;
      printf(" |\n -----------------------------------------\n");
    }
  }
  return;
}

bitboard castle_rights(chess *game)
{
  return game->kings & (compass(game->kings & 0x800000000000008, -4, 0) |
                        compass(game->kings & 0x800000000000008, 3, 0));
}

bitboard true_pawns(chess *game) { return game->pawns & 0xffffffffffff00; }
bitboard true_kings(chess *game) { return game->kings & ~castle_rights(game); }
piece get_piece_at_offset(chess *game, offset index)
{
  piece id = 0;
  bitboard board = offset_to_bitboard(index);
  if (board & true_pawns(game))
  {
    id |= 1;
  }
  id = id << 1;
  if (board & true_kings(game))
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

void display_move(char *focus_move, chess *game, move *legal_moves,
                  int *game_length, chess *past_boards)
{
  move real_move = encode_string_move(focus_move);
  if (true || in_set(legal_moves, real_move))
  {
    bit_move(game, real_move);
    *game_length += 1;
    set_from_source(&past_boards[*game_length], game);
    int pseudo_length = set_pseudo_legal_moves(game, legal_moves, 0);
    set_legal_moves(game, legal_moves, pseudo_length);
    print_game_state(game);
    print_legal_moves(game, legal_moves, 0);
  }
  else
  {
    printf("not a legal move!\n");
  }
  return;
}

void undo_move(chess *game, move *legal_moves, int *game_length,
               chess *past_boards)
{
  if (*game_length == 0)
  {
    return;
  }
  *game_length -= 1;
  set_from_source(game, &past_boards[*game_length]);
  int pseudo_length = set_pseudo_legal_moves(game, legal_moves, 0);
  set_legal_moves(game, legal_moves, pseudo_length);
  print_game_state(game);
  print_legal_moves(game, legal_moves, 0);
}

void initialize_game(chess *game, move *legal_moves, chess *past_boards)
{
  reset(game);
  set_from_source(&past_boards[0], game);
  int pseudo_length = set_pseudo_legal_moves(game, legal_moves, 0);
  set_legal_moves(game, legal_moves, pseudo_length);
  print_game_state(game);
  print_legal_moves(game, legal_moves, 0);
}

chess *clone(chess *game)
{
  chess *output = chess_create();
  output->pawns = game->pawns;
  output->friendly_pieces = game->friendly_pieces;
  output->enemy_pieces = game->enemy_pieces;
  output->orthogonal_pieces = game->orthogonal_pieces;
  output->diagonal_pieces = game->diagonal_pieces;
  output->kings = game->kings;
  return output;
}

void set_from_source(chess *dest, chess *source)
{
  dest->pawns = source->pawns;
  dest->friendly_pieces = source->friendly_pieces;
  dest->enemy_pieces = source->enemy_pieces;
  dest->orthogonal_pieces = source->orthogonal_pieces;
  dest->diagonal_pieces = source->diagonal_pieces;
  dest->kings = source->kings;
  return;
}

bitboard flip_bitboard(bitboard board)
{
  bitboard scan = 0xff00000000000000;
  bitboard output = 0x0;
  for (int i = 0; i < 8; i++)
  {
    output |= compass(compass(scan, 0, i) & board, 0, 7 - 2 * i);
  }
  return output;
}

void flip_perspective(chess *game)
{
  game->pawns = flip_bitboard(game->pawns);
  game->orthogonal_pieces = flip_bitboard(game->orthogonal_pieces);
  game->diagonal_pieces = flip_bitboard(game->diagonal_pieces);
  game->friendly_pieces = flip_bitboard(game->friendly_pieces);
  game->enemy_pieces = flip_bitboard(game->enemy_pieces);
  game->kings = flip_bitboard(game->kings);
  bitboard temp = game->friendly_pieces;
  game->friendly_pieces = game->enemy_pieces;
  game->enemy_pieces = temp;
  return;
}

void make_move(chess *game, char *focus_move)
{
  bit_move(game, encode_string_move(focus_move));
  return;
}

bool in_set(move *legal_moves, move focus_move)
{
  int i = 0;
  while (true)
  {
    if (legal_moves[i] == focus_move)
    {
      return true;
    }
    if (legal_moves[i] == NULL_MOVE)
    {
      return false;
    }
    i++;
  }
}

void print_legal_moves(chess *game, move *legal_moves, int index_offset)
{
  printf("{");
  for (int i = index_offset; legal_moves[i] != NULL_MOVE; i++)
  {
    if (i > index_offset)
    {
      printf(",");
    }
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
    switch (the_move & 3)
    {
    case 0:
      if ((starting_piece == pawn) && ending_offset > 55)
      {
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
  printf("}\n");
  return;
}

void bit_move(
    chess *game,
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
  if (starting_piece == king)
  {
    if ((castle_rights(game) & 0x1) && ending_offset == 2)
    {
      remove_from_board(game, rook, 0);
      remove_from_board(game, get_piece_at_offset(game, 3), 3);
      add_to_board(game, rook, 3);
    }
    if ((castle_rights(game) & offset_to_bitboard(7)) && ending_offset == 6)
    {
      remove_from_board(game, rook, 7);
      remove_from_board(game, get_piece_at_offset(game, 5), 5);
      add_to_board(game, rook, 5);
    }
    game->kings &= ~offset_to_bitboard(0);
    game->kings &= ~offset_to_bitboard(7);
  }
  if (ending_piece == king)
  {
    game->kings &= ~offset_to_bitboard(56);
    game->kings &= ~offset_to_bitboard(63);
  }
  if (starting_offset == 0 | starting_offset == 7)
  {
    game->kings &= ~offset_to_bitboard(starting_offset);
  }
  if (ending_offset == 56 | ending_offset == 63)
  {
    game->kings &= ~offset_to_bitboard(ending_offset);
  }
  remove_from_board(game, starting_piece, starting_offset);
  remove_from_board(game, ending_piece, ending_offset);
  if ((starting_piece == pawn) && starting_offset > 31 &&
      starting_offset < 40 &&
      ((ending_offset - starting_offset == 7) ||
       (ending_offset - starting_offset == 9)) &&
      (compass(game->pawns, 0, 5) & offset_to_bitboard(ending_offset)))
  {
    remove_from_board(game, get_piece_at_offset(game, ending_offset - 8),
                      ending_offset - 8);
  }
  if ((starting_piece == pawn) && ending_offset > 55)
  {
    add_to_board(game, promotion_piece, ending_offset);
  }
  else
  {
    add_to_board(game, starting_piece, ending_offset);
  }
  game->pawns &= (0xffffffffffffff);
  game->pawns |= compass((game->pawns & 0xff), 0, -7);
  game->pawns &= ~0xff;
  if ((starting_piece == pawn) && starting_offset > 7 && starting_offset < 16 &&
      ending_offset > 23 && ending_offset < 32)
  {
    game->pawns |= offset_to_bitboard(starting_offset + 48);
  }
  flip_perspective(game);
  game->pawns ^= 1;
}

bool is_same_rank(offset a, offset b) { return (a / 8) == (b / 8); }

bool is_same_file(offset a, offset b) { return (a % 8) == (b % 8); }

void remove_from_board(chess *game, piece focus_piece, offset square)
{
  if (focus_piece == none)
  {
    return;
  }
  bitboard board = offset_to_bitboard(square);
  if (game->friendly_pieces & board)
  {
    game->friendly_pieces &= ~board;
  }
  else
  {
    game->enemy_pieces &= ~board;
  }
  switch (focus_piece)
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
    game->kings &= ~board;
    break;
  case pawn:
    game->pawns &= ~board;
    break;
  default:
    break;
  }
}

void add_to_board(chess *game, piece focus_piece, offset square)
{
  if (focus_piece == none)
  {
    return;
  }
  bitboard board = offset_to_bitboard(square);
  game->friendly_pieces |= board;
  switch (focus_piece)
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
    game->kings |= board;
    break;
  case pawn:
    game->pawns |= board;
    break;
  default:
    break;
  }
}

offset get_next_offset(bitboard board, offset index)
{
  index = 63 - index;
  do
  {
    index -= 1;
  } while (index != -1 && ((1ull << index) & board) == 0);
  return 63 - index;
}
