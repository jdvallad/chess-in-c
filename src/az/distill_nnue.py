#!/usr/bin/env python3
"""Singularity v5 — Zero-C v2 -> AVX2 King-Bucketed NNUE Knowledge Distillation Trainer.

Compresses Zero-C v2's 3.56M-param 10x128 SE-ResNet (`data/az/latest.pt`) into a
blazing-fast 200ns/node quantized C11 AVX2 King-Bucketed SCReLU NNUE
(`data/chess_net_zero_v4.nnue`, exact `CINCNNUE` v3 binary format loaded by `src/engine/nnue.c`).

Pipeline:
  1. Loads up to `max_samples` positions from `data/az/replay.npz` (STM-perspective planes + WDL)
  2. Runs Zero-C v2 FP16 teacher inference on GPU to label each position with clean WDL soft targets,
     blended with actual self-play game outcomes (75% teacher / 25% game WDL)
  3. Extracts bit-exact `(4x768 -> 256)x2 -> 8` King-Bucketed + Phase-Bucketed sparse indices
  4. Trains factorized SCReLU NNUE (`W_base[768,256] + W_bucket[3072,256]`) on GPU with AdamW + Cosine LR
  5. Atomically exports quantized int16/int32 `data/chess_net_zero_v4.nnue` (exact AVX2-safe bounds)
"""
import argparse
import json
import math
import os
import signal
import struct
import sys
import time
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model import load_checkpoint

NNUE_VERSION = 3
NNUE_KING_BUCKETS = 4
NNUE_BASE_INPUTS = 768
NNUE_INPUTS = NNUE_KING_BUCKETS * NNUE_BASE_INPUTS  # 3072
NNUE_HIDDEN = 256
NNUE_BUCKETS = 8
NNUE_QA = 255
NNUE_QB = 64
NNUE_SCALE = 400.0
MAX_ACTIVE = 32  # At most 32 chess pieces on the board

STOP_REQUESTED = False


def _handle_stop(signum, frame):
    global STOP_REQUESTED
    STOP_REQUESTED = True


def atomic_write_json(path: str, data: dict):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f)
    os.replace(tmp, path)


def king_bucket_and_hm(ksq_arr: np.ndarray):
    """Exact vectorized match to `nnue_king_bucket()` in `src/engine/nnue.h`."""
    hm = np.where((ksq_arr & 7) < 4, 7, 0).astype(np.int32)
    m_ksq = ksq_arr ^ hm
    r = m_ksq >> 3
    f = m_ksq & 7  # 4..7
    kb = np.where(r <= 1, np.where(f == 4, 0, 1), np.where(r <= 3, 2, 3)).astype(np.int32)
    return kb, hm


def extract_nnue_features_chunk(planes_12: np.ndarray):
    """Convert `(N, 12, 8, 8)` STM-perspective uint8 planes into exact NNUE sparse feature arrays.

    Returns:
      stm_feats: (N, 32) int32 padded with NNUE_INPUTS (padding_idx)
      opp_feats: (N, 32) int32 padded with NNUE_INPUTS (padding_idx)
      buckets:   (N,)    int32 phase bucket 0..7
      valid_mask:(N,)    bool  True if both kings are present
    """
    n = planes_12.shape[0]
    flat = planes_12.reshape(n, 12, 64)

    # Locate STM king (plane 5) and OPP king (plane 11) in STM coordinates
    stm_king_present = flat[:, 5, :].any(axis=1)
    opp_king_present = flat[:, 11, :].any(axis=1)
    valid_mask = stm_king_present & opp_king_present

    ksq_stm = flat[:, 5, :].argmax(axis=1).astype(np.int32)
    ksq_opp_in_stm = flat[:, 11, :].argmax(axis=1).astype(np.int32)
    ksq_opp = ksq_opp_in_stm ^ 56  # vertical flip into opponent perspective

    kb_stm, hm_stm = king_bucket_and_hm(ksq_stm)
    kb_opp, hm_opp = king_bucket_and_hm(ksq_opp)

    # Phase bucket: clamp((total_pieces - 2) // 4, 0, 7)
    piece_counts = flat.sum(axis=(1, 2)).astype(np.int32)
    buckets = np.clip((piece_counts - 2) // 4, 0, NNUE_BUCKETS - 1)

    # Vectorized sparse extraction of up to 32 pieces per board
    flat_768 = flat.reshape(n, 768)
    nz_row, nz_idx = np.nonzero(flat_768)
    counts = np.bincount(nz_row, minlength=n)
    starts = np.zeros(n, dtype=np.int64)
    if n > 1:
        starts[1:] = np.cumsum(counts[:-1])
    pos_in_row = np.arange(len(nz_row), dtype=np.int64) - starts[nz_row]
    keep = pos_in_row < MAX_ACTIVE
    nz_row = nz_row[keep]
    nz_idx = nz_idx[keep].astype(np.int32)
    pos_in_row = pos_in_row[keep]

    c_pt = nz_idx >> 6  # 0..11
    sq = nz_idx & 63    # 0..63

    # STM perspective feature index
    f_stm = kb_stm[nz_row] * 768 + c_pt * 64 + (sq ^ hm_stm[nz_row])
    # OPP perspective feature index: swap friendly/enemy (0..5 <-> 6..11) + vertical flip (sq^56)
    opp_c_pt = (c_pt + 6) % 12
    f_opp = kb_opp[nz_row] * 768 + opp_c_pt * 64 + ((sq ^ 56) ^ hm_opp[nz_row])

    stm_feats = np.full((n, MAX_ACTIVE), NNUE_INPUTS, dtype=np.int32)
    opp_feats = np.full((n, MAX_ACTIVE), NNUE_INPUTS, dtype=np.int32)
    stm_feats[nz_row, pos_in_row] = f_stm
    opp_feats[nz_row, pos_in_row] = f_opp
    return stm_feats, opp_feats, buckets, valid_mask


class FactorizedKingBucketNNUE(nn.Module):
    """PyTorch mirror of `src/engine/nnue.c` with base-feature factorization for generalization."""

    def __init__(self):
        super().__init__()
        # Extra padding row at index NNUE_INPUTS / NNUE_BASE_INPUTS stays strictly zero
        self.w_base = nn.Embedding(NNUE_BASE_INPUTS + 1, NNUE_HIDDEN, padding_idx=NNUE_BASE_INPUTS)
        self.w_bucket = nn.Embedding(NNUE_INPUTS + 1, NNUE_HIDDEN, padding_idx=NNUE_INPUTS)
        self.b1 = nn.Parameter(torch.zeros(NNUE_HIDDEN))
        self.w2 = nn.Parameter(torch.empty(NNUE_BUCKETS, NNUE_HIDDEN * 2))
        self.b2 = nn.Parameter(torch.zeros(NNUE_BUCKETS))

        nn.init.normal_(self.w_base.weight, mean=0.0, std=0.04)
        with torch.no_grad():
            self.w_base.weight[NNUE_BASE_INPUTS].zero_()
            self.w_bucket.weight.zero_()
        nn.init.normal_(self.w2, mean=0.0, std=0.04)

    def _acc(self, feat_idx: torch.Tensor) -> torch.Tensor:
        # feat_idx: (B, 32) in [0..3072]; 3072 is padding
        is_pad = feat_idx == NNUE_INPUTS
        base_idx = torch.where(is_pad, torch.full_like(feat_idx, NNUE_BASE_INPUTS), feat_idx % NNUE_BASE_INPUTS)
        h = self.w_base(base_idx).sum(dim=1) + self.w_bucket(feat_idx).sum(dim=1) + self.b1
        # Exact SCReLU: clamp(h, 0, 1)^2
        c = torch.clamp(h, 0.0, 1.0)
        return c * c

    def forward(self, stm_idx: torch.Tensor, opp_idx: torch.Tensor, buckets: torch.Tensor) -> torch.Tensor:
        h_stm = self._acc(stm_idx)
        h_opp = self._acc(opp_idx)
        cat = torch.cat([h_stm, h_opp], dim=1)  # (B, 512)
        w = self.w2[buckets]  # (B, 512)
        b = self.b2[buckets]  # (B,)
        out = (cat * w).sum(dim=1) + b
        return out * NNUE_SCALE  # centipawns (STM perspective)

    @torch.no_grad()
    def clamp_quant_bounds(self):
        # Keep out_weights strictly within AVX2 int16-safe [-127/64, 127/64]
        self.w2.clamp_(-127.0 / NNUE_QB, 127.0 / NNUE_QB)
        self.w_base.weight[NNUE_BASE_INPUTS].zero_()
        self.w_bucket.weight[NNUE_INPUTS].zero_()

    @torch.no_grad()
    def export_quantized(self, out_path: str):
        os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
        wb = self.w_base.weight[:NNUE_BASE_INPUTS].detach().cpu().numpy().astype(np.float64)
        wk = self.w_bucket.weight[:NNUE_INPUTS].detach().cpu().numpy().astype(np.float64)
        b1 = self.b1.detach().cpu().numpy().astype(np.float64)
        w2 = self.w2.detach().cpu().numpy().astype(np.float64)
        b2 = self.b2.detach().cpu().numpy().astype(np.float64)

        # Fuse base weights into every king-bucket slice
        fused_w1 = wk + np.tile(wb, (NNUE_KING_BUCKETS, 1))
        qw1 = np.clip(np.rint(fused_w1 * NNUE_QA), -32767, 32767).astype("<i2")
        qb1 = np.clip(np.rint(b1 * NNUE_QA), -32767, 32767).astype("<i2")
        qw2 = np.clip(np.rint(w2 * NNUE_QB), -127, 127).astype("<i2")
        qb2 = np.rint(b2 * (NNUE_QA * NNUE_QB)).astype("<i4")

        hdr = struct.pack("<8sIIII", b"CINCNNUE", NNUE_VERSION, NNUE_INPUTS, NNUE_HIDDEN, NNUE_BUCKETS)
        tmp = out_path + ".tmp"
        with open(tmp, "wb") as f:
            f.write(hdr)
            f.write(qw1.tobytes())
            f.write(qb1.tobytes())
            f.write(qw2.tobytes())
            f.write(qb2.tobytes())
        os.replace(tmp, out_path)


def main():
    signal.signal(signal.SIGINT, _handle_stop)
    signal.signal(signal.SIGTERM, _handle_stop)

    ap = argparse.ArgumentParser(description="Singularity v5 Knowledge Distillation (Zero-C v2 -> AVX2 NNUE)")
    ap.add_argument("--ckpt", default="data/az/latest.pt")
    ap.add_argument("--replay", default="data/az/replay.npz")
    ap.add_argument("--out", default="data/chess_net_zero_v4.nnue")
    ap.add_argument("--status", default="data/az/distill_status.json")
    ap.add_argument("--max-samples", type=int, default=800000)
    ap.add_argument("--epochs", type=int, default=20)
    ap.add_argument("--batch-size", type=int, default=4096)
    ap.add_argument("--lr", type=float, default=0.004)
    ap.add_argument("--teacher-blend", type=float, default=0.75)
    args = ap.parse_args()

    t0 = time.time()
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    use_amp = device.type == "cuda"

    def write_st(phase: str, running: bool, progress: float, msg: str, extra: dict = None):
        blob = {
            "ok": True,
            "running": running,
            "phase": phase,
            "progress_pct": round(float(progress), 1),
            "message": msg,
            "out_path": args.out,
            "distilled": os.path.exists(args.out),
            "elapsed_sec": round(time.time() - t0, 1),
            "updated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        }
        if extra:
            blob.update(extra)
        atomic_write_json(args.status, blob)

    try:
        if not os.path.exists(args.ckpt):
            write_st("error", False, 0.0, f"Zero-C v2 checkpoint not found: {args.ckpt}")
            return 1
        if not os.path.exists(args.replay):
            write_st("error", False, 0.0, f"Replay buffer not found: {args.replay}")
            return 1

        write_st("loading", True, 2.0, "Loading Zero-C v2 teacher & replay buffer...")
        teacher, ckpt = load_checkpoint(args.ckpt, device)
        teacher.eval()
        if use_amp:
            teacher = teacher.half().to(memory_format=torch.channels_last)
        teacher_gen = int(ckpt.get("meta", {}).get("gen", 0))

        npz = np.load(args.replay)
        planes_all = npz["planes"]
        wdl_all = npz["wdl"]
        n_avail = int(planes_all.shape[0])
        if n_avail < 1024:
            write_st("error", False, 0.0, f"Need at least 1,024 replay positions (found {n_avail}).")
            return 1

        rng = np.random.default_rng(42)
        if n_avail > args.max_samples:
            sel = rng.choice(n_avail, size=args.max_samples, replace=False)
            sel.sort()
            planes_sub = planes_all[sel]
            wdl_sub = wdl_all[sel]
        else:
            planes_sub = planes_all
            wdl_sub = wdl_all
        n_total = int(planes_sub.shape[0])

        # Stage 1: Teacher relabeling + sparse NNUE feature extraction (chunked to stay light on RAM/VRAM)
        stm_chunks = []
        opp_chunks = []
        bkt_chunks = []
        tgt_chunks = []
        chunk_sz = 32768
        infer_bs = 2048

        for off in range(0, n_total, chunk_sz):
            if STOP_REQUESTED:
                write_st("stopped", False, 0.0, "Distillation cancelled by user.")
                return 0
            end = min(n_total, off + chunk_sz)
            p_chunk = planes_sub[off:end]
            w_chunk = wdl_sub[off:end].astype(np.float32)

            # Extract NNUE sparse features
            stm_f, opp_f, bkts, valid = extract_nnue_features_chunk(p_chunk[:, :12, :, :])

            # Teacher WDL inference on GPU FP16
            t_scores = np.empty(end - off, dtype=np.float32)
            with torch.no_grad():
                for i in range(0, end - off, infer_bs):
                    j = min(end - off, i + infer_bs)
                    xb = torch.from_numpy(p_chunk[i:j]).to(device, dtype=torch.float16 if use_amp else torch.float32)
                    if use_amp:
                        xb = xb.to(memory_format=torch.channels_last)
                    _pol, wdl_logits, _ml = teacher(xb)
                    probs = F.softmax(wdl_logits.float(), dim=-1)
                    # Expected score in [0, 1]: P(Win) + 0.5 * P(Draw)
                    exp_s = probs[:, 0] + 0.5 * probs[:, 1]
                    t_scores[i:j] = exp_s.cpu().numpy()

            # Blend teacher expected score with replay game WDL outcome
            game_s = np.clip(w_chunk[:, 0] + 0.5 * w_chunk[:, 1], 0.0, 1.0)
            blended = float(args.teacher_blend) * t_scores + (1.0 - float(args.teacher_blend)) * game_s

            stm_chunks.append(stm_f[valid])
            opp_chunks.append(opp_f[valid])
            bkt_chunks.append(bkts[valid])
            tgt_chunks.append(blended[valid].astype(np.float32))

            pct = 5.0 + 35.0 * (end / float(n_total))
            write_st(
                "relabeling",
                True,
                pct,
                f"Teacher relabeling Zero-C v2 (Gen {teacher_gen}): {end:,} / {n_total:,} positions...",
                {"samples": end, "total_samples": n_total, "teacher_gen": teacher_gen},
            )

        del teacher, planes_sub, wdl_sub, npz
        if use_amp:
            torch.cuda.empty_cache()

        stm_all = np.concatenate(stm_chunks, axis=0)
        opp_all = np.concatenate(opp_chunks, axis=0)
        bkt_all = np.concatenate(bkt_chunks, axis=0)
        tgt_all = np.concatenate(tgt_chunks, axis=0)
        del stm_chunks, opp_chunks, bkt_chunks, tgt_chunks

        n_valid = int(stm_all.shape[0])
        perm = rng.permutation(n_valid)
        n_val = max(2048, int(n_valid * 0.05))
        val_idx = perm[:n_val]
        trn_idx = perm[n_val:]

        # Stage 2: Train quantized-ready King-Bucketed SCReLU NNUE on GPU
        net = FactorizedKingBucketNNUE().to(device)
        opt = torch.optim.AdamW(net.parameters(), lr=args.lr, weight_decay=1e-5)

        val_stm = torch.from_numpy(stm_all[val_idx]).to(device, dtype=torch.long)
        val_opp = torch.from_numpy(opp_all[val_idx]).to(device, dtype=torch.long)
        val_bkt = torch.from_numpy(bkt_all[val_idx]).to(device, dtype=torch.long)
        val_tgt = torch.from_numpy(tgt_all[val_idx]).to(device, dtype=torch.float32)

        best_val = 1e9
        best_state = None
        eval_scale_k = 1.0 / 400.0  # Matches Titan-C's sigmoid(cp / 400) win probability

        for ep in range(1, args.epochs + 1):
            if STOP_REQUESTED:
                write_st("stopped", False, 0.0, "Distillation stopped before completion.")
                return 0

            lr_now = args.lr * (0.05 + 0.95 * 0.5 * (1.0 + math.cos(math.pi * (ep - 1) / max(1, args.epochs))))
            for pg in opt.param_groups:
                pg["lr"] = lr_now

            net.train()
            rng.shuffle(trn_idx)
            trn_loss_sum = 0.0
            trn_batches = 0

            for off in range(0, len(trn_idx), args.batch_size):
                b_ids = trn_idx[off : off + args.batch_size]
                bs = torch.from_numpy(stm_all[b_ids]).to(device, dtype=torch.long, non_blocking=True)
                bo = torch.from_numpy(opp_all[b_ids]).to(device, dtype=torch.long, non_blocking=True)
                bk = torch.from_numpy(bkt_all[b_ids]).to(device, dtype=torch.long, non_blocking=True)
                bt = torch.from_numpy(tgt_all[b_ids]).to(device, dtype=torch.float32, non_blocking=True)

                opt.zero_grad(set_to_none=True)
                cp_pred = net(bs, bo, bk)
                wp_pred = torch.sigmoid(cp_pred * eval_scale_k)
                loss = F.mse_loss(wp_pred, bt)
                loss.backward()
                opt.step()
                net.clamp_quant_bounds()

                trn_loss_sum += float(loss.item())
                trn_batches += 1

            net.eval()
            with torch.no_grad():
                v_cp = net(val_stm, val_opp, val_bkt)
                v_wp = torch.sigmoid(v_cp * eval_scale_k)
                val_loss = float(F.mse_loss(v_wp, val_tgt).item())

            if val_loss < best_val:
                best_val = val_loss
                best_state = {k: v.detach().cpu().clone() for k, v in net.state_dict().items()}

            pct = 40.0 + 57.0 * (ep / float(args.epochs))
            write_st(
                "training",
                True,
                pct,
                f"Training Singularity v5 NNUE — Epoch {ep}/{args.epochs} (val MSE={val_loss:.5f}, lr={lr_now:.5f})",
                {
                    "epoch": ep,
                    "epochs": args.epochs,
                    "train_mse": round(trn_loss_sum / max(1, trn_batches), 5),
                    "val_mse": round(val_loss, 5),
                    "best_val_mse": round(best_val, 5),
                    "samples": n_valid,
                    "teacher_gen": teacher_gen,
                },
            )

        # Stage 3: Atomic Quantized Export
        write_st("exporting", True, 98.0, f"Quantizing & exporting {args.out} (AVX2 CINCNNUE v3)...")
        if best_state is not None:
            net.load_state_dict(best_state)
            net.clamp_quant_bounds()
        net.export_quantized(args.out)

        write_st(
            "complete",
            False,
            100.0,
            f"Singularity v5 NNUE forged! ({n_valid:,} positions distilled from Gen {teacher_gen}, val MSE={best_val:.5f})",
            {
                "epoch": args.epochs,
                "epochs": args.epochs,
                "val_mse": round(best_val, 5),
                "samples": n_valid,
                "teacher_gen": teacher_gen,
                "distilled": True,
            },
        )
        return 0
    except Exception as exc:
        write_st("error", False, 0.0, f"Distillation failed: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
