#include "legal_moves.h"

int set_pseudo_legal_moves(chess *game, move *legal_moves) {
  int legal_moves_count = 0;
  legal_moves[legal_moves_count] = NULL_MOVE;
  legal_moves_count =
      add_single_pawn_pushes(game, legal_moves, legal_moves_count);
  legal_moves_count =
      add_double_pawn_pushes(game, legal_moves, legal_moves_count);
  legal_moves_count =
      add_right_pawn_captures(game, legal_moves, legal_moves_count);
  legal_moves_count =
      add_left_pawn_captures(game, legal_moves, legal_moves_count);

  legal_moves_count =
      add_en_passant_capture(game, legal_moves, legal_moves_count);
  legal_moves_count = add_knight_moves(game, legal_moves, legal_moves_count);

  legal_moves_count = add_diagonal_moves(game, legal_moves, legal_moves_count);
  legal_moves_count =
      add_orthogonal_moves(game, legal_moves, legal_moves_count);
  legal_moves_count = add_king_moves(game, legal_moves, legal_moves_count);
  legal_moves_count = add_castling_moves(game, legal_moves, legal_moves_count);
  return legal_moves_count;
}

int set_legal_moves(chess *game, move *legal_moves) {
  if (game->pawns & 4) {
    legal_moves[0] = NULL_MOVE;
    return 0;
  }
  set_pseudo_legal_moves(game, legal_moves);
  int num_legal_moves = trim_legal_moves(game, legal_moves);
  offset friendly_king_offset =
      get_next_offset(true_kings(game) & game->friendly_pieces, -1);
  if (attacked_offset(game, friendly_king_offset)) {
    game->pawns |= 2;
  } else {
    game->pawns &= ~2;
  }
  if (num_legal_moves == 0) {
    game->pawns |= 4;
    if (game->pawns & 2) {
    } else {
      game->pawns |= 8;
    }
  }
  return num_legal_moves;
}

bool is_legal_move(chess *game, move focus_move) {
  bitboard pawns = game->pawns;
  bitboard kings = game->kings;
  bitboard friendly_pieces = game->friendly_pieces;
  bitboard enemy_pieces = game->enemy_pieces;
  bitboard orthogonal_pieces = game->orthogonal_pieces;
  bitboard diagonal_pieces = game->diagonal_pieces;
  bit_move(game, focus_move);
  offset enemy_king_offset =
      get_next_offset(true_kings(game) & game->enemy_pieces, -1);
  bool is_legal = true;
  if (attacking_offset(game, enemy_king_offset)) {
    is_legal = false;
  }
  bitboard castle_pass_through_board =
      get_castle_pass_through_board(game, focus_move);
  for (offset pass = -1;
       (pass = get_next_offset(castle_pass_through_board, pass)) < 64;) {
    if (attacked_offset(game, pass)) {
      is_legal = false;
    }
  }
  game->pawns = pawns;
  game->kings = kings;
  game->enemy_pieces = enemy_pieces;
  game->friendly_pieces = friendly_pieces;
  game->orthogonal_pieces = orthogonal_pieces;
  game->diagonal_pieces = diagonal_pieces;
  return is_legal;
}

int trim_legal_moves(chess *game, move *legal_moves) {
  int index = 0;
  int legal_index = index;
  while (legal_moves[index] != NULL_MOVE) {
    if (is_legal_move(game, legal_moves[index])) {
      legal_moves[legal_index] = legal_moves[index];
      legal_index++;
    }
    index++;
  }
  legal_moves[legal_index] = NULL_MOVE;
  return legal_index;
}

int add_en_passant_capture(chess *game, move *legal_moves,
                           int legal_moves_count) {
  offset en_passant = get_next_offset(ROW_1 & game->pawns, -1);
  if (compass(offset_to_bitboard[en_passant], 1, 4) & game->pawns &
      game->friendly_pieces) {
    legal_moves[legal_moves_count] =
        encode_move(33 + en_passant, 40 + en_passant, none);
    legal_moves_count++;
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  if (compass(offset_to_bitboard[en_passant], -1, 4) & game->pawns &
      game->friendly_pieces) {
    legal_moves[legal_moves_count] =
        encode_move(31 + en_passant, 40 + en_passant, none);
    legal_moves_count++;
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

int add_castling_moves(chess *game, move *legal_moves, int legal_moves_count) {
  if ((game->kings & offset_to_bitboard[0]) &&
      !((game->friendly_pieces | game->enemy_pieces) &
        (offset_to_bitboard[1] | offset_to_bitboard[2] |
         offset_to_bitboard[3]))) {
    legal_moves[legal_moves_count] = encode_move(4, 2, none);
    legal_moves_count++;
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  if ((game->kings & offset_to_bitboard[7]) &&
      !((game->friendly_pieces | game->enemy_pieces) &
        (offset_to_bitboard[6] | offset_to_bitboard[5]))) {
    legal_moves[legal_moves_count] = encode_move(4, 6, none);
    legal_moves_count++;
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

int add_double_pawn_pushes(chess *game, move *legal_moves,
                           int legal_moves_count) {
  bitboard output_squares = game->friendly_pieces & true_pawns(game);
  bitboard entire_board = game->friendly_pieces | game->enemy_pieces;
  output_squares = output_squares & compass(ROW_1, 0, 1);
  output_squares = (~entire_board) & compass(output_squares, 0, 1);
  output_squares = (~entire_board) & compass(output_squares, 0, 1);
  for (offset end_index = -1;
       (end_index = get_next_offset(output_squares, end_index)) < 64;) {
    legal_moves[legal_moves_count] =
        encode_move(end_index - 16, end_index, none);
    legal_moves_count++;
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

int add_single_pawn_pushes(chess *game, move *legal_moves,
                           int legal_moves_count) {
  bitboard output_squares = game->friendly_pieces & true_pawns(game);
  bitboard entire_board = game->friendly_pieces | game->enemy_pieces;
  output_squares = (~entire_board) & compass(output_squares, 0, 1);
  for (offset end_index = -1;
       (end_index = get_next_offset(output_squares, end_index)) < 64;) {
    legal_moves[legal_moves_count] =
        encode_move(end_index - 8, end_index, none);
    legal_moves_count++;
    if (end_index > 55) {
      legal_moves[legal_moves_count] =
          encode_move(end_index - 8, end_index, rook);
      legal_moves_count++;
      legal_moves[legal_moves_count] =
          encode_move(end_index - 8, end_index, bishop);
      legal_moves_count++;
      legal_moves[legal_moves_count] =
          encode_move(end_index - 8, end_index, knight);
      legal_moves_count++;
    }
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

int add_left_pawn_captures(chess *game, move *legal_moves,
                           int legal_moves_count) {
  bitboard output_squares = game->friendly_pieces & true_pawns(game);
  output_squares = (game->enemy_pieces) & compass(output_squares, -1, 1);
  for (offset end_index = -1;
       (end_index = get_next_offset(output_squares, end_index)) < 64;) {
    legal_moves[legal_moves_count] =
        encode_move(end_index - 7, end_index, none);
    legal_moves_count++;
    if (end_index > 55) {
      legal_moves[legal_moves_count] =
          encode_move(end_index - 7, end_index, rook);
      legal_moves_count++;
      legal_moves[legal_moves_count] =
          encode_move(end_index - 7, end_index, bishop);
      legal_moves_count++;
      legal_moves[legal_moves_count] =
          encode_move(end_index - 7, end_index, knight);
      legal_moves_count++;
    }
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

int add_right_pawn_captures(chess *game, move *legal_moves,
                            int legal_moves_count) {
  bitboard output_squares = game->friendly_pieces & true_pawns(game);
  output_squares = (game->enemy_pieces) & compass(output_squares, 1, 1);
  for (offset end_index = -1;
       (end_index = get_next_offset(output_squares, end_index)) < 64;) {
    legal_moves[legal_moves_count] =
        encode_move(end_index - 9, end_index, none);
    legal_moves_count++;
    if (end_index > 55) {
      legal_moves[legal_moves_count] =
          encode_move(end_index - 9, end_index, rook);
      legal_moves_count++;
      legal_moves[legal_moves_count] =
          encode_move(end_index - 9, end_index, bishop);
      legal_moves_count++;
      encode_move(end_index - 9, end_index, knight);
      legal_moves_count++;
    }
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

bitboard get_king_destination_bitboard(chess *game, offset king_index) {
  bitboard king = offset_to_bitboard[king_index];
  bitboard output_squares = king | compass(king, 1, 0);
  output_squares |= compass(output_squares, -1, 0);
  output_squares |= compass(output_squares, 0, 1);
  output_squares |= compass(output_squares, 0, -1);
  output_squares &= ~king;
  output_squares &= (~game->friendly_pieces);
  return output_squares;
}

int add_king_moves(chess *game, move *legal_moves, int legal_moves_count) {
  bitboard king = game->friendly_pieces & true_kings(game);
  bitboard output_squares = king | compass(king, 1, 0);
  output_squares |= compass(output_squares, -1, 0);
  output_squares |= compass(output_squares, 0, 1);
  output_squares |= compass(output_squares, 0, -1);
  output_squares &= (~game->friendly_pieces);
  for (offset end_index = -1;
       (end_index = get_next_offset(output_squares, end_index)) < 64;) {
    legal_moves[legal_moves_count] =
        encode_move(get_next_offset(king, -1), end_index, none);
    legal_moves_count++;
    legal_moves[legal_moves_count] = NULL_MOVE;
  }
  return legal_moves_count;
}

int add_diagonal_moves(chess *game, move *legal_moves, int legal_moves_count) {
  bitboard diagonals = game->friendly_pieces & game->diagonal_pieces;
  for (offset start_index = -1;
       (start_index = get_next_offset(diagonals, start_index)) < 64;) {
    bitboard output_squares =
        get_diagonal_destination_bitboard(game, start_index);
    for (offset end_index = -1;
         (end_index = get_next_offset(output_squares, end_index)) < 64;) {
      legal_moves[legal_moves_count] =
          encode_move(start_index, end_index, none);
      legal_moves_count++;
      legal_moves[legal_moves_count] = NULL_MOVE;
    }
  }
  return legal_moves_count;
}

bitboard get_diagonal_destination_bitboard(chess *game, offset start_index) {
  bitboard start_board = offset_to_bitboard[start_index];
  bitboard output_board = 0x0;
  bitboard bullet = compass(start_board, -1, 1);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces)) {
    output_board |= bullet;
    bullet = compass(bullet, -1, 1);
  }
  if (bullet & game->enemy_pieces) {
    output_board |= bullet;
  }
  bullet = compass(start_board, 1, 1);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces)) {
    output_board |= bullet;
    bullet = compass(bullet, 1, 1);
  }
  if (bullet & game->enemy_pieces) {
    output_board |= bullet;
  }
  bullet = compass(start_board, 1, -1);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces)) {
    output_board |= bullet;
    bullet = compass(bullet, 1, -1);
  }
  if (bullet & game->enemy_pieces) {
    output_board |= bullet;
  }
  bullet = compass(start_board, -1, -1);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces)) {
    output_board |= bullet;
    bullet = compass(bullet, -1, -1);
  }
  if (bullet & game->enemy_pieces) {
    output_board |= bullet;
  }
  return output_board;
}

int add_orthogonal_moves(chess *game, move *legal_moves,
                         int legal_moves_count) {
  bitboard orthogonals = game->friendly_pieces & game->orthogonal_pieces;
  for (offset start_index = -1;
       (start_index = get_next_offset(orthogonals, start_index)) < 64;) {
    bitboard output_squares =
        get_orthogonal_destination_bitboard(game, start_index);
    for (offset end_index = -1;
         (end_index = get_next_offset(output_squares, end_index)) < 64;) {
      legal_moves[legal_moves_count] =
          encode_move(start_index, end_index, none);
      legal_moves_count++;
      legal_moves[legal_moves_count] = NULL_MOVE;
    }
  }
  return legal_moves_count;
}

bitboard get_orthogonal_destination_bitboard(chess *game, offset start_index) {
  bitboard start_board = offset_to_bitboard[start_index];
  bitboard output_board = 0x0;
  bitboard bullet = compass(start_board, 0, 1);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces)) {
    output_board |= bullet;
    bullet = compass(bullet, 0, 1);
  }
  if (bullet & game->enemy_pieces) {
    output_board |= bullet;
  }
  bullet = compass(start_board, 1, 0);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces)) {
    output_board |= bullet;
    bullet = compass(bullet, 1, 0);
  }
  if (bullet & game->enemy_pieces) {
    output_board |= bullet;
  }
  bullet = compass(start_board, 0, -1);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces)) {
    output_board |= bullet;
    bullet = compass(bullet, 0, -1);
  }
  if (bullet & game->enemy_pieces) {
    output_board |= bullet;
  }
  bullet = compass(start_board, -1, 0);
  while (bullet & ~(game->friendly_pieces | game->enemy_pieces)) {
    output_board |= bullet;
    bullet = compass(bullet, -1, 0);
  }
  if (bullet & game->enemy_pieces) {
    output_board |= bullet;
  }
  return output_board;
}

int add_knight_moves(chess *game, move *legal_moves, int legal_moves_count) {
  bitboard knights = game->friendly_pieces & (~true_pawns(game)) &
                     (~game->orthogonal_pieces) & ~(game->diagonal_pieces) &
                     (~true_kings(game));
  for (offset start_index = -1;
       (start_index = get_next_offset(knights, start_index)) < 64;) {
    bitboard output_squares =
        get_knight_destination_bitboard(game, start_index);
    for (offset end_index = -1;
         (end_index = get_next_offset(output_squares, end_index)) < 64;) {
      legal_moves[legal_moves_count] =
          encode_move(start_index, end_index, none);
      legal_moves_count++;
      legal_moves[legal_moves_count] = NULL_MOVE;
    }
  }
  return legal_moves_count;
}

bool in_set(move *legal_moves, move focus_move) {
  int i = 0;
  while (true) {
    if (legal_moves[i] == focus_move) {
      return true;
    }
    if (legal_moves[i] == NULL_MOVE) {
      return false;
    }
    i++;
  }
}

bool attacked_offset(chess *game, offset focus_offset) {
  bitboard attack_board = offset_to_bitboard[focus_offset];
  bitboard knights = (game->friendly_pieces | game->enemy_pieces) &
                     (~game->orthogonal_pieces) & (~game->diagonal_pieces) &
                     ~(true_pawns(game)) & ~(true_kings(game));
  bitboard knight_attackers =
      game->enemy_pieces & knights &
      get_knight_destination_bitboard(game, focus_offset);
  bitboard diagonal_attackers =
      game->enemy_pieces & game->diagonal_pieces &
      get_diagonal_destination_bitboard(game, focus_offset);
  bitboard orthogonal_attackers =
      game->enemy_pieces & game->orthogonal_pieces &
      get_orthogonal_destination_bitboard(game, focus_offset);
  bitboard king_attackers = game->enemy_pieces & true_kings(game) &
                            get_king_destination_bitboard(game, focus_offset);
  bitboard pawn_attackers =
      game->enemy_pieces & game->pawns &
      (compass(attack_board, -1, 1) | compass(attack_board, 1, 1));
  if (pawn_attackers | knight_attackers | diagonal_attackers |
      orthogonal_attackers | king_attackers) {
    return true;
  }
  return false;
}

bool attacking_offset(chess *game, offset focus_offset) {
  bitboard attack_board = offset_to_bitboard[focus_offset];
  bitboard knights = (game->friendly_pieces | game->enemy_pieces) &
                     (~game->orthogonal_pieces) & (~game->diagonal_pieces) &
                     ~(true_pawns(game)) & ~(true_kings(game));
  bitboard temp = game->friendly_pieces;
  game->friendly_pieces = game->enemy_pieces;
  game->enemy_pieces = temp;
  bitboard knight_attackers =
      game->enemy_pieces & knights &
      get_knight_destination_bitboard(game, focus_offset);
  bitboard diagonal_attackers =
      game->enemy_pieces & game->diagonal_pieces &
      get_diagonal_destination_bitboard(game, focus_offset);
  bitboard orthogonal_attackers =
      game->enemy_pieces & game->orthogonal_pieces &
      get_orthogonal_destination_bitboard(game, focus_offset);
  bitboard king_attackers = game->enemy_pieces & true_kings(game) &
                            get_king_destination_bitboard(game, focus_offset);
  temp = game->friendly_pieces;
  game->friendly_pieces = game->enemy_pieces;
  game->enemy_pieces = temp;
  bitboard pawn_attackers =
      game->friendly_pieces & game->pawns &
      (compass(attack_board, -1, -1) | compass(attack_board, 1, -1));
  if (pawn_attackers | knight_attackers | diagonal_attackers |
      orthogonal_attackers | king_attackers) {
    return true;
  }
  return false;
}

bitboard get_attacking_board(chess *game) {
  bitboard output = 0x0;
  for (offset i = 0; i < 64; i++) {
    if (attacking_offset(game, i)) {
      output |= offset_to_bitboard[i];
    }
  }
  return output;
}

bitboard get_attacked_board(chess *game) {
  bitboard output = 0x0;
  for (offset i = 0; i < 64; i++) {
    if (attacked_offset(game, i)) {
      output |= offset_to_bitboard[i];
    }
  }
  return output;
}

bitboard get_knight_destination_bitboard(chess *game, offset start_index) {
  bitboard start_board = offset_to_bitboard[start_index];
  return (~game->friendly_pieces) &
         (compass(start_board, 2, 1) | compass(start_board, 1, 2) |
          compass(start_board, 2, -1) | compass(start_board, -1, 2) |
          compass(start_board, -2, 1) | compass(start_board, 1, -2) |
          compass(start_board, -2, -1) | compass(start_board, -1, -2));
}

bool in_check(chess *game) { return game->pawns & 2; }

bitboard get_castle_pass_through_board(chess *game, move focus_move) {
  offset ending_offset = get_ending_offset(focus_move);
  piece starting_piece = get_starting_piece(game, focus_move);
  if (starting_piece == king) {
    if ((castle_rights(game) & offset_to_bitboard[0]) && ending_offset == 2) {
      return offset_to_bitboard[2] | offset_to_bitboard[3];
    }
    if ((castle_rights(game) & offset_to_bitboard[7]) && ending_offset == 6) {
      return offset_to_bitboard[6] | offset_to_bitboard[5];
    }
  }
  return 0x0;
}
