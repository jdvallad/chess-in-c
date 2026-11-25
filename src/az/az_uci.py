#!/usr/bin/env python3
"""Zero-C v2 UCI Engine: Pure FP16 Batched C MCTS + Subtree Reuse + Path-C Tactical Hybrid."""
import math
import os
import sys
import time
import numpy as np
import torch
import torch.nn.functional as F

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcts_bindings import AzArena, AzConfig
from model import ZeroCNet, load_checkpoint


def wdl_to_cp(w: float, l: float) -> int:
    q = max(-0.995, min(0.995, float(w - l)))
    cp = 290.0 * math.atanh(q)
    return int(max(-3000, min(3000, round(cp))))


class ZeroCUci:
    def __init__(self):
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.use_amp = self.device.type == "cuda"
        if self.use_amp:
            torch.backends.cudnn.benchmark = True
        self.root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
        self.ckpt_path = os.path.join(self.root, "data", "az", "latest.pt")
        self.ckpt_mtime = -1.0
        self.gen = 0
        self.simulations = 600
        self.temperature = 0.0
        self.opening_variety = True
        self.tactical_hybrid = True
        self.model = None
        self.arena = None
        self.fen = None
        self.moves = ""
        self.col_arange = torch.arange(512, device=self.device, dtype=torch.int32)
        self._load_model_if_needed(force=True)
        self._rebuild_arena()

    def _load_model_if_needed(self, force=False):
        if os.path.exists(self.ckpt_path):
            try:
                mt = os.path.getmtime(self.ckpt_path)
            except OSError:
                mt = self.ckpt_mtime
            if force or mt != self.ckpt_mtime or self.model is None:
                try:
                    new_model, ckpt = load_checkpoint(self.ckpt_path, self.device)
                    if self.use_amp:
                        new_model = new_model.half().to(memory_format=torch.channels_last)
                    new_model.eval()
                    self.model = new_model
                    self.ckpt_mtime = mt
                    self.gen = int(ckpt.get("meta", {}).get("gen", 0))
                    if self.arena is not None:
                        self.arena.lib.az_arena_reset_game(self.arena.ptr, 0, None)
                    return True
                except Exception:
                    # Keep previous loaded model hot if checkpoint is mid-replace; retry next move
                    if self.model is not None:
                        return False
        if self.model is None:
            self.model = ZeroCNet(10, 128).to(self.device)
            if self.use_amp:
                self.model = self.model.half().to(memory_format=torch.channels_last)
            self.model.eval()
            self.gen = 0
            return True
        return False

    def _rebuild_arena(self):
        if self.arena is not None:
            self.arena.close()
        cfg = AzConfig(
            num_games=1,
            fast_sims=self.simulations,
            full_sims=self.simulations,
            full_search_prob=1.0,
            c_puct=1.75,
            fpu_reduction=0.25,
            dirichlet_alpha=0.30,
            dirichlet_eps=0.0,  # Pure match search (no exploration noise)
            temp_plies=0,
            max_plies=400,
            batch_per_game=12,
            forced_playouts=0,
            num_threads=1,
            seed=int.from_bytes(os.urandom(8), "little"),
            resign_threshold=0.0,
            tactical_mode=1 if self.tactical_hybrid else 0,
        )
        self.arena = AzArena(cfg, pinned_torch=torch if self.use_amp else None)

    @torch.no_grad()
    def search(self, movetime_ms: int = 2000, max_nodes: int = 0):
        self._load_model_if_needed(force=False)
        target_sims = max_nodes if max_nodes > 0 else self.simulations
        if target_sims != self.arena.cfg.full_sims:
            self.simulations = target_sims
            self._rebuild_arena()

        # Automatically reuses the C MCTS subtree if the game advanced 1 or 2 plies!
        self.arena.set_position(0, self.fen, self.moves)
        t0 = time.time()
        deadline = t0 + (movetime_ms / 1000.0) if movetime_ms > 0 else 1e18
        total_evals = 0

        while time.time() < deadline:
            b = self.arena.collect_leaves()
            if b <= 0:
                break
            max_nl = int(self.arena.legal_counts[:b].max())
            if max_nl <= 0:
                break
            if self.arena.pinned and self.use_amp:
                x = self.arena.planes_t[:b].to(
                    self.device, dtype=torch.float16, non_blocking=True
                ).to(memory_format=torch.channels_last)
                la = self.arena.legal_actions_t[:b, :max_nl].to(
                    self.device, dtype=torch.long, non_blocking=True
                )
                lc = self.arena.legal_counts_t[:b].to(self.device, non_blocking=True)
            else:
                x = torch.from_numpy(self.arena.planes_buf[:b]).to(self.device)
                la = torch.from_numpy(self.arena.legal_actions[:b, :max_nl]).to(
                    self.device, dtype=torch.long
                )
                lc = torch.from_numpy(self.arena.legal_counts[:b]).to(self.device)

            pol_logits, wdl_logits, ml_pred = self.model(x)
            gathered = pol_logits.float().gather(1, la)
            valid = self.col_arange[:max_nl].unsqueeze(0) < lc.unsqueeze(1)
            gathered = gathered.masked_fill(~valid, -1e9)
            priors = F.softmax(gathered, dim=-1) * valid.float()
            wdl_probs = F.softmax(wdl_logits.float(), dim=-1)
            ml_out = ml_pred.float()

            self.arena.priors_buf[:b, :max_nl] = priors.cpu().numpy()
            self.arena.wdl_buf[:b] = wdl_probs.cpu().numpy()
            self.arena.ml_buf[:b] = ml_out.cpu().numpy()
            self.arena.apply_evals(b)
            total_evals += b
            if max_nodes > 0 and total_evals >= max_nodes:
                break

        elapsed = max(0.001, time.time() - t0)
        cur_ply = len(self.moves.split()) if self.moves else 0
        eff_temp = self.temperature
        if self.opening_variety and eff_temp <= 0.05 and cur_ply < 10:
            eff_temp = 0.50
        best_uci, wdl, ml = self.arena.best_move(0, temperature=eff_temp)
        snap = self.arena.get_snapshot(0)
        cp = wdl_to_cp(wdl[0], wdl[2])
        nps = int(total_evals / elapsed)
        eff_visits = snap["sims_done"] if snap else total_evals
        depth_est = max(1, int(math.log2(max(2, eff_visits))))
        mode_tag = "Hybrid" if self.tactical_hybrid else "Pure"
        if snap and snap["top"]:
            top_str = " ".join(
                f"{t['san']}:{t['visits']}({int(t['prior']*100)}%)" for t in snap["top"]
            )
            print(
                f"info string Zero-C Gen {self.gen} [{mode_tag} {eff_visits}v] "
                f"WDL={wdl[0]*100:.0f}/{wdl[1]*100:.0f}/{wdl[2]*100:.0f}% ML={ml:.0f} top=[{top_str}]",
                flush=True,
            )
        print(
            f"info depth {depth_est} score cp {cp} nodes {eff_visits} nps {nps} time {int(elapsed*1000)} pv {best_uci}",
            flush=True,
        )
        print(f"bestmove {best_uci}", flush=True)

    def loop(self):
        for raw in sys.stdin:
            line = raw.strip()
            if not line:
                continue
            if line == "uci":
                print(f"id name Zero-C v2 AlphaZero (Gen {self.gen})", flush=True)
                print("id author Chess-in-C", flush=True)
                print("option name Checkpoint type string default data/az/latest.pt", flush=True)
                print("option name Simulations type spin default 600 min 16 max 8192", flush=True)
                print("option name OpeningVariety type check default true", flush=True)
                print("option name TacticalHybrid type check default true", flush=True)
                print("uciok", flush=True)
            elif line == "isready":
                self._load_model_if_needed(force=False)
                print("readyok", flush=True)
            elif line == "ucinewgame":
                self.fen = None
                self.moves = ""
                if self.arena is not None:
                    self.arena.lib.az_arena_reset_game(self.arena.ptr, 0, None)
                self._load_model_if_needed(force=False)
            elif line.startswith("setoption "):
                parts = line.split(" name ", 1)
                if len(parts) == 2:
                    rest = parts[1]
                    if " value " in rest:
                        name, val = rest.split(" value ", 1)
                    else:
                        name, val = rest, ""
                    name = name.strip()
                    val = val.strip()
                    if name == "Checkpoint" and val:
                        self.ckpt_path = val if os.path.isabs(val) else os.path.join(self.root, val)
                        self._load_model_if_needed(force=True)
                    elif name == "Simulations":
                        try:
                            self.simulations = max(16, min(8192, int(val)))
                            self._rebuild_arena()
                        except ValueError:
                            pass
                    elif name == "OpeningVariety":
                        self.opening_variety = val.lower() in ("true", "1", "yes", "on")
                    elif name == "TacticalHybrid":
                        new_val = val.lower() in ("true", "1", "yes", "on")
                        if new_val != self.tactical_hybrid:
                            self.tactical_hybrid = new_val
                            self._rebuild_arena()
            elif line.startswith("position "):
                p = line[9:].strip()
                if " moves " in p:
                    pos_part, mv_part = p.split(" moves ", 1)
                    self.moves = mv_part.strip()
                else:
                    pos_part = p
                    self.moves = ""
                if pos_part.startswith("startpos"):
                    self.fen = None
                elif pos_part.startswith("fen "):
                    self.fen = pos_part[4:].strip()
            elif line.startswith("go"):
                toks = line.split()
                mt = 2000
                nodes = 0
                for i in range(1, len(toks) - 1):
                    if toks[i] == "movetime":
                        mt = int(toks[i + 1])
                    elif toks[i] == "nodes":
                        nodes = int(toks[i + 1])
                self.search(movetime_ms=mt, max_nodes=nodes)
            elif line == "quit":
                break
        if self.arena is not None:
            self.arena.close()


if __name__ == "__main__":
    ZeroCUci().loop()
