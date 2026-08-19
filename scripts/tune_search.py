#!/usr/bin/env python3
"""Search-parameter tuner for La Mano ("solve score" optimizer).

Evaluates search-parameter configurations of the UCI-tunable v0.4.4 build
(branch tune-uci-v044, binary bin_tune044/talshand_exe) on a labeled position
set (scripts/tune_positions.py). Per-position score (continuous, in [0, 2]):

    held  = fraction of the movetime budget during which the engine's PV was
            the reference move (per-depth intervals; the final PV counts from
            its completion to the end of the budget)
    score = (final bestmove == reference ? 1 : 0) + held

This rewards finding the best move, finding it EARLY, and HOLDING it, without
the all-or-nothing cliff of a pure "held to the end" rule (a PV blip at the
last depth costs only its interval, not the whole position). Raw per-depth
records [depth, time_ms, nodes, cp, move] are stored, so any other formula
(e.g. node-based earliness) can be recomputed offline without replaying.

Anti-overfit: positions are deterministically split into TRAIN/HOLDOUT halves
(FEN hash parity). Rank configs on train; trust a winner only if holdout
agrees; final acceptance is always a real match (scripts/version_match.py).

Every run is logged durably and incrementally (atomic rewrite after each
position — Ctrl-C loses nothing), with the engine + net identity embedded.

Modes
-----
    tune_search.py --tag baseline                          # defaults only
    tune_search.py --tag lmr --sweep "LMRK10=200,260,300,340,380"
    tune_search.py --tag fut --grid "FutBase=50,75,100;FutSlope=50,75,100"
    tune_search.py --tag x --params "NMPBase=3" --sweep "NMPDepthDiv=4,6,8"
    tune_search.py --tag calib --movetime 10000            # achievability calibration

Timing hygiene: Threads=1, one engine at a time, fresh process per position.
Do not run anything heavy on the machine while a run is in progress.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import itertools
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from version_match import net_content_hash, atomic_write_json  # noqa: E402

DEFAULT_ENGINE = REPO / "bin_tune044" / "talshand_exe"
DEFAULT_NET = REPO / "models" / "n256_h16x16_sq"
DEFAULT_POSITIONS = REPO / "version_test_results" / "tuning" / "positions.json"
RUNS_DIR = REPO / "version_test_results" / "tuning" / "runs"

INFO_RE = re.compile(r"^info depth (\d+) score cp (-?\d+) nodes (\d+) nps \d+ time (\d+) pv (\S+)")

# The 17 tunable options exposed by the tune-uci-v044 build (defaults mirrored here
# so runs are self-describing even if the binary's defaults ever drift).
DEFAULTS = {
    "LMRK10": 230, "LMRHistDiv": 8000,
    "LMPBase": 3, "LMPDiv": 2, "LMPMaxDepth": 6,
    "NMPBase": 2, "NMPDepthDiv": 6, "NMPMinDepth": 3,
    "RFPMargin": 175, "RFPMaxDepth": 3,
    "FutBase": 75, "FutSlope": 75, "FutMaxDepth": 6, "FutMinMoves": 2,
    "SEEMargin": 75, "SEEMaxDepth": 6, "SEEQSMargin": 120,
}

OPTION_BOUNDS = {  # engine-side clamps (engine.cpp TUNABLES) — reject out-of-range early
    "LMRK10": (0, 1000), "LMRHistDiv": (1, 1000000),
    "LMPBase": (0, 1000), "LMPDiv": (1, 64), "LMPMaxDepth": (0, 31),
    "NMPBase": (0, 16), "NMPDepthDiv": (1, 64), "NMPMinDepth": (1, 32),
    "RFPMargin": (0, 4000), "RFPMaxDepth": (0, 16),
    "FutBase": (0, 4000), "FutSlope": (0, 4000), "FutMaxDepth": (0, 16),
    "FutMinMoves": (1, 64),
    "SEEMargin": (0, 4000), "SEEMaxDepth": (0, 16), "SEEQSMargin": (0, 4000),
}


class EngineRunError(RuntimeError):
    pass


def split_of(fen: str) -> str:
    """Deterministic TRAIN/HOLDOUT split by FEN-board hash parity."""
    key = " ".join(fen.split()[:4])
    return "train" if hashlib.md5(key.encode()).digest()[0] % 2 == 0 else "holdout"


def run_position(engine: Path, net: Path, fen: str, movetime_ms: int,
                 params: dict[str, int]) -> dict:
    """One engine process, one position, exact `movetime_ms` budget.
    Raises EngineRunError on a dead/silent engine instead of faking a 0 score."""
    cmds = ["uci", "setoption name Threads value 1"]
    cmds += [f"setoption name {k} value {v}" for k, v in params.items()]
    cmds += [f"position fen {fen}", f"go movetime {movetime_ms}"]
    env = dict(os.environ, NNUEU_NET=str(net) + "/")
    last_exc: Exception | None = None
    for attempt in (1, 2):  # one retry for transient failures
        try:
            proc = subprocess.run([str(engine)], input="\n".join(cmds) + "\n",
                                  capture_output=True, text=True, env=env,
                                  timeout=movetime_ms / 1000 + 60)
        except subprocess.TimeoutExpired as exc:
            last_exc = exc
            continue
        records, bestmove = [], None
        for line in proc.stdout.splitlines():
            m = INFO_RE.match(line)
            if m:
                records.append([int(m.group(1)), int(m.group(4)), int(m.group(3)),
                                int(m.group(2)), m.group(5)])
            elif line.startswith("bestmove"):
                bestmove = line.split()[1]
        if proc.returncode == 0 and bestmove is not None:
            return {"records": records, "bestmove": bestmove}
        last_exc = EngineRunError(
            f"engine failed (rc={proc.returncode}, bestmove={bestmove}) "
            f"stderr: {proc.stderr.strip()[:300]!r}")
    raise EngineRunError(f"engine failed twice on {fen!r}: {last_exc}")


def score_position(result: dict, bm: str, movetime_ms: int) -> tuple[float, float | None]:
    """(score, t_first_held_ms).

    held  = sum of PV==bm intervals / budget. Record i's PV is in effect from its
            completion time to the next record's; the last record's to budget end.
    score = (bestmove == bm) + held  -> [0, 2].
    t_first_held_ms = time of the earliest record from which the PV stayed == bm
    (None when never held to the end) — diagnostic only, not part of the score.
    """
    records = result["records"]
    if not records:
        return 0.0, None
    held_ms = 0.0
    t_star = None
    for i, (_d, t_ms, _n, _cp, move) in enumerate(records):
        t_next = records[i + 1][1] if i + 1 < len(records) else max(movetime_ms, t_ms)
        if move == bm:
            held_ms += max(0, t_next - t_ms)
            if t_star is None:
                t_star = t_ms
        else:
            t_star = None
    held = min(1.0, held_ms / movetime_ms)
    solved = 1.0 if result["bestmove"] == bm else 0.0
    return solved + held, (float(min(t_star, movetime_ms)) if t_star is not None else None)


def summarize(per_pos: list[dict]) -> dict:
    """Aggregates over a per-position result list (overall + train/holdout splits)."""
    def agg(rows: list[dict]) -> dict:
        solved = [x for x in rows if x["solved"]]
        return {"n": len(rows),
                "score": round(sum(x["score"] for x in rows), 3),
                "solved": len(solved),
                "mean_t_star_ms": round(sum(x["t_star_ms"] for x in solved if x["t_star_ms"] is not None)
                                        / max(1, sum(1 for x in solved if x["t_star_ms"] is not None)), 1)
                                  if solved else None,
                "mean_final_depth": round(sum(x["final_depth"] for x in rows) / len(rows), 2) if rows else None}
    return {"overall": agg(per_pos),
            "train": agg([x for x in per_pos if x["split"] == "train"]),
            "holdout": agg([x for x in per_pos if x["split"] == "holdout"])}


def eval_config(engine: Path, net: Path, positions: list[dict], movetime_ms: int,
                params: dict[str, int], repeats: int, on_progress) -> dict:
    """Run a config over the set (optionally `repeats` times, scores averaged);
    records kept from the last repeat. Returns the full result entry."""
    per_pos: list[dict] = []
    for i, p in enumerate(positions):
        scores, t_star, result = [], None, None
        for _ in range(repeats):
            result = run_position(engine, net, p["fen"], movetime_ms, params)
            s, ts = score_position(result, p["bm"], movetime_ms)
            scores.append(s)
            t_star = ts if ts is not None else t_star
        s_avg = sum(scores) / len(scores)
        per_pos.append({"fen": p["fen"], "bm": p["bm"], "split": split_of(p["fen"]),
                        "band": p.get("band", "hard"),
                        "score": round(s_avg, 4), "repeat_scores": [round(x, 4) for x in scores],
                        "solved": result["bestmove"] == p["bm"], "t_star_ms": t_star,
                        "final_depth": result["records"][-1][0] if result["records"] else 0,
                        "bestmove": result["bestmove"], "records": result["records"]})
        on_progress(i + 1, per_pos)
    return {"params": params, **summarize(per_pos),
            "score": round(sum(x["score"] for x in per_pos), 3),  # convenience: == overall.score
            "per_position": per_pos}


def parse_assignments(spec: str) -> dict[str, list[int]]:
    """"A=1,2,3;B=4,5" -> {A:[1,2,3], B:[4,5]} (validates names and bounds)."""
    out: dict[str, list[int]] = {}
    for part in filter(None, (s.strip() for s in spec.split(";"))):
        name, vals = part.split("=", 1)
        name = name.strip()
        if name not in DEFAULTS:
            raise SystemExit(f"unknown option {name!r} — valid: {', '.join(DEFAULTS)}")
        values = [int(v) for v in vals.split(",")]
        lo, hi = OPTION_BOUNDS[name]
        bad = [v for v in values if not lo <= v <= hi]
        if bad:
            raise SystemExit(f"{name} values {bad} outside engine clamp [{lo}, {hi}]")
        out[name] = values
    return out


def canon_key(params: dict[str, int], defaults: dict[str, int]) -> str:
    return json.dumps({k: params.get(k, defaults[k]) for k in defaults}, sort_keys=True)


def main() -> int:
    ap = argparse.ArgumentParser(description="La Mano search-parameter tuner")
    ap.add_argument("--engine", default=str(DEFAULT_ENGINE))
    ap.add_argument("--net", default=str(DEFAULT_NET))
    ap.add_argument("--positions", default=str(DEFAULT_POSITIONS))
    ap.add_argument("--movetime", type=int, default=2500, help="per-position budget, ms")
    ap.add_argument("--max-positions", type=int, default=0, help="cap the set (0 = all)")
    ap.add_argument("--band", default="hard", choices=["hard", "soft", "all"],
                    help="which label band to run (soft = over-pruning canary set)")
    ap.add_argument("--repeats", type=int, default=1,
                    help="repeat each position N times, average the score (noise control)")
    ap.add_argument("--tag", required=True, help="run name (output file prefix)")
    ap.add_argument("--params", default="", help="fixed overrides: 'Name=val;Name=val' (single values)")
    ap.add_argument("--sweep", default="", help="one-at-a-time: 'Name=v1,v2,v3'")
    ap.add_argument("--grid", default="", help="cartesian: 'N1=a,b;N2=c,d'")
    ap.add_argument("--no-baseline", action="store_true",
                    help="skip the leading reference config (defaults + --params)")
    ap.add_argument("--resume", default="", help="existing run JSON: skip finished configs")
    args = ap.parse_args()

    pos_doc = json.loads(Path(args.positions).read_text())
    positions = pos_doc["positions"]
    if args.band != "all":
        positions = [p for p in positions if p.get("band", "hard") == args.band]
    if args.max_positions:
        positions = positions[: args.max_positions]
    if not positions:
        raise SystemExit("empty position set")
    fen_seq = [p["fen"] for p in positions]

    fixed_spec = parse_assignments(args.params) if args.params else {}
    multi = [k for k, v in fixed_spec.items() if len(v) != 1]
    if multi:
        raise SystemExit(f"--params takes single values; {multi} have several (use --sweep/--grid)")
    fixed = {k: v[0] for k, v in fixed_spec.items()}

    configs: list[dict[str, int]] = []
    if not args.no_baseline:
        configs.append(dict(fixed))  # reference config: defaults + fixed overrides
    if args.sweep:
        sw = parse_assignments(args.sweep)
        if len(sw) != 1:
            raise SystemExit("--sweep takes exactly one option (use --grid for several)")
        name, vals = next(iter(sw.items()))
        configs += [{**fixed, name: v} for v in vals]
    if args.grid:
        gr = parse_assignments(args.grid)
        names = list(gr)
        configs += [{**fixed, **dict(zip(names, combo))}
                    for combo in itertools.product(*(gr[n] for n in names))]
    uniq, seen = [], set()
    for c in configs:
        key = canon_key(c, DEFAULTS)
        if key not in seen:
            seen.add(key)
            uniq.append(c)
    configs = uniq

    net = Path(args.net)
    net_sha = net_content_hash(net)

    # Resume: only import prior configs from a run with IDENTICAL settings —
    # same positions (fen sequence), movetime, engine, net and defaults.
    done_keys: set[str] = set()
    prior_results: list[dict] = []
    if args.resume:
        prior = json.loads(Path(args.resume).read_text())
        pm = prior.get("meta", {})
        problems = []
        if pm.get("movetime_ms") != args.movetime:
            problems.append(f"movetime {pm.get('movetime_ms')} != {args.movetime}")
        if pm.get("engine", {}).get("binary") != str(args.engine):
            problems.append("different engine binary")
        if pm.get("engine", {}).get("net_sha256") != net_sha:
            problems.append("different net")
        if pm.get("repeats", 1) != args.repeats:
            problems.append(f"repeats {pm.get('repeats', 1)} != {args.repeats}")
        prior_defaults = pm.get("defaults", DEFAULTS)
        for r in prior.get("results", []):
            pp = r.get("per_position") or []
            if [x["fen"] for x in pp] != fen_seq:
                continue  # different/incomplete position sequence -> not importable
            if "overall" not in r:
                continue  # partial holder without aggregates
            prior_results.append(r)
            done_keys.add(canon_key(r["params"], prior_defaults))
        if problems:
            raise SystemExit("refusing --resume (settings differ): " + "; ".join(problems))
        print(f"[tune] resume: imported {len(prior_results)} finished configs", flush=True)

    RUNS_DIR.mkdir(parents=True, exist_ok=True)
    ts = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    out_path = RUNS_DIR / f"{ts}_{args.tag}.json"
    meta = {
        "tag": args.tag, "status": "in_progress",
        "started_at": datetime.datetime.now().isoformat(timespec="seconds"),
        "movetime_ms": args.movetime, "repeats": args.repeats,
        "n_positions": len(positions), "band": args.band,
        "positions_file": str(args.positions),
        "positions_meta": pos_doc.get("meta", {}),
        "engine": {"binary": str(args.engine), "net_dir": str(net),
                   "net_name": net.name, "net_sha256": net_sha,
                   "branch": "tune-uci-v044"},
        "defaults": DEFAULTS, "n_configs": len(configs),
        "score_formula": "(bestmove==bm) + held_time_fraction; records=[depth,time_ms,nodes,cp,move]",
    }
    results: list[dict] = list(prior_results)

    def flush(status: str) -> None:
        meta["status"] = status
        meta["updated_at"] = datetime.datetime.now().isoformat(timespec="seconds")
        atomic_write_json(str(out_path), {"meta": meta, "results": results})

    flush("in_progress")
    print(f"[tune] {len(configs)} configs x {len(positions)} positions ({args.band} band) "
          f"@ {args.movetime}ms x{args.repeats}  ->  {out_path}", flush=True)

    try:
        for ci, cfg in enumerate(configs):
            key = canon_key(cfg, DEFAULTS)
            label = ", ".join(f"{k}={v}" for k, v in cfg.items()) or "defaults"
            if key in done_keys:
                print(f"[tune] config {ci+1}/{len(configs)} ({label}) already done — skipped", flush=True)
                continue
            print(f"[tune] config {ci+1}/{len(configs)}: {label}", flush=True)
            partial_holder: dict = {"params": cfg, "score": None, "per_position": []}
            results.append(partial_holder)

            def on_progress(n_done: int, per_pos: list) -> None:
                partial_holder["per_position"] = per_pos
                partial_holder["score"] = round(sum(x["score"] for x in per_pos), 3)
                if n_done % 10 == 0 or n_done == len(positions):
                    flush("in_progress")
                if n_done % 25 == 0:
                    print(f"    {n_done}/{len(positions)}  score so far {partial_holder['score']}", flush=True)

            full = eval_config(Path(args.engine), net, positions, args.movetime,
                               cfg, args.repeats, on_progress)
            results[-1] = full
            done_keys.add(key)
            flush("in_progress")
            o, tr, ho = full["overall"], full["train"], full["holdout"]
            print(f"    => score {o['score']} (train {tr['score']} / holdout {ho['score']})"
                  f"  solved {o['solved']}/{o['n']}  mean_depth {o['mean_final_depth']}", flush=True)
    except KeyboardInterrupt:
        print("[tune] interrupted — partial run preserved", flush=True)
        flush("stopped")
        return 1
    except EngineRunError as exc:
        print(f"[tune] ABORT — engine failure (not scored as 0): {exc}", flush=True)
        flush("stopped")
        return 2

    flush("completed")
    # Reference for deltas = the leading config (defaults + fixed overrides).
    base_key = canon_key(fixed, DEFAULTS)
    base = next((r for r in results if "overall" in r and canon_key(r["params"], DEFAULTS) == base_key), None)
    print("\n===================== TUNE SUMMARY =====================")
    for r in results:
        if "overall" not in r:
            continue
        label = ", ".join(f"{k}={v}" for k, v in r["params"].items()) or "defaults"
        o, tr, ho = r["overall"], r["train"], r["holdout"]
        delta = ""
        if base is not None and r is not base:
            delta = (f"  Δtrain {tr['score'] - base['train']['score']:+.2f}"
                     f" Δhold {ho['score'] - base['holdout']['score']:+.2f}")
        print(f"  {label:<40} score {o['score']:>8}  solved {o['solved']}/{o['n']}"
              f"  depth {o['mean_final_depth']}{delta}")
    print("========================================================")
    print("[tune] rank on TRAIN, trust only if HOLDOUT agrees; final gate = version_match")
    print(f"[tune] wrote {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
