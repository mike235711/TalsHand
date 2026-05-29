#!/usr/bin/env python3
"""Generate a version-progression report from the per-version metric JSONs.

Reads ``version_test_results/*.json`` (as written by collect_release_metrics.py)
and produces, under ``version_test_results/``:

  * ``charts/*.png`` - perft NPS, mates solved and (if present) match Elo per
    version, rendered with matplotlib, and
  * ``REPORT.md`` - a table of every tracked version plus the embedded charts,
    which renders directly on GitHub (no notebook / Jupyter needed).

Run after collecting metrics for one or more versions:
    python3 scripts/generate_report.py
"""
from __future__ import annotations

import csv
import json
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")  # headless
import matplotlib.pyplot as plt

REPO = Path(__file__).resolve().parent.parent
RESULTS_DIR = REPO / "version_test_results"
CHARTS_DIR = RESULTS_DIR / "charts"


def semver_key(v: str):
    parts = []
    for p in v.split("-")[0].split("."):
        try:
            parts.append(int(p))
        except ValueError:
            parts.append(0)
    return tuple(parts)


def load_versions() -> list[dict]:
    records = []
    for path in RESULTS_DIR.glob("*.json"):
        try:
            records.append(json.loads(path.read_text()))
        except Exception as exc:  # noqa: BLE001
            print(f"[report] skipping {path.name}: {exc}")
    records.sort(key=lambda r: semver_key(r.get("version", "0")))
    return records


def _save(fig, name: str) -> str:
    CHARTS_DIR.mkdir(parents=True, exist_ok=True)
    rel = f"charts/{name}"
    fig.savefig(RESULTS_DIR / rel, bbox_inches="tight", dpi=110)
    plt.close(fig)
    return rel


def chart_nps(records: list[dict]) -> str | None:
    pts = [(r["version"], r.get("perft", {})) for r in records if r.get("perft")]
    if not pts:
        return None
    versions = [v for v, _ in pts]
    ab = [p.get("AB_median_nps", 0) / 1e6 for _, p in pts]
    qs = [p.get("QS_median_nps", 0) / 1e6 for _, p in pts]
    fig, ax = plt.subplots(figsize=(7, 4))
    ax.plot(versions, ab, marker="o", label="AB perft")
    ax.plot(versions, qs, marker="s", label="QS perft")
    ax.set_ylabel("median nodes/sec (millions)")
    ax.set_xlabel("version")
    ax.set_title("Perft speed by version (higher is better)")
    ax.grid(True, alpha=0.3)
    ax.legend()
    return _save(fig, "perft_nps.png")


def chart_mates(records: list[dict]) -> str | None:
    pts = [(r["version"], r.get("mates", {})) for r in records if r.get("mates")]
    if not pts:
        return None
    versions = [v for v, _ in pts]
    solved = [m.get("solved", 0) for _, m in pts]
    total = max((m.get("total", 0) for _, m in pts), default=0)
    fig, ax = plt.subplots(figsize=(7, 4))
    ax.bar(versions, solved, color="#4c72b0")
    if total:
        ax.axhline(total, ls="--", color="gray", label=f"total ({total})")
        ax.legend()
    ax.set_ylabel("mate puzzles solved")
    ax.set_xlabel("version")
    ax.set_title("Mate puzzles solved by version")
    ax.grid(True, axis="y", alpha=0.3)
    return _save(fig, "mates_solved.png")


def chart_elo(records: list[dict]) -> str | None:
    pts = []
    for r in records:
        mv = r.get("match_vs")
        if mv and "overall" in mv:
            pts.append((r["version"], mv["overall"].get("elo", 0.0),
                        mv["overall"].get("elo_error", 0.0), mv.get("opponent", "?")))
    if not pts:
        return None
    versions = [v for v, *_ in pts]
    elos = [e for _, e, *_ in pts]
    errs = [er for _, _, er, _ in pts]
    fig, ax = plt.subplots(figsize=(7, 4))
    ax.errorbar(versions, elos, yerr=errs, marker="o", capsize=4)
    ax.axhline(0, ls="--", color="gray")
    ax.set_ylabel("Elo vs previous version")
    ax.set_xlabel("version")
    ax.set_title("Measured strength gain vs the opponent build")
    ax.grid(True, alpha=0.3)
    return _save(fig, "elo_gain.png")


def load_tactic_times() -> dict[str, dict[str, dict[int, float]]]:
    """version -> tactic -> depth -> time, from version_test_results/tactic_results_v*.csv."""
    data: dict[str, dict[str, dict[int, float]]] = defaultdict(lambda: defaultdict(dict))
    for path in RESULTS_DIR.glob("tactic_results_v*.csv"):
        version = path.stem.replace("tactic_results_v", "")
        try:
            for row in csv.DictReader(path.open()):
                data[version][row["TacticName"]][int(row["Depth"])] = float(row["TimeTakenSeconds"])
        except Exception as exc:  # noqa: BLE001
            print(f"[report] skipping {path.name}: {exc}")
    return data


def tactic_totals() -> tuple[dict[str, float], int]:
    """Per-version total time to solve every tactic to the deepest depth that is
    common to all versions (apples-to-apples; lower is better). Returns
    ({version: total_seconds}, common_depth)."""
    data = load_tactic_times()
    if not data:
        return ({}, 0)
    # Depth contiguous from 2, so the common reference depth is the smallest
    # per-(version,tactic) max depth across the whole dataset.
    dstar = min(max(depths) for ver in data.values() for depths in ver.values())
    totals: dict[str, float] = {}
    for version, tactics in data.items():
        totals[version] = sum(depths.get(dstar, 0.0) for depths in tactics.values())
    return (totals, dstar)


def chart_tactic_times() -> tuple[str | None, dict[str, float], int]:
    totals, dstar = tactic_totals()
    if len(totals) < 2:
        return (None, totals, dstar)
    versions = sorted(totals, key=semver_key)
    times_ms = [totals[v] * 1000.0 for v in versions]
    fig, ax = plt.subplots(figsize=(7, 4))
    ax.plot(versions, times_ms, marker="o", color="#c44e52")
    ax.set_ylabel(f"total time to solve tactics @ depth {dstar} (ms)")
    ax.set_xlabel("version")
    ax.set_title("Tactic search time by version (lower is better)")
    ax.grid(True, alpha=0.3)
    return (_save(fig, "tactic_times.png"), totals, dstar)


def build_table(records: list[dict]) -> str:
    rows = ["| Version | AB Mnps | QS Mnps | Mates | Elo vs prev |",
            "|---|---|---|---|---|"]
    for r in records:
        perft = r.get("perft", {})
        ab = perft.get("AB_median_nps", 0) / 1e6
        qs = perft.get("QS_median_nps", 0) / 1e6
        mates = r.get("mates", {})
        mate_str = f"{mates.get('solved', '-')}/{mates.get('total', '-')}" if mates else "-"
        mv = r.get("match_vs", {})
        if mv and "overall" in mv:
            o = mv["overall"]
            elo_str = f"{o.get('elo', 0):+.0f} ± {o.get('elo_error', 0):.0f}"
        else:
            elo_str = "-"
        rows.append(f"| {r.get('version', '?')} | {ab:.1f} | {qs:.1f} | {mate_str} | {elo_str} |")
    return "\n".join(rows)


def main() -> int:
    records = load_versions()
    if not records:
        print("[report] no version_test_results/*.json found; run collect_release_metrics.py first")
        return 1

    print(f"[report] {len(records)} version(s): " +
          ", ".join(r.get("version", "?") for r in records))

    tactic_chart, tactic_totals_map, tactic_depth = chart_tactic_times()
    charts = {
        "Perft speed": chart_nps(records),
        "Tactic search time": tactic_chart,
        "Mate puzzles": chart_mates(records),
        "Strength gain (Elo)": chart_elo(records),
    }
    if tactic_totals_map:
        print("[report] tactic totals @ depth "
              f"{tactic_depth}: " +
              ", ".join(f"{v}={t*1000:.1f}ms" for v, t in sorted(tactic_totals_map.items(),
                                                                 key=lambda kv: semver_key(kv[0]))))

    lines = [
        "# TalsHand version progression report",
        "",
        "_Generated by `scripts/generate_report.py` from `version_test_results/*.json`._",
        "",
        "## Summary",
        "",
        build_table(records),
        "",
    ]
    for title, rel in charts.items():
        if rel:
            lines += [f"## {title}", "", f"![{title}]({rel})", ""]

    report = RESULTS_DIR / "REPORT.md"
    report.write_text("\n".join(lines))
    print(f"[report] wrote {report}")
    for title, rel in charts.items():
        if rel:
            print(f"[report] chart: {rel}")
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
