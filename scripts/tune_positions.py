#!/usr/bin/env python3
"""Build the search-parameter tuning position set.

Samples real game positions from the engine's own match PGNs
(version_test_results/*.pgn) and labels each with Stockfish. A position is kept
only when its best move is UNAMBIGUOUS and STABLE, so the tuner's
"found the best move" signal is clean:

  - |eval| <= max-cp           (not already hopeless/won — many moves win there)
  - multipv gap >= soft-gap    (bm clearly better than the 2nd-best move)
  - bm identical at the shallow and deep label depths (stability)
  - position not in the opening book region (ply >= min-ply)

Kept positions carry a band tag: "hard" (gap >= min-gap: sharp, mostly tactical
decisions — the main tuning score) and "soft" (soft-gap <= gap < min-gap:
quieter positional choices — the over-pruning canary set, since LMP/futility/RFP
regressions show up exactly there and barely on the hard band).

NOTE: labels are produced with multi-threaded Stockfish, so they are NOT
bit-reproducible run to run (SMP nondeterminism); the sampling IS deterministic.

Output: a self-describing JSON (positions + provenance + filter settings).

    /usr/local/bin/python3 scripts/tune_positions.py \
        --out version_test_results/tuning/positions.json
"""
from __future__ import annotations

import argparse
import datetime
import json
import os
import random
import sys
from pathlib import Path

import chess
import chess.engine
import chess.pgn

REPO = Path(__file__).resolve().parent.parent
MATE_CP = 10000  # mate scores mapped to +/- this many cp


def sample_candidates(pgn_dir: Path, min_ply: int, max_ply: int, stride: int,
                      cap: int, seed: int) -> list[dict]:
    """Walk every *.pgn, take positions every `stride` plies inside [min_ply, max_ply],
    dedupe by FEN (board+stm+castling+ep), shuffle deterministically, cap."""
    seen: set[str] = set()
    out: list[dict] = []
    for pgn_path in sorted(pgn_dir.glob("*.pgn")):
        with open(pgn_path) as fh:
            while True:
                game = chess.pgn.read_game(fh)
                if game is None:
                    break
                board = game.board()
                for ply, move in enumerate(game.mainline_moves(), start=1):
                    board.push(move)
                    if ply < min_ply or ply > max_ply or (ply - min_ply) % stride:
                        continue
                    key = " ".join(board.fen().split()[:4])  # drop move counters
                    if key in seen or board.is_game_over(claim_draw=True):
                        continue
                    if board.legal_moves.count() < 8:  # skip near-trivial positions
                        continue
                    seen.add(key)
                    out.append({"fen": board.fen(), "src": pgn_path.name, "ply": ply})
    random.Random(seed).shuffle(out)
    return out[:cap]


def score_cp(s: chess.engine.PovScore, pov: chess.Color) -> int:
    v = s.pov(pov)
    if v.is_mate():
        return MATE_CP if v.mate() > 0 else -MATE_CP
    return v.score()


def label(cands: list[dict], sf_path: str, shallow: int, deep: int,
          max_cp: int, min_gap: int, soft_gap: int, threads: int, out_path: Path,
          meta: dict) -> None:
    """Label candidates with Stockfish; write the (partial) output atomically after
    every position so an interrupted run keeps everything labeled so far."""
    kept: list[dict] = []
    stats = {"candidates": len(cands), "labeled": 0, "kept": 0,
             "kept_hard": 0, "kept_soft": 0,
             "drop_eval": 0, "drop_gap": 0, "drop_unstable": 0, "drop_nopv": 0}

    def flush(status: str) -> None:
        doc = {"meta": {**meta, "status": status, "stats": stats,
                        "updated_at": datetime.datetime.now().isoformat(timespec="seconds")},
               "positions": kept}
        tmp = out_path.with_name(out_path.name + ".tmp")
        tmp.write_text(json.dumps(doc, indent=1))
        os.replace(tmp, out_path)

    def fresh_engine():
        e = chess.engine.SimpleEngine.popen_uci(sf_path)
        e.configure({"Threads": threads, "Hash": 256})
        return e

    flush("in_progress")
    engine = fresh_engine()
    status = "completed"
    try:
        for i, cand in enumerate(cands):
            board = chess.Board(cand["fen"])
            try:
                # game=i => python-chess sends ucinewgame per position (hash isolation)
                info_shallow = engine.analyse(board, chess.engine.Limit(depth=shallow), game=i)
                info_deep = engine.analyse(board, chess.engine.Limit(depth=deep), multipv=2, game=i)
            except chess.engine.EngineError:
                try:
                    engine.close()  # never leak the dead/wedged process
                except Exception:
                    pass
                engine = fresh_engine()
                continue
            stats["labeled"] += 1
            if "pv" not in info_shallow or not info_deep or "pv" not in info_deep[0]:
                stats["drop_nopv"] += 1
                continue
            bm_shallow = info_shallow["pv"][0]
            bm_deep = info_deep[0]["pv"][0]
            cp1 = score_cp(info_deep[0]["score"], board.turn)
            cp2 = score_cp(info_deep[1]["score"], board.turn) \
                if len(info_deep) > 1 and "score" in info_deep[1] else -MATE_CP
            gap = cp1 - cp2
            if abs(cp1) > max_cp:
                stats["drop_eval"] += 1
            elif gap < soft_gap:
                stats["drop_gap"] += 1
            elif bm_shallow != bm_deep:
                stats["drop_unstable"] += 1
            else:
                band = "hard" if gap >= min_gap else "soft"
                kept.append({"fen": cand["fen"], "bm": bm_deep.uci(), "cp": cp1,
                             "gap": gap, "band": band,
                             "src": cand["src"], "ply": cand["ply"]})
                stats["kept"] += 1
                stats[f"kept_{band}"] += 1
            if (i + 1) % 10 == 0 or kept and kept[-1]["fen"] == cand["fen"]:
                flush("in_progress")
            if (i + 1) % 25 == 0:
                print(f"[label] {i+1}/{len(cands)} labeled, kept {stats['kept']} "
                      f"(hard {stats['kept_hard']} / soft {stats['kept_soft']})", flush=True)
    except KeyboardInterrupt:
        print("[label] interrupted — partial set preserved", flush=True)
        status = "stopped"
    finally:
        try:
            engine.quit()
        except Exception:
            pass
        flush(status)
    print(f"[label] {status}: kept {stats['kept']} (hard {stats['kept_hard']}, "
          f"soft {stats['kept_soft']}) of {stats['labeled']} labeled "
          f"(drops: eval {stats['drop_eval']}, gap {stats['drop_gap']}, "
          f"unstable {stats['drop_unstable']}, nopv {stats['drop_nopv']}) -> {out_path}",
          flush=True)


def main() -> int:
    ap = argparse.ArgumentParser(description="Build the tuning position set")
    ap.add_argument("--pgn-dir", default=str(REPO / "version_test_results"))
    ap.add_argument("--out", default=str(REPO / "version_test_results/tuning/positions.json"))
    ap.add_argument("--stockfish", default="/opt/homebrew/bin/stockfish")
    ap.add_argument("--candidates", type=int, default=450, help="candidates to label")
    ap.add_argument("--min-ply", type=int, default=12)
    ap.add_argument("--max-ply", type=int, default=90)
    ap.add_argument("--stride", type=int, default=7)
    ap.add_argument("--shallow-depth", type=int, default=15)
    ap.add_argument("--deep-depth", type=int, default=22)
    ap.add_argument("--max-cp", type=int, default=400)
    ap.add_argument("--min-gap", type=int, default=50, help="hard-band threshold")
    ap.add_argument("--soft-gap", type=int, default=25, help="soft-band (canary) threshold")
    ap.add_argument("--threads", type=int, default=6)
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    cands = sample_candidates(Path(args.pgn_dir), args.min_ply, args.max_ply,
                              args.stride, args.candidates, args.seed)
    print(f"[sample] {len(cands)} candidate positions from {args.pgn_dir}", flush=True)
    meta = {"generated": datetime.datetime.now().isoformat(timespec="seconds"),
            "stockfish": args.stockfish,
            "label_depths": [args.shallow_depth, args.deep_depth],
            "filters": {"max_cp": args.max_cp, "min_gap": args.min_gap,
                        "soft_gap": args.soft_gap,
                        "min_ply": args.min_ply, "max_ply": args.max_ply,
                        "stride": args.stride, "candidates": args.candidates},
            "pgn_dir": str(args.pgn_dir), "threads": args.threads,
            "reproducible_labels": False,  # SMP Stockfish: labels vary run to run
            "seed": args.seed}
    label(cands, args.stockfish, args.shallow_depth, args.deep_depth,
          args.max_cp, args.min_gap, args.soft_gap, args.threads, out_path, meta)
    return 0


if __name__ == "__main__":
    sys.exit(main())
