#include "features/review.h"

#include "core/board.h"
#include "core/game.h"
#include "core/movegen.h"
#include "core/notation.h"
#include "engine/see.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// Official Lichess winning-chances sigmoid (maps centipawns -> 0.0 .. 100.0%).
double review_lichess_win_pct(int cp) {
    if (cp > MATE_BOUND) {
        return 100.0;
    }
    if (cp < -MATE_BOUND) {
        return 0.0;
    }
    double cl = (double) cp;
    if (cl > 4000.0) {
        cl = 4000.0;
    } else if (cl < -4000.0) {
        cl = -4000.0;
    }
    return 50.0 + 50.0 * (2.0 / (1.0 + exp(-0.00368208 * cl)) - 1.0);
}

static double lichess_move_accuracy(double win_drop_pct, bool blundered_mate) {
    if (blundered_mate) {
        return 0.0;
    }
    if (win_drop_pct <= 0.0) {
        return 100.0;
    }
    double pct = 103.1668 * exp(-0.04354 * win_drop_pct) - 3.1669;
    if (pct > 100.0) {
        pct = 100.0;
    }
    if (pct < 0.0) {
        pct = 0.0;
    }
    return pct;
}

static int quick_material_cp(const chess *b) {
    u64 f = b->friendly_pieces;
    u64 e = b->enemy_pieces;
    u64 tp = b->pawns & PAWN_MASK;
    u64 orth = b->orthogonal_pieces;
    u64 diag = b->diagonal_pieces;
    u64 kings = b->kings;
    int fm = __builtin_popcountll(f & tp) * 100
             + __builtin_popcountll(f & ~(tp | orth | diag | kings)) * 320
             + __builtin_popcountll(f & diag & ~orth) * 330
             + __builtin_popcountll(f & orth & ~diag) * 500
             + __builtin_popcountll(f & diag & orth) * 900;
    int em = __builtin_popcountll(e & tp) * 100
             + __builtin_popcountll(e & ~(tp | orth | diag | kings)) * 320
             + __builtin_popcountll(e & diag & ~orth) * 330
             + __builtin_popcountll(e & orth & ~diag) * 500
             + __builtin_popcountll(e & diag & orth) * 900;
    return fm - em;
}

// Highest net SEE gain the opponent has on any immediate capture in `b`.
static int max_stm_capture_see(const chess *b) {
    chess tmp = *b;
    u64 mvs[MAX_LEGAL_MOVES];
    int n = set_legal_moves(&tmp, mvs);
    int best = 0;
    for (int i = 0; i < n; i++) {
        if (get_end_piece(mvs[i]) != NONE || is_en_passant_move(mvs[i])) {
            int s = engine_see(b, mvs[i]);
            if (s > best) {
                best = s;
            }
        }
    }
    return best;
}

static bool is_brilliant_sacrifice(const chess *before_board, u64 played, int num_legal,
    int move_num, bool is_best, double win_drop_pct, int eval_before_mover, int eval_after_mover) {
    (void) eval_before_mover;
    if (!is_best && win_drop_pct > 1.5) {
        return false;
    }
    // Must be a real choice (not forced) and past the first 4 opening moves
    if (num_legal < 3 || move_num < 5) {
        return false;
    }
    // Don't award Brilliant in trivial +500cp material mop-ups or lost positions
    if (quick_material_cp(before_board) >= 500 || eval_after_mover < -40) {
        return false;
    }
    u8 sp = get_start_piece(played);
    u8 pr = get_promotion_piece(played);
    // 1. Active piece sacrifice onto a defended square (e.g. Qxd8+, Bxh7+, Nxf7!, Rxc3!)
    if (sp != FRIENDLY_PAWN || pr != sp) {
        int see_val = engine_see(before_board, played);
        if (see_val <= -180) {
            return true;
        }
    }
    // 2. Quiet tactical sacrifice: leaving a minor/major piece en prise for an attack or mate!
    if (eval_after_mover >= 50) {
        chess after = *before_board;
        chess_make_move(&after, played);
        int opp_cap_after = max_stm_capture_see(&after);
        if (opp_cap_after >= 280) {
            // Confirm it wasn't just recapturing/ignoring an already-equal trade
            chess before_flipped = *before_board;
            flip_perspective(&before_flipped);
            int opp_cap_before = max_stm_capture_see(&before_flipped);
            if (opp_cap_after >= opp_cap_before + 180) {
                return true;
            }
        }
    }
    return false;
}

// Authentic Lichess win%-based move classification + Chess.com-style Brilliant (!!).
static review_move_class classify_move(const chess *before_board, u64 played, int num_legal,
    int move_num, double win_drop_pct, bool is_best, int eval_before_mover, int eval_after_mover) {
    if (is_brilliant_sacrifice(before_board, played, num_legal, move_num, is_best, win_drop_pct,
            eval_before_mover, eval_after_mover)) {
        return REV_BRILLIANT;
    }
    if (is_best) {
        return REV_BEST;
    }

    // Explicit forced-mate transitions so mate blunders are never masked
    bool was_mated = (eval_before_mover < -MATE_BOUND);
    bool now_mated = (eval_after_mover < -MATE_BOUND);
    bool had_mate = (eval_before_mover > MATE_BOUND);
    bool keeps_mate = (eval_after_mover > MATE_BOUND);

    if (!was_mated && now_mated) {
        int mate_plies = MATE_SCORE + eval_after_mover;
        return (mate_plies <= 4 || eval_before_mover > -550) ? REV_BLUNDER : REV_MISTAKE;
    }
    if (had_mate && !keeps_mate) {
        if (eval_after_mover < 250) {
            return REV_BLUNDER;
        }
        if (eval_after_mover < 600) {
            return REV_MISTAKE;
        }
        return REV_INACCURACY;
    }

    // Pure Lichess Win% drop thresholds (on 0..100% scale, matching lila Advice.scala):
    //   < 2%  -> Best
    //   < 5%  -> Good
    //   < 10% -> Inaccuracy (?!)   [delta winningChances >= 0.10]
    //   < 20% -> Mistake (?)       [delta winningChances >= 0.20]
    //  >= 20% -> Blunder (??)      [delta winningChances >= 0.40]
    if (win_drop_pct < 2.0) {
        return REV_BEST;
    }
    if (win_drop_pct < 5.0) {
        return REV_GOOD;
    }
    if (win_drop_pct < 10.0) {
        return REV_INACCURACY;
    }
    if (win_drop_pct < 20.0) {
        return REV_MISTAKE;
    }
    return REV_BLUNDER;
}

const char *review_class_name(review_move_class cls) {
    switch (cls) {
    case REV_BRILLIANT: return "brilliant";
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

// Empirically calibrated Lichess/FIDE Accuracy % -> Performance Elo curve.
static double accuracy_to_elo_curve(double acc) {
    static const double xs[]
        = { 0.0, 35.0, 50.0, 60.0, 68.0, 74.0, 80.0, 85.0, 90.0, 93.5, 96.5, 98.5, 100.0 };
    static const double ys[]
        = { 400.0, 500.0, 650.0, 900.0, 1100.0, 1300.0, 1550.0, 1800.0, 2050.0, 2250.0, 2450.0,
            2650.0, 2800.0 };
    const int n = (int) (sizeof(xs) / sizeof(xs[0]));
    if (acc <= xs[0]) {
        return ys[0];
    }
    if (acc >= xs[n - 1]) {
        return ys[n - 1];
    }
    for (int i = 0; i < n - 1; i++) {
        if (acc <= xs[i + 1]) {
            double t = (acc - xs[i]) / (xs[i + 1] - xs[i]);
            return ys[i] + t * (ys[i + 1] - ys[i]);
        }
    }
    return ys[n - 1];
}

// Calibrated Performance Elo: blends Lichess accuracy curve + capped ACPL without double-penalizing!
static int estimate_game_elo(const review_side_stats *s, int game_result_for_side) {
    if (!s || s->move_count <= 0) {
        return 0;
    }
    double acc = s->accuracy;
    if (acc < 0.0) {
        acc = 0.0;
    } else if (acc > 100.0) {
        acc = 100.0;
    }
    double acpl = (double) (s->acpl < 0 ? 0 : (s->acpl > 250 ? 250 : s->acpl));

    double elo_acc = accuracy_to_elo_curve(acc);
    double elo_acpl = 2650.0 * exp(-0.0085 * acpl);
    double raw = 0.78 * elo_acc + 0.22 * elo_acpl;

    // Gentle outcome nudge on real games (>= 12 moves)
    if (s->move_count >= 12) {
        if (game_result_for_side > 0) {
            raw += 40.0;
        } else if (game_result_for_side < 0) {
            raw -= 40.0;
        }
    }

    // Smooth Bayesian shrinkage toward 1250 on short games (< 14 moves) to prevent wild 2700 spikes
    if (s->move_count < 14) {
        double w = (double) s->move_count / 14.0;
        raw = w * raw + (1.0 - w) * 1250.0;
    }

    int rounded = ((int) lround(raw / 25.0)) * 25;
    if (rounded < 400) {
        rounded = 400;
    }
    if (rounded > 2800) {
        rounded = 2800;
    }
    return rounded;
}

// Lichess combined accuracy: 50% volatility-weighted arithmetic mean + 50% harmonic mean.
static double compute_lichess_side_accuracy(
    const review_ply *plies, const double *win_pcts_w, int total_plies, bool for_black) {
    double w_sum = 0.0;
    double w_acc_sum = 0.0;
    double h_inv_sum = 0.0;
    int n = 0;

    for (int p = 0; p < total_plies; p++) {
        if (plies[p].was_black != for_black) {
            continue;
        }
        // Compute local win% volatility (stddev over a 5-ply window centered at p)
        int lo = p - 2;
        int hi = p + 2;
        if (lo < 0) {
            lo = 0;
        }
        if (hi > total_plies) {
            hi = total_plies;
        }
        int wn = hi - lo + 1;
        double mean = 0.0;
        for (int k = lo; k <= hi; k++) {
            mean += win_pcts_w[k];
        }
        mean /= (double) wn;
        double var = 0.0;
        for (int k = lo; k <= hi; k++) {
            double diff = win_pcts_w[k] - mean;
            var += diff * diff;
        }
        double sd = sqrt(var / (double) wn);
        double weight = sd;
        if (weight < 0.5) {
            weight = 0.5;
        } else if (weight > 12.0) {
            weight = 12.0;
        }

        double ma = plies[p].move_acc;
        w_sum += weight;
        w_acc_sum += weight * ma;
        h_inv_sum += 1.0 / (ma < 2.0 ? 2.0 : ma);
        n++;
    }
    if (n <= 0) {
        return 0.0;
    }
    double weighted_mean = (w_sum > 1e-9) ? (w_acc_sum / w_sum) : 0.0;
    double harmonic_mean = (double) n / h_inv_sum;
    double combined = 0.5 * weighted_mean + 0.5 * harmonic_mean;
    if (combined > 100.0) {
        combined = 100.0;
    }
    if (combined < 0.0) {
        combined = 0.0;
    }
    return combined;
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
    double *win_pcts_w = (double *) calloc((size_t) (total_plies + 1), sizeof(double));
    full_chess *replay = full_chess_create();
    if (!plies || !evals_w || !win_pcts_w || !replay) {
        free(plies);
        free(evals_w);
        free(win_pcts_w);
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
    win_pcts_w[0] = review_lichess_win_pct(evals_w[0]);

    long w_cpl_sum = 0, b_cpl_sum = 0;

    for (int p = 0; p < total_plies; p++) {
        if (progress_cb) {
            progress_cb(p, total_plies, user_data);
        }

        u64 played = mv_list[p];
        chess before_board = *(replay->game);
        int n_legal = num_legal_moves(replay);
        bool was_black = is_flipped(replay->game);
        int abs_ply = p + (game->start_black_to_move ? 1 : 0);
        int mv_num = game->start_full_move + (abs_ply / 2);
        plies[p].move_num = mv_num;
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
        win_pcts_w[p + 1] = review_lichess_win_pct(after_w);
        plies[p].eval_after_w = after_w;

        double mover_wp_b = was_black ? (100.0 - win_pcts_w[p]) : win_pcts_w[p];
        double mover_wp_a = was_black ? (100.0 - win_pcts_w[p + 1]) : win_pcts_w[p + 1];
        double win_drop_pct = mover_wp_b - mover_wp_a;
        if (win_drop_pct < 0.0) {
            win_drop_pct = 0.0;
        }

        int eval_before_mover = was_black ? -evals_w[p] : evals_w[p];
        int eval_after_mover = was_black ? -after_w : after_w;

        // Capped CPL (Lichess caps evals at +/-1000 cp when computing ACPL!)
        int cb = (evals_w[p] > 1000) ? 1000 : ((evals_w[p] < -1000) ? -1000 : evals_w[p]);
        int ca = (after_w > 1000) ? 1000 : ((after_w < -1000) ? -1000 : after_w);
        int acpl_delta = was_black ? (ca - cb) : (cb - ca);
        if (acpl_delta < 0) {
            acpl_delta = 0;
        }

        bool is_best = (played == prev_sr.best_move);
        review_move_class cls = classify_move(&before_board, played, n_legal, mv_num, win_drop_pct,
            is_best, eval_before_mover, eval_after_mover);
        bool blundered_mate
            = (!is_best && eval_before_mover >= -MATE_BOUND && eval_after_mover < -MATE_BOUND);
        double m_acc = lichess_move_accuracy(win_drop_pct, blundered_mate);

        plies[p].cpl = acpl_delta;
        plies[p].win_drop = win_drop_pct / 100.0;
        plies[p].move_acc = m_acc;
        plies[p].cls = cls;

        if (plies[p].win_drop > out->turning_point_drop) {
            out->turning_point_drop = plies[p].win_drop;
            out->turning_point_ply = p;
        }

        if (!was_black) {
            out->white.counts[cls]++;
            w_cpl_sum += acpl_delta;
            out->white.move_count++;
        } else {
            out->black.counts[cls]++;
            b_cpl_sum += acpl_delta;
            out->black.move_count++;
        }

        prev_sr = next_sr;
    }

    int final_white_res = 0;
    if (replay->is_game_over && !replay->is_draw) {
        final_white_res = is_flipped(replay->game) ? 1 : -1;
    } else if (evals_w[total_plies] >= 400) {
        final_white_res = 1;
    } else if (evals_w[total_plies] <= -400) {
        final_white_res = -1;
    }

    out->total_plies = total_plies;
    out->plies = plies;
    out->evals_w = evals_w;
    out->win_pcts_w = win_pcts_w;
    out->white.accuracy = compute_lichess_side_accuracy(plies, win_pcts_w, total_plies, false);
    out->black.accuracy = compute_lichess_side_accuracy(plies, win_pcts_w, total_plies, true);
    out->white.acpl = (out->white.move_count > 0) ? (int) (w_cpl_sum / out->white.move_count) : 0;
    out->black.acpl = (out->black.move_count > 0) ? (int) (b_cpl_sum / out->black.move_count) : 0;
    out->white.est_elo = estimate_game_elo(&out->white, final_white_res);
    out->black.est_elo = estimate_game_elo(&out->black, -final_white_res);

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
    free(rev->win_pcts_w);
    memset(rev, 0, sizeof(*rev));
    rev->turning_point_ply = -1;
}
