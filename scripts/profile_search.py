#!/usr/bin/env python3
"""Measure one engine's search-tree shape + absolute-time profile on a fixed position set."""
import argparse, json, re, subprocess, sys
from pathlib import Path
POSITIONS = {
    "startpos":        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "open_sicilian":   "rnbqkb1r/pp2pppp/3p1n2/8/3NP3/2N5/PPP2PPP/R1BQKB1R b KQkq - 0 5",
    "italian_mid":     "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/2P2N2/PP1P1PPP/RNBQK2R w KQkq - 0 5",
    "tactical_mid":    "2r2rk1/1b3ppp/p1qpp3/1P6/1Pn1P2b/2NB1P1P/1BP1R1P1/R2Q2K1 b - - 0 19",
    "kiwipete":        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "quiet_queenless": "r3kb1r/pp3ppp/2n1bn2/2pp4/3P4/2N1PN2/PPP2PPP/R1B1KB1R w KQkq - 0 8",
    "rook_endgame":    "8/5pk1/6p1/3R3p/r6P/6P1/5PK1/8 w - - 0 1",
    "pawn_endgame":    "8/3k4/3p4/3P4/3K4/8/8/8 w - - 0 1",
}
DEPTH_RE = re.compile(r"\binfo\b.*\bdepth\s+(\d+)\b")
NODES_RE = re.compile(r"\bnodes\s+(\d+)\b"); NPS_RE = re.compile(r"\bnps\s+(\d+)\b"); TIME_RE = re.compile(r"\btime\s+(\d+)\b")
def run(exe, fen, depth, cwd):
    p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1, cwd=cwd)
    setup = "startpos" if fen == POSITIONS["startpos"] else f"fen {fen}"
    p.stdin.write(f"uci\nsetoption name Threads value 1\nposition {setup}\ngo depth {depth}\n"); p.stdin.flush()
    best = {}; breakdown = None
    for line in p.stdout:
        line = line.strip(); md = DEPTH_RE.search(line)
        if md and "nodes" in line:
            d = int(md.group(1)); mn, mt, mp = NODES_RE.search(line), TIME_RE.search(line), NPS_RE.search(line)
            if mn: best[d] = (int(mn.group(1)), int(mt.group(1)) if mt else None, int(mp.group(1)) if mp else None)
        if line.startswith("info string nodes") or line.startswith("info string lm"): breakdown = line[len("info string "):]
        if line.startswith("bestmove"): break
    p.stdin.write("quit\n"); p.stdin.flush()
    try: p.wait(timeout=5)
    except subprocess.TimeoutExpired: p.kill()
    if not best: return None
    dmax = max(best); nodes, t_ms, nps = best[dmax]
    return {"depth_reached": dmax, "nodes": nodes, "time_ms": t_ms, "nps": nps,
            "ebf": round(nodes ** (1.0/dmax), 3) if nodes and dmax else None, "breakdown": breakdown}
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--engine", required=True); ap.add_argument("--label", required=True)
    ap.add_argument("--depth", type=int, default=13); ap.add_argument("--cwd", default=""); ap.add_argument("--out", required=True)
    a = ap.parse_args(); cwd = a.cwd or None
    res = {"label": a.label, "depth": a.depth, "positions": {}}
    for name, fen in POSITIONS.items():
        r = run(a.engine, fen, a.depth, cwd); res["positions"][name] = r
        if r: print(f"[{a.label}] {name:<16} d{r['depth_reached']:>2} nodes {r['nodes']:>10} time {str(r['time_ms'])+'ms':>9} nps {r['nps'] or '?':>9} ebf {r['ebf']}", file=sys.stderr)
    import math
    full = [p for p in res["positions"].values() if p and p["depth_reached"] >= a.depth]
    res["mean_ebf"] = round(math.exp(sum(math.log(p["ebf"]) for p in full if p["ebf"])/len([p for p in full if p["ebf"]])),3) if full else None
    res["mean_nps"] = int(sum(p["nps"] for p in full if p["nps"])/max(1,len([p for p in full if p["nps"]]))) if full else None
    res["total_time_ms"] = sum(p["time_ms"] for p in full if p["time_ms"]); res["total_nodes"] = sum(p["nodes"] for p in full); res["n_full"] = len(full)
    Path(a.out).write_text(json.dumps(res, indent=2))
    print(f"WROTE {a.out}: mean_ebf={res['mean_ebf']} mean_nps={res['mean_nps']} n_full={res['n_full']}", file=sys.stderr)
if __name__ == "__main__": main()
