#include "features/history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// Classic ECO Opening Recognizer (longest prefix match on SAN moves)
// ============================================================================

typedef struct opening_line {
    const char *name;
    int len;
    const char *moves[10];
} opening_line;

static const opening_line g_openings_book[] = {
    { "Sicilian Defense: Najdorf", 10,
        { "e4", "c5", "Nf3", "d6", "d4", "cxd4", "Nxd4", "Nf6", "Nc3", "a6" } },
    { "Sicilian Defense: Dragon", 10,
        { "e4", "c5", "Nf3", "d6", "d4", "cxd4", "Nxd4", "Nf6", "Nc3", "g6" } },
    { "Sicilian Defense: Open", 6, { "e4", "c5", "Nf3", "d6", "d4", "cxd4" } },
    { "Sicilian Defense: Alapin", 3, { "e4", "c5", "c3" } },
    { "Sicilian Defense: Closed", 3, { "e4", "c5", "Nc3" } },
    { "Sicilian Defense", 2, { "e4", "c5" } },

    { "Ruy Lopez: Berlin Defense", 6, { "e4", "e5", "Nf3", "Nc6", "Bb5", "Nf6" } },
    { "Ruy Lopez: Morphy Defense", 6, { "e4", "e5", "Nf3", "Nc6", "Bb5", "a6" } },
    { "Ruy Lopez", 5, { "e4", "e5", "Nf3", "Nc6", "Bb5" } },

    { "Italian Game: Fried Liver Setup", 7, { "e4", "e5", "Nf3", "Nc6", "Bc4", "Nf6", "Ng5" } },
    { "Italian Game: Two Knights", 6, { "e4", "e5", "Nf3", "Nc6", "Bc4", "Nf6" } },
    { "Italian Game: Giuoco Piano", 6, { "e4", "e5", "Nf3", "Nc6", "Bc4", "Bc5" } },
    { "Italian Game", 5, { "e4", "e5", "Nf3", "Nc6", "Bc4" } },

    { "Scotch Game", 5, { "e4", "e5", "Nf3", "Nc6", "d4" } },
    { "Four Knights Game", 6, { "e4", "e5", "Nf3", "Nc6", "Nc3", "Nf6" } },
    { "Petroff's Defense", 4, { "e4", "e5", "Nf3", "Nf6" } },
    { "Philidor Defense", 4, { "e4", "e5", "Nf3", "d6" } },
    { "King's Gambit Accepted", 4, { "e4", "e5", "f4", "exf4" } },
    { "King's Gambit", 3, { "e4", "e5", "f4" } },
    { "Vienna Game", 3, { "e4", "e5", "Nc3" } },
    { "Bishop's Opening", 3, { "e4", "e5", "Bc4" } },
    { "King's Pawn Game", 2, { "e4", "e5" } },

    { "French Defense: Advance", 5, { "e4", "e6", "d4", "d5", "e5" } },
    { "French Defense: Exchange", 5, { "e4", "e6", "d4", "d5", "exd5" } },
    { "French Defense: Winawer", 6, { "e4", "e6", "d4", "d5", "Nc3", "Bb4" } },
    { "French Defense", 2, { "e4", "e6" } },

    { "Caro-Kann Defense: Advance", 5, { "e4", "c6", "d4", "d5", "e5" } },
    { "Caro-Kann Defense: Classical", 6, { "e4", "c6", "d4", "d5", "Nc3", "dxe4" } },
    { "Caro-Kann Defense", 2, { "e4", "c6" } },

    { "Scandinavian Defense", 2, { "e4", "d5" } },
    { "Pirc Defense", 4, { "e4", "d6", "d4", "Nf6" } },
    { "Alekhine's Defense", 2, { "e4", "Nf6" } },
    { "Modern Defense", 2, { "e4", "g6" } },

    { "Queen's Gambit Accepted", 4, { "d4", "d5", "c4", "dxc4" } },
    { "Queen's Gambit Declined", 4, { "d4", "d5", "c4", "e6" } },
    { "Slav Defense", 4, { "d4", "d5", "c4", "c6" } },
    { "Queen's Gambit", 3, { "d4", "d5", "c4" } },
    { "London System", 4, { "d4", "d5", "Bf4", "Nf6" } },
    { "London System", 3, { "d4", "d5", "Bf4" } },
    { "London System", 5, { "d4", "Nf6", "Nf3", "d5", "Bf4" } },
    { "London System", 3, { "d4", "Nf6", "Bf4" } },

    { "King's Indian Defense", 6, { "d4", "Nf6", "c4", "g6", "Nc3", "Bg7" } },
    { "Grunfeld Defense", 6, { "d4", "Nf6", "c4", "g6", "Nc3", "d5" } },
    { "Nimzo-Indian Defense", 6, { "d4", "Nf6", "c4", "e6", "Nc3", "Bb4" } },
    { "Queen's Indian Defense", 6, { "d4", "Nf6", "c4", "e6", "Nf3", "b6" } },
    { "Benoni Defense", 4, { "d4", "Nf6", "c4", "c5" } },
    { "Indian Game", 2, { "d4", "Nf6" } },
    { "Dutch Defense", 2, { "d4", "f5" } },
    { "Queen's Pawn Game", 2, { "d4", "d5" } },

    { "English Opening: Symmetrical", 2, { "c4", "c5" } },
    { "English Opening", 1, { "c4" } },
    { "Reti Opening", 2, { "Nf3", "d5" } },
    { "Zukertort Opening", 1, { "Nf3" } },
    { "Bird's Opening", 1, { "f4" } },
    { "King's Pawn Opening", 1, { "e4" } },
    { "Queen's Pawn Opening", 1, { "d4" } },
};

const char *history_detect_opening(const char (*san_moves)[16], int count) {
    if (!san_moves || count <= 0) {
        return "Starting Position";
    }
    const char *best_name = "Custom Opening";
    int best_len = 0;
    int n = (int) (sizeof(g_openings_book) / sizeof(g_openings_book[0]));
    for (int i = 0; i < n; i++) {
        const opening_line *op = &g_openings_book[i];
        if (op->len > count || op->len <= best_len) {
            continue;
        }
        bool match = true;
        for (int k = 0; k < op->len; k++) {
            if (strcmp(san_moves[k], op->moves[k]) != 0) {
                match = false;
                break;
            }
        }
        if (match) {
            best_len = op->len;
            best_name = op->name;
        }
    }
    return best_name;
}

// ============================================================================
// Persistent JSON History Storage
// ============================================================================

static char *dup_str(const char *s) {
    if (!s) {
        s = "";
    }
    size_t n = strlen(s);
    char *copy = (char *) malloc(n + 1);
    if (copy) {
        memcpy(copy, s, n + 1);
    }
    return copy;
}

void history_init(game_history *h) {
    if (!h) {
        return;
    }
    memset(h, 0, sizeof(*h));
    h->next_id = 1;
}

void history_free(game_history *h) {
    if (!h) {
        return;
    }
    for (int i = 0; i < h->count; i++) {
        free(h->entries[i].pgn);
        h->entries[i].pgn = NULL;
    }
    h->count = 0;
}

static void fprint_json_str(FILE *fp, const char *s) {
    fputc('"', fp);
    if (s) {
        for (const unsigned char *p = (const unsigned char *) s; *p; p++) {
            switch (*p) {
            case '"': fputs("\\\"", fp); break;
            case '\\': fputs("\\\\", fp); break;
            case '\n': fputs("\\n", fp); break;
            case '\r': fputs("\\r", fp); break;
            case '\t': fputs("\\t", fp); break;
            default: fputc((int) *p, fp); break;
            }
        }
    }
    fputc('"', fp);
}

static bool extract_obj_str(const char *obj, const char *key, char *out, size_t out_size) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(obj, pat);
    if (!p) {
        return false;
    }
    p += strlen(pat);
    while (*p == ' ' || *p == ':' || *p == '\t') {
        p++;
    }
    if (*p != '"') {
        return false;
    }
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_size) {
        if (*p == '\\' && p[1] != '\0') {
            p++;
            switch (*p) {
            case 'n': out[i++] = '\n'; break;
            case 'r': out[i++] = '\r'; break;
            case 't': out[i++] = '\t'; break;
            default: out[i++] = *p; break;
            }
            p++;
        } else {
            out[i++] = *p++;
        }
    }
    out[i] = '\0';
    return true;
}

static int extract_obj_int(const char *obj, const char *key, int def_val) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(obj, pat);
    if (!p) {
        return def_val;
    }
    p += strlen(pat);
    while (*p == ' ' || *p == ':' || *p == '\t') {
        p++;
    }
    if (*p != '-' && (*p < '0' || *p > '9')) {
        return def_val;
    }
    return atoi(p);
}

bool history_load(const char *path, game_history *h) {
    history_free(h);
    history_init(h);

    FILE *fp = fopen(path ? path : GAME_HISTORY_FILE, "rb");
    if (!fp) {
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0 || sz > 8 * 1024 * 1024) {
        fclose(fp);
        return false;
    }
    char *buf = (char *) malloc((size_t) sz + 1);
    if (!buf) {
        fclose(fp);
        return false;
    }
    size_t n = fread(buf, 1, (size_t) sz, fp);
    fclose(fp);
    buf[n] = '\0';

    h->next_id = extract_obj_int(buf, "next_id", 1);
    const char *arr = strstr(buf, "\"games\"");
    if (!arr) {
        free(buf);
        return false;
    }

    const char *p = strchr(arr, '[');
    while (p && h->count < MAX_HISTORY_GAMES) {
        const char *obj_start = strchr(p, '{');
        if (!obj_start) {
            break;
        }
        // Scan matching '}' respecting quoted strings
        const char *q = obj_start + 1;
        bool in_str = false;
        while (*q) {
            if (*q == '"' && (q == obj_start + 1 || q[-1] != '\\')) {
                in_str = !in_str;
            } else if (*q == '\\' && in_str && q[1] != '\0') {
                q++;
            } else if (*q == '}' && !in_str) {
                break;
            }
            q++;
        }
        if (*q != '}') {
            break;
        }
        size_t olen = (size_t) (q - obj_start + 1);
        char *obj = (char *) malloc(olen + 1);
        if (!obj) {
            break;
        }
        memcpy(obj, obj_start, olen);
        obj[olen] = '\0';

        history_entry *e = &h->entries[h->count];
        memset(e, 0, sizeof(*e));
        e->id = extract_obj_int(obj, "id", h->count + 1);
        if (e->id >= h->next_id) {
            h->next_id = e->id + 1;
        }
        extract_obj_str(obj, "date", e->date, sizeof(e->date));
        extract_obj_str(obj, "white", e->white, sizeof(e->white));
        extract_obj_str(obj, "black", e->black, sizeof(e->black));
        extract_obj_str(obj, "result", e->result, sizeof(e->result));
        extract_obj_str(obj, "reason", e->reason, sizeof(e->reason));
        extract_obj_str(obj, "opening", e->opening, sizeof(e->opening));
        extract_obj_str(obj, "mode", e->mode, sizeof(e->mode));
        e->plies = extract_obj_int(obj, "plies", 0);
        e->white_elo = extract_obj_int(obj, "white_elo", 0);
        e->black_elo = extract_obj_int(obj, "black_elo", 0);

        char *pgn_buf = (char *) malloc(olen + 1);
        if (pgn_buf) {
            pgn_buf[0] = '\0';
            extract_obj_str(obj, "pgn", pgn_buf, olen + 1);
            e->pgn = pgn_buf;
        } else {
            e->pgn = dup_str("");
        }
        h->count++;
        free(obj);
        p = q + 1;
    }

    free(buf);
    return true;
}

bool history_save(const char *path, const game_history *h) {
    if (!h) {
        return false;
    }
    FILE *fp = fopen(path ? path : GAME_HISTORY_FILE, "wb");
    if (!fp) {
        return false;
    }
    fprintf(fp, "{\n  \"next_id\": %d,\n  \"games\": [\n", h->next_id);
    for (int i = 0; i < h->count; i++) {
        const history_entry *e = &h->entries[i];
        fprintf(fp, "    {\"id\": %d, \"date\": ", e->id);
        fprint_json_str(fp, e->date);
        fprintf(fp, ", \"white\": ");
        fprint_json_str(fp, e->white);
        fprintf(fp, ", \"black\": ");
        fprint_json_str(fp, e->black);
        fprintf(fp, ", \"result\": ");
        fprint_json_str(fp, e->result);
        fprintf(fp, ", \"reason\": ");
        fprint_json_str(fp, e->reason);
        fprintf(fp, ", \"opening\": ");
        fprint_json_str(fp, e->opening);
        fprintf(fp, ", \"mode\": ");
        fprint_json_str(fp, e->mode);
        fprintf(fp, ", \"plies\": %d, \"white_elo\": %d, \"black_elo\": %d, \"pgn\": ", e->plies,
            e->white_elo, e->black_elo);
        fprint_json_str(fp, e->pgn ? e->pgn : "");
        fprintf(fp, "}%s\n", (i + 1 < h->count) ? "," : "");
    }
    fprintf(fp, "  ]\n}\n");
    fclose(fp);
    return true;
}

int history_add(game_history *h, const char *white, const char *black, const char *result,
    const char *reason, const char *opening, const char *mode, int plies, int white_elo,
    int black_elo, const char *pgn) {
    if (!h) {
        return 0;
    }
    if (h->count >= MAX_HISTORY_GAMES) {
        free(h->entries[MAX_HISTORY_GAMES - 1].pgn);
        h->count = MAX_HISTORY_GAMES - 1;
    }
    if (h->count > 0) {
        memmove(&h->entries[1], &h->entries[0], (size_t) h->count * sizeof(history_entry));
    }
    history_entry *e = &h->entries[0];
    memset(e, 0, sizeof(*e));
    e->id = h->next_id++;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        strftime(e->date, sizeof(e->date), "%Y-%m-%d %H:%M", tm_info);
    } else {
        snprintf(e->date, sizeof(e->date), "unknown");
    }
    snprintf(e->white, sizeof(e->white), "%s", white ? white : "White");
    snprintf(e->black, sizeof(e->black), "%s", black ? black : "Black");
    snprintf(e->result, sizeof(e->result), "%s", result ? result : "*");
    snprintf(e->reason, sizeof(e->reason), "%s", reason ? reason : "");
    snprintf(e->opening, sizeof(e->opening), "%s", opening ? opening : "");
    snprintf(e->mode, sizeof(e->mode), "%s", mode ? mode : "play");
    e->plies = plies;
    e->white_elo = white_elo;
    e->black_elo = black_elo;
    e->pgn = dup_str(pgn);
    h->count++;
    return e->id;
}

bool history_update_elos(game_history *h, int id, int white_elo, int black_elo) {
    if (!h || id <= 0) {
        return false;
    }
    for (int i = 0; i < h->count; i++) {
        if (h->entries[i].id == id) {
            h->entries[i].white_elo = white_elo;
            h->entries[i].black_elo = black_elo;
            return true;
        }
    }
    return false;
}

bool history_delete_by_id(game_history *h, int id) {
    if (!h || id <= 0) {
        return false;
    }
    for (int i = 0; i < h->count; i++) {
        if (h->entries[i].id == id) {
            free(h->entries[i].pgn);
            if (i + 1 < h->count) {
                memmove(&h->entries[i], &h->entries[i + 1],
                    (size_t) (h->count - i - 1) * sizeof(history_entry));
            }
            h->count--;
            return true;
        }
    }
    return false;
}

void history_clear(game_history *h) {
    if (!h) {
        return;
    }
    for (int i = 0; i < h->count; i++) {
        free(h->entries[i].pgn);
        h->entries[i].pgn = NULL;
    }
    h->count = 0;
}

const history_entry *history_find_by_id(const game_history *h, int id) {
    if (!h || id <= 0) {
        return NULL;
    }
    for (int i = 0; i < h->count; i++) {
        if (h->entries[i].id == id) {
            return &h->entries[i];
        }
    }
    return NULL;
}
