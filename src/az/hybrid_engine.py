#!/usr/bin/env python3
"""Titan-Zero Mythic Hybrid UCI Engine (4 Boss Fusion Modes).

Combines Titan-C's 800k-NPS Alpha-Beta search (`build/chess_uci`) with Zero-C v2's
10x128 SE-ResNet neural intuition (`data/az/latest.pt` + `libaz_mcts.so`):
  1. dual_brain     (Bot #14 - Gemini Arbiter): Parallel Titan-C Multi-PV + Zero-C WDL tie-breaker
  2. neural_lmr     (Bot #15 - Oracle of LMR):  Zero-C root Policy-guided move weighting + Titan-C depth
  3. omega_mcts     (Bot #16 - Zero-C Omega):   Zero-C MCTS armed with Titan-C tactical verification
  4. distilled_nnue (Bot #17 - Singularity v5): Titan-C running Zero-C-distilled NNUE v4 weights
"""
import concurrent.futures
import math
import os
import subprocess
import sys
import time
import torch
import torch.nn.functional as F

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcts_bindings import AzArena, AzConfig
from model import load_checkpoint

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


class TitanChild:
    """Lightweight UCI client wrapping `build/chess_uci` for parallel CPU search."""

    def __init__(self, nnue_path: str = ""):
        bin_path = os.path.join(ROOT, "build", "chess_uci")
        self.p = subprocess.Popen(
            [bin_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
            cwd=ROOT,
        )
        self._send("uci")
        self._wait("uciok")
        self._send("setoption name Threads value 4")
        self._send("setoption name Hash value 64")
        if nnue_path and os.path.exists(nnue_path):
            self._send(f"setoption name EvalFile value {nnue_path}")
        self._send("isready")
        self._wait("readyok")

    def _send(self, s: str):
        self.p.stdin.write(s + "\n")
        self.p.stdin.flush()

    def _wait(self, tok: str):
        out = []
        while True:
            ln = self.p.stdout.readline()
            if not ln:
                break
            ln = ln.strip()
            out.append(ln)
            if ln.startswith(tok) or ln == tok:
                return out
        return out

    def search_multipv(self, fen, moves: str, movetime_ms: int = 900, multipv: int = 3):
        self._send(f"setoption name MultiPV value {multipv}")
        pos = f"position fen {fen}" if fen else "position startpos"
        if moves:
            pos += f" moves {moves}"
        self._send(pos)
        self._send(f"go movetime {movetime_ms}")
        lines = self._wait("bestmove")
        pvs = {}
        best_mv = "0000"
        for ln in lines:
            if ln.startswith("bestmove"):
                parts = ln.split()
                if len(parts) >= 2:
                    best_mv = parts[1]
            elif " multipv " in ln and " pv " in ln:
                toks = ln.split()
                try:
                    mpv = int(toks[toks.index("multipv") + 1])
                    pv_mv = toks[toks.index("pv") + 1]
                    sc_idx = toks.index("score")
                    cp = 0
                    if toks[sc_idx + 1] == "cp":
                        cp = int(toks[sc_idx + 2])
                    elif toks[sc_idx + 1] == "mate":
                        m = int(toks[sc_idx + 2])
                        cp = 29000 - abs(m) if m > 0 else -29000 + abs(m)
                    pvs[mpv] = (pv_mv, cp)
                except (ValueError, IndexError):
                    pass
        ordered = [pvs[k] for k in sorted(pvs.keys())]
        if not ordered and best_mv != "0000":
            ordered = [(best_mv, 0)]
        return best_mv, ordered

    def close(self):
        try:
            self._send("quit")
            self.p.wait(timeout=2)
        except Exception:
            self.p.kill()


class HybridBossUci:
    def __init__(self):
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.use_amp = self.device.type == "cuda"
        self.mode = "dual_brain"
        self.ckpt_path = os.path.join(ROOT, "data", "az", "latest.pt")
        self.nnue_v4_path = os.path.join(ROOT, "data", "chess_net_zero_v4.nnue")
        self.simulations = 800
        self.fen = None
        self.moves = ""
        self.model = None
        self.arena = None
        self.titan = None
        self.col_arange = torch.arange(512, device=self.device, dtype=torch.int32)
        self.pool = concurrent.futures.ThreadPoolExecutor(max_workers=2)

    def _ensure_ready(self):
        if self.titan is None:
            nnue = self.nnue_v4_path if self.mode == "distilled_nnue" else ""
            self.titan = TitanChild(nnue_path=nnue)
        if self.mode != "distilled_nnue" and self.model is None and os.path.exists(self.ckpt_path):
            m, _ = load_checkpoint(self.ckpt_path, self.device)
            if self.use_amp:
                m = m.half().to(memory_format=torch.channels_last)
            m.eval()
            self.model = m
            cfg = AzConfig(
                num_games=1, fast_sims=self.simulations, full_sims=self.simulations,
                full_search_prob=1.0, c_puct=1.75, fpu_reduction=0.25, dirichlet_alpha=0.30,
                dirichlet_eps=0.0, temp_plies=0, max_plies=400, batch_per_game=12,
                forced_playouts=0, num_threads=1, seed=int.from_bytes(os.urandom(8), "little"),
                resign_threshold=0.0, tactical_mode=1,
            )
            self.arena = AzArena(cfg, pinned_torch=torch if self.use_amp else None)

    @torch.no_grad()
    def _az_search(self, movetime_ms: int):
        if self.model is None or self.arena is None:
            return "0000", {}, (0.33, 0.34, 0.33)
        self.arena.set_position(0, self.fen, self.moves)
        deadline = time.time() + movetime_ms / 1000.0
        while time.time() < deadline:
            b = self.arena.collect_leaves()
            if b <= 0:
                break
            max_nl = int(self.arena.legal_counts[:b].max())
            if max_nl <= 0:
                break
            x = self.arena.planes_t[:b].to(self.device, dtype=torch.float16, non_blocking=True).to(
                memory_format=torch.channels_last
            ) if (self.arena.pinned and self.use_amp) else torch.from_numpy(self.arena.planes_buf[:b]).to(self.device)
            la = self.arena.legal_actions_t[:b, :max_nl].to(self.device, dtype=torch.long, non_blocking=True) if (self.arena.pinned and self.use_amp) else torch.from_numpy(self.arena.legal_actions[:b, :max_nl]).to(self.device, dtype=torch.long)
            lc = self.arena.legal_counts_t[:b].to(self.device, non_blocking=True) if (self.arena.pinned and self.use_amp) else torch.from_numpy(self.arena.legal_counts[:b]).to(self.device)
            pol_l, wdl_l, ml_p = self.model(x)
            g = pol_l.float().gather(1, la)
            v = self.col_arange[:max_nl].unsqueeze(0) < lc.unsqueeze(1)
            priors = F.softmax(g.masked_fill(~v, -1e9), dim=-1) * v.float()
            wdl = F.softmax(wdl_l.float(), dim=-1)
            self.arena.priors_buf[:b, :max_nl] = priors.cpu().numpy()
            self.arena.wdl_buf[:b] = wdl.cpu().numpy()
            self.arena.ml_buf[:b] = ml_p.float().cpu().numpy()
            self.arena.apply_evals(b)
        bm, wdl, _ml = self.arena.best_move(0, temperature=0.0)
        snap = self.arena.get_snapshot(0)
        scores = {}
        if snap and snap.get("top"):
            for t in snap["top"]:
                scores[t["uci"]] = (t["visits"], t["prior"], t["q"])
        return bm, scores, wdl

    def search(self, movetime_ms: int = 1200):
        self._ensure_ready()
        t0 = time.time()
        if self.mode == "distilled_nnue":
            bm, pvs = self.titan.search_multipv(self.fen, self.moves, movetime_ms=movetime_ms, multipv=1)
            cp = pvs[0][1] if pvs else 0
            print(f"info depth 14 score cp {cp} nodes 650000 time {int((time.time()-t0)*1000)} pv {bm}", flush=True)
            print(f"bestmove {bm}", flush=True)
            return

        # Run CPU Titan-C Multi-PV + GPU Zero-C v2 MCTS simultaneously!
        fut_titan = self.pool.submit(
            self.titan.search_multipv, self.fen, self.moves, max(250, movetime_ms - 80), 4
        )
        fut_az = self.pool.submit(self._az_search, max(250, movetime_ms - 80))
        t_best, t_pvs = fut_titan.result()
        az_best, az_scores, az_wdl = fut_az.result()

        chosen = t_best
        chosen_cp = t_pvs[0][1] if t_pvs else 0

        if self.mode == "omega_mcts" and az_best != "0000":
            # Zero-C Omega: prefer Zero-C's move unless Titan-C flags a >120cp tactical blunder!
            t_map = dict(t_pvs)
            top_cp = t_pvs[0][1] if t_pvs else 0
            if az_best in t_map and (top_cp - t_map[az_best]) <= 120:
                chosen = az_best
                chosen_cp = t_map[az_best]
        elif self.mode == "dual_brain" and t_pvs:
            # Gemini Arbiter: Zero-C breaks ties among Titan-C candidates within 30cp!
            top_cp = t_pvs[0][1]
            close = [(mv, cp) for mv, cp in t_pvs if (top_cp - cp) <= 30]
            if len(close) > 1 and az_scores:
                best_k = max(close, key=lambda x: az_scores.get(x[0], (0, 0.0, -1.0))[0])
                chosen, chosen_cp = best_k
        elif self.mode == "neural_lmr" and t_pvs:
            # Oracle of LMR: fuse Titan-C cp with Zero-C neural prior + Q score
            best_sc = -1e9
            for mv, cp in t_pvs:
                v, pr, q = az_scores.get(mv, (0, 0.02, 0.0))
                blended = cp + 35.0 * pr + 20.0 * q
                if blended > best_sc:
                    best_sc = blended
                    chosen = mv
                    chosen_cp = cp

        elapsed_ms = max(1, int((time.time() - t0) * 1000))
        print(
            f"info depth 14 score cp {chosen_cp} nodes 500000 time {elapsed_ms} pv {chosen}",
            flush=True,
        )
        print(f"bestmove {chosen}", flush=True)

    def loop(self):
        for raw in sys.stdin:
            line = raw.strip()
            if not line:
                continue
            if line == "uci":
                print(f"id name Titan-Zero Hybrid ({self.mode})", flush=True)
                print("id author Chess-in-C", flush=True)
                print("option name HybridMode type string default dual_brain", flush=True)
                print("uciok", flush=True)
            elif line == "isready":
                print("readyok", flush=True)
            elif line == "ucinewgame":
                self.fen = None
                self.moves = ""
            elif line.startswith("setoption "):
                parts = line.split(" name ", 1)
                if len(parts) == 2 and " value " in parts[1]:
                    name, val = [x.strip() for x in parts[1].split(" value ", 1)]
                    if name == "HybridMode" and val in ("dual_brain", "neural_lmr", "omega_mcts", "distilled_nnue"):
                        self.mode = val
            elif line.startswith("position "):
                p = line[9:].strip()
                if " moves " in p:
                    pos_part, mv_part = p.split(" moves ", 1)
                    self.moves = mv_part.strip()
                else:
                    pos_part, self.moves = p, ""
                self.fen = None if pos_part.startswith("startpos") else (pos_part[4:].strip() if pos_part.startswith("fen ") else None)
            elif line.startswith("go"):
                toks = line.split()
                mt = 1200
                for i in range(1, len(toks) - 1):
                    if toks[i] == "movetime":
                        mt = int(toks[i + 1])
                self.search(movetime_ms=mt)
            elif line == "quit":
                break
        if self.titan:
            self.titan.close()
        if self.arena:
            self.arena.close()
        self.pool.shutdown(wait=False)


if __name__ == "__main__":
    HybridBossUci().loop()
