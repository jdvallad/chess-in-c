#include "MoveSet.c"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define MAX_GAME_LENGTH 100
#define HASH_LENGTH 71
#define WHITE 0
#define BLACK 1
#define NORTH = 1
#define SOUTH -1
#define EAST 1
#define WEST -1
#define NONE_DIR 0
#define QUEEN 0
#define KNIGHT 1
#define BISHOP 2
#define ROOK 3
#define KING 4
#define PAWN 5
#define NONE_PIECE 6
#define QUEENSIDE 0
#define KINGSIDE 1
#define RANK_8 255ull
#define RANK_7 65280ull
#define RANK_6 16711680ull
#define RANK_5 4278190080ull
#define RANK_4 1095216660480ull
#define RANK_3 280375465082880ull
#define RANK_2 71776119061217280ull
#define RANK_1 18374686479671623680ull
#define H_FILE 72340172838076673ull
#define G_FILE 144680345676153346ull
#define F_FILE 289360691352306692ull
#define E_FILE 578721382704613384ull
#define D_FILE 1157442765409226768ull
#define C_FILE 2314885530818453536ull
#define B_FILE 4629771061636907072ull
#define A_FILE 9259542123273814144ull
#define FLAG_STANDARD 0
#define FLAG_CASTLE 1
#define FLAG_EN_PASSANT 2
#define FLAG_PROMOTION 3

uint8_t PROMOTION_PIECES[4] = {QUEEN, KNIGHT, BISHOP, ROOK};
uint8_t FLAGS[4] = {FLAG_STANDARD, FLAG_CASTLE, FLAG_EN_PASSANT,
                    FLAG_PROMOTION};
uint8_t PIECES[6] = {QUEEN, KNIGHT, BISHOP, ROOK, KING, PAWN};
uint64_t FILES[8] = {A_FILE, B_FILE, C_FILE, D_FILE,
                     E_FILE, F_FILE, G_FILE, H_FILE};
uint64_t FILES_REVERSED[8] = {H_FILE, G_FILE, F_FILE, E_FILE,
                              D_FILE, C_FILE, B_FILE, A_FILE};
uint64_t RANKS[8] = {RANK_1, RANK_2, RANK_3, RANK_4,
                     RANK_5, RANK_6, RANK_7, RANK_8};
uint64_t RANKS_REVERSED[8] = {RANK_8, RANK_7, RANK_6, RANK_5,
                              RANK_4, RANK_3, RANK_2, RANK_1};

char PIECE_CHARS_ASCII[2][7] = {{'Q', 'N', 'B', 'R', 'K', 'P', ' '},
                                {'q', 'n', 'b', 'r', 'k', 'p', ' '}};
uint8_t COLORS[2] = {WHITE, BLACK};
uint8_t COLORS_REVERSED[2] = {BLACK, WHITE};
uint8_t SIDES[2] = {QUEENSIDE, KINGSIDE};
uint8_t SIDES_REVERSED[2] = {KINGSIDE, QUEENSIDE};
uint64_t VERTICAL_OFFSETS[17] = {0ull,
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
uint64_t HORIZONTAL_OFFSETS[17] = {0ull,
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
typedef struct Chess
{
    MoveSet legalShortMoves;
    uint64_t piece_boards[2][6];
    uint64_t combined_boards[2];
    bool castle_rights[2][2];
    uint64_t enPassantSquare;
    uint64_t halfMoveCount;
    uint64_t fullMoveCount;
    MoveSet pseudoLegalMoves[2];
    uint64_t reversibleMoves[MAX_GAME_LENGTH];
    char hashList[MAX_GAME_LENGTH][HASH_LENGTH];
    uint8_t turn;
    bool gameOver;
} Chess;

Chess *chess_create()
{
    Chess *output = (Chess *)malloc(sizeof(Chess));
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

void print_bitboard(uint64_t board)
{
    printf("\n");
    for (int i = 0; i < 8; i++)
    {
        for (int j = 7; j >= 0; j--)
        {
            printf("%lu ", 1ul & (board >> (8 * i + j)));
        }
        printf("\n");
    }
    return;
}

uint64_t offset_to_bitboard(int8_t off) { return 1ul << off; }

int8_t coordinate_to_offset(int8_t x, int8_t y) { return 8 * x + y; }

uint64_t compass(uint64_t input, int8_t north, int8_t east)
{
    input = input & VERTICAL_OFFSETS[north + 8] & HORIZONTAL_OFFSETS[east + 8];
    input = north >= 0 ? input >> (north * 8) : input << -(north * 8);
    input = east >= 0 ? input >> east : input << -east;
    return input;
}

void printBoard(uint8_t perspective)
{
    bool flag = false;
    printf("                                ");
    if (perspective == WHITE)
    {
        printf("  a   b   c   d   e   f   g   h\n");
    }
    else
    {
        printf("  h   g   f   e   d   c   b   a\n");
    }
    printf("                                ");
    printf("|---|---|---|---|---|---|---|---|\n");
    printf("                              ");
    if (perspective == WHITE)
    {
        printf("8 ");
    }
    else
    {
        printf("1 ");
    }
    printf("|");
    uint64_t *ordered_ranks = (perspective == BLACK ? RANKS : RANKS_REVERSED);
    int i;
    uint64_t rank;
    for (i = 0, rank = ordered_ranks[i]; i < 8; i++, rank = ordered_ranks[i])
    {
        if (flag)
        {
            printf("\r\n                                ");
            printf("|---|---|---|---|---|---|---|---|\n");
            printf("                              ");
            printf("%d ", (perspective == BLACK ? 1 + i : 8 - i));
            printf("|");
        }
        flag = true;
        uint64_t *ordered_files = (perspective == BLACK ? FILES_REVERSED : FILES);
        uint64_t file;
        int j;
    foundPiece:
        for (j = 0, file = ordered_files[j]; j < 8; j++, file = ordered_files[j])
        {
            
        }
    }
    return;
}
