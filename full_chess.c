#include "full_chess.h"
#include "chess.h"
#include "primitives.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

full_chess *full_chess_create() {
  full_chess *game = (full_chess *)malloc(sizeof(full_chess));
  game->move_count = 0;
  game->half_move_count = 0;
  game->board_history[0].pawns = 0xff00000000ff00;
  game->board_history[0].friendly_pieces = 0xffff000000000000;
  game->board_history[0].enemy_pieces = 0xffff;
  game->board_history[0].orthogonal_pieces = 0x9100000000000091;
  game->board_history[0].diagonal_pieces = 0x3400000000000034;
  game->board_history[0].kings =
      0x8900000000000089; // contains castle rights in corners
  game->board_history[0].pawns &=
      ~((0x0) & offset_to_bitboard[63]); // board isn't color-flipped
  game->board_history[0].pawns &=
      ~((0x0) & offset_to_bitboard[62]); // player to move isn't in check
  game->board_history[0].pawns &=
      ~((0x0) & offset_to_bitboard[61]); // game isn't over
  game->board_history[0].pawns &=
      ~compass(0xffff000000000000, 0, -1); // no en-passant is possible
  game->current_board = &(game->board_history[0]);
  game->current_legal_moves = (game->legal_moves_history[game->move_count]);
  set_legal_moves(game->current_board, game->current_legal_moves);
  game->is_static = false;
  return game;
}

void empty(full_chess *game) {
  game->move_count = 0;
  game->half_move_count = 0;
  game->board_history[0].pawns = 0;
  game->board_history[0].friendly_pieces = 0;
  game->board_history[0].enemy_pieces = 0;
  game->board_history[0].orthogonal_pieces = 0;
  game->board_history[0].diagonal_pieces = 0;
  game->board_history[0].kings = 0;
  game->current_board = game->board_history;
  game->current_legal_moves = (game->legal_moves_history[game->move_count]);
  game->current_legal_moves[0] = NULL_MOVE;
}

void set_from_fen(full_chess *game, char *fen) {
  empty(game);
  int str_index = 0;
  char focus_char = fen[str_index];
  bool is_numeric;
  int board_index;
  board_index = 56;
  game->move_count = 0;
  while (focus_char != ' ') {
    is_numeric = (focus_char >= '0') && (focus_char <= '9');
    bool is_friendly = (focus_char >= 'A' && focus_char <= 'Z');
    if (is_numeric) {
      for (int i = 0; i < focus_char - '0'; i++) {
        remove_from_board(game->current_board, none, board_index);
        board_index++;
        if (board_index % 8 == 0) {
          board_index -= 16;
        }
      }
    } else {
      focus_char = is_friendly ? focus_char : focus_char + ('A' - 'a');
      piece focus_piece;
      switch (focus_char) {
      case 'K':
        focus_piece = king;
        add_to_board(game->current_board, focus_piece, board_index,
                     is_friendly);
        board_index++;
        if (board_index % 8 == 0) {
          board_index -= 16;
        }
        break;
      case 'Q':
        focus_piece = queen;
        add_to_board(game->current_board, focus_piece, board_index,
                     is_friendly);
        board_index++;
        if (board_index % 8 == 0) {
          board_index -= 16;
        }
        break;
      case 'R':
        focus_piece = rook;
        add_to_board(game->current_board, focus_piece, board_index,
                     is_friendly);
        board_index++;
        if (board_index % 8 == 0) {
          board_index -= 16;
        }
        break;
      case 'B':
        focus_piece = bishop;
        add_to_board(game->current_board, focus_piece, board_index,
                     is_friendly);
        board_index++;
        if (board_index % 8 == 0) {
          board_index -= 16;
        }
        break;
      case 'N':
        focus_piece = knight;
        add_to_board(game->current_board, focus_piece, board_index,
                     is_friendly);
        board_index++;
        if (board_index % 8 == 0) {
          board_index -= 16;
        }
        break;
      case 'P':
        focus_piece = pawn;
        add_to_board(game->current_board, focus_piece, board_index,
                     is_friendly);
        board_index++;
        if (board_index % 8 == 0) {
          board_index -= 16;
        }
        break;
      case '/':
        break;
      default:
        break;
      }
    }
    str_index += 1;
    focus_char = fen[str_index];
  }
  str_index++;
  focus_char = fen[str_index];
  if (focus_char == 'b') {
    game->current_board->pawns |= 1;
  }
  str_index += 2;
  focus_char = fen[str_index];
  if (focus_char != '-') {
    while (focus_char != ' ') {
      switch (focus_char) {
      case 'K':
        game->current_board->kings |= offset_to_bitboard[7];
        break;
      case 'Q':
        game->current_board->kings |= offset_to_bitboard[0];
        break;
      case 'k':
        game->current_board->kings |= offset_to_bitboard[63];
        break;
      case 'q':
        game->current_board->kings |= offset_to_bitboard[56];
        break;
      default:
        break;
      }
      str_index++;
      focus_char = fen[str_index];
    }
  } else {
    str_index++;
    focus_char = fen[str_index];
  }
  str_index++;
  focus_char = fen[str_index];
  if (focus_char != '-') {
    int en_passant = focus_char - 'a';
    str_index++;
    focus_char = fen[str_index];
    en_passant += 8 * (focus_char - '1');
    str_index += 2;
    focus_char = fen[str_index];
    game->current_board->pawns |= offset_to_bitboard[en_passant];
  } else {
    str_index += 2;
    focus_char = fen[str_index];
  }
  int count = 0;
  while (focus_char != ' ') {
    count *= 10;
    count += focus_char - '0';
    str_index++;
    focus_char = fen[str_index];
  }
  game->half_move_count += count;
  count = 0;
  str_index++;
  focus_char = fen[str_index];
  while (focus_char != ' ' && focus_char != '\0') {
    count *= 10;
    count += focus_char - '0';
    str_index++;
    focus_char = fen[str_index];
  }
  game->move_count += (game->current_board->pawns & 1) + 2 * (count - 1);
  set_legal_moves(game->current_board, game->current_legal_moves);
  return;
}

char *get_fen(full_chess *game, char *fen) {
  int str_index = 0;
  int empty_count = 0;
  char temp_char;
  bitboard castle = castle_rights(game->current_board);
  for (offset i = 56; i != -8; i++) {
    temp_char = get_piece_char_at_offset(game, i);
    if (temp_char == ' ') {
      empty_count++;
    } else {
      if (empty_count > 0) {
        fen[str_index] = '0' + empty_count;
        str_index++;
        empty_count = 0;
      }
      fen[str_index] = temp_char;
      str_index++;
    }
    if (i % 8 == 7) {
      i -= 16;
      if (empty_count > 0) {
        fen[str_index] = '0' + empty_count;
        str_index++;
        empty_count = 0;
      }
      fen[str_index] = '/';
      str_index++;
    }
  }
  fen[str_index - 1] = ' ';
  fen[str_index] = game->current_board->pawns & 1 ? 'b' : 'w';
  str_index++;
  fen[str_index] = ' ';
  str_index++;
  bool any_castle = false;
  if (castle & offset_to_bitboard[7]) {
    fen[str_index] = 'K';
    str_index++;
    any_castle = true;
  }
  if (castle & offset_to_bitboard[0]) {
    fen[str_index] = 'Q';
    str_index++;
    any_castle = true;
  }
  if (castle & offset_to_bitboard[63]) {
    fen[str_index] = 'k';
    str_index++;
    any_castle = true;
  }
  if (castle & offset_to_bitboard[56]) {
    fen[str_index] = 'q';
    str_index++;
    any_castle = true;
  }
  if (!any_castle) {
    fen[str_index] = '-';
    str_index++;
  }
  fen[str_index] = ' ';
  str_index++;
  bitboard en_passant = game->current_board->pawns & ROW_1;
  offset en_passant_offset =
      get_next_offset(game->current_board->pawns & ROW_1, -1);
  if (en_passant) {
    fen[str_index] = 'a' + en_passant_offset % 8;
    str_index++;
    fen[str_index] = '1' + (en_passant_offset / 8);
    str_index++;
  } else {
    fen[str_index] = '-';
    str_index++;
  }
  fen[str_index] = ' ';
  str_index++;
  if (game->half_move_count < 10) {
    fen[str_index] = '0' + game->half_move_count;
    str_index++;
  } else {
    fen[str_index] = '0' + (game->half_move_count / 10);
    str_index++;
    fen[str_index] = '0' + (game->half_move_count % 10);
    str_index++;
  }
  fen[str_index] = ' ';
  str_index++;

  int full_move_count =
      1 + ((game->move_count - (game->current_board->pawns & 1)) / 2);
  if (full_move_count < 10) {
    fen[str_index] = '0' + full_move_count;
    str_index++;
  } else if (full_move_count < 100) {
    fen[str_index] = '0' + (full_move_count / 10);
    str_index++;
    fen[str_index] = '0' + (full_move_count % 10);
    str_index++;
  } else {
    fen[str_index] = '0' + (full_move_count / 100);
    str_index++;
    fen[str_index] =
        '0' + ((full_move_count - 100 * (full_move_count / 100)) / 10);
    str_index++;
    fen[str_index] = '0' + (full_move_count % 10);
    str_index++;
  }
  fen[str_index] = '\0';
  return fen;
}

void print_fen(full_chess *game) {
  int str_index = 0;
  int empty_count = 0;
  char temp_char;
  bitboard castle = castle_rights(game->current_board);
  for (offset i = 56; i != -8; i++) {
    temp_char = get_piece_char_at_offset(game, i);
    if (temp_char == ' ') {
      empty_count++;
    } else {
      if (empty_count > 0) {
        printf("%c", '0' + empty_count);
        empty_count = 0;
      }
      printf("%c", temp_char);
    }
    if (i % 8 == 7) {
      i -= 16;
      if (empty_count > 0) {
        printf("%c", '0' + empty_count);
        empty_count = 0;
      }
      {
        if (i != -9)
          printf("%c", '/');
      }
    }
  }
  printf("%c", ' ');
  printf("%c", game->current_board->pawns & 1 ? 'b' : 'w');
  printf("%c", ' ');
  bool any_castle = false;
  if (castle & offset_to_bitboard[7]) {
    printf("%c", 'K');
    any_castle = true;
  }
  if (castle & offset_to_bitboard[0]) {
    printf("%c", 'Q');
    any_castle = true;
  }
  if (castle & offset_to_bitboard[63]) {
    printf("%c", 'k');
    any_castle = true;
  }
  if (castle & offset_to_bitboard[56]) {
    printf("%c", 'q');
    any_castle = true;
  }
  if (!any_castle) {
    printf("%c", '-');
  }
  printf("%c", ' ');
  bitboard en_passant = game->current_board->pawns & ROW_1;
  offset en_passant_offset =
      get_next_offset(game->current_board->pawns & ROW_1, -1);
  if (en_passant) {
    printf("%c", 'a' + en_passant_offset % 8);
    printf("%c", '1' + (en_passant_offset / 8));
  } else {
    printf("%c", '-');
  }
  printf("%c", ' ');
  str_index++;
  if (game->half_move_count < 10) {
    printf("%c", '0' + game->half_move_count);
  } else {
    printf("%c", '0' + (game->half_move_count / 10));
    printf("%c", '0' + (game->half_move_count % 10));
  }
  printf("%c", ' ');
  int full_move_count = 1 + (game->move_count / 2);
  if (full_move_count < 10) {
    printf("%c", '0' + full_move_count);
  } else if (full_move_count < 100) {
    printf("%c", '0' + (full_move_count / 10));
    printf("%c", '0' + (full_move_count % 10));
  } else {
    printf("%c", '0' + (full_move_count / 100));
    printf("%c",
           '0' + ((full_move_count - 100 * (full_move_count / 100)) / 10));
    printf("%c", '0' + (full_move_count % 10));
  }
}

char get_piece_char_at_offset(full_chess *game, offset index) {
  piece focus_piece = get_piece_at_offset(game->current_board, index);
  bool is_friendly =
      offset_to_bitboard[index] & game->current_board->friendly_pieces;
  switch (focus_piece) {
  case none:
    return ' ';
  case queen:
    return is_friendly ? 'Q' : 'q';
  case rook:
    return is_friendly ? 'R' : 'r';
  case bishop:
    return is_friendly ? 'B' : 'b';
  case knight:
    return is_friendly ? 'N' : 'n';
  case pawn:
    return is_friendly ? 'P' : 'p';
  case king:
    return is_friendly ? 'K' : 'k';
  default:
    return '?';
  }
}

void full_chess_initialize(full_chess *game) {
  game->move_count = 0;
  game->half_move_count = 0;
  game->board_history[0].pawns = 0xff00000000ff00;
  game->board_history[0].friendly_pieces = 0xffff000000000000;
  game->board_history[0].enemy_pieces = 0xffff;
  game->board_history[0].orthogonal_pieces = 0x9100000000000091;
  game->board_history[0].diagonal_pieces = 0x3400000000000034;
  game->board_history[0].kings =
      0x8900000000000089; // contains castle rights in corners
  game->board_history[0].pawns &=
      ~((0x0) & offset_to_bitboard[63]); // board isn't color-flipped
  game->board_history[0].pawns &=
      ~((0x0) & offset_to_bitboard[62]); // player to move isn't in check
  game->board_history[0].pawns &=
      ~((0x0) & offset_to_bitboard[61]); // game isn't over
  game->board_history[0].pawns &=
      ~compass(0xffff000000000000, 0, -1); // no en-passant is possible
  game->current_board = &(game->board_history[0]);
  game->current_legal_moves = (game->legal_moves_history[game->move_count]);
  set_legal_moves(game->current_board, game->current_legal_moves);
  return;
}

void reset(full_chess *game) {
  game->move_count = 0;
  game->half_move_count = 0;
  game->current_board = &(game->board_history[game->move_count]);
  game->current_legal_moves = (game->legal_moves_history[game->move_count]);
  return;
}

void full_chess_delete(full_chess **input) {
  if (*input) {
    free(*input);
    *input = NULL;
  }
  return;
}

int perft(full_chess *game, int depth, bool top_call) {
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
        print_move(game, game->current_legal_moves[index]);
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
      print_move(game, game->current_legal_moves[index]);
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

void make_move(full_chess *game, move real_move) {
  if (real_move == NULL_MOVE) {
    return;
  }
  if (in_set(game->current_legal_moves, real_move)) {
    if ((get_starting_piece(game->current_board, real_move) == pawn) ||
        (get_ending_piece(game->current_board, real_move) != none)) {
      game->half_move_count = 0;
    } else {
      game->half_move_count += 1;
    }
    game->move_count += 1;
    game->board_history[game->move_count] = *(game->current_board);
    game->current_board = &(game->board_history[game->move_count]);
    bit_move(game->current_board, real_move);
    game->current_legal_moves = (game->legal_moves_history[game->move_count]);
    if (game->half_move_count >= 75) {
      game->current_board->pawns |= 4;
    }
    if (position_frequency(game) >= 5) {
      game->current_board->pawns |= 4;
      game->current_board->pawns |= 8;
    }
    set_legal_moves(game->current_board, game->current_legal_moves);
  } else {
    printf("not a legal move!\n");
  }
  return;
}

move get_random_move(full_chess *game) {
  time_t t;
  srand((unsigned)time(&t));
  return game->current_legal_moves[rand() % num_legal_moves(game)];
}

int position_frequency(full_chess *game) { // returns min of 5 and the num of
                                           // times this position has occurred
  int count = 0;
  for (int i = game->move_count - 1; i >= 0; i--) {
    if (chess_equal(game->current_board, &(game->board_history[i]))) {
      count++;
    }
    if (count >= 5) {
      return 5;
    }
  }
  return count;
}

void string_move(full_chess *game, char *focus_move) {
  make_move(game, encode_string(focus_move));
  return;
}

void unmake_move(full_chess *game) {
  if (game->move_count == 0) {
    return;
  }
  game->move_count -= 1;
  game->current_board = &(game->board_history[game->move_count]);
  game->current_legal_moves = (game->legal_moves_history[game->move_count]);
}

void print_legal_moves(full_chess *game) {
  printf("{");
  for (int i = 0; game->current_legal_moves[i] != NULL_MOVE; i++) {
    if (i > 0) {
      printf(",");
    }
    print_move(game, game->current_legal_moves[i]);
  }
  printf("}\n");
  return;
}

int num_legal_moves(full_chess *game) {
  int count = 0;
  while (game->current_legal_moves[count] != NULL_MOVE) {
    count++;
  }
  return count;
}

void print_move(full_chess *game, move focus_move) {
  offset starting_offset = get_starting_offset(focus_move);
  offset ending_offset = get_ending_offset(focus_move);
  printf("%c", 'a' + (starting_offset % 8));
  printf("%c", '1' + (starting_offset / 8));
  printf("%c", 'a' + (ending_offset % 8));
  printf("%c", '1' + (ending_offset / 8));
  piece starting_piece =
      get_piece_at_offset(game->current_board, starting_offset);
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

void print_board(full_chess *full_game) {
  chess *game = full_game->current_board;
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
