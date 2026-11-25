#!/usr/bin/env python3
"""Zero-C: Pure Tabula Rasa AlphaZero / KataGo Self-Play Trainer.

Hardware-accelerated pipeline:
  - Multithreaded C MCTS (pthread worker pool across all CPU cores)
  - Double-buffered dual-arena async CPU/GPU overlap with pinned host DMA
  - GPU-side legal-move gather & softmax (zero Python loop, ~80x less PCIe traffic)
  - FP16 Tensor Cores + channels_last (NHWC) + cuDNN autotuner
  - Compact NumPy-backed persistent replay buffer (resumes cleanly across restarts)
"""
import argparse
import json
import os
import signal
import subprocess
import sys
import time
import numpy as np
import torch
import torch.nn.functional as F

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcts_bindings import (
    AZ_NUM_ACTIONS,
    AZ_PLANES,
    MAX_LEGAL_MOVES,
    REPLAY_MAX_POL,
    AzArena,
    AzConfig,
)
from model import PRESET_ARCHS, ZeroCNet, count_parameters, load_checkpoint, save_checkpoint

STOP_REQUESTED = False


def _handle_stop(signum, frame):
    global STOP_REQUESTED
    STOP_REQUESTED = True


def atomic_write_json(path: str, data: dict):
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f)
    os.replace(tmp, path)


class CompactReplayBuffer:
    """Fast, vectorized, disk-persistent NumPy ring buffer for Zero-C v2 training."""

    def __init__(self, max_size: int = 1200000):
        self.max_size = int(max_size)
        self.planes = np.empty((self.max_size, AZ_PLANES, 8, 8), dtype=np.uint8)
        self.pol_acts = np.zeros((self.max_size, REPLAY_MAX_POL), dtype=np.int16)
        self.pol_probs = np.zeros((self.max_size, REPLAY_MAX_POL), dtype=np.float16)
        self.pol_n = np.zeros((self.max_size,), dtype=np.uint8)
        self.wdl = np.empty((self.max_size, 3), dtype=np.float32)
        self.ml = np.empty((self.max_size,), dtype=np.float32)
        self.is_full = np.empty((self.max_size,), dtype=np.uint8)
        self.size = 0
        self.head = 0
        self._col_grid = np.arange(REPLAY_MAX_POL, dtype=np.int32)[None, :]

    def __len__(self):
        return self.size

    def add_compact(self, batch):
        if batch is None:
            return 0
        p_u8, acts, probs, pn, wdl, ml, is_full = batch
        n = int(p_u8.shape[0])
        if n <= 0:
            return 0
        if n >= self.max_size:
            off = n - self.max_size
            self.planes[:] = p_u8[off:]
            self.pol_acts[:] = acts[off:]
            self.pol_probs[:] = probs[off:]
            self.pol_n[:] = pn[off:]
            self.wdl[:] = wdl[off:]
            self.ml[:] = ml[off:]
            self.is_full[:] = is_full[off:]
            self.size = self.max_size
            self.head = 0
            return n

        end = self.head + n
        if end <= self.max_size:
            sl = slice(self.head, end)
            self.planes[sl] = p_u8
            self.pol_acts[sl] = acts
            self.pol_probs[sl] = probs
            self.pol_n[sl] = pn
            self.wdl[sl] = wdl
            self.ml[sl] = ml
            self.is_full[sl] = is_full
        else:
            first = self.max_size - self.head
            rem = n - first
            self.planes[self.head :] = p_u8[:first]
            self.pol_acts[self.head :] = acts[:first]
            self.pol_probs[self.head :] = probs[:first]
            self.pol_n[self.head :] = pn[:first]
            self.wdl[self.head :] = wdl[:first]
            self.ml[self.head :] = ml[:first]
            self.is_full[self.head :] = is_full[:first]

            self.planes[:rem] = p_u8[first:]
            self.pol_acts[:rem] = acts[first:]
            self.pol_probs[:rem] = probs[first:]
            self.pol_n[:rem] = pn[first:]
            self.wdl[:rem] = wdl[first:]
            self.ml[:rem] = ml[first:]
            self.is_full[:rem] = is_full[first:]

        self.head = end % self.max_size
        self.size = min(self.max_size, self.size + n)
        return n

    def sample_minibatch(self, batch_size: int, rng: np.random.Generator):
        idxs = rng.integers(0, self.size, size=batch_size)
        planes = self.planes[idxs].astype(np.float32) * (1.0 / 255.0)
        wdl_tgt = self.wdl[idxs]
        ml_tgt = self.ml[idxs]
        pol_mask = self.is_full[idxs].astype(np.float32)

        acts = self.pol_acts[idxs].astype(np.int64)
        probs = self.pol_probs[idxs].astype(np.float32)
        pn = self.pol_n[idxs].astype(np.int32)

        policy_tgt = np.zeros((batch_size, AZ_NUM_ACTIONS), dtype=np.float32)
        valid = self._col_grid < pn[:, None]
        rows, cols = np.nonzero(valid)
        if rows.size > 0:
            policy_tgt[rows, acts[rows, cols]] = probs[rows, cols]

        return planes, policy_tgt, wdl_tgt, ml_tgt, pol_mask

    def save(self, path: str):
        if self.size <= 0:
            return
        tmp = path + ".tmp.npz"
        n = self.size
        np.savez(
            tmp,
            planes=self.planes[:n],
            pol_acts=self.pol_acts[:n],
            pol_probs=self.pol_probs[:n],
            pol_n=self.pol_n[:n],
            wdl=self.wdl[:n],
            ml=self.ml[:n],
            is_full=self.is_full[:n],
            head=np.int64(self.head % self.max_size),
        )
        os.replace(tmp, path)

    def load(self, path: str) -> int:
        if not os.path.exists(path):
            return 0
        try:
            with np.load(path) as data:
                p = data["planes"]
                n = min(int(p.shape[0]), self.max_size)
                if n <= 0:
                    return 0
                old_c = int(p.shape[1])
                if old_c == AZ_PLANES:
                    self.planes[:n] = p[:n]
                elif old_c < AZ_PLANES:
                    self.planes[:n].fill(0)
                    self.planes[:n, :old_c] = p[:n]
                else:
                    self.planes[:n] = p[:n, :AZ_PLANES]
                self.pol_acts[:n] = data["pol_acts"][:n]
                self.pol_probs[:n] = data["pol_probs"][:n]
                self.pol_n[:n] = data["pol_n"][:n]
                self.wdl[:n] = data["wdl"][:n]
                self.ml[:n] = data["ml"][:n]
                self.is_full[:n] = data["is_full"][:n]
                self.size = n
                h = int(data["head"]) if "head" in data else (n % self.max_size)
                self.head = h % self.max_size
                return n
        except Exception:
            return 0


class HardwareMonitor:
    """Lightweight, non-blocking GPU & CPU utilization sampler."""

    def __init__(self, use_cuda: bool):
        self.use_cuda = use_cuda
        self.last_t = 0.0
        self.last_cpu_t = time.monotonic()
        self.last_proc_cpu = time.process_time()
        self.gpu_util = None
        self.gpu_temp = None
        self.vram_used_mb = 0
        self.vram_total_mb = 0
        self.cpu_pct = 0.0

    def sample(self):
        now = time.monotonic()
        if now - self.last_t < 2.5:
            return
        self.last_t = now

        # Process CPU% (100% = 1 full core, 600% = 6 full cores)
        cur_cpu = time.process_time()
        dt = max(1e-3, now - self.last_cpu_t)
        self.cpu_pct = round(100.0 * (cur_cpu - self.last_proc_cpu) / dt, 0)
        self.last_cpu_t = now
        self.last_proc_cpu = cur_cpu

        if not self.use_cuda:
            return
        try:
            out = subprocess.check_output(
                [
                    "nvidia-smi",
                    "--query-gpu=utilization.gpu,memory.used,memory.total,temperature.gpu",
                    "--format=csv,noheader,nounits",
                ],
                timeout=0.35,
                stderr=subprocess.DEVNULL,
                text=True,
            ).strip()
            line = out.splitlines()[0]
            parts = [p.strip() for p in line.split(",")]
            if len(parts) >= 4:
                self.gpu_util = int(float(parts[0]))
                self.vram_used_mb = int(float(parts[1]))
                self.vram_total_mb = int(float(parts[2]))
                self.gpu_temp = int(float(parts[3]))
        except Exception:
            self.vram_used_mb = int(torch.cuda.memory_allocated() / (1024 * 1024))


BUCKET_SIZES = (64, 128, 256, 512, 1024, 2048)


def _bucket_batch_size(b: int) -> int:
    for s in BUCKET_SIZES:
        if b <= s:
            return s
    return b


def sync_infer_model(train_model: ZeroCNet, infer_model: ZeroCNet):
    """Copy FP32 master weights into pure FP16 channels_last inference model (zero autocast overhead)."""
    infer_model.load_state_dict(train_model.state_dict())
    infer_model.half().to(memory_format=torch.channels_last).eval()


@torch.no_grad()
def launch_gpu_eval(
    infer_model: ZeroCNet,
    arena: AzArena,
    batch_size: int,
    device: torch.device,
    use_amp: bool,
    col_arange: torch.Tensor,
):
    """Evaluate a batch on GPU with fixed Tensor-Core bucketing, pure FP16, and GPU-side softmax."""
    max_nl = int(arena.legal_counts[:batch_size].max())
    if max_nl <= 0:
        return None

    if arena.pinned and device.type == "cuda":
        # Pad up to a fixed Tensor-Core-aligned bucket so cuDNN never re-benchmarks!
        pad_b = _bucket_batch_size(batch_size)
        if pad_b <= arena.buf_cap:
            if pad_b > batch_size:
                arena.planes_t[batch_size:pad_b].zero_()
            x_in = arena.planes_t[:pad_b].to(device, dtype=torch.float16, non_blocking=True)
            x_in = x_in.to(memory_format=torch.channels_last)
        else:
            x_in = arena.planes_t[:batch_size].to(device, dtype=torch.float16, non_blocking=True)
            x_in = x_in.to(memory_format=torch.channels_last)
            pad_b = batch_size

        la = arena.legal_actions_t[:batch_size, :max_nl].to(
            device, dtype=torch.long, non_blocking=True
        )
        lc = arena.legal_counts_t[:batch_size].to(device, non_blocking=True)
        pol_logits, wdl_logits, ml_pred = infer_model(x_in)
        if pad_b > batch_size:
            pol_logits = pol_logits[:batch_size]
            wdl_logits = wdl_logits[:batch_size]
            ml_pred = ml_pred[:batch_size]
    else:
        x_in = torch.from_numpy(arena.planes_buf[:batch_size]).to(device)
        la = torch.from_numpy(arena.legal_actions[:batch_size, :max_nl]).to(
            device, dtype=torch.long
        )
        lc = torch.from_numpy(arena.legal_counts[:batch_size]).to(device)
        pol_logits, wdl_logits, ml_pred = infer_model(x_in)

    # GPU-side legal-move gather & masked softmax: cuts PCIe D2H traffic by ~80x!
    gathered = pol_logits.float().gather(1, la)
    valid = col_arange[:max_nl].unsqueeze(0) < lc.unsqueeze(1)
    gathered = gathered.masked_fill(~valid, -1e9)
    priors = F.softmax(gathered, dim=-1) * valid.float()
    wdl_probs = F.softmax(wdl_logits.float(), dim=-1)
    ml_out = ml_pred.float()

    if arena.pinned and device.type == "cuda":
        arena.priors_t[:batch_size, :max_nl].copy_(priors, non_blocking=True)
        arena.wdl_t[:batch_size].copy_(wdl_probs, non_blocking=True)
        arena.ml_t[:batch_size].copy_(ml_out, non_blocking=True)
        ev = torch.cuda.Event()
        ev.record()
        return ev

    arena.priors_buf[:batch_size, :max_nl] = priors.cpu().numpy()
    arena.wdl_buf[:batch_size] = wdl_probs.cpu().numpy()
    arena.ml_buf[:batch_size] = ml_out.cpu().numpy()
    return None


def scheduled_lr(base_lr: float, gen_num: int) -> float:
    """Smooth warmup + cosine decay to floor (8% of base_lr) across multi-day runs."""
    if gen_num < 12:
        return base_lr * (0.25 + 0.75 * (gen_num / 12.0))
    decay_gens = 2500.0
    t = min(1.0, max(0.0, (gen_num - 12.0) / decay_gens))
    cos_factor = 0.5 * (1.0 + float(np.cos(np.pi * t)))
    return base_lr * (0.08 + 0.92 * cos_factor)


def prune_old_checkpoints(ckpt_dir: str, keep_recent: int = 25, milestone_step: int = 10):
    """Keep all milestone checkpoints (Gen 0, 10, 20, 30...) plus the most recent `keep_recent` gens."""
    if not os.path.isdir(ckpt_dir):
        return
    gens = []
    for fn in os.listdir(ckpt_dir):
        if fn.startswith("gen_") and fn.endswith(".pt"):
            try:
                g = int(fn[4:-3])
                gens.append((g, os.path.join(ckpt_dir, fn)))
            except ValueError:
                continue
    if len(gens) <= keep_recent:
        return
    gens.sort(key=lambda x: x[0])
    cutoff_gen = gens[-keep_recent][0]
    for g, path in gens[:-keep_recent]:
        if g % milestone_step != 0 and g < cutoff_gen:
            try:
                os.remove(path)
            except OSError:
                pass


def train_epoch(
    model: ZeroCNet,
    optimizer: torch.optim.Optimizer,
    scaler: torch.amp.GradScaler,
    replay: CompactReplayBuffer,
    steps: int,
    batch_size: int,
    device: torch.device,
    use_amp: bool,
    rng: np.random.Generator,
):
    model.train()
    tot_loss = pol_loss_s = wdl_loss_s = ml_loss_s = pol_acc_s = 0.0
    pol_steps = 0

    for _ in range(steps):
        planes, pol_tgt, wdl_tgt, ml_tgt, pol_mask = replay.sample_minibatch(batch_size, rng)
        x = torch.from_numpy(planes).to(device, non_blocking=True)
        if device.type == "cuda":
            x = x.to(memory_format=torch.channels_last)
        y_pol = torch.from_numpy(pol_tgt).to(device, non_blocking=True)
        y_wdl = torch.from_numpy(wdl_tgt).to(device, non_blocking=True)
        y_ml = torch.from_numpy(ml_tgt).to(device, non_blocking=True)
        m_pol = torch.from_numpy(pol_mask).to(device, non_blocking=True)

        optimizer.zero_grad(set_to_none=True)
        with torch.amp.autocast(device_type=device.type, dtype=torch.float16, enabled=use_amp):
            pol_logits, wdl_logits, ml_pred = model(x)

            # Policy cross-entropy ONLY on full-search playout-cap moves (KataGo trick!)
            log_p = F.log_softmax(pol_logits.float(), dim=-1)
            per_sample_pol = -(y_pol * log_p).sum(dim=-1)
            m_sum = m_pol.sum().clamp(min=1.0)
            loss_pol = (per_sample_pol * m_pol).sum() / m_sum

            # WDL cross-entropy on ALL samples
            log_wdl = F.log_softmax(wdl_logits.float(), dim=-1)
            loss_wdl = -(y_wdl * log_wdl).sum(dim=-1).mean()

            # Moves-left auxiliary Huber loss
            loss_ml = F.smooth_l1_loss(ml_pred.float(), y_ml)

            loss = loss_pol + 1.5 * loss_wdl + 0.15 * loss_ml

        scaler.scale(loss).backward()
        scaler.unscale_(optimizer)
        torch.nn.utils.clip_grad_norm_(model.parameters(), 2.0)
        scaler.step(optimizer)
        scaler.update()

        tot_loss += float(loss.item())
        pol_loss_s += float(loss_pol.item())
        wdl_loss_s += float(loss_wdl.item())
        ml_loss_s += float(loss_ml.item())

        if m_pol.sum().item() > 0.5:
            with torch.no_grad():
                pred_top = pol_logits.argmax(dim=-1)
                tgt_top = y_pol.argmax(dim=-1)
                acc = ((pred_top == tgt_top).float() * m_pol).sum().item() / m_sum.item()
                pol_acc_s += acc
                pol_steps += 1

    model.eval()
    inv = 1.0 / max(1, steps)
    return {
        "loss": round(tot_loss * inv, 4),
        "pol_loss": round(pol_loss_s * inv, 4),
        "wdl_loss": round(wdl_loss_s * inv, 4),
        "ml_loss": round(ml_loss_s * inv, 4),
        "pol_acc": round((pol_acc_s / max(1, pol_steps)) * 100.0, 1),
    }


def main():
    signal.signal(signal.SIGINT, _handle_stop)
    signal.signal(signal.SIGTERM, _handle_stop)

    ap = argparse.ArgumentParser(description="Zero-C v2 AlphaZero Self-Play Trainer")
    ap.add_argument("--arch", choices=list(PRESET_ARCHS.keys()), default="pro")
    ap.add_argument("--games", type=int, default=256, help="Total concurrent C MCTS self-play games")
    ap.add_argument("--fast-sims", type=int, default=48)
    ap.add_argument("--full-sims", type=int, default=300)
    ap.add_argument("--full-prob", type=float, default=0.25)
    ap.add_argument("--games-per-gen", type=int, default=128)
    ap.add_argument("--train-steps", type=int, default=0, help="0 = auto-scale with games-per-gen")
    ap.add_argument("--batch-size", type=int, default=512)
    ap.add_argument("--replay-size", type=int, default=1200000)
    ap.add_argument("--resign-threshold", type=float, default=-0.94)
    ap.add_argument("--mcts-threads", type=int, default=0, help="0 = auto CPU cores")
    ap.add_argument("--lr", type=float, default=0.0015)
    ap.add_argument("--max-gens", type=int, default=0, help="0 = continuous until stopped")
    ap.add_argument("--data-dir", default="data/az")
    ap.add_argument("--reset", action="store_true")
    args = ap.parse_args()
    if args.train_steps <= 0:
        args.train_steps = max(120, int(round(args.games_per_gen * 1.1)))

    os.makedirs(args.data_dir, exist_ok=True)
    ckpt_dir = os.path.join(args.data_dir, "checkpoints")
    os.makedirs(ckpt_dir, exist_ok=True)
    status_path = os.path.join(args.data_dir, "status.json")
    latest_path = os.path.join(args.data_dir, "latest.pt")
    replay_path = os.path.join(args.data_dir, "replay.npz")

    if args.reset:
        for fn in os.listdir(ckpt_dir):
            if fn.endswith(".pt"):
                os.remove(os.path.join(ckpt_dir, fn))
        for p in (latest_path, status_path, replay_path):
            if os.path.exists(p):
                os.remove(p)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    use_amp = device.type == "cuda"
    gpu_name = torch.cuda.get_device_name(0) if use_amp else "CPU (no CUDA)"
    if use_amp:
        torch.backends.cudnn.benchmark = True
        torch.backends.cuda.matmul.allow_tf32 = True
        torch.backends.cudnn.allow_tf32 = True

    blocks, channels = PRESET_ARCHS[args.arch]
    gen = 0
    total_games = 0
    total_positions = 0
    total_evals = 0
    total_w = total_d = total_b = 0
    history = []
    ckpt_meta = []
    cur_lr = scheduled_lr(args.lr, 0)

    if os.path.exists(latest_path) and not args.reset:
        model, ckpt = load_checkpoint(latest_path, device)
        optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
        if "optimizer" in ckpt:
            try:
                optimizer.load_state_dict(ckpt["optimizer"])
            except Exception:
                pass
        meta = ckpt.get("meta", {})
        gen = int(meta.get("gen", 0))
        total_games = int(meta.get("total_games", 0))
        total_positions = int(meta.get("total_positions", 0))
        total_evals = int(meta.get("total_evals", 0))
        total_w = int(meta.get("total_w", 0))
        total_d = int(meta.get("total_d", 0))
        total_b = int(meta.get("total_b", 0))
        history = list(meta.get("history", []))
        ckpt_meta = list(meta.get("checkpoints", []))
        prune_old_checkpoints(ckpt_dir, keep_recent=25, milestone_step=10)
        ckpt_meta = [
            c for c in ckpt_meta if os.path.exists(os.path.join(ckpt_dir, c.get("file", "")))
        ]
        blocks, channels = model.num_blocks, model.channels
    else:
        model = ZeroCNet(blocks, channels).to(device)
        optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
        gen0_path = os.path.join(ckpt_dir, "gen_0000.pt")
        save_checkpoint(gen0_path, model, optimizer, {"gen": 0, "total_games": 0})
        save_checkpoint(latest_path, model, optimizer, {"gen": 0, "total_games": 0})
        ckpt_meta.append(
            {
                "gen": 0,
                "file": "gen_0000.pt",
                "games": 0,
                "positions": 0,
                "loss": None,
                "pol_acc": None,
                "time": time.strftime("%H:%M:%S"),
            }
        )

    if use_amp:
        model = model.to(memory_format=torch.channels_last)
        infer_model = ZeroCNet(blocks, channels).to(device)
        sync_infer_model(model, infer_model)
    else:
        infer_model = model
    model.eval()
    scaler = torch.amp.GradScaler(device.type, enabled=use_amp)
    n_params = count_parameters(model)
    col_arange = torch.arange(MAX_LEGAL_MOVES, device=device, dtype=torch.int32)

    # Double-buffered dual-arena setup for true simultaneous CPU MCTS + GPU inference overlap
    total_par_games = max(8, min(1024, args.games))
    use_double_buf = use_amp and total_par_games >= 16
    if use_double_buf:
        g0 = min(512, total_par_games // 2)
        g1 = min(512, total_par_games - g0)
        arena_game_counts = [g0, g1]
    else:
        arena_game_counts = [min(512, total_par_games)]

    # Tune virtual-loss leaves per game so GPU gets rich 256..1024 Tensor Core batches
    bpg = 4 if arena_game_counts[0] >= 64 else (3 if arena_game_counts[0] >= 32 else 2)
    base_seed = int(time.time() * 1000) & 0xFFFFFFFFFFFFFFFF

    arenas = []
    for idx, gc in enumerate(arena_game_counts):
        cfg = AzConfig(
            num_games=gc,
            fast_sims=max(8, args.fast_sims),
            full_sims=max(16, args.full_sims),
            full_search_prob=float(args.full_prob),
            c_puct=1.75,
            fpu_reduction=0.25,
            dirichlet_alpha=0.30,
            dirichlet_eps=0.25,
            temp_plies=16,
            max_plies=300,
            batch_per_game=bpg,
            forced_playouts=1,
            num_threads=args.mcts_threads,
            seed=(base_seed + idx * 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF,
            resign_threshold=float(args.resign_threshold),
            tactical_mode=0,  # 100% pure tabula rasa Path-A self-play!
        )
        arenas.append(AzArena(cfg, pinned_torch=torch if use_amp else None))

    mcts_threads = arenas[0].num_threads
    replay = CompactReplayBuffer(max_size=args.replay_size)
    if not args.reset:
        replay.load(replay_path)

    hw = HardwareMonitor(use_cuda=use_amp)
    rng = np.random.default_rng(int(time.time()))

    start_wall = time.time()
    last_status_t = 0.0
    evals_window_t = time.time()
    evals_window_n = 0
    batches_window_n = 0
    evals_per_sec = 0.0
    avg_batch = 0.0
    games_per_hr = 0.0
    session_games = 0
    gen_games = 0
    gen_w = gen_d = gen_b = gen_plies = 0
    phase = "selfplay"

    def write_status(running_flag: bool = True, msg: str = ""):
        hw.sample()
        vram_mb = (
            hw.vram_used_mb
            if hw.vram_used_mb > 0
            else (int(torch.cuda.memory_allocated() / (1024 * 1024)) if use_amp else 0)
        )
        snaps = []
        gid_offset = 0
        per_arena = 3 if len(arenas) > 1 else 6
        for ar in arenas:
            for gi in range(min(per_arena, ar.cfg.num_games)):
                s = ar.get_snapshot(gi)
                if s is not None:
                    s["id"] = gid_offset + gi
                    snaps.append(s)
            gid_offset += ar.cfg.num_games

        payload = {
            "ok": True,
            "running": running_flag,
            "pid": os.getpid() if running_flag else 0,
            "phase": phase,
            "message": msg,
            "device": gpu_name,
            "fp16": use_amp,
            "vram_mb": vram_mb,
            "vram_total_mb": hw.vram_total_mb,
            "gpu_util": hw.gpu_util,
            "gpu_temp": hw.gpu_temp,
            "cpu_pct": hw.cpu_pct,
            "mcts_threads": mcts_threads,
            "double_buffered": use_double_buf,
            "avg_batch": round(avg_batch, 0),
            "arch": f"{blocks}x{channels} SE-ResNet",
            "params": n_params,
            "config": {
                "arch": args.arch,
                "games": sum(arena_game_counts),
                "fast_sims": args.fast_sims,
                "full_sims": args.full_sims,
                "full_prob": float(args.full_prob),
                "games_per_gen": args.games_per_gen,
            },
            "gen": gen,
            "gen_games": gen_games,
            "games_per_gen": args.games_per_gen,
            "total_games": total_games,
            "total_positions": total_positions,
            "replay_size": len(replay),
            "replay_max": replay.max_size,
            "lr": round(float(cur_lr), 7),
            "base_lr": float(args.lr),
            "evals_per_sec": round(evals_per_sec, 1),
            "games_per_hr": round(games_per_hr, 1),
            "elapsed_sec": round(time.time() - start_wall, 1),
            "outcomes": {"white": total_w, "draw": total_d, "black": total_b},
            "history": history[-1500:],
            "checkpoints": ckpt_meta[-200:],
            "live_games": snaps,
            "updated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        }
        atomic_write_json(status_path, payload)

    def drain_arena_moves(ar: AzArena):
        nonlocal gen_games, total_games, session_games
        nonlocal gen_w, gen_d, gen_b, gen_plies, total_w, total_d, total_b, total_positions
        fin, ww, dr, bw, plies_sum = ar.step_moves(auto_reset=True)
        if fin > 0:
            gen_games += fin
            total_games += fin
            session_games += fin
            gen_w += ww
            gen_d += dr
            gen_b += bw
            gen_plies += plies_sum
            total_w += ww
            total_d += dr
            total_b += bw
            while True:
                compact = ar.pop_samples_compact(4096)
                if compact is None:
                    break
                added = replay.add_compact(compact)
                total_positions += added

    write_status(
        True,
        f"Zero-C Gen {gen} turbo self-play ({sum(arena_game_counts)} games, {mcts_threads}T C MCTS + {gpu_name})",
    )

    in_flight_b = [0] * len(arenas)
    in_flight_ev = [None] * len(arenas)

    def flush_all_in_flight():
        for i, ar in enumerate(arenas):
            if in_flight_b[i] > 0:
                if in_flight_ev[i] is not None:
                    in_flight_ev[i].synchronize()
                    in_flight_ev[i] = None
                ar.apply_evals(in_flight_b[i])
                in_flight_b[i] = 0
                drain_arena_moves(ar)

    try:
        cur = 0
        while not STOP_REQUESTED:
            if args.max_gens > 0 and gen >= args.max_gens:
                break

            phase = "selfplay"

            if use_double_buf:
                nxt = 1 - cur
                # 1. If current arena has no in-flight GPU work, collect & launch it now
                if in_flight_b[cur] == 0:
                    b0 = arenas[cur].collect_leaves()
                    if b0 > 0:
                        in_flight_ev[cur] = launch_gpu_eval(
                            infer_model, arenas[cur], b0, device, use_amp, col_arange
                        )
                        in_flight_b[cur] = b0
                        total_evals += b0
                        evals_window_n += b0
                        batches_window_n += 1
                    else:
                        drain_arena_moves(arenas[cur])

                # 2. OVERLAP! While GPU computes arenas[cur], CPU finishes + collects arenas[nxt]
                if in_flight_b[nxt] > 0:
                    if in_flight_ev[nxt] is not None:
                        in_flight_ev[nxt].synchronize()
                        in_flight_ev[nxt] = None
                    arenas[nxt].apply_evals(in_flight_b[nxt])
                    in_flight_b[nxt] = 0
                    drain_arena_moves(arenas[nxt])

                b1 = arenas[nxt].collect_leaves()
                if b1 > 0:
                    in_flight_ev[nxt] = launch_gpu_eval(
                        infer_model, arenas[nxt], b1, device, use_amp, col_arange
                    )
                    in_flight_b[nxt] = b1
                    total_evals += b1
                    evals_window_n += b1
                    batches_window_n += 1
                else:
                    drain_arena_moves(arenas[nxt])

                # 3. Switch focus: nxt is now running on GPU while we finish cur next iteration!
                cur = nxt
            else:
                ar = arenas[0]
                b = ar.collect_leaves()
                if b > 0:
                    ev = launch_gpu_eval(infer_model, ar, b, device, use_amp, col_arange)
                    if ev is not None:
                        ev.synchronize()
                    ar.apply_evals(b)
                    total_evals += b
                    evals_window_n += b
                    batches_window_n += 1
                drain_arena_moves(ar)

            now = time.time()
            if now - evals_window_t >= 1.5:
                dt = now - evals_window_t
                evals_per_sec = evals_window_n / max(0.01, dt)
                if batches_window_n > 0:
                    avg_batch = evals_window_n / float(batches_window_n)
                evals_window_n = 0
                batches_window_n = 0
                evals_window_t = now
                elapsed_hr = max(1e-4, (now - start_wall) / 3600.0)
                games_per_hr = session_games / elapsed_hr

            if now - last_status_t >= 0.65:
                last_status_t = now
                write_status(True, f"Self-play Gen {gen}: {gen_games}/{args.games_per_gen} games")

            if gen_games >= args.games_per_gen and len(replay) >= args.batch_size:
                flush_all_in_flight()
                phase = "training"
                cur_lr = scheduled_lr(args.lr, gen)
                for pg in optimizer.param_groups:
                    pg["lr"] = cur_lr
                write_status(
                    True,
                    f"Training Gen {gen + 1} ({args.train_steps} FP16 steps on {len(replay):,} pos, lr={cur_lr:.5f})...",
                )
                metrics = train_epoch(
                    model, optimizer, scaler, replay, args.train_steps, args.batch_size, device, use_amp, rng
                )
                if use_amp:
                    sync_infer_model(model, infer_model)
                gen += 1
                decisive_pct = round(100.0 * (gen_w + gen_b) / max(1, gen_games), 1)
                white_pct = round(100.0 * gen_w / max(1, gen_games), 1)
                draw_pct = round(100.0 * gen_d / max(1, gen_games), 1)
                black_pct = round(100.0 * gen_b / max(1, gen_games), 1)
                avg_plies = round(gen_plies / max(1, gen_games), 1)
                hist_row = {
                    "gen": gen,
                    "games": total_games,
                    "positions": total_positions,
                    "loss": metrics["loss"],
                    "pol_loss": metrics["pol_loss"],
                    "wdl_loss": metrics["wdl_loss"],
                    "ml_loss": metrics["ml_loss"],
                    "pol_acc": metrics["pol_acc"],
                    "decisive_pct": decisive_pct,
                    "white_pct": white_pct,
                    "draw_pct": draw_pct,
                    "black_pct": black_pct,
                    "avg_plies": avg_plies,
                    "lr": round(float(cur_lr), 7),
                    "evals_per_sec": round(evals_per_sec, 1),
                }
                history.append(hist_row)
                ckpt_file = f"gen_{gen:04d}.pt"
                ckpt_meta.append(
                    {
                        "gen": gen,
                        "file": ckpt_file,
                        "games": total_games,
                        "positions": total_positions,
                        "loss": metrics["loss"],
                        "pol_acc": metrics["pol_acc"],
                        "decisive_pct": decisive_pct,
                        "time": time.strftime("%H:%M:%S"),
                    }
                )
                meta_blob = {
                    "gen": gen,
                    "total_games": total_games,
                    "total_positions": total_positions,
                    "total_evals": total_evals,
                    "total_w": total_w,
                    "total_d": total_d,
                    "total_b": total_b,
                    "history": history[-2000:],
                    "checkpoints": ckpt_meta[-250:],
                }
                # Historical league checkpoints omit optimizer state (3x smaller on disk);
                # latest.pt keeps full optimizer state for seamless training resume.
                save_checkpoint(os.path.join(ckpt_dir, ckpt_file), model, None, meta_blob)
                prune_old_checkpoints(ckpt_dir, keep_recent=25, milestone_step=10)
                ckpt_meta = [
                    c for c in ckpt_meta if os.path.exists(os.path.join(ckpt_dir, c.get("file", "")))
                ]
                meta_blob["checkpoints"] = ckpt_meta[-100:]
                save_checkpoint(latest_path, model, optimizer, meta_blob)
                if gen % 25 == 0:
                    replay.save(replay_path)
                gen_games = gen_w = gen_d = gen_b = gen_plies = 0
                phase = "selfplay"
                write_status(True, f"Completed Gen {gen} (loss={metrics['loss']}, pol_acc={metrics['pol_acc']}%)")
                last_status_t = time.time()
    finally:
        flush_all_in_flight()
        if len(replay) > 0:
            try:
                replay.save(replay_path)
            except Exception:
                pass
        phase = "idle"
        write_status(False, f"Paused cleanly at Gen {gen} ({total_games} games, {len(replay):,} replay pos saved).")
        for ar in arenas:
            ar.close()


if __name__ == "__main__":
    main()
