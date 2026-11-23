#include "engine/zobrist.h"

#include "core/board.h"
#include "engine/engine_internal.h"

u64 g_zobrist_piece[12][64];
u64 g_zobrist_castle[16];
u64 g_zobrist_ep[9];
u64 g_zobrist_side;

// Castling mask clear table for O(1) incremental Zobrist hashing
// Bits: 0=fK, 1=fQ, 2=eK, 3=eQ (pre-flip perspective)
static const u8 g_zcastle_keep[64] = {
    13,
    15,
    15,
    15,
    12,
    15,
    15,
    14,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    7,
    15,
    15,
    15,
    3,
    15,
    15,
    11,
};

void zobrist_init(void) {
    u64 seed = 0xC0FFEE123456789Bull;
    for (int p = 0; p < 12; p++) {
        for (int sq = 0; sq < 64; sq++) {
            g_zobrist_piece[p][sq] = engine_splitmix64(&seed);
        }
    }
    for (int c = 0; c < 16; c++) {
        g_zobrist_castle[c] = engine_splitmix64(&seed);
    }
    for (int e = 0; e < 9; e++) {
        g_zobrist_ep[e] = engine_splitmix64(&seed);
    }
    g_zobrist_side = engine_splitmix64(&seed);
}

static inline int raw_castle_bits(const chess *b) {
    return ((b->pawns & FRIENDLY & KINGSIDE) ? 1 : 0) | ((b->pawns & FRIENDLY & QUEENSIDE) ? 2 : 0)
           | ((b->pawns & ENEMY & KINGSIDE) ? 4 : 0) | ((b->pawns & ENEMY & QUEENSIDE) ? 8 : 0);
}

static inline int to_static_castle(int raw, bool flip) {
    return flip ? (((raw & 3) << 2) | ((raw >> 2) & 3)) : raw;
}

u64 engine_hash_board(const chess *b) {
    engine_global_init();
    bool flip = is_flipped(b);
    int f_color = flip ? 1 : 0;
    int e_color = f_color ^ 1;
    int sq_xor = flip ? 56 : 0;
    u64 h = 0ull;

    u64 occ = b->friendly_pieces | b->enemy_pieces;
    while (occ) {
        int sq = __builtin_ctzll(occ);
        occ &= occ - 1ull;
        u8 pc = get_piece_at_offset(b, sq);
        int pt = piece_type_idx(pc);
        if (pt >= 0) {
            int c = (pc & (1 << 1)) ? f_color : e_color;
            h ^= g_zobrist_piece[c * 6 + pt][sq ^ sq_xor];
        }
    }

    h ^= g_zobrist_castle[to_static_castle(raw_castle_bits(b), flip)];
    h ^= g_zobrist_ep[has_en_passant(b) ? (get_en_passant_offset(b) - 40) : 8];
    if (flip) {
        h ^= g_zobrist_side;
    }
    return h;
}

u64 engine_update_hash(u64 hash, const chess *before, u64 move) {
    bool flip = is_flipped(before);
    int f_color = flip ? 1 : 0;
    int e_color = f_color ^ 1;
    int sq_xor = flip ? 56 : 0;
    int start = get_start_offset(move);
    int end = get_end_offset(move);
    u8 sp = get_start_piece(move);
    u8 epc = get_end_piece(move);
    u8 promo = get_promotion_piece(move);

    int old_ep = has_en_passant(before) ? (get_en_passant_offset(before) - 40) : 8;
    hash ^= g_zobrist_ep[old_ep];

    int old_raw = raw_castle_bits(before);
    int new_raw = old_raw & g_zcastle_keep[start] & g_zcastle_keep[end];
    if (new_raw != old_raw) {
        hash ^= g_zobrist_castle[to_static_castle(old_raw, flip)]
                ^ g_zobrist_castle[to_static_castle(new_raw, flip)];
    }

    int sp_pt = piece_type_idx(sp);
    int pr_pt = piece_type_idx(promo);
    hash ^= g_zobrist_piece[f_color * 6 + sp_pt][start ^ sq_xor];
    if (epc != NONE) {
        hash ^= g_zobrist_piece[e_color * 6 + piece_type_idx(epc)][end ^ sq_xor];
    }
    hash ^= g_zobrist_piece[f_color * 6 + pr_pt][end ^ sq_xor];

    if (__builtin_expect(is_en_passant_move(move), 0)) {
        hash ^= g_zobrist_piece[e_color * 6 + 0][(end - 8) ^ sq_xor];
    }
    if (__builtin_expect(is_castle_king_move(move), 0)) {
        hash ^= g_zobrist_piece[f_color * 6 + 3][7 ^ sq_xor]
                ^ g_zobrist_piece[f_color * 6 + 3][5 ^ sq_xor];
    }
    if (__builtin_expect(is_castle_queen_move(move), 0)) {
        hash ^= g_zobrist_piece[f_color * 6 + 3][0 ^ sq_xor]
                ^ g_zobrist_piece[f_color * 6 + 3][3 ^ sq_xor];
    }

    int new_ep = (sp == FRIENDLY_PAWN && start < 16 && end > 23) ? (end & 7) : 8;
    hash ^= g_zobrist_ep[new_ep] ^ g_zobrist_side;
    return hash;
}
