#!/usr/bin/env python3
"""Play a match between two TalsHand engine builds and report the score / Elo.

Two "engines" can be given either as a path to an already-built UCI binary or as
a git tag/ref, in which case the engine is built (Release) in a throw-away git
worktree. Games are played with python-chess driving the UCI protocol; clocks
are tracked here and a side that runs out of time loses on time (python-chess
does not enforce this itself).

Each opening position is played twice, once with each engine as White, so colour
imbalance cancels out. Results are reported per time control and aggregated, with
an Elo estimate and error margin, and written to a JSON file.

Examples
--------
    # current Release build vs the previous release, default 4 time controls
    python3 scripts/version_match.py --old v0.3.1

    # two explicit binaries, a single fast control, JSON out
    python3 scripts/version_match.py \
        --new build_release/src/talshand_exe \
        --old /tmp/old/talshand_exe \
        --tc bullet-1+1 --output /tmp/match.json
"""
from __future__ import annotations

import argparse
import json
import math
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path

import chess
import chess.engine

REPO = Path(__file__).resolve().parent.parent
DEFAULT_NEW_BINARY = REPO / "build_release" / "src" / "talshand_exe"

# Time controls: name -> (base_seconds, increment_seconds)
TIME_CONTROLS: dict[str, tuple[float, float]] = {
    "bullet-1+1": (60.0, 1.0),
    "bullet-1+3": (60.0, 3.0),
    "blitz-3+2": (180.0, 2.0),
    "blitz-5+2": (300.0, 2.0),
}
DEFAULT_TCS = list(TIME_CONTROLS.keys())

# A small book of reasonably balanced opening positions (after a few book moves).
# Each is played twice (one game with each engine as White).
DEFAULT_OPENINGS: list[str] = [
    # name, fen
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",            # start
    "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2",       # Sicilian 1.e4 c5
    "rnbqkbnr/ppp1pppp/8/3p4/3P4/8/PPP1PPPP/RNBQKBNR w KQkq d6 0 2",       # Queen's pawn 1.d4 d5
    "rnbqkb1r/pppppppp/5n2/8/2P5/8/PP1PPPPP/RNBQKBNR w KQkq - 1 2",        # English vs Nf6
    "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2",      # Open Sicilian
    "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",    # 1.e4 e5 2.Nf3 Nc6
    "rnbqkb1r/pppp1ppp/4pn2/8/2PP4/8/PP2PPPP/RNBQKBNR w KQkq - 0 3",       # Indian
    "rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3",          # Centre game-ish
]

MAX_PLIES = 200  # adjudicate as draw if a game runs this long (keeps drawn endgames from dragging on with increment)


@dataclass
class TCResult:
    tc: str
    base: float
    inc: float
    wins: int = 0       # from the "new" engine's perspective
    draws: int = 0
    losses: int = 0
    new_time_losses: int = 0  # games the NEW engine lost on the clock
    old_time_losses: int = 0  # games the OLD engine lost on the clock
    games: list[dict] = field(default_factory=list)

    @property
    def n(self) -> int:
        return self.wins + self.draws + self.losses

    @property
    def points(self) -> float:
        return self.wins + 0.5 * self.draws


def build_engine_from_tag(ref: str, keep: bool) -> Path:
    """Build the given git ref in a throw-away worktree and return the binary."""
    workdir = Path(tempfile.mkdtemp(prefix=f"talshand-{ref.replace('/', '_')}-"))
    print(f"[build] adding worktree for {ref} at {workdir}", flush=True)
    subprocess.run(["git", "worktree", "add", "--detach", str(workdir), ref],
                   cwd=REPO, check=True)
    build = workdir / "build_release"
    subprocess.run(["cmake", "-S", str(workdir), "-B", str(build),
                    "-DCMAKE_BUILD_TYPE=Release", "-DENABLE_VERBOSE_DEBUG=OFF"],
                   check=True)
    subprocess.run(["cmake", "--build", str(build)], check=True)
    binary = build / "src" / "talshand_exe"
    if not binary.exists():
        raise FileNotFoundError(f"built engine not found for {ref}: {binary}")
    if not keep:
        # Remember the worktree path so the caller can clean it up afterwards.
        binary = binary.resolve()
    print(f"[build] {ref} -> {binary}", flush=True)
    return binary


def resolve_engine(spec: str, built: list[Path]) -> Path:
    """A spec is either a path to a binary or a git ref to build."""
    p = Path(spec)
    if p.exists() and p.is_file():
        return p.resolve()
    # treat as a git ref
    binary = build_engine_from_tag(spec, keep=True)
    built.append(binary)
    return binary


def play_game(white: chess.engine.SimpleEngine,
              black: chess.engine.SimpleEngine,
              start_fen: str, base: float, inc: float) -> tuple[str, str]:
    """Play one game. Returns (result, reason) where result is from White's POV:
    "1-0", "0-1" or "1/2-1/2"."""
    board = chess.Board(start_fen)
    clocks = {chess.WHITE: base, chess.BLACK: base}

    for engine in (white, black):
        try:
            engine.protocol.send_line("ucinewgame")  # best-effort; ignored if unknown
        except Exception:
            pass

    plies = 0
    while not board.is_game_over(claim_draw=True) and plies < MAX_PLIES:
        mover = white if board.turn == chess.WHITE else black
        limit = chess.engine.Limit(
            white_clock=clocks[chess.WHITE], black_clock=clocks[chess.BLACK],
            white_inc=inc, black_inc=inc)
        t0 = time.monotonic()
        try:
            result = mover.play(board, limit)
        except chess.engine.EngineError as exc:
            loser = board.turn
            try:  # record the position that made the engine die (for crash repro)
                with open("/tmp/crash_fens.txt", "a") as fh:
                    fh.write(board.fen() + "\n")
            except Exception:
                pass
            return ("0-1" if loser == chess.WHITE else "1-0", f"engine error: {exc}")
        elapsed = time.monotonic() - t0

        clocks[board.turn] -= elapsed
        if clocks[board.turn] < 0:
            loser = board.turn
            return ("0-1" if loser == chess.WHITE else "1-0", "time forfeit")
        clocks[board.turn] += inc

        if result.move is None or result.move not in board.legal_moves:
            loser = board.turn
            return ("0-1" if loser == chess.WHITE else "1-0",
                    f"illegal/no move: {result.move}")
        board.push(result.move)
        plies += 1

    outcome = board.outcome(claim_draw=True)
    if outcome is None:
        return ("1/2-1/2", f"max plies ({MAX_PLIES})")
    if outcome.winner is None:
        return ("1/2-1/2", outcome.termination.name.lower())
    return ("1-0" if outcome.winner == chess.WHITE else "0-1",
            outcome.termination.name.lower())


def elo_with_error(points: float, n: int, results: list[float]) -> tuple[float, float]:
    """Elo difference and ~95% error margin from a list of per-game scores
    (1.0 win / 0.5 draw / 0.0 loss, from the new engine's POV)."""
    if n == 0:
        return (0.0, 0.0)
    score = points / n
    if score <= 0.0:
        return (-800.0, 0.0)
    if score >= 1.0:
        return (800.0, 0.0)
    elo = -400.0 * math.log10(1.0 / score - 1.0)
    mean = score
    var = sum((r - mean) ** 2 for r in results) / max(1, (n - 1))
    stderr = math.sqrt(var / n)
    # Propagate the score stderr to Elo via the derivative of the logistic.
    d_elo = 400.0 / (math.log(10) * score * (1.0 - score))
    return (elo, 1.96 * d_elo * stderr)


def safe_quit(engine) -> None:
    """Quit an engine, tolerating one that has already crashed/terminated
    (quitting a dead engine otherwise raises EngineTerminatedError)."""
    if engine is None:
        return
    try:
        engine.quit()
    except Exception:
        try:
            engine.close()
        except Exception:
            pass


def run_match(new_bin: Path, old_bin: Path, tc_map: dict[str, tuple[float, float]],
              openings: list[str]) -> dict:
    print(f"[match] NEW={new_bin}\n[match] OLD={old_bin}", flush=True)
    per_tc: list[TCResult] = []

    for tc, (base, inc) in tc_map.items():
        res = TCResult(tc=tc, base=base, inc=inc)
        print(f"\n[match] === {tc} ({base}+{inc}) ===", flush=True)
        for op_idx, fen in enumerate(openings):
            # Two games: new as White, then new as Black.
            for new_is_white in (True, False):
                new_eng = old_eng = None
                try:
                    new_eng = chess.engine.SimpleEngine.popen_uci(str(new_bin))
                    old_eng = chess.engine.SimpleEngine.popen_uci(str(old_bin))
                    if new_is_white:
                        white_result, reason = play_game(new_eng, old_eng, fen, base, inc)
                        new_pov = white_result
                    else:
                        white_result, reason = play_game(old_eng, new_eng, fen, base, inc)
                        # flip to new engine's POV
                        new_pov = {"1-0": "0-1", "0-1": "1-0", "1/2-1/2": "1/2-1/2"}[white_result]
                except Exception as exc:  # noqa: BLE001
                    # Setup/teardown failure (e.g. an engine that crashed at
                    # startup). Don't kill the whole match; record a void game.
                    new_pov, reason = ("1/2-1/2", f"harness error: {exc}")
                finally:
                    safe_quit(new_eng)
                    safe_quit(old_eng)

                if new_pov == "1-0":
                    res.wins += 1
                elif new_pov == "0-1":
                    res.losses += 1
                else:
                    res.draws += 1
                if reason == "time forfeit":
                    if new_pov == "0-1":
                        res.new_time_losses += 1
                    elif new_pov == "1-0":
                        res.old_time_losses += 1
                res.games.append({"opening": op_idx, "new_white": new_is_white,
                                  "result_new_pov": new_pov, "reason": reason})
                print(f"  op{op_idx} new_{'W' if new_is_white else 'B'}: "
                      f"{new_pov} ({reason})  running W-D-L={res.wins}-{res.draws}-{res.losses}",
                      flush=True)
        per_tc.append(res)

    # Aggregate.
    all_results: list[float] = []
    total = TCResult(tc="ALL", base=0, inc=0)
    summary = []
    for res in per_tc:
        scores = [1.0 if g["result_new_pov"] == "1-0" else 0.0 if g["result_new_pov"] == "0-1" else 0.5
                  for g in res.games]
        all_results += scores
        total.wins += res.wins
        total.draws += res.draws
        total.losses += res.losses
        total.new_time_losses += res.new_time_losses
        total.old_time_losses += res.old_time_losses
        elo, err = elo_with_error(res.points, res.n, scores)
        summary.append({"tc": res.tc, "base": res.base, "inc": res.inc,
                        "wins": res.wins, "draws": res.draws, "losses": res.losses,
                        "n": res.n, "points": res.points,
                        "score_pct": (100.0 * res.points / res.n) if res.n else 0.0,
                        "elo": elo, "elo_error": err,
                        "new_time_losses": res.new_time_losses,
                        "old_time_losses": res.old_time_losses,
                        # >0 means the NEW build manages the clock better than the old one
                        "time_mgmt_diff": res.old_time_losses - res.new_time_losses,
                        "games": res.games})

    elo, err = elo_with_error(total.points, total.n, all_results)
    return {
        "new_binary": str(new_bin), "old_binary": str(old_bin),
        "per_tc": summary,
        "overall": {"wins": total.wins, "draws": total.draws, "losses": total.losses,
                    "n": total.n, "points": total.points,
                    "score_pct": (100.0 * total.points / total.n) if total.n else 0.0,
                    "elo": elo, "elo_error": err,
                    "new_time_losses": total.new_time_losses,
                    "old_time_losses": total.old_time_losses,
                    "time_mgmt_diff": total.old_time_losses - total.new_time_losses},
    }


def print_summary(report: dict) -> None:
    print("\n========================= MATCH SUMMARY =========================")
    print(f"{'TC':<14} {'W-D-L':<12} {'score%':>7}  {'Elo':>8} {'±':>6}  {'timeLoss(new/old)':>17}")
    for s in report["per_tc"]:
        wdl = f"{s['wins']}-{s['draws']}-{s['losses']}"
        tl = f"{s['new_time_losses']}/{s['old_time_losses']}"
        print(f"{s['tc']:<14} {wdl:<12} {s['score_pct']:>6.1f}%  "
              f"{s['elo']:>+8.1f} {s['elo_error']:>6.1f}  {tl:>17}")
    o = report["overall"]
    wdl = f"{o['wins']}-{o['draws']}-{o['losses']}"
    tl = f"{o['new_time_losses']}/{o['old_time_losses']}"
    print("-" * 65)
    print(f"{'OVERALL':<14} {wdl:<12} {o['score_pct']:>6.1f}%  "
          f"{o['elo']:>+8.1f} {o['elo_error']:>6.1f}  {tl:>17}")
    # Time-management score: net clock losses avoided vs the old build.
    diff = o["time_mgmt_diff"]
    verdict = ("better" if diff > 0 else "worse" if diff < 0 else "equal")
    print(f"time-management: new lost {o['new_time_losses']} on time, old lost "
          f"{o['old_time_losses']} -> new is {verdict} (diff {diff:+d})")
    print("=================================================================")


def main() -> int:
    ap = argparse.ArgumentParser(description="TalsHand version match")
    ap.add_argument("--new", default=str(DEFAULT_NEW_BINARY),
                    help="new engine: path to binary or git ref (default: current Release build)")
    ap.add_argument("--old", default="v0.3.1",
                    help="old engine: path to binary or git ref (default: v0.3.1)")
    ap.add_argument("--tc", action="append", choices=list(TIME_CONTROLS.keys()),
                    help="time control(s) to play (repeatable; default: all four)")
    ap.add_argument("--base", type=float,
                    help="custom time control base seconds (overrides --tc; use with --inc)")
    ap.add_argument("--inc", type=float, default=0.0,
                    help="custom time control increment seconds (with --base)")
    ap.add_argument("--openings", help="path to a file with one FEN per line (default: built-in book)")
    ap.add_argument("--max-openings", type=int, default=None,
                    help="use only the first N openings (handy for quick runs)")
    ap.add_argument("--output", help="write the full JSON report to this path")
    args = ap.parse_args()

    if args.base is not None:
        tc_map = {f"custom-{args.base:g}+{args.inc:g}": (args.base, args.inc)}
    else:
        tcs = args.tc if args.tc else DEFAULT_TCS
        tc_map = {tc: TIME_CONTROLS[tc] for tc in tcs}
    openings = DEFAULT_OPENINGS
    if args.openings:
        openings = [ln.strip() for ln in Path(args.openings).read_text().splitlines() if ln.strip()]
    if args.max_openings:
        openings = openings[: args.max_openings]

    built: list[Path] = []
    try:
        new_bin = resolve_engine(args.new, built)
        old_bin = resolve_engine(args.old, built)
        report = run_match(new_bin, old_bin, tc_map, openings)
    finally:
        # Clean up any worktrees we created.
        subprocess.run(["git", "worktree", "prune"], cwd=REPO, check=False)

    print_summary(report)
    if args.output:
        Path(args.output).write_text(json.dumps(report, indent=2))
        print(f"\n[match] wrote {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
