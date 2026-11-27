#!/usr/bin/env python3
"""Project 3000 — Parallel SPSA Search Parameter Tuner for Titan-C / Apex-C.

Simultaneously tunes all 16 search pruning/LMR/CorrHist constants in `src/engine/search.c`
using fast fixed-node self-play matches between `build/chess_uci (+delta)` and `(-delta)`.

Safe to run alongside Zero-C overnight using `--threads 2` (or `--threads 10` solo)!

Usage:
  python3 src/tools/spsa_tuner.py --threads 2 --iters 200 --pairs-per-iter 6 --nodes 12000
"""
import argparse
import concurrent.futures
import json
import math
import os
import random
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
UCI_BIN = os.path.join(ROOT, "build", "chess_uci")

# (name, init_val, min_val, max_val, step_c)
PARAMS = [
    ("RFPBase", 75.0, 40.0, 120.0, 8.0),
    ("RFPImpr", 15.0, 0.0, 40.0, 4.0),
    ("FutMult", 110.0, 60.0, 180.0, 10.0),
    ("FutBase", 70.0, 20.0, 140.0, 10.0),
    ("ProbCutM", 190.0, 110.0, 290.0, 14.0),
    ("AspDelta", 25.0, 10.0, 50.0, 4.0),
    ("SEDepth", 7.0, 5.0, 10.0, 1.0),
    ("SEDblM", 35.0, 15.0, 75.0, 5.0),
    ("HistPrune", 2400.0, 1000.0, 4800.0, 250.0),
    ("HistLMRDiv", 8192.0, 3072.0, 16384.0, 700.0),
    ("RazMargin", 240.0, 120.0, 420.0, 20.0),
    ("CorrDiv", 512.0, 192.0, 1024.0, 48.0),
    ("LMRCapDiv", 6144.0, 2048.0, 12288.0, 600.0),
    ("NMPDiv", 6.0, 3.0, 10.0, 1.0),
]

OPENINGS = [
    "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6",
    "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6",
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6",
    "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6",
    "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6",
    "e2e4 e7e6 d2d4 d7d5 b1c3 g8f6",
    "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4",
    "g1f3 d7d5 d2d4 g8f6 c2c4 c7c6",
]


def _spawn_uci(opts: dict, nnue_path: str = ""):
    p = subprocess.Popen(
        [UCI_BIN],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1,
        cwd=ROOT,
    )
    p.stdin.write("uci\nsetoption name OwnBook value false\nsetoption name Threads value 1\nsetoption name Hash value 16\n")
    if nnue_path and os.path.exists(nnue_path):
        p.stdin.write(f"setoption name EvalFile value {nnue_path}\n")
    for k, v in opts.items():
        p.stdin.write(f"setoption name {k} value {int(round(v))}\n")
    p.stdin.write("isready\n")
    p.stdin.flush()
    while True:
        ln = p.stdout.readline()
        if not ln or ln.strip() == "readyok":
            break
    return p


def _search_move(p, moves: list, nodes: int):
    pos = "position startpos" + (f" moves {' '.join(moves)}" if moves else "")
    p.stdin.write(f"{pos}\ngo nodes {nodes}\n")
    p.stdin.flush()
    last_cp = 0
    while True:
        ln = p.stdout.readline()
        if not ln:
            return "0000", 0
        s = ln.strip()
        if " score cp " in s:
            toks = s.split()
            try:
                last_cp = int(toks[toks.index("cp") + 1])
            except Exception:
                pass
        elif " score mate " in s:
            toks = s.split()
            try:
                m = int(toks[toks.index("mate") + 1])
                last_cp = 29000 if m > 0 else -29000
            except Exception:
                pass
        elif s.startswith("bestmove"):
            parts = s.split()
            return (parts[1] if len(parts) >= 2 else "0000"), last_cp


def play_game(white_opts: dict, black_opts: dict, opening: str, nodes: int, nnue_path: str = "") -> float:
    """Returns score for White: 1.0 = White win, 0.5 = Draw, 0.0 = Black win."""
    pw = _spawn_uci(white_opts, nnue_path)
    pb = _spawn_uci(black_opts, nnue_path)
    moves = opening.split() if opening else []
    win_streak = 0
    draw_streak = 0
    res = 0.5
    try:
        for ply in range(180):
            white_turn = (len(moves) % 2) == 0
            p = pw if white_turn else pb
            mv, cp = _search_move(p, moves, nodes)
            if mv == "0000" or mv == "(none)":
                res = 0.0 if (white_turn and cp < -20000) else (1.0 if (not white_turn and cp < -20000) else 0.5)
                break
            moves.append(mv)
            w_cp = cp if white_turn else -cp
            if abs(w_cp) >= 1200:
                win_streak += 1
                draw_streak = 0
                if win_streak >= 4:
                    res = 1.0 if w_cp > 0 else 0.0
                    break
            elif ply >= 50 and abs(w_cp) <= 15:
                draw_streak += 1
                win_streak = 0
                if draw_streak >= 8:
                    res = 0.5
                    break
            else:
                win_streak = 0
                draw_streak = 0
    finally:
        for p in (pw, pb):
            try:
                p.stdin.write("quit\n")
                p.stdin.flush()
                p.wait(timeout=1)
            except Exception:
                p.kill()
    return res


def main():
    ap = argparse.ArgumentParser(description="Project 3000 Parallel SPSA Tuner")
    ap.add_argument("--threads", type=int, default=2, help="Parallel games (2 while Zero-C trains, 8-10 solo)")
    ap.add_argument("--iters", type=int, default=150)
    ap.add_argument("--pairs-per-iter", type=int, default=4, help="Opening pairs (2 games each) per SPSA step")
    ap.add_argument("--nodes", type=int, default=12000)
    ap.add_argument("--nnue", default="")
    ap.add_argument("--resume", action="store_true", help="Resume from existing --out parameters instead of baseline")
    ap.add_argument("--out", default="data/spsa_best.json")
    ap.add_argument("--status", default="data/p3k_spsa_status.json")
    args = ap.parse_args()

    if not args.nnue:
        default_v4 = os.path.join(ROOT, "data", "titan_v6_1024.nnue")
        if os.path.exists(default_v4):
            args.nnue = default_v4

    hist = []
    if args.resume and os.path.exists(args.status):
        try:
            old_st = json.load(open(args.status))
            if isinstance(old_st.get("history"), list):
                hist = list(old_st["history"])
        except Exception:
            pass

    def write_status(running: bool, it: int, plus_sc: float = 0.0, n_g: int = 0, rounded: dict = None):
        if not args.status:
            return
        os.makedirs(os.path.dirname(args.status) or ".", exist_ok=True)
        blob = {
            "ok": True,
            "running": running,
            "iter": it,
            "max_iters": args.iters,
            "threads": args.threads,
            "nodes": args.nodes,
            "plus_score": round(plus_sc, 1),
            "n_games": n_g,
            "progress_pct": round(100.0 * it / max(1, args.iters), 1),
            "params": rounded or {k: int(round(v)) for k, v in theta.items()},
            "history": hist[-400:],
            "elapsed_sec": round(time.time() - t0, 1),
            "updated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        }
        with open(args.status + ".tmp", "w") as sf:
            json.dump(blob, sf)
        os.replace(args.status + ".tmp", args.status)

    theta = {p[0]: p[1] for p in PARAMS}
    if args.resume and os.path.exists(args.out):
        try:
            saved = json.load(open(args.out))
            for k, v in saved.get("params", {}).items():
                if k in theta:
                    theta[k] = float(v)
            print(f"[SPSA] Resumed from {args.out}")
        except Exception:
            pass

    rng = random.Random(42)
    t0 = time.time()
    write_status(True, 0)
    print(
        f"[SPSA] Tuning {len(PARAMS)} params | threads={args.threads} | nodes={args.nodes} | "
        f"pairs/iter={args.pairs_per_iter} | nnue={args.nnue or 'default v3'}"
    )

    last_it = 0
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.threads) as pool:
            for it in range(1, args.iters + 1):
                # Dimensionless step_scale (~0.18 -> ~0.06 over 200-300 iters) so parameters actually
                # traverse their [min, max] trust region rather than stalling at +-1 integer!
                step_scale = 0.18 / (((it / 35.0) + 1.0) ** 0.50)
                c_scale = 1.0 / (it ** 0.101)
                delta = {p[0]: rng.choice((-1.0, 1.0)) for p in PARAMS}

                plus_opts = {}
                minus_opts = {}
                for name, _iv, lo, hi, c0 in PARAMS:
                    ck = c0 * c_scale
                    plus_opts[name] = max(lo, min(hi, theta[name] + delta[name] * ck))
                    minus_opts[name] = max(lo, min(hi, theta[name] - delta[name] * ck))

                futs = []
                for _ in range(args.pairs_per_iter):
                    op = rng.choice(OPENINGS)
                    futs.append((1, pool.submit(play_game, plus_opts, minus_opts, op, args.nodes, args.nnue)))
                    futs.append((-1, pool.submit(play_game, minus_opts, plus_opts, op, args.nodes, args.nnue)))

                plus_score = 0.0
                n_games = len(futs)
                for sign, fut in futs:
                    w_sc = fut.result()
                    plus_score += w_sc if sign == 1 else (1.0 - w_sc)

                diff = (plus_score - (n_games - plus_score)) / float(n_games)  # in [-1, 1]
                for name, _iv, lo, hi, c0 in PARAMS:
                    theta[name] = max(lo, min(hi, theta[name] + step_scale * c0 * diff * delta[name]))

                rounded = {k: int(round(v)) for k, v in theta.items()}
                last_it = it
                hist.append({
                    "iter": it,
                    "plus_score": round(plus_score, 1),
                    "n_games": n_games,
                    "win_pct": round(100.0 * plus_score / max(1, n_games), 1),
                    "elapsed": round(time.time() - t0, 1),
                    "RFPBase": rounded.get("RFPBase", 90),
                    "FutMult": rounded.get("FutMult", 85),
                    "HistLMRDiv": rounded.get("HistLMRDiv", 4500),
                    "CorrDiv": rounded.get("CorrDiv", 190),
                    "SEDepth": rounded.get("SEDepth", 7),
                    "NMPDiv": rounded.get("NMPDiv", 180),
                })
                print(
                    f"[SPSA] Iter {it:3d}/{args.iters} | +delta score={plus_score:.1f}/{n_games} | "
                    f"{time.time()-t0:.0f}s | RFP={rounded['RFPBase']} Fut={rounded['FutMult']} "
                    f"LMR={rounded['HistLMRDiv']} Corr={rounded['CorrDiv']}",
                    flush=True,
                )
                os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
                with open(args.out + ".tmp", "w") as f:
                    json.dump({"iter": it, "params": rounded, "updated_at": time.strftime("%Y-%m-%d %H:%M:%S")}, f, indent=2)
                os.replace(args.out + ".tmp", args.out)
                write_status(True, it, plus_score, n_games, rounded)
    finally:
        write_status(False, last_it)
        try:
            sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
            import training_reports
            training_reports.generate_all_reports()
        except Exception:
            pass


if __name__ == "__main__":
    main()
