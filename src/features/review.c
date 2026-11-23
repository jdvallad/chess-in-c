#include "features/review.h"

#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/notation.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static review_move_class classify_move(
    int raw_cpl, double win_drop, bool is_best, int eval_before_mover, int eval_after_mover) {
    if (is_best) {
        return REV_BEST;
    }

    // Special handling for forced-mate transitions so walking into mate or missing mate
    // is NEVER masked by win-probability saturation or centipawn clamping!
    bool was_mated = (eval_before_mover < -MATE_BOUND);
    bool now_mated = (eval_after_mover < -MATE_BOUND);
    bool had_mate = (eval_before_mover > MATE_BOUND);
    bool keeps_mate = (eval_after_mover > MATE_BOUND);

    if (!was_mated && now_mated) {
        // Walked into a forced checkmate when a non-mate defense existed
        int mate_plies = MATE_SCORE + eval_after_mover;
        return (mate_plies <= 4 || eval_before_mover > -800) ? REV_BLUNDER : REV_MISTAKE;
    }
    if (had_mate && !keeps_mate) {
        // Missed a forced checkmate
        return (eval_after_mover < 400) ? REV_BLUNDER : REV_MISTAKE;
    }

    // Normal centipawn / win-probability classification using UNCLAMPED raw_cpl
    if (raw_cpl <= 10 && win_drop < 0.02) {
        return REV_BEST;
    }
    if (raw_cpl <= 45 && win_drop < 0.05) {
        return REV_GOOD;
    }
    if (raw_cpl <= 100 && win_drop < 0.10) {
        return REV_INACCURACY;
    }
    if (raw_cpl <= 240 && win_drop < 0.22) {
        return REV_MISTAKE;
    }
    return REV_BLUNDER;
}

static double accuracy_pct(double win_drop, int raw_cpl, bool blundered_mate) {
    if (blundered_mate) {
        return 0.0;
    }
    // Blend win-probability drop with a mild raw-CPL penalty so hanging pieces in lost
    // positions still lowers accuracy instead of showing 100%
    double effective_drop = win_drop;
    if (raw_cpl > 100 && effective_drop < 0.05) {
        double cpl_drop = (double) raw_cpl / 1200.0;
        if (cpl_drop > 0.35) {
            cpl_drop = 0.35;
        }
        if (cpl_drop > effective_drop) {
            effective_drop = cpl_drop;
        }
    }
    if (effective_drop <= 0.0) {
        return 100.0;
    }
    double pct = 103.1668 * exp(-4.354 * effective_drop) - 3.1669;
    if (pct > 100.0) {
        pct = 100.0;
    }
    if (pct < 0.0) {
        pct = 0.0;
    }
    return pct;
}

const char *review_class_name(review_move_class cls) {
    switch (cls) {
    case REV_BEST: return "best";
    case REV_GOOD: return "good";
    case REV_INACCURACY: return "inaccuracy";
    case REV_MISTAKE: return "mistake";
    default: return "blunder";
    }
}

const char *review_elo_tier(int elo) {
    if (elo >= 2450) {
        return "Grandmaster";
    }
    if (elo >= 2200) {
        return "Master";
    }
    if (elo >= 1950) {
        return "Expert";
    }
    if (elo >= 1650) {
        return "Class A";
    }
    if (elo >= 1350) {
        return "Club";
    }
    if (elo >= 1000) {
        return "Casual";
    }
    return "Novice";
}

static int estimate_game_elo(const review_side_stats *s) {
    if (!s || s->move_count <= 0) {
        return 0;
    }
    double acc = s->accuracy;
    if (acc < 0.0) {
        acc = 0.0;
    }
    if (acc > 100.0) {
        acc = 100.0;
    }
    double acpl = (double) (s->acpl < 0 ? 0 : s->acpl);

    // Smooth performance model combining accuracy % and ACPL
    double elo_acc = 400.0 + 2300.0 * pow(acc / 100.0, 1.85);
    double elo_acpl = 2750.0 * exp(-0.0085 * acpl);
    double raw = 0.55 * elo_acc + 0.45 * elo_acpl;

    // Penalize high blunder/mistake density
    double blunder_rate = (double) s->counts[REV_BLUNDER] / (double) s->move_count;
    double mistake_rate = (double) s->counts[REV_MISTAKE] / (double) s->move_count;
    raw -= blunder_rate * 480.0 + mistake_rate * 160.0;

    // Shrink very short games (< 10 moves) smoothly toward 1350 to avoid extreme noise
    if (s->move_count < 10) {
        double w = (double) s->move_count / 10.0;
        raw = w * raw + (1.0 - w) * 1350.0;
    }

    int rounded = ((int) lround(raw / 50.0)) * 50;
    if (rounded < 400) {
        rounded = 400;
    }
    if (rounded > 2850) {
        rounded = 2850;
    }
    return rounded;
}

bool review_analyze_game_ex(engine *eng, const full_chess *game, const u64 *moves, int move_count,
    int depth, int time_ms, review_eval_fn eval_fn, void *eval_ctx,
    review_progress_cb progress_cb, void *user_data, game_review *out) {
    if ((!eng && !eval_fn) || !game || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->turning_point_ply = -1;

    const u64 *mv_list = moves ? moves : game->move_history;
    int total_plies = moves ? move_count : (int) game->move_count;
    if (total_plies <= 0) {
        return false;
    }

    review_ply *plies = (review_ply *) calloc((size_t) total_plies, sizeof(review_ply));
    int *evals_w = (int *) calloc((size_t) (total_plies + 1), sizeof(int));
    full_chess *replay = full_chess_create();
    if (!plies || !evals_w || !replay) {
        free(plies);
        free(evals_w);
        full_chess_delete(&replay);
        return false;
    }

    replay->game = &replay->current_board;
    *(replay->game) = game->game_history[0];
    replay->game_history[0] = *(replay->game);
    replay->move_count = 0;
    replay->half_move_count = game->half_move_history[0];
    replay->half_move_history[0] = replay->half_move_count;
    replay->start_full_move = game->start_full_move;
    replay->start_black_to_move = game->start_black_to_move;
    replay->is_static = true;
    replay->is_draw = false;
    replay->is_game_over = false;
    set_legal_moves(replay->game, replay->current_legal_moves);

    if (eng && !eval_fn) {
        engine_clear_tt(eng);
    }
    search_result prev_sr
        = eval_fn ? eval_fn(replay, eval_ctx) : engine_search(eng, replay, depth, time_ms);
    evals_w[0] = engine_score_to_white(replay->game, prev_sr.score_cp);

    double w_acc_sum = 0.0, b_acc_sum = 0.0;
    long w_cpl_sum = 0, b_cpl_sum = 0;

    for (int p = 0; p < total_plies; p++) {
        if (progress_cb) {
            progress_cb(p, total_plies, user_data);
        }

        u64 played = mv_list[p];
        bool was_black = is_flipped(replay->game);
        int abs_ply = p + (game->start_black_to_move ? 1 : 0);
        plies[p].move_num = game->start_full_move + (abs_ply / 2);
        plies[p].was_black = was_black;
        plies[p].eval_before_w = evals_w[p];

        static_move_to_san(replay->game, replay->current_legal_moves, played, plies[p].played_san,
            sizeof(plies[p].played_san));
        static_move_to_san(replay->game, replay->current_legal_moves, prev_sr.best_move,
            plies[p].best_san, sizeof(plies[p].best_san));

        make_move(replay, played);

        search_result next_sr;
        int after_w;
        if (replay->is_game_over) {
            memset(&next_sr, 0, sizeof(next_sr));
            after_w = replay->is_draw ? 0 : engine_score_to_white(replay->game, -MATE_SCORE);
        } else {
            next_sr
                = eval_fn ? eval_fn(replay, eval_ctx) : engine_search(eng, replay, depth, time_ms);
            after_w = engine_score_to_white(replay->game, next_sr.score_cp);
        }
        evals_w[p + 1] = after_w;
        plies[p].eval_after_w = after_w;

        double wp_b = engine_win_prob(evals_w[p]);
        double wp_a = engine_win_prob(after_w);
        double mover_b = was_black ? (1.0 - wp_b) : wp_b;
        double mover_a = was_black ? (1.0 - wp_a) : wp_a;
        double win_drop = mover_b - mover_a;
        if (win_drop < 0.0) {
            win_drop = 0.0;
        }

        int eval_before_mover = was_black ? -evals_w[p] : evals_w[p];
        int eval_after_mover = was_black ? -after_w : after_w;

        // Unclamped centipawn loss for accurate move classification (even in -16 positions!)
        int raw_b = (eval_before_mover > MATE_BOUND)
                        ? 3000
                        : ((eval_before_mover < -MATE_BOUND) ? -3000 : eval_before_mover);
        int raw_a = (eval_after_mover > MATE_BOUND)
                        ? 3000
                        : ((eval_after_mover < -MATE_BOUND) ? -3000 : eval_after_mover);
        int raw_cpl = raw_b - raw_a;
        if (raw_cpl < 0) {
            raw_cpl = 0;
        }

        // Clamped CPL for summary ACPL so mate swings don't explode the average
        int cb = (evals_w[p] > 1500) ? 1500 : ((evals_w[p] < -1500) ? -1500 : evals_w[p]);
        int ca = (after_w > 1500) ? 1500 : ((after_w < -1500) ? -1500 : after_w);
        int acpl_delta = was_black ? (ca - cb) : (cb - ca);
        if (acpl_delta < 0) {
            acpl_delta = 0;
        }

        bool is_best = (played == prev_sr.best_move);
        review_move_class cls
            = classify_move(raw_cpl, win_drop, is_best, eval_before_mover, eval_after_mover);
        bool blundered_mate
            = (!is_best && eval_before_mover >= -MATE_BOUND && eval_after_mover < -MATE_BOUND);

        plies[p].cpl = raw_cpl;
        plies[p].win_drop = win_drop;
        plies[p].cls = cls;

        if (win_drop > out->turning_point_drop) {
            out->turning_point_drop = win_drop;
            out->turning_point_ply = p;
        }

        double acc = accuracy_pct(win_drop, raw_cpl, blundered_mate);
        if (!was_black) {
            out->white.counts[cls]++;
            w_acc_sum += acc;
            w_cpl_sum += acpl_delta;
            out->white.move_count++;
        } else {
            out->black.counts[cls]++;
            b_acc_sum += acc;
            b_cpl_sum += acpl_delta;
            out->black.move_count++;
        }

        prev_sr = next_sr;
    }

    out->total_plies = total_plies;
    out->plies = plies;
    out->evals_w = evals_w;
    out->white.accuracy
        = (out->white.move_count > 0) ? (w_acc_sum / (double) out->white.move_count) : 0.0;
    out->black.accuracy
        = (out->black.move_count > 0) ? (b_acc_sum / (double) out->black.move_count) : 0.0;
    out->white.acpl = (out->white.move_count > 0) ? (int) (w_cpl_sum / out->white.move_count) : 0;
    out->black.acpl = (out->black.move_count > 0) ? (int) (b_cpl_sum / out->black.move_count) : 0;
    out->white.est_elo = estimate_game_elo(&out->white);
    out->black.est_elo = estimate_game_elo(&out->black);

    full_chess_delete(&replay);
    return true;
}

bool review_analyze_game(engine *eng, const full_chess *game, const u64 *moves, int move_count,
    int depth, int time_ms, review_progress_cb progress_cb, void *user_data, game_review *out) {
    return review_analyze_game_ex(
        eng, game, moves, move_count, depth, time_ms, NULL, NULL, progress_cb, user_data, out);
}

void review_free(game_review *rev) {
    if (!rev) {
        return;
    }
    free(rev->plies);
    free(rev->evals_w);
    memset(rev, 0, sizeof(*rev));
    rev->turning_point_ply = -1;
}
