#ifndef PRIMITIVES_H
#define PRIMITIVES_H
#include <stdint.h>
#include <stdbool.h>
typedef uint64_t bitboard;
typedef int8_t offset;
typedef struct offset_set {
uint64_t board;
uint8_t size;
uint8_t index;
uint8_t remaining;
} offset_set;

typedef uint64_t bitboard;

typedef int8_t offset;

typedef struct MoveSet {
  char temp;
} MoveSet;

typedef struct Chess {
  MoveSet legalShortMoves;
  uint64_t piece_boards[2][6];
  uint64_t combined_boards[2];
  bool castle_rights[2][2];
  uint64_t enPassantSquare;
  uint64_t halfMoveCount;
  uint64_t fullMoveCount;
  MoveSet pseudoLegalMoves[2];
  uint64_t reversibleMoves[1];
  char hashList[1][1];
  uint8_t turn;
  bool gameOver;
} Chess;
#endif
