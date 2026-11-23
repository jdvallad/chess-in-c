#include "engine/book.h"

#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/notation.h"
#include "core/primitives.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Curated Grandmaster Opening Repertoire (UCI move sequences played from startpos).
// Covers Ruy Lopez, Italian, Scotch, Sicilian (Najdorf/Classical/Dragon), French, Caro-Kann,
// Queen's Gambit Declined/Accepted, Slav, King's Indian, Nimzo-Indian, English, and London.
static const char *const g_builtin_lines[] = {
    // Ruy Lopez
    "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7",
    "e2e4 e7e5 g1f3 b8c6 f1b5 g8f6 e1g1 f6e4 d2d4 e4d6",
    // Italian Game & Two Knights
    "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d3 d7d6",
    "e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 d2d3 f8c5 e1g1 d7d6",
    "e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4 g8f6 b1c3 f8b4",
    // Sicilian Defense (Open, Najdorf, Dragon, Classical, Alapin)
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6",
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 g7g6",
    "e2e4 c7c5 g1f3 b8c6 d2d4 c5d4 f3d4 g8f6 b1c3 e7e5",
    "e2e4 c7c5 g1f3 e7e6 d2d4 c5d4 f3d4 a7a6 b1c3 d8c7",
    "e2e4 c7c5 c2c3 g8f6 e4e5 f6d5 d2d4 c5d4 g1f3 b8c6",
    // French Defense
    "e2e4 e7e6 d2d4 d7d5 b1c3 g8f6 e4e5 f6d7 f2f4 c7c5",
    "e2e4 e7e6 d2d4 d7d5 b1d2 c7c5 e4d5 d8d5 g1f3 c5d4",
    "e2e4 e7e6 d2d4 d7d5 e4e5 c7c5 c2c3 b8c6 g1f3 d8b6",
    // Caro-Kann Defense
    "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 c8f5 e4g3 f5g6",
    "e2e4 c7c6 d2d4 d7d5 e4e5 c8f5 g1f3 e7e6 f1e2 c6c5",
    // Scandinavian & Petrov
    "e2e4 d7d5 e4d5 d8d5 b1c3 d5a5 d2d4 g8f6 g1f3 c7c6",
    "e2e4 e7e5 g1f3 g8f6 f3e5 d7d6 e5f3 f6e4 d2d4 d6d5",
    // Queen's Gambit Declined, Accepted & Slav
    "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7 e2e3 e8g8",
    "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 d5c4 a2a4 c8f5",
    "d2d4 d7d5 c2c4 d5c4 g1f3 g8f6 e2e3 e7e6 f1c4 c7c5",
    // King's Indian & Nimzo-Indian & Queen's Indian
    "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 g1f3 e8g8",
    "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 e2e3 e8g8 f1d3 d7d5",
    "d2d4 g8f6 c2c4 e7e6 g1f3 b7b6 g2g3 c8b7 f1g2 f8e7",
    "d2d4 g8f6 c2c4 g7g6 b1c3 d7d5 c4d5 f6d5 e2e4 d5c3",
    // London System & Reti & English
    "d2d4 d7d5 c1f4 g8f6 e2e3 c7c5 c2c3 b8c6 b1d2 e7e6",
    "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6 g2g3 f8b4 f1g2 e8g8",
    "c2c4 c7c5 b1c3 b8c6 g2g3 g7g6 f1g2 f8g7 g1f3 g8f6",
    "g1f3 d7d5 g2g3 g8f6 f1g2 c7c6 e1g1 c8f5 d2d3 e7e6",
};

static u64 book_rand(u64 *s) {
    u64 x = (*s += 0x9e3779b97f4a7c15ull);
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

// ============================================================================
// Polyglot .bin Binary Book Support (standard 16-byte big-endian entries)
// ============================================================================

static u64 read_be64(const u8 *p) {
    return ((u64) p[0] << 56) | ((u64) p[1] << 48) | ((u64) p[2] << 40) | ((u64) p[3] << 32)
           | ((u64) p[4] << 24) | ((u64) p[5] << 16) | ((u64) p[6] << 8) | (u64) p[7];
}

static u16 read_be16(const u8 *p) {
    return (u16) (((u16) p[0] << 8) | (u16) p[1]);
}

// Lightweight Polyglot-compatible hash (note: external Polyglot books require the exact 781
// Random64 constants; we check custom .bin files using Polyglot's move format when present).
u64 book_polyglot_hash(const full_chess *game) {
    // Deterministic position fingerprint in White-at-bottom static orientation
    chess wv = *(game->game);
    bool btm = is_flipped(&wv);
    if (btm) {
        flip_perspective(&wv);
    }
    u64 h = 0xcbf29ce484222325ull;
    const u64 prime = 0x100000001b3ull;
    u64 fields[6] = { wv.pawns, wv.friendly_pieces, wv.enemy_pieces, wv.orthogonal_pieces,
        wv.diagonal_pieces, wv.kings };
    for (int i = 0; i < 6; i++) {
        h ^= fields[i];
        h *= prime;
    }
    if (btm) {
        h ^= 0x9e3779b97f4a7c15ull;
    }
    return h;
}

static u64 decode_polyglot_move(const full_chess *game, u16 pg_move) {
    int to_file = pg_move & 7;
    int to_rank = (pg_move >> 3) & 7;
    int from_file = (pg_move >> 6) & 7;
    int from_rank = (pg_move >> 9) & 7;
    int promo = (pg_move >> 12) & 7;

    // Polyglot encodes castling as king-captures-rook (e1h1, e1a1, e8h8, e8a8)
    if (from_rank == 0 && from_file == 4 && to_rank == 0 && to_file == 7) {
        to_file = 6;
    } else if (from_rank == 0 && from_file == 4 && to_rank == 0 && to_file == 0) {
        to_file = 2;
    } else if (from_rank == 7 && from_file == 4 && to_rank == 7 && to_file == 7) {
        to_file = 6;
    } else if (from_rank == 7 && from_file == 4 && to_rank == 7 && to_file == 0) {
        to_file = 2;
    }

    const char promo_chars[5] = { '\0', 'n', 'b', 'r', 'q' };
    char uci[8];
    if (promo >= 1 && promo <= 4) {
        snprintf(uci, sizeof(uci), "%c%c%c%c%c", 'a' + from_file, '1' + from_rank, 'a' + to_file,
            '1' + to_rank, promo_chars[promo]);
    } else {
        snprintf(uci, sizeof(uci), "%c%c%c%c", 'a' + from_file, '1' + from_rank, 'a' + to_file,
            '1' + to_rank);
    }
    return static_encode_string(game, uci);
}

static u64 probe_polyglot_file(const full_chess *game, const char *path, u64 *rng_state) {
    if (!path || path[0] == '\0') {
        return NULL_MOVE;
    }
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return NULL_MOVE;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    if (fsize < 16) {
        fclose(fp);
        return NULL_MOVE;
    }
    long count = fsize / 16;
    u64 key = book_polyglot_hash(game);

    // Binary search sorted 16-byte entries
    long lo = 0, hi = count - 1, found = -1;
    u8 rec[16];
    while (lo <= hi) {
        long mid = lo + (hi - lo) / 2;
        fseek(fp, mid * 16, SEEK_SET);
        if (fread(rec, 1, 16, fp) != 16) {
            break;
        }
        u64 k = read_be64(rec);
        if (k < key) {
            lo = mid + 1;
        } else if (k > key) {
            hi = mid - 1;
        } else {
            found = mid;
            break;
        }
    }
    if (found < 0) {
        fclose(fp);
        return NULL_MOVE;
    }

    // Rewind to first matching key
    while (found > 0) {
        fseek(fp, (found - 1) * 16, SEEK_SET);
        if (fread(rec, 1, 16, fp) != 16 || read_be64(rec) != key) {
            break;
        }
        found--;
    }

    u64 cands[32];
    u32 weights[32];
    int nc = 0;
    u32 total_w = 0;

    for (long idx = found; idx < count && nc < 32; idx++) {
        fseek(fp, idx * 16, SEEK_SET);
        if (fread(rec, 1, 16, fp) != 16 || read_be64(rec) != key) {
            break;
        }
        u64 m = decode_polyglot_move(game, read_be16(rec + 8));
        u16 w = read_be16(rec + 10);
        if (m != NULL_MOVE && w > 0) {
            cands[nc] = m;
            weights[nc] = w;
            total_w += w;
            nc++;
        }
    }
    fclose(fp);

    if (nc == 0 || total_w == 0) {
        return NULL_MOVE;
    }
    u32 pick = (u32) (book_rand(rng_state) % total_w);
    u32 acc = 0;
    for (int i = 0; i < nc; i++) {
        acc += weights[i];
        if (pick < acc) {
            return cands[i];
        }
    }
    return cands[0];
}

// ============================================================================
// Built-In Opening Book Lookup (position-matched via chess_equal!)
// ============================================================================

u64 book_probe(const full_chess *game, const char *polyglot_path, u64 *rng_state) {
    if (!game || game->is_game_over || game->move_count >= 14) {
        return NULL_MOVE;
    }

    u64 fallback_seed = 0x12345678ull;
    u64 *rng = rng_state ? rng_state : &fallback_seed;

    // 1. Check external Polyglot .bin book if provided, or present at data/book.bin or ./book.bin
    u64 pg = NULL_MOVE;
    if (polyglot_path && polyglot_path[0]) {
        pg = probe_polyglot_file(game, polyglot_path, rng);
    } else {
        pg = probe_polyglot_file(game, "data/book.bin", rng);
        if (pg == NULL_MOVE) {
            pg = probe_polyglot_file(game, "book.bin", rng);
        }
    }
    if (pg != NULL_MOVE) {
        return pg;
    }

    // 2. Match against built-in master repertoire (handles transpositions automatically!)
    int n_lines = (int) (sizeof(g_builtin_lines) / sizeof(g_builtin_lines[0]));
    u64 cands[64];
    int weights[64];
    int nc = 0;
    int total_w = 0;

    full_chess *tmp = full_chess_create();
    if (!tmp) {
        return NULL_MOVE;
    }

    for (int l = 0; l < n_lines; l++) {
        full_chess_initialize(tmp);
        tmp->is_static = true;
        const char *p = g_builtin_lines[l];

        while (*p) {
            while (*p == ' ') {
                p++;
            }
            if (*p == '\0') {
                break;
            }
            char uci[8];
            int k = 0;
            while (p[k] && p[k] != ' ' && k < 7) {
                uci[k] = p[k];
                k++;
            }
            uci[k] = '\0';
            p += k;

            u64 m = static_encode_string(tmp, uci);
            if (m == NULL_MOVE) {
                break;
            }

            if (chess_equal(tmp->game, game->game)) {
                u64 legal_m = static_encode_string(game, uci);
                if (legal_m != NULL_MOVE) {
                    int existing = -1;
                    for (int c = 0; c < nc; c++) {
                        if (cands[c] == legal_m) {
                            existing = c;
                            break;
                        }
                    }
                    if (existing >= 0) {
                        weights[existing]++;
                        total_w++;
                    } else if (nc < 64) {
                        cands[nc] = legal_m;
                        weights[nc] = 1;
                        total_w++;
                        nc++;
                    }
                }
                break;
            }

            make_move(tmp, m);
            if (tmp->move_count > game->move_count) {
                break;
            }
        }
    }
    full_chess_delete(&tmp);

    if (nc == 0 || total_w == 0) {
        return NULL_MOVE;
    }
    int pick = (int) (book_rand(rng) % (u64) total_w);
    int acc = 0;
    for (int i = 0; i < nc; i++) {
        acc += weights[i];
        if (pick < acc) {
            return cands[i];
        }
    }
    return cands[0];
}
