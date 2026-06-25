#!/usr/bin/env python3
"""Compare the search "tree shape" of La Mano de Miguelito vs Stockfish.

Stockfish can't dump a full per-node tree over UCI, but both engines emit
`info depth N ... nodes M` once per iterative-deepening iteration. That curve is
the thing that matters most: how many nodes each engine spends to reach a given
depth, i.e. its effective branching factor (EBF). Stockfish's EBF is famously
~1.5-2; ours is higher — that gap *is* "Stockfish prunes better".

Usage:
    # record Stockfish's reference curves (run once, commit the JSON):
    python3 scripts/search_tree_compare.py --record --depth 13 \
        --sf /opt/homebrew/bin/stockfish --out version_test_results/sf_search_tree_ref.json
    # compare the current LaMano build against the saved reference:
    python3 scripts/search_tree_compare.py --check --depth 13 \
        --lamano build/src/talshand_exe --ref version_test_results/sf_search_tree_ref.json
"""
from __future__ import annotations
import argparse, json, re, subprocess, sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# Diverse position set: openings, tactical & quiet middlegames, endgames.
POSITIONS: dict[str, str] = {
    "startpos":        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "open_sicilian":   "rnbqkb1r/pp2pppp/3p1n2/8/3NP3/2N5/PPP2PPP/R1BQKB1R b KQkq - 0 5",
    "italian_mid":     "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/2P2N2/PP1P1PPP/RNBQK2R w KQkq - 0 5",
    "tactical_mid":    "2r2rk1/1b3ppp/p1qpp3/1P6/1Pn1P2b/2NB1P1P/1BP1R1P1/R2Q2K1 b - - 0 19",
    "kiwipete":        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "quiet_queenless": "r3kb1r/pp3ppp/2n1bn2/2pp4/3P4/2N1PN2/PPP2PPP/R1B1KB1R w KQkq - 0 8",
    "rook_endgame":    "8/5pk1/6p1/3R3p/r6P/6P1/5PK1/8 w - - 0 1",
    "pawn_endgame":    "8/3k4/3p4/3P4/3K4/8/8/8 w - - 0 1",
}

INFO_RE = re.compile(r"\binfo\b.*\bdepth\s+(\d+)\b.*\bnodes\s+(\d+)\b")


def run(exe: str, fen: str, depth: int, cwd: str | None) -> dict:
    """Drive one engine to `go depth N`; return {depth: cumulative_nodes}, bestmove, breakdown."""
    p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         text=True, bufsize=1, cwd=cwd)
    setup = "startpos" if fen == POSITIONS["startpos"] else f"fen {fen}"
    # Threads=1 keeps the instrumented-Stockfish counters single-threaded (and is LaMano's default).
    p.stdin.write(f"uci\nsetoption name Threads value 1\nposition {setup}\ngo depth {depth}\n")
    p.stdin.flush()
    per_depth: dict[int, int] = {}
    bestmove, breakdown = None, None
    for line in p.stdout:
        line = line.strip()
        m = INFO_RE.search(line)
        if m:
            d, n = int(m.group(1)), int(m.group(2))
            per_depth[d] = max(per_depth.get(d, 0), n)  # final/max line for that depth
        if line.startswith("info string nodes") or line.startswith("info string lm"):
            breakdown = line[len("info string "):]       # LaMano or instrumented-SF pruning breakdown
        if line.startswith("bestmove"):
            bestmove = line.split()[1] if len(line.split()) > 1 else None
            break
    p.stdin.write("quit\n"); p.stdin.flush()
    try: p.wait(timeout=5)
    except subprocess.TimeoutExpired: p.kill()
    return {"per_depth": per_depth, "bestmove": bestmove, "breakdown": breakdown}


def ebf(per_depth: dict[int, int], depth: int) -> float | None:
    """Effective branching factor = nodes(depth) ** (1/depth)."""
    n = per_depth.get(depth)
    return round(n ** (1.0 / depth), 3) if n and depth > 0 else None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--record", action="store_true", help="record Stockfish reference curves")
    ap.add_argument("--check", action="store_true", help="compare LaMano against the reference")
    ap.add_argument("--depth", type=int, default=13)
    ap.add_argument("--sf", default="/opt/homebrew/bin/stockfish")
    ap.add_argument("--lamano", default=str(REPO / "build" / "src" / "talshand_exe"))
    ap.add_argument("--lamano-cwd", default=str(REPO), help="cwd so LaMano resolves models/w32_wdl0")
    ap.add_argument("--out", default=str(REPO / "version_test_results" / "sf_search_tree_ref.json"))
    ap.add_argument("--ref", default=str(REPO / "version_test_results" / "sf_search_tree_ref.json"))
    args = ap.parse_args()

    if args.record:
        ref = {"depth": args.depth, "engine": "stockfish", "positions": {}}
        for name, fen in POSITIONS.items():
            r = run(args.sf, fen, args.depth, cwd=None)
            ref["positions"][name] = {"fen": fen, "per_depth": r["per_depth"],
                                       "ebf": ebf(r["per_depth"], args.depth), "bestmove": r["bestmove"],
                                       "breakdown": r["breakdown"]}
            print(f"[sf] {name:<16} d{args.depth} nodes {r['per_depth'].get(args.depth,'?'):>10} "
                  f"ebf {ref['positions'][name]['ebf']}")
        Path(args.out).write_text(json.dumps(ref, indent=2))
        print(f"\nwrote reference -> {args.out}")
        return 0

    if args.check:
        ref = json.loads(Path(args.ref).read_text())
        D = ref["depth"]
        print(f"{'position':<16}{'LaMano nodes':>14}{'SF nodes':>12}{'ratio':>8}{'LaMano EBF':>12}{'SF EBF':>9}")
        lo_ebf, sf_ebf = [], []
        for name, fen in POSITIONS.items():
            r = run(args.lamano, fen, D, cwd=args.lamano_cwd)
            ln = r["per_depth"].get(D)
            sn = ref["positions"].get(name, {}).get("per_depth", {}).get(str(D)) \
                 or ref["positions"].get(name, {}).get("per_depth", {}).get(D)
            le, se = ebf(r["per_depth"], D), ref["positions"].get(name, {}).get("ebf")
            if le: lo_ebf.append(le)
            if se: sf_ebf.append(se)
            ratio = f"{ln/sn:.1f}x" if (ln and sn) else "?"
            print(f"{name:<16}{ln or '?':>14}{sn or '?':>12}{ratio:>8}{le or '?':>12}{se or '?':>9}")
            if r["breakdown"]: print(f"                 └─ {r['breakdown']}")
        gm = lambda xs: round((eval('*'.join(map(str, xs))) ** (1.0/len(xs))), 3) if xs else None
        print(f"\nmean EBF — LaMano {gm(lo_ebf)}  vs  Stockfish {gm(sf_ebf)}  (lower = better pruning)")
        return 0

    ap.error("pass --record or --check")


if __name__ == "__main__":
    sys.exit(main())
