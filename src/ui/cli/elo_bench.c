#define _POSIX_C_SOURCE 200809L

#include "engine/engine.h"
#include "features/gauntlet.h"
#include "features/uci_client.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static gauntlet *g_cli_gauntlet = NULL;

static void handle_sigint(int sig) {
    (void) sig;
    if (g_cli_gauntlet) {
        gauntlet_stop(g_cli_gauntlet);
    }
}

int main(int argc, char **argv) {
    int games_per_match = 6;
    int threads = 0; // 0 = auto CPU count
    int only_level = 0;
    int sf_time_ms = 500;
    bool force_internal = false;
    const char *sf_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--games") == 0 && i + 1 < argc) {
            games_per_match = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--threads") == 0 || strcmp(argv[i], "-j") == 0)
                   && i + 1 < argc) {
            threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--level") == 0 && i + 1 < argc) {
            only_level = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--sf-ms") == 0 && i + 1 < argc) {
            sf_time_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--internal") == 0) {
            force_internal = true;
        } else if (strcmp(argv[i], "--stockfish") == 0 && i + 1 < argc) {
            sf_path = argv[++i];
        }
    }

    signal(SIGINT, handle_sigint);

    gauntlet *g = gauntlet_create(sf_path, ELO_RATINGS_FILE);
    g_cli_gauntlet = g;

    gauntlet_status st;
    gauntlet_snapshot(g, &st);
    int use_threads = (threads > 0) ? threads : st.cpu_cores;

    printf("\n  +==================================================================+\n");
    printf(
        "  |        CHESS-IN-C  ::  MULTITHREADED ELO GAUNTLET (%2d cores)     |\n", use_threads);
    printf("  +==================================================================+\n\n");

    gauntlet_start(g, games_per_match, use_threads, only_level, !force_internal, sf_time_ms);
    gauntlet_snapshot(g, &st);

    if (st.vs_stockfish) {
        printf("  Opponent: %s  |  Threads: %d  |  Games/level: %d\n", st.opponent_name,
            st.num_threads, st.games_per_level);
    } else {
        printf("  Mode: Internal Ladder (Stockfish not found)  |  Threads: %d  |  Games/pair: %d\n",
            st.num_threads, st.games_per_level);
    }
    printf("  Saving ratings to: %s\n\n", ELO_RATINGS_FILE);

    int printed_done[GAUNTLET_NUM_LEVELS] = { 0 };
    printf("  %-20s %-15s %-11s %-8s %s\n", "Bot / Level", "Opponent", "W-D-L", "Score", "Est. Elo");
    printf("  ------------------------------------------------------------------------\n");

    struct timespec nap = { 0, 150000000L }; // 150ms poll
    while (true) {
        gauntlet_snapshot(g, &st);
        for (int i = 0; i < GAUNTLET_NUM_LEVELS; i++) {
            const level_progress *lp = &st.levels[i];
            if (lp->state == 2 && !printed_done[i] && lp->done_games > 0) {
                printed_done[i] = 1;
                double score
                    = ((double) lp->wins + 0.5 * (double) lp->draws) / (double) lp->done_games;
                char lbl[40];
                snprintf(lbl, sizeof(lbl), "%s", gauntlet_level_name(lp->level));
                if (lp->is_relative) {
                    printf("  %-20s %-15s %2d-%2d-%-4d  %5.1f%%   %+d (rel)\n", lbl,
                        lp->opponent_label, lp->wins, lp->draws, lp->losses, score * 100.0,
                        lp->est_elo);
                } else {
                    printf("  %-20s %-15s %2d-%2d-%-4d  %5.1f%%   ~%d Elo\n", lbl,
                        lp->opponent_label, lp->wins, lp->draws, lp->losses, score * 100.0,
                        lp->est_elo);
                }
                fflush(stdout);
            }
        }
        if (!st.running) {
            break;
        }
        nanosleep(&nap, NULL);
    }

    gauntlet_wait(g);
    gauntlet_snapshot(g, &st);
    printf("\n  Completed %d/%d games in %.1fs. Ratings saved to %s.\n\n", st.completed_games_total,
        st.scheduled_games_total, st.elapsed_sec, ELO_RATINGS_FILE);

    g_cli_gauntlet = NULL;
    gauntlet_delete(&g);
    return 0;
}
