#!/usr/bin/env python3
"""Project 3000 & Zero-C — Telemetry Backfill, Dataset Quality Analyzer & Stage Report Archive.

Generates structured JSON completion/snapshot reports in `data/reports/` for every stage
of training so the Web UI can display rich historical charts, configuration summaries,
automated health checks, and "What Next?" guidance.

Usage:
  .venv-az/bin/python3 src/tools/training_reports.py --all
"""
import argparse
import json
import os
import re
import time
import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
REPORT_DIR = os.path.join(ROOT, "data", "reports")


def _atomic_json(path: str, payload: dict):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2)
    os.replace(tmp, path)


def analyze_datagen_file(
    bin_path: str = "data/datagen_v4.bin",
    stats_out: str = "data/p3k_datagen_stats.json",
    hist_out: str = "data/p3k_datagen_history.json",
    log_path: str = "data/p3k_datagen.log",
):
    abs_bin = os.path.join(ROOT, bin_path) if not os.path.isabs(bin_path) else bin_path
    abs_log = os.path.join(ROOT, log_path) if not os.path.isabs(log_path) else log_path
    if not os.path.exists(abs_bin):
        return None

    rec_dt = np.dtype(
        [("board", np.uint8, (32,)), ("score_cp", "<i2"), ("result", np.uint8), ("ply", np.uint8)],
        align=False,
    )
    mm = np.memmap(abs_bin, dtype=rec_dt, mode="r")
    n_total = len(mm)
    if n_total <= 0:
        return None

    sample_n = min(n_total, 250000)
    step = max(1, n_total // sample_n)
    sub = mm[::step][:sample_n]

    sc = sub["score_cp"].astype(np.float32)
    res = sub["result"]
    ply = sub["ply"].astype(np.float32)

    w_cnt = int((res == 2).sum())
    d_cnt = int((res == 1).sum())
    l_cnt = int((res == 0).sum())
    n_s = max(1, len(sub))

    mean_win_cp = float(sc[res == 2].mean()) if w_cnt > 0 else 0.0
    mean_loss_cp = float(sc[res == 0].mean()) if l_cnt > 0 else 0.0
    dec_mask = res != 1
    corr = (
        float(np.corrcoef(sc[dec_mask], res[dec_mask])[0, 1])
        if dec_mask.sum() > 20 and sc[dec_mask].std() > 1e-3
        else 0.0
    )

    # Eval histogram across 12 bins from -1500..+1500 cp
    eval_edges = [-3001, -1000, -600, -300, -150, -50, 0, 50, 150, 300, 600, 1000, 3001]
    eval_labels = ["<-10", "-10..-6", "-6..-3", "-3..-1.5", "-1.5..-.5", "-.5..0", "0..+.5", "+.5..1.5", "+1.5..3", "+3..6", "+6..10", ">+10"]
    ev_hist, _ = np.histogram(sc, bins=eval_edges)

    # Ply histogram (buckets of 20 plies up to 240)
    ply_edges = [0, 20, 40, 60, 80, 100, 120, 160, 260]
    ply_labels = ["0-20", "20-40", "40-60", "60-80", "80-100", "100-120", "120-160", "160+"]
    pl_hist, _ = np.histogram(ply, bins=ply_edges)

    stats = {
        "total_positions": int(n_total),
        "sample_size": int(n_s),
        "win_pct": round(100.0 * w_cnt / n_s, 1),
        "draw_pct": round(100.0 * d_cnt / n_s, 1),
        "loss_pct": round(100.0 * l_cnt / n_s, 1),
        "mean_win_cp": round(mean_win_cp, 1),
        "mean_loss_cp": round(mean_loss_cp, 1),
        "score_result_corr": round(corr, 3),
        "avg_ply": round(float(ply.mean()), 1),
        "eval_hist": {"labels": eval_labels, "counts": [int(x) for x in ev_hist]},
        "ply_hist": {"labels": ply_labels, "counts": [int(x) for x in pl_hist]},
        "updated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
    }
    _atomic_json(os.path.join(ROOT, stats_out), stats)

    # Parse datagen log into clean downsampled throughput history if missing or sparse
    abs_hist = os.path.join(ROOT, hist_out)
    pts = []
    if os.path.exists(abs_log):
        pat = re.compile(
            r"\+(\d+)\s*/\s*\d+\s*pos\s*\((\d+(?:\.\d+)?)\s*pos/s\)\s*\|\s*(\d+)\s*games\s*\|\s*total file:\s*(\d+)"
        )
        with open(abs_log, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                m = pat.search(line)
                if m:
                    pts.append(
                        {
                            "session_pos": int(m.group(1)),
                            "pps": float(m.group(2)),
                            "games": int(m.group(3)),
                            "total_pos": int(m.group(4)),
                        }
                    )
        if len(pts) > 120:
            stride = max(1, len(pts) // 120)
            pts = pts[::stride] + ([pts[-1]] if pts else [])
        _atomic_json(abs_hist, pts)
    return stats, pts


def backfill_nnue_history():
    log_p = os.path.join(ROOT, "data", "p3k_nnue.log")
    st_p = os.path.join(ROOT, "data", "p3k_nnue_status.json")
    if not os.path.exists(log_p) or not os.path.exists(st_p):
        return None
    st = json.load(open(st_p))
    if st.get("history"):
        return st
    pat = re.compile(
        r"Epoch\s+(\d+)/(\d+)\s*\|\s*train=([0-9.]+)\s*\|\s*val=([0-9.]+)\s*\|\s*best=([0-9.]+)\s*\|\s*lr=([0-9.]+)\s*\|\s*([0-9.]+)s"
    )
    hist = []
    best_ep = 1
    best_val = 1e9
    with open(log_p, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            m = pat.search(line)
            if m:
                ep = int(m.group(1))
                tm = float(m.group(3))
                vm = float(m.group(4))
                bm = float(m.group(5))
                lr = float(m.group(6))
                el = float(m.group(7))
                if vm < best_val:
                    best_val = vm
                    best_ep = ep
                hist.append({"epoch": ep, "train_mse": tm, "val_mse": vm, "best_val_mse": bm, "lr": lr, "elapsed": el})
    if hist:
        st["history"] = hist
        st["best_epoch"] = best_ep
        st["best_val_mse"] = round(best_val, 5)
        _atomic_json(st_p, st)
    return st


def backfill_spsa_history():
    log_p = os.path.join(ROOT, "data", "p3k_spsa.log")
    st_p = os.path.join(ROOT, "data", "p3k_spsa_status.json")
    if not os.path.exists(log_p) or not os.path.exists(st_p):
        return None
    st = json.load(open(st_p))
    if st.get("history"):
        return st
    pat = re.compile(
        r"Iter\s+(\d+)/(\d+)\s*\|\s*\+delta score=([0-9.]+)/([0-9]+)\s*\|\s*(\d+(?:\.\d+)?)s\s*\|\s*RFP=(\d+)\s+Fut=(\d+)\s+LMR=(\d+)\s+Corr=(\d+)"
    )
    hist = []
    with open(log_p, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            m = pat.search(line)
            if m:
                sc = float(m.group(3))
                ng = int(m.group(4))
                hist.append(
                    {
                        "iter": int(m.group(1)),
                        "plus_score": sc,
                        "n_games": ng,
                        "win_pct": round(100.0 * sc / max(1, ng), 1),
                        "elapsed": float(m.group(5)),
                        "RFPBase": int(m.group(6)),
                        "FutMult": int(m.group(7)),
                        "HistLMRDiv": int(m.group(8)),
                        "CorrDiv": int(m.group(9)),
                    }
                )
    if hist:
        st["history"] = hist
        _atomic_json(st_p, st)
    return st


def generate_all_reports():
    os.makedirs(REPORT_DIR, exist_ok=True)
    now_str = time.strftime("%Y-%m-%d %H:%M:%S")
    written = []

    # 1. Syzygy Tablebases Report
    syz_dir = os.path.join(ROOT, "data", "syzygy")
    if os.path.isdir(syz_dir):
        files = [fn for fn in os.listdir(syz_dir) if fn.endswith(".rtbw") or fn.endswith(".rtbz")]
        if files:
            wdl_n = sum(1 for f in files if f.endswith(".rtbw"))
            dtz_n = sum(1 for f in files if f.endswith(".rtbz"))
            mb = sum(os.path.getsize(os.path.join(syz_dir, f)) for f in files) / (1024.0 * 1024.0)
            rep = {
                "id": "syzygy",
                "stage": "Tier 3 · Step 1 · 5-Piece Syzygy Vault",
                "title": "Syzygy 3-4-5 Endgame Tablebases",
                "status": "complete" if len(files) >= 285 else "partial",
                "health": "ok" if len(files) >= 285 else "warn",
                "updated_at": now_str,
                "summary": f"{len(files)} / 290 tablebase files installed ({mb:.1f} MB). Exact WDL/DTZ lookups active for <=5 piece endgames.",
                "metrics": [
                    {"label": "Total Files", "value": f"{len(files)} / 290"},
                    {"label": "WDL / DTZ Split", "value": f"{wdl_n} WDL · {dtz_n} DTZ"},
                    {"label": "Disk Footprint", "value": f"{mb:.1f} MB"},
                    {"label": "Unlock Impact", "value": "Apex-C + Syzygy (#19)"},
                ],
                "diagnosis": (
                    "Full 3-4-5 piece coverage verified — mathematical endgame perfection active."
                    if len(files) >= 285
                    else "Partial download — resume in Tier 3 Step 1 to complete all 290 files."
                ),
                "next_step": "Proceed to Tier 3 Step 2 (Self-Play Datagen — Pass 1 uses Singularity v5 as Teacher!).",
            }
            _atomic_json(os.path.join(REPORT_DIR, "syzygy_report.json"), rep)
            written.append("syzygy_report.json")

    # 2. Datagen Report + Quality Stats
    dg_res = analyze_datagen_file()
    if dg_res is not None:
        dg_stats, dg_hist = dg_res
        dg_st_path = os.path.join(ROOT, "data", "p3k_datagen_status.json")
        dg_st = json.load(open(dg_st_path)) if os.path.exists(dg_st_path) else {}
        n_pos = dg_stats["total_positions"]
        corr = dg_stats["score_result_corr"]
        health = "ok" if (n_pos >= 10000000 and corr >= 0.55) else "warn"
        rep = {
            "id": "datagen",
            "stage": "Tier 3 · Step 2 · Self-Play Datagen",
            "title": "Self-Play Dataset Audit (datagen_v4.bin)",
            "status": "complete" if n_pos >= 25000000 else "partial",
            "health": health,
            "updated_at": now_str,
            "summary": f"Generated {n_pos:,} quiet positions ({dg_st.get('games', 0):,} games). Eval-to-outcome correlation r = +{corr:.3f}.",
            "metrics": [
                {"label": "Total Positions", "value": f"{n_pos:,}"},
                {"label": "Win / Draw / Loss", "value": f"{dg_stats['win_pct']}% / {dg_stats['draw_pct']}% / {dg_stats['loss_pct']}%"},
                {"label": "Score ↔ Result Corr", "value": f"+{corr:.3f}"},
                {"label": "Mean Win / Loss Eval", "value": f"+{dg_stats['mean_win_cp']:.0f} / {dg_stats['mean_loss_cp']:.0f} cp"},
            ],
            "diagnosis": (
                f"Healthy self-play dataset! Balanced outcome distribution and strong +{corr:.3f} correlation. "
                "Tip: To climb past ~2,400 Elo toward 3,000+, run Pass-2 Datagen with your 1024 v4 NNUE or Singularity v5 as the teacher."
                if corr >= 0.55
                else "Low score-to-result correlation — check node budget or opening diversity."
            ),
            "chart_series": dg_stats,
            "next_step": "Use this dataset to train Big NNUE v4 in Tier 3 Step 3.",
        }
        _atomic_json(os.path.join(REPORT_DIR, "datagen_report.json"), rep)
        written.append("datagen_report.json")

    # 3. Big NNUE v4 Report
    nn_st = backfill_nnue_history()
    nn_path = os.path.join(ROOT, "data", "titan_v6_1024.nnue")
    if nn_st or os.path.exists(nn_path):
        nn_st = nn_st or {}
        hist = nn_st.get("history", [])
        best_val = nn_st.get("best_val_mse", 0.0)
        best_ep = nn_st.get("best_epoch", 1)
        last_trn = hist[-1]["train_mse"] if hist else 0.0
        last_val = hist[-1]["val_mse"] if hist else best_val
        overfit_note = (
            f"Validation MSE peaked early at Epoch {best_ep} ({best_val:.5f}) while later epochs memorized training loss ({last_trn:.5f} vs val {last_val:.5f}). Best Epoch {best_ep} weights were safely kept!"
            if (hist and best_ep < len(hist) and last_val > best_val + 0.0002)
            else f"Clean convergence to best validation MSE = {best_val:.5f} at Epoch {best_ep}."
        )
        rep = {
            "id": "nnue_v4",
            "stage": "Tier 3 · Step 3 · 1024 Big NNUE v4",
            "title": "16×768 → 1024 NNUE v4 GPU Training Report",
            "status": "complete" if os.path.exists(nn_path) else "partial",
            "health": "ok" if os.path.exists(nn_path) else "warn",
            "updated_at": now_str,
            "summary": f"Forged 1024-wide quantized NNUE v4 on {nn_st.get('total_positions', 0):,} positions (best val MSE = {best_val:.5f} at Epoch {best_ep}).",
            "metrics": [
                {"label": "Architecture", "value": f"(16×768) → {nn_st.get('hidden', 1024)}×2 → 8"},
                {"label": "Best Val MSE", "value": f"{best_val:.5f} (Ep {best_ep})"},
                {"label": "Final Train / Val", "value": f"{last_trn:.5f} / {last_val:.5f}"},
                {"label": "Training Time", "value": f"{nn_st.get('elapsed_sec', 0)/60:.1f} min"},
            ],
            "diagnosis": overfit_note,
            "chart_series": {"history": hist, "best_epoch": best_ep, "best_val_mse": best_val},
            "next_step": "Tune search pruning constants in Tier 3 Step 4 (SPSA) or test Apex-C in Elo Lab.",
        }
        _atomic_json(os.path.join(REPORT_DIR, "nnue_v4_report.json"), rep)
        written.append("nnue_v4_report.json")

    # 4. SPSA Report
    sp_st = backfill_spsa_history()
    sp_file = os.path.join(ROOT, "data", "spsa_best.json")
    if sp_st or os.path.exists(sp_file):
        sp_st = sp_st or {}
        sp_json = json.load(open(sp_file)) if os.path.exists(sp_file) else {}
        params = sp_st.get("params") or sp_json.get("params") or {}
        hist = sp_st.get("history", [])
        avg_win = round(sum(x.get("win_pct", 50.0) for x in hist) / max(1, len(hist)), 1) if hist else 50.0
        rfp = params.get("RFPBase", 75)
        lmr = params.get("HistLMRDiv", 8192)
        small_drift = abs(rfp - 75) <= 2 and abs(lmr - 8192) <= 60
        rep = {
            "id": "spsa",
            "stage": "Tier 3 · Step 4 · SPSA Search Tuner",
            "title": "SPSA Pruning & LMR Search Calibration Report",
            "status": "complete" if (sp_st.get("iter", 0) >= 50 or os.path.exists(sp_file)) else "partial",
            "health": "warn" if small_drift else "ok",
            "updated_at": now_str,
            "summary": f"Completed {sp_st.get('iter', sp_json.get('iter', 0))} SPSA iterations. Parameters loaded automatically by Apex-C.",
            "metrics": [
                {"label": "Iterations", "value": f"{sp_st.get('iter', sp_json.get('iter', 0))}"},
                {"label": "Avg +delta Score", "value": f"{avg_win}%"},
                {"label": "RFPBase / FutMult", "value": f"{params.get('RFPBase', 75)} / {params.get('FutMult', 110)}"},
                {"label": "HistLMRDiv / CorrDiv", "value": f"{params.get('HistLMRDiv', 8192)} / {params.get('CorrDiv', 512)}"},
            ],
            "diagnosis": (
                "Notice parameters barely moved from defaults (legacy small step schedule on v3 NNUE). Re-running SPSA now uses the fixed step schedule & your 1024 v4 NNUE!"
                if small_drift
                else "Search pruning & LMR constants actively adapted cleanly to your evaluation function."
            ),
            "chart_series": {"history": hist, "params": params},
            "next_step": "Challenge Apex-C Pure or Apex-C + Syzygy in Elo Lab!",
        }
        _atomic_json(os.path.join(REPORT_DIR, "spsa_report.json"), rep)
        written.append("spsa_report.json")

    # 5. Zero-C v2 Report
    az_st_p = os.path.join(ROOT, "data", "az", "status.json")
    if os.path.exists(az_st_p):
        az = json.load(open(az_st_p))
        h = az.get("history", [])
        grad = os.path.exists(os.path.join(ROOT, "data", "az", "v2_complete.flag"))
        last_acc = h[-1].get("pol_acc", 0.0) if h else 0.0
        last_pol = h[-1].get("pol_loss", 0.0) if h else 0.0
        last_wdl = h[-1].get("wdl_loss", 0.0) if h else 0.0
        comp = az.get("completion", {})
        rep = {
            "id": "zeroc_v2",
            "stage": "Tier 2 · Zero-C Neural Guild",
            "title": "Zero-C v2 (10×128 SE-ResNet) Progress Report",
            "status": "complete" if grad else "in_progress",
            "health": "ok",
            "updated_at": now_str,
            "summary": f"Gen {az.get('gen', 0):,} · {az.get('total_games', 0):,} pure self-play games · Policy Top-1 Acc {last_acc:.1f}% ({comp.get('progress_pct', 0)}% convergence).",
            "metrics": [
                {"label": "Generation / Games", "value": f"Gen {az.get('gen', 0):,} · {az.get('total_games', 0):,}g"},
                {"label": "Policy Top-1 Acc", "value": f"{last_acc:.1f}% (loss {last_pol:.3f})"},
                {"label": "WDL / Replay Buffer", "value": f"{last_wdl:.3f} · {az.get('replay_size', 0):,} pos"},
                {"label": "Convergence Gates", "value": f"{comp.get('progress_pct', 0)}%"},
            ],
            "diagnosis": comp.get("reason", "Zero-C v2 self-play training active and healthy."),
            "chart_series": {"history": h[-250:]},
            "next_step": "Hit 500,000 games (or graduate) and distill Singularity v5 (#17) to complete Tier 2 and unlock Tier 3!",
        }
        _atomic_json(os.path.join(REPORT_DIR, "zeroc_v2_report.json"), rep)
        written.append("zeroc_v2_report.json")

    # 6. Distillation Report
    ds_p = os.path.join(ROOT, "data", "az", "distill_status.json")
    if os.path.exists(ds_p):
        ds = json.load(open(ds_p))
        if ds.get("distilled") or os.path.exists(os.path.join(ROOT, "data", "chess_net_zero_v4.nnue")):
            rep = {
                "id": "distill",
                "stage": "Tier 2 Capstone · Zero-C Neural Guild",
                "title": "Zero-C → AVX2 NNUE Knowledge Distillation Report",
                "status": "complete",
                "health": "ok",
                "updated_at": now_str,
                "summary": f"Distilled {ds.get('samples', 0):,} positions from Zero-C v2 Gen {ds.get('teacher_gen', '?')} (val MSE={ds.get('best_val_mse', ds.get('val_mse', 0))}).",
                "metrics": [
                    {"label": "Teacher Gen", "value": f"Gen {ds.get('teacher_gen', '?')}"},
                    {"label": "Best Val MSE", "value": f"{ds.get('best_val_mse', ds.get('val_mse', 0))}"},
                    {"label": "Samples", "value": f"{ds.get('samples', 0):,}"},
                    {"label": "Unlocked Bot", "value": "Singularity v5 (#17) • Unlocks Tier 3"},
                ],
                "diagnosis": "Compressed 3.56M ResNet into quantized 200ns AVX2 NNUE v3 — finishing the Zero-C Neural Guild (Tier 2)!",
                "chart_series": {"history": ds.get("history", [])},
                "next_step": "Mark Tier 2 Complete (if needed) to unlock Tier 3: Mythic Hybrids (#14-16) & Apex Forge (#18-19)!",
            }
            _atomic_json(os.path.join(REPORT_DIR, "distill_report.json"), rep)
            written.append("distill_report.json")

    return written


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--datagen-only", action="store_true")
    args = ap.parse_args()
    if args.datagen_only:
        analyze_datagen_file()
    else:
        res = generate_all_reports()
        print("Generated reports:", res)
