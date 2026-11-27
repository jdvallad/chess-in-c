#!/usr/bin/env python3
"""Project 3000 — GPU Trainer for Big NNUE v4: (16x768) -> 1024x2 -> 8 SCReLU.

Trains on compact 36-byte position files produced by `build/datagen` via zero-copy
NumPy memmap (scales effortlessly to 50M-100M+ positions within 15 GiB RAM) and
exports a bit-exact quantized `CINCNNUE` v4 binary (`data/titan_v6_1024.nnue`)
loaded natively by `src/engine/nnue.c` with AVX2 acceleration!

Usage:
  .venv-az/bin/python3 src/tools/train_big_nnue.py --data data/datagen_v4.bin --out data/titan_v6_1024.nnue
"""
import argparse
import json
import math
import os
import struct
import sys
import time
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

NNUE_VERSION_V4 = 4
NNUE_V4_KING_BUCKETS = 16
NNUE_BASE_INPUTS = 768
NNUE_V4_INPUTS = NNUE_V4_KING_BUCKETS * NNUE_BASE_INPUTS  # 12288
NNUE_BUCKETS = 8
NNUE_QA = 255
NNUE_QB = 64
NNUE_SCALE = 400.0
MAX_ACTIVE = 32

REC_DTYPE = np.dtype(
    [
        ("board", np.uint8, (32,)),
        ("score_cp", "<i2"),
        ("result", np.uint8),
        ("ply", np.uint8),
    ],
    align=False,
)
assert REC_DTYPE.itemsize == 36


def king_bucket_v4_and_hm(ksq_arr: np.ndarray):
    """Exact vectorized match to `nnue_king_bucket_for(16, ...)` in `src/engine/nnue.h`."""
    hm = np.where((ksq_arr & 7) < 4, 7, 0).astype(np.int32)
    m_ksq = ksq_arr ^ hm
    r = m_ksq >> 3
    f = m_ksq & 7  # 4..7
    kb = np.where(
        r == 0,
        f - 4,
        np.where(
            r == 1,
            4 + (f - 4),
            np.where(
                r <= 3,
                8 + (r - 2) * 2 + np.where(f >= 6, 1, 0),
                12 + np.where(r >= 6, 2, 0) + np.where(f >= 6, 1, 0),
            ),
        ),
    ).astype(np.int32)
    return kb, hm


def decode_batch_to_v4_features(board_packed: np.ndarray):
    """Unpack `(B, 32)` nibble boards into exact v4 `(16x768)` sparse STM & OPP indices."""
    b = board_packed.shape[0]
    sq_codes = np.empty((b, 64), dtype=np.uint8)
    sq_codes[:, 0::2] = board_packed & 0x0F
    sq_codes[:, 1::2] = (board_packed >> 4) & 0x0F

    # King squares: code 6 = STM king, code 12 = OPP king (in STM coordinates)
    ksq_stm = (sq_codes == 6).argmax(axis=1).astype(np.int32)
    ksq_opp_in_stm = (sq_codes == 12).argmax(axis=1).astype(np.int32)
    ksq_opp = ksq_opp_in_stm ^ 56

    kb_stm, hm_stm = king_bucket_v4_and_hm(ksq_stm)
    kb_opp, hm_opp = king_bucket_v4_and_hm(ksq_opp)

    pieces = (sq_codes > 0).sum(axis=1).astype(np.int32)
    buckets = np.clip((pieces - 2) // 4, 0, NNUE_BUCKETS - 1)

    nz_row, nz_sq = np.nonzero(sq_codes)
    codes = sq_codes[nz_row, nz_sq].astype(np.int32) - 1  # 0..11
    counts = np.bincount(nz_row, minlength=b)
    starts = np.zeros(b, dtype=np.int64)
    if b > 1:
        starts[1:] = np.cumsum(counts[:-1])
    pos = np.arange(len(nz_row), dtype=np.int64) - starts[nz_row]
    keep = pos < MAX_ACTIVE
    nz_row = nz_row[keep]
    nz_sq = nz_sq[keep].astype(np.int32)
    codes = codes[keep]
    pos = pos[keep]

    f_stm = kb_stm[nz_row] * 768 + codes * 64 + (nz_sq ^ hm_stm[nz_row])
    opp_codes = (codes + 6) % 12
    f_opp = kb_opp[nz_row] * 768 + opp_codes * 64 + ((nz_sq ^ 56) ^ hm_opp[nz_row])

    stm_feats = np.full((b, MAX_ACTIVE), NNUE_V4_INPUTS, dtype=np.int32)
    opp_feats = np.full((b, MAX_ACTIVE), NNUE_V4_INPUTS, dtype=np.int32)
    stm_feats[nz_row, pos] = f_stm
    opp_feats[nz_row, pos] = f_opp
    return stm_feats, opp_feats, buckets


class BigFactorizedNNUEv4(nn.Module):
    """Factorized `(16x768) -> hidden x 2 -> 8` SCReLU NNUE matching `src/engine/nnue.c` v4."""

    def __init__(self, hidden: int = 1024):
        super().__init__()
        assert 64 <= hidden <= 1024 and hidden % 16 == 0
        self.hidden = hidden
        self.w_base = nn.Embedding(NNUE_BASE_INPUTS + 1, hidden, padding_idx=NNUE_BASE_INPUTS)
        self.w_bucket = nn.Embedding(NNUE_V4_INPUTS + 1, hidden, padding_idx=NNUE_V4_INPUTS)
        self.b1 = nn.Parameter(torch.zeros(hidden))
        self.w2 = nn.Parameter(torch.empty(NNUE_BUCKETS, hidden * 2))
        self.b2 = nn.Parameter(torch.zeros(NNUE_BUCKETS))

        nn.init.normal_(self.w_base.weight, mean=0.0, std=0.03)
        with torch.no_grad():
            self.w_base.weight[NNUE_BASE_INPUTS].zero_()
            self.w_bucket.weight.zero_()
        nn.init.normal_(self.w2, mean=0.0, std=0.03)

    def _acc(self, feat_idx: torch.Tensor) -> torch.Tensor:
        is_pad = feat_idx == NNUE_V4_INPUTS
        base_idx = torch.where(
            is_pad, torch.full_like(feat_idx, NNUE_BASE_INPUTS), feat_idx % NNUE_BASE_INPUTS
        )
        h = self.w_base(base_idx).sum(dim=1) + self.w_bucket(feat_idx).sum(dim=1) + self.b1
        c = torch.clamp(h, 0.0, 1.0)
        return c * c

    def forward(self, stm_idx: torch.Tensor, opp_idx: torch.Tensor, buckets: torch.Tensor) -> torch.Tensor:
        cat = torch.cat([self._acc(stm_idx), self._acc(opp_idx)], dim=1)
        out = (cat * self.w2[buckets]).sum(dim=1) + self.b2[buckets]
        return out * NNUE_SCALE

    @torch.no_grad()
    def clamp_quant_bounds(self):
        self.w2.clamp_(-127.0 / NNUE_QB, 127.0 / NNUE_QB)
        self.w_base.weight[NNUE_BASE_INPUTS].zero_()
        self.w_bucket.weight[NNUE_V4_INPUTS].zero_()

    @torch.no_grad()
    def export_v4(self, out_path: str):
        os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
        wb = self.w_base.weight[:NNUE_BASE_INPUTS].detach().cpu().numpy().astype(np.float64)
        wk = self.w_bucket.weight[:NNUE_V4_INPUTS].detach().cpu().numpy().astype(np.float64)
        b1 = self.b1.detach().cpu().numpy().astype(np.float64)
        w2 = self.w2.detach().cpu().numpy().astype(np.float64)
        b2 = self.b2.detach().cpu().numpy().astype(np.float64)

        fused = wk + np.tile(wb, (NNUE_V4_KING_BUCKETS, 1))
        qw1 = np.clip(np.rint(fused * NNUE_QA), -32767, 32767).astype("<i2")
        qb1 = np.clip(np.rint(b1 * NNUE_QA), -32767, 32767).astype("<i2")
        qw2 = np.clip(np.rint(w2 * NNUE_QB), -127, 127).astype("<i2")
        qb2 = np.rint(b2 * (NNUE_QA * NNUE_QB)).astype("<i4")

        hdr = struct.pack(
            "<8sIIII", b"CINCNNUE", NNUE_VERSION_V4, NNUE_V4_INPUTS, self.hidden, NNUE_BUCKETS
        )
        tmp = out_path + ".tmp"
        with open(tmp, "wb") as f:
            f.write(hdr)
            f.write(qw1.tobytes())
            f.write(qb1.tobytes())
            f.write(qw2.tobytes())
            f.write(qb2.tobytes())
        os.replace(tmp, out_path)


def main():
    ap = argparse.ArgumentParser(description="Project 3000 Big NNUE v4 (16x768->1024) GPU Trainer")
    ap.add_argument("--data", default="data/datagen_v4.bin")
    ap.add_argument("--out", default="data/titan_v6_1024.nnue")
    ap.add_argument("--status", default="data/p3k_nnue_status.json")
    ap.add_argument("--hidden", type=int, default=1024)
    ap.add_argument("--epochs", type=int, default=24)
    ap.add_argument("--batch-size", type=int, default=8192)
    ap.add_argument("--lr", type=float, default=0.003)
    ap.add_argument("--eval-blend", type=float, default=0.75)
    ap.add_argument("--max-steps-per-epoch", type=int, default=4000)
    ap.add_argument("--init-untrained", action="store_true",
                    help="Export an untrained random v4 net ONLY if --out does not already exist (Skip Compute).")
    args = ap.parse_args()

    t0 = time.time()
    hist = []

    def write_st(phase: str, running: bool, ep: int, pct: float, msg: str, extra: dict = None):
        if not args.status:
            return
        os.makedirs(os.path.dirname(args.status) or ".", exist_ok=True)
        blob = {
            "ok": True,
            "running": running,
            "phase": phase,
            "epoch": ep,
            "epochs": args.epochs,
            "hidden": args.hidden,
            "progress_pct": round(float(pct), 1),
            "message": msg,
            "history": hist[-200:],
            "elapsed_sec": round(time.time() - t0, 1),
            "updated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        }
        if extra:
            blob.update(extra)
        with open(args.status + ".tmp", "w") as sf:
            json.dump(blob, sf)
        os.replace(args.status + ".tmp", args.status)

    if args.init_untrained:
        if os.path.exists(args.out):
            print(f"[NNUE-v4] Refusing --init-untrained: {args.out} already exists!", file=sys.stderr)
            return 0
        net = BigFactorizedNNUEv4(hidden=args.hidden)
        net.export_v4(args.out)
        write_st(
            "skipped_untrained", False, 0, 0.0,
            f"Initialized UNTRAINED random NNUE v4 ({args.hidden}-wide) — Training skipped (not recommended)!",
            {"total_positions": 0, "untrained": True},
        )
        print(f"[NNUE-v4] Initialized untrained random net at {args.out}")
        return 0

    if not os.path.exists(args.data):
        msg = f"Dataset not found: {args.data} (run Datagen first!)"
        write_st("error", False, 0, 0.0, msg)
        print(msg, file=sys.stderr)
        return 1

    mm = np.memmap(args.data, dtype=REC_DTYPE, mode="r")
    n_total = len(mm)
    if n_total < 1024:
        msg = f"Dataset too small ({n_total} pos). Generate more positions with Datagen first."
        write_st("error", False, 0, 0.0, msg)
        print(msg, file=sys.stderr)
        return 1

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    write_st(
        "preparing",
        True,
        0,
        2.0,
        f"Memmapped {n_total:,} positions — preparing validation set on {device}...",
        {"total_positions": n_total},
    )
    print(
        f"[NNUE-v4] Memmapped {n_total:,} positions from {args.data} | "
        f"arch=(16x768)->{args.hidden}x2->8 | device={device}"
    )

    n_val = min(65536, max(512, n_total // 20))
    n_trn = n_total - n_val
    rng = np.random.default_rng(42)

    net = BigFactorizedNNUEv4(hidden=args.hidden).to(device)
    opt = torch.optim.AdamW(net.parameters(), lr=args.lr, weight_decay=1e-5)
    eval_k = 1.0 / 400.0
    blend = float(args.eval_blend)

    # Pre-decode validation set once
    val_recs = mm[n_trn : n_trn + n_val]
    v_stm, v_opp, v_bkt = decode_batch_to_v4_features(val_recs["board"])
    v_ev = 1.0 / (1.0 + np.exp(-val_recs["score_cp"].astype(np.float32) * eval_k))
    v_wdl = val_recs["result"].astype(np.float32) * 0.5
    v_tgt = torch.from_numpy((blend * v_ev + (1.0 - blend) * v_wdl).astype(np.float32)).to(device)
    v_stm_t = torch.from_numpy(v_stm).to(device, dtype=torch.long)
    v_opp_t = torch.from_numpy(v_opp).to(device, dtype=torch.long)
    v_bkt_t = torch.from_numpy(v_bkt).to(device, dtype=torch.long)

    best_val = 1e9
    best_state = None
    steps_per_ep = min(args.max_steps_per_epoch, max(1, n_trn // args.batch_size))

    for ep in range(1, args.epochs + 1):
        lr_now = args.lr * (0.04 + 0.96 * 0.5 * (1.0 + math.cos(math.pi * (ep - 1) / max(1, args.epochs))))
        for pg in opt.param_groups:
            pg["lr"] = lr_now

        net.train()
        loss_sum = 0.0
        for _ in range(steps_per_ep):
            idx = rng.integers(0, n_trn, size=args.batch_size)
            batch = mm[idx]
            s_f, o_f, bk = decode_batch_to_v4_features(batch["board"])
            ev_s = 1.0 / (1.0 + np.exp(-batch["score_cp"].astype(np.float32) * eval_k))
            wdl_s = batch["result"].astype(np.float32) * 0.5
            tgt = (blend * ev_s + (1.0 - blend) * wdl_s).astype(np.float32)

            bs = torch.from_numpy(s_f).to(device, dtype=torch.long, non_blocking=True)
            bo = torch.from_numpy(o_f).to(device, dtype=torch.long, non_blocking=True)
            bb = torch.from_numpy(bk).to(device, dtype=torch.long, non_blocking=True)
            bt = torch.from_numpy(tgt).to(device, dtype=torch.float32, non_blocking=True)

            opt.zero_grad(set_to_none=True)
            pred_wp = torch.sigmoid(net(bs, bo, bb) * eval_k)
            loss = F.mse_loss(pred_wp, bt)
            loss.backward()
            opt.step()
            net.clamp_quant_bounds()
            loss_sum += float(loss.item())

        net.eval()
        with torch.no_grad():
            vp = torch.sigmoid(net(v_stm_t, v_opp_t, v_bkt_t) * eval_k)
            val_mse = float(F.mse_loss(vp, v_tgt).item())
        if val_mse < best_val:
            best_val = val_mse
            best_state = {k: v.detach().cpu().clone() for k, v in net.state_dict().items()}
            net.export_v4(args.out)

        pct = 3.0 + 95.0 * (ep / float(args.epochs))
        trn_avg = loss_sum / steps_per_ep
        hist.append({
            "epoch": ep,
            "train_mse": round(trn_avg, 5),
            "val_mse": round(val_mse, 5),
            "best_val_mse": round(best_val, 5),
            "lr": round(lr_now, 5),
        })
        write_st(
            "training",
            True,
            ep,
            pct,
            f"Epoch {ep}/{args.epochs} — train MSE={trn_avg:.5f}, val MSE={val_mse:.5f} (best={best_val:.5f})",
            {
                "total_positions": n_total,
                "train_mse": round(trn_avg, 5),
                "val_mse": round(val_mse, 5),
                "best_val_mse": round(best_val, 5),
                "lr": round(lr_now, 5),
            },
        )
        print(
            f"[NNUE-v4] Epoch {ep:2d}/{args.epochs} | train={trn_avg:.5f} | "
            f"val={val_mse:.5f} | best={best_val:.5f} | lr={lr_now:.5f} | {time.time()-t0:.1f}s",
            flush=True,
        )

    if best_state is not None:
        net.load_state_dict(best_state)
        net.clamp_quant_bounds()
        net.export_v4(args.out)
    write_st(
        "complete",
        False,
        args.epochs,
        100.0,
        f"Big NNUE v4 ({args.hidden}-wide) exported to {args.out}! Best val MSE={best_val:.5f}",
        {"total_positions": n_total, "best_val_mse": round(best_val, 5)},
    )
    try:
        import training_reports
        training_reports.generate_all_reports()
    except Exception:
        pass
    print(f"[NNUE-v4] Exported {args.out} (best val MSE={best_val:.5f})!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
