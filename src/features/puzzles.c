#include "features/puzzles.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const chess_puzzle g_puzzles[] = {
    { 1, "Back-Rank Knockout", "mate", 1, 800,
        "Black's king is trapped behind its own pawn shield. Deliver an instant back-rank mate.",
        "Use your rook to strike the 8th rank.", "6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1", 1,
        { "d1d8" } },
    { 2, "Scholar's Punishment", "mate", 1, 850,
        "Classic opening trap! White's queen and bishop zero in on Black's weakest square.",
        "Target the f7 pawn next to Black's king.",
        "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4", 1, { "h5f7" } },
    { 3, "Arabian Mate", "mate", 1, 950,
        "One of the oldest checkmate patterns in chess history: rook and knight teamwork in the "
        "corner.",
        "The knight already guards g8 and h7 — slide the rook in!",
        "7k/5R2/5N2/8/8/8/8/6K1 w - - 0 1", 1, { "f7h7" } },
    { 4, "The Royal Fork", "tactic", 1, 1000,
        "A leaping knight attacks king and queen simultaneously.",
        "Look for a checking knight square on the 7th rank.",
        "4k3/ppp2ppp/4q3/3N4/8/8/PPP2PPP/6K1 w - - 0 1", 3, { "d5c7", "e8d7", "c7e6" } },
    { 5, "Absolute Pin", "tactic", 1, 1100,
        "Black's queen and king sit on the same long diagonal. Pin and win the queen!",
        "Place your bishop on d4 where it is protected by your rook.",
        "7k/6pp/5q2/8/8/4B3/6PP/3R2K1 w - - 0 1", 1, { "e3d4" } },
    { 6, "Morphy's Opera Game", "mate", 2, 1150,
        "Paris, 1858 — Paul Morphy sacrifices his queen to open the d-file for an immortal "
        "checkmate.",
        "Sacrifice the queen on b8 to deflect the knight from defending d8!",
        "4kb1r/p2n1ppp/4q3/4p1B1/4P3/1Q6/PPP2PPP/2KR4 w k - 1 16", 3, { "b3b8", "d7b8", "d1d8" } },
    { 7, "Anastasia's Mate", "mate", 2, 1200,
        "Named after an 1803 chess novel: knight seals the escape squares while queen and rook "
        "storm the h-file.",
        "Sacrifice your queen on h7 to drag the king onto the h-file!",
        "4r2k/4N1pp/8/7Q/8/4R3/5PPP/6K1 w - - 0 1", 3, { "h5h7", "h8h7", "e3h3" } },
    { 8, "Legal's Mate", "mate", 2, 1250,
        "Black greedily captured White's queen on d1 — punish the exposed king with minor pieces "
        "alone!",
        "Start with a forcing bishop check on f7.",
        "r2qkb1r/ppp2ppp/2np4/4N3/2B1P3/2N5/PPPP1PPP/R1BbK2R w KQkq - 0 6", 3,
        { "c4f7", "e8e7", "c3d5" } },
    { 9, "Boden's Criss-Cross Mate", "mate", 2, 1300,
        "Sacrifice the queen to rip open the diagonal for two lethal criss-crossing bishops.",
        "Capture on c6 with check, then unleash the light-squared bishop!",
        "2kr3r/pp1nqppp/2p1p3/8/3P1B2/2N2Q2/PPP2PPP/R3RBK1 w - - 0 1", 3,
        { "f3c6", "b7c6", "f1a6" } },
    { 10, "Queen Trapper (WAC.007)", "tactic", 2, 1300,
        "Black's knight dives deep into White's camp — look at White's trapped queen and weak "
        "king.",
        "Jump the knight to e3! If White takes it, Qh4+ is devastating.",
        "rnbqkb1r/pppp1ppp/8/4P3/6n1/7P/PPPNPPP1/R1BQKBNR b KQkq - 0 1", 1, { "g4e3" } },
    { 11, "Pawn Breakthrough", "endgame", 2, 1350,
        "Three pawns face three pawns, yet White can force an unstoppable promotion by sacrifice!",
        "Push the middle pawn to b6 to blow open a path to the 8th rank.",
        "8/ppp5/8/PPP5/8/8/8/4K2k w - - 0 1", 3, { "b5b6", "a7b6", "c5c6" } },
    { 12, "Diagonal Pin (WAC.020)", "tactic", 2, 1350,
        "White's queen and king are lined up on the a6-f1 diagonal.",
        "Swing Black's bishop to b5 to pin the queen to the king.",
        "r2qkb1r/1ppb1ppp/p7/4p3/P1Q1P3/2P5/5PPP/R1B2KNR b kq - 0 1", 1, { "d7b5" } },
    { 13, "Rook Skewer Trap (WAC.018)", "endgame", 3, 1400,
        "White's a-pawn is one square from promotion, but Black's rook guards a8. Find the winning "
        "sidestep!",
        "Move your rook to the h-file so Rxa7 runs into a deadly skewer on the 7th rank.",
        "R7/P4k2/8/8/8/8/r7/6K1 w - - 0 1", 1, { "a8h8" } },
    { 14, "Tactical Fork Threat (WAC.015)", "tactic", 3, 1400,
        "Black's bishop on b7 appears defended by the rook — look one move deeper!",
        "Take on b7! If Black recaptures with the rook, c6+ forks king and rook.",
        "1R6/1brk2p1/4p2p/p1P1Pp2/P7/6P1/1P4P1/2R3K1 w - - 0 1", 1, { "b8b7" } },
    { 15, "Double-Check Discovery (WAC.004)", "mate", 3, 1450,
        "White's pieces swarm the kingside. Sacrifice the queen for a stunning double-check mate!",
        "Take on h7 with the queen, then push your h-pawn for a discovered double check!",
        "r1bq2rk/pp3pbp/2p1p1pQ/7P/3P4/2PB1N2/PP3PPR/2KR4 w - - 0 1", 3,
        { "h6h7", "h8h7", "h5g6" } },
    { 16, "Removing the Guard (WAC.011)", "tactic", 3, 1450,
        "Black's queen is overloaded defending both the c6 pawn and the back rank.",
        "Capture c6 with your bishop — if Qxc6, Qd8 is checkmate!",
        "r1b1kb1r/3q1ppp/pBp1pn2/8/Np3P2/5B2/PPP3PP/R2Q1RK1 w kq - 0 1", 3,
        { "f3c6", "d7c6", "d1d8" } },
    { 17, "Knight Underpromotion!", "endgame", 3, 1500,
        "Promoting to a queen isn't always best! Find the stylish underpromotion that wins on the "
        "spot.",
        "Promote to a knight on e8 to fork Black's king and queen!",
        "8/2q1P3/5k2/8/8/8/8/6K1 w - - 0 1", 3, { "e7e8n", "f6e7", "e8c7" } },
    { 18, "Back-Rank Fork Sac (WAC.013)", "sacrifice", 3, 1500,
        "Sacrifice your queen to lure Black's king onto a fatal knight-fork square!",
        "Capture the rook on f8 with check, then leap Nd7+!",
        "5rk1/pp4p1/2n1p2p/2Npq3/2p5/6P1/P3P1BP/R4Q1K w - - 0 1", 3, { "f1f8", "g8f8", "c5d7" } },
    { 19, "Triple Family Fork (WAC.017)", "tactic", 3, 1500,
        "Black's heavy pieces are clumsily clustered on the e6 and g6 squares.",
        "Plant your knight on e5, attacking queen, rook, and bishop at once!",
        "1k5r/pppbn1pp/4q1r1/1P3p2/2NPp3/1QP5/P4PPP/R1B1R1K1 w - - 0 1", 1, { "c4e5" } },
    { 20, "Philidor's Smothered Mate", "mate", 4, 1550,
        "The most celebrated checkmate motif in chess: double check, queen sacrifice, and a "
        "suffocated king!",
        "Start with a double check (Nh6+), then sacrifice the queen on g8!",
        "5rk1/5Npp/8/8/8/1Q6/5PPP/6K1 w - - 0 1", 5, { "f7h6", "g8h8", "b3g8", "f8g8", "h6f7" } },
    { 21, "Pin & Back-Rank Mate (WAC.012)", "sacrifice", 4, 1550,
        "Black's rook on g7 eyes g1, and White's knight on f3 is the only defender.",
        "Smash through with Qxf3+! After Rxf3, Rg1# finishes the game.",
        "4k1r1/2p3r1/1pR1p3/3pP2p/3P2qP/P4N2/1PQ4P/5R1K b - - 0 1", 3, { "g4f3", "f1f3", "g7g1" } },
    { 22, "Rook-Lift Mating Net (WAC.014)", "mate", 4, 1550,
        "White's rook on f3 is poised to swing to the h-file the instant the h7 shield falls.",
        "Sacrifice the queen on h7, then swing Rh3+ and Rh8#!",
        "r2rb1k1/pp1q1p1p/2n1p1p1/2bp4/5P2/PP1BPR1Q/1BPN2PP/R5K1 w - - 0 1", 5,
        { "h3h7", "g8h7", "f3h3", "h7g8", "h3h8" } },
    { 23, "The Classic Greek Gift", "sacrifice", 4, 1600,
        "Without a knight on f6, Black's castled king is vulnerable to the timeless Bxh7+ "
        "sacrifice!",
        "Sacrifice the bishop on h7, follow with Ng5+, and bring the queen to h5!",
        "r1bq1rk1/pppn1ppp/3bp3/3pP3/3P4/2NB1N2/PPP2PPP/R1BQK2R w KQ - 0 7", 5,
        { "d3h7", "g8h7", "f3g5", "h7g8", "d1h5" } },
    { 24, "Deflection Hammer (WAC.008)", "sacrifice", 4, 1600,
        "Black's queen and rook barely hold the g7 mating square together. Overload them!",
        "Offer your rook on f7 — threatening both Rxf8+ and Qxg7#!",
        "r4q1k/p2bR1rp/2p2Q1N/5p2/5p2/2P5/PP3PPP/R5K1 w - - 0 1", 1, { "e7f7" } },
    { 25, "The Immortal Qg6! (WAC.001)", "master", 5, 1650,
        "Perhaps the most famous test puzzle in chess programming history. Find White's stunning "
        "queen offer!",
        "Slide the queen to g6! Taking it allows instant mate with Nxg6#.",
        "2rr3k/pp3pp1/1nnqbN1p/3pN3/2pP4/2P3Q1/PPB4P/R4RK1 w - - 0 1", 3,
        { "g3g6", "f7g6", "e5g6" } },
    { 26, "Endgame Rook Sac (WAC.002)", "endgame", 5, 1700,
        "Black's connected passed pawns look blocked — unleash a shocking rook sacrifice to free "
        "them!",
        "Capture on b2 with the rook! After Rxb2, c3 creates an unstoppable pawn roller.",
        "8/7p/5k2/5p2/p1p2P2/Pr1pPK2/1P1R3P/8 b - - 0 1", 3, { "b3b2", "d2b2", "c4c3" } },
    { 27, "Crashing Through h7 (WAC.010)", "master", 5, 1750,
        "Black's kingside is held together by a single knight on h7. Tear it apart!",
        "Sacrifice the rook on h7 — opening the floodgates for Qe8+ and Rd8!",
        "2br2k1/2q3rn/p2NppQ1/2p1P3/Pp5R/4P3/1P3PPP/3R2K1 w - - 0 1", 1, { "h4h7" } },
    { 28, "Grandmaster Pin (WAC.003)", "master", 5, 1800,
        "Black's queen seems safely active on g5, but one quiet rook slide seals its fate.",
        "Play Rg3! Pinning the queen to the king while blocking the g-file.",
        "5rk1/1ppb3p/p1pb4/6q1/3P1p1r/2P1R2P/PP1BQ1P1/5RKN w - - 0 1", 1, { "e3g3" } },
};

int puzzle_count(void) {
    return (int) (sizeof(g_puzzles) / sizeof(g_puzzles[0]));
}

const chess_puzzle *puzzle_get(int idx) {
    int n = puzzle_count();
    if (idx < 0 || idx >= n) {
        return NULL;
    }
    return &g_puzzles[idx];
}

const chess_puzzle *puzzle_find_by_id(int id) {
    int n = puzzle_count();
    for (int i = 0; i < n; i++) {
        if (g_puzzles[i].id == id) {
            return &g_puzzles[i];
        }
    }
    return NULL;
}

void puzzle_stats_init(puzzle_stats *st) {
    if (!st) {
        return;
    }
    memset(st, 0, sizeof(*st));
    st->rating = 1200;
}

static int parse_int_field(const char *src, const char *key, int def_val) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(src, pat);
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

bool puzzle_stats_load(const char *path, puzzle_stats *st) {
    puzzle_stats_init(st);
    FILE *fp = fopen(path ? path : PUZZLE_STATS_FILE, "rb");
    if (!fp) {
        return false;
    }
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';

    st->rating = parse_int_field(buf, "rating", 1200);
    st->streak = parse_int_field(buf, "streak", 0);
    st->best_streak = parse_int_field(buf, "best_streak", 0);
    st->attempts = parse_int_field(buf, "attempts", 0);

    const char *arr = strstr(buf, "\"solved\"");
    if (arr) {
        const char *lb = strchr(arr, '[');
        const char *rb = lb ? strchr(lb, ']') : NULL;
        if (lb && rb) {
            const char *p = lb + 1;
            while (p < rb) {
                while (p < rb && (*p < '0' || *p > '9')) {
                    p++;
                }
                if (p >= rb) {
                    break;
                }
                int id = atoi(p);
                if (id >= 1 && id < 64 && !st->solved[id]) {
                    st->solved[id] = true;
                    st->solved_count++;
                }
                while (p < rb && *p >= '0' && *p <= '9') {
                    p++;
                }
            }
        }
    }
    return true;
}

bool puzzle_stats_save(const char *path, const puzzle_stats *st) {
    if (!st) {
        return false;
    }
    FILE *fp = fopen(path ? path : PUZZLE_STATS_FILE, "wb");
    if (!fp) {
        return false;
    }
    fprintf(fp, "{\n");
    fprintf(fp, "  \"rating\": %d,\n", st->rating);
    fprintf(fp, "  \"streak\": %d,\n", st->streak);
    fprintf(fp, "  \"best_streak\": %d,\n", st->best_streak);
    fprintf(fp, "  \"attempts\": %d,\n", st->attempts);
    fprintf(fp, "  \"solved\": [");
    bool first = true;
    for (int i = 1; i < 64; i++) {
        if (st->solved[i]) {
            fprintf(fp, "%s%d", first ? "" : ", ", i);
            first = false;
        }
    }
    fprintf(fp, "]\n}\n");
    fclose(fp);
    return true;
}

int puzzle_stats_record(puzzle_stats *st, int puzzle_id, int puzzle_rating, bool solved_clean) {
    if (!st) {
        return 0;
    }
    st->attempts++;
    double expected = 1.0 / (1.0 + pow(10.0, (double) (puzzle_rating - st->rating) / 400.0));
    double actual = solved_clean ? 1.0 : 0.0;
    int delta = (int) lround(32.0 * (actual - expected));
    if (solved_clean && delta < 6) {
        delta = 6;
    } else if (!solved_clean && delta > -6) {
        delta = -6;
    }
    st->rating += delta;
    if (st->rating < 600) {
        st->rating = 600;
    }
    if (st->rating > 2800) {
        st->rating = 2800;
    }
    if (solved_clean) {
        st->streak++;
        if (st->streak > st->best_streak) {
            st->best_streak = st->streak;
        }
        if (puzzle_id >= 1 && puzzle_id < 64 && !st->solved[puzzle_id]) {
            st->solved[puzzle_id] = true;
            st->solved_count++;
        }
    } else {
        st->streak = 0;
    }
    return delta;
}
