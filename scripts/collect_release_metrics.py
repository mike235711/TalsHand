#!/usr/bin/env python3
"""Collect release metrics for the current build into a per-version JSON record.

Produces ``version_test_results/<version>.json`` capturing objective, quickly
measured signals that we want to track across versions:

  * perft nodes/second (AB and QS move generation) at a fixed depth, and
  * the number of "mate in X" puzzles solved.

Optionally (``--match <ref>``) it also plays a version match against another
build and stores the Elo result. The match is slow, so it is opt-in.

The version label defaults to the latest entry in CHANGELOG.md (falling back to
``git describe``). Use --version to override and --tag to mark an uncommitted
working tree (e.g. "0.3.2-dev").

Run from the repo root after a Release build, e.g.:
    cmake -B build_release -DCMAKE_BUILD_TYPE=Release && cmake --build build_release
    python3 scripts/collect_release_metrics.py
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
RESULTS_DIR = REPO / "version_test_results"
BUILD = REPO / "build_release"


def detect_version() -> str:
    changelog = REPO / "CHANGELOG.md"
    if changelog.exists():
        m = re.search(r"^##\s*\[(\d+\.\d+\.\d+)\]", changelog.read_text(), re.MULTILINE)
        if m:
            return m.group(1)
    try:
        out = subprocess.run(["git", "describe", "--tags", "--always"],
                             cwd=REPO, capture_output=True, text=True, check=True)
        return out.stdout.strip().lstrip("v")
    except Exception:
        return "unknown"


def collect_perft_nps(depth: int) -> dict:
    binary = BUILD / "tests" / "perft_perf_tests"
    if not binary.exists():
        raise FileNotFoundError(f"{binary} not found; build the Release tests first")
    env = {"TALSHAND_PERFT_DEPTH": str(depth)}
    proc = subprocess.run([str(binary)], cwd=binary.parent, capture_output=True,
                          text=True, env={**_os_environ(), **env})
    ab, qs = [], []
    for line in proc.stdout.splitlines():
        m = re.search(r"\[Perft-(AB|QS)\] Position (\d+):.*nps=(\d+)", line)
        if m:
            (ab if m.group(1) == "AB" else qs).append(
                {"position": int(m.group(2)), "nps": int(m.group(3))})
    return {
        "depth": depth,
        "AB": ab,
        "QS": qs,
        "AB_median_nps": _median([x["nps"] for x in ab]),
        "QS_median_nps": _median([x["nps"] for x in qs]),
    }


def collect_mates() -> dict:
    binary = BUILD / "tests" / "mate_tests"
    if not binary.exists():
        raise FileNotFoundError(f"{binary} not found; build the Release tests first")
    proc = subprocess.run([str(binary)], cwd=binary.parent, capture_output=True, text=True)
    solved, total = 0, 0
    puzzles = []
    for line in proc.stdout.splitlines():
        m = re.search(r"\[mate\] (.+?) \| fen=.* \| move=(\S+) \| score=(-?\d+)", line)
        if m:
            total += 1
            score = int(m.group(3))
            ok = score >= 29000
            solved += int(ok)
            puzzles.append({"name": m.group(1), "move": m.group(2),
                            "score": score, "solved": ok})
    return {"solved": solved, "total": total, "puzzles": puzzles,
            "passed": proc.returncode == 0}


def _median(xs: list[int]) -> float:
    if not xs:
        return 0.0
    s = sorted(xs)
    n = len(s)
    return float(s[n // 2]) if n % 2 else (s[n // 2 - 1] + s[n // 2]) / 2.0


def _os_environ() -> dict:
    import os
    return dict(os.environ)


def main() -> int:
    ap = argparse.ArgumentParser(description="Collect TalsHand release metrics")
    ap.add_argument("--version", help="version label (default: from CHANGELOG / git describe)")
    ap.add_argument("--perft-depth", type=int, default=4, help="perft NPS benchmark depth (default 4)")
    ap.add_argument("--match", help="also play a match vs this engine (path or git ref) and record Elo")
    ap.add_argument("--date", help="ISO date to stamp (default: omitted)")
    ap.add_argument("--output", help="output path (default: version_test_results/<version>.json)")
    args = ap.parse_args()

    version = args.version or detect_version()
    print(f"[metrics] version = {version}", flush=True)

    record: dict = {"version": version}
    if args.date:
        record["date"] = args.date

    print("[metrics] perft NPS ...", flush=True)
    record["perft"] = collect_perft_nps(args.perft_depth)
    print(f"  AB median nps={record['perft']['AB_median_nps']:.0f} "
          f"QS median nps={record['perft']['QS_median_nps']:.0f}", flush=True)

    print("[metrics] mate puzzles ...", flush=True)
    record["mates"] = collect_mates()
    print(f"  solved {record['mates']['solved']}/{record['mates']['total']}", flush=True)

    if args.match:
        print(f"[metrics] match vs {args.match} ... (this can take a while)", flush=True)
        import tempfile
        out = Path(tempfile.mktemp(suffix=".json"))
        subprocess.run([sys.executable, str(REPO / "scripts" / "version_match.py"),
                        "--old", args.match, "--output", str(out)], check=True)
        match = json.loads(out.read_text())
        record["match_vs"] = {"opponent": args.match, **match}
        out.unlink(missing_ok=True)

    RESULTS_DIR.mkdir(exist_ok=True)
    output = Path(args.output) if args.output else RESULTS_DIR / f"{version}.json"
    output.write_text(json.dumps(record, indent=2))
    print(f"[metrics] wrote {output}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
