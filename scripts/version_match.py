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
import datetime
import hashlib
import json
import math
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path

import chess
import chess.engine
import chess.pgn

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
    "r1bqkbnr/1ppp1ppp/p1n5/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4",   # Ruy Lopez, Morphy
    "rnbqkb1r/pp2pppp/3p1n2/8/3NP3/2N5/PPP2PPP/R1BQKB1R w KQkq - 0 6",     # Sicilian Najdorf-ish
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


# ---------------------------------------------------------------------------
# Net identity + durable, incremental match logging
# ---------------------------------------------------------------------------
def net_content_hash(net_dir: Path) -> str | None:
    """SHA-256 (first 12 hex) over the name-sorted concatenation of a net's weight
    CSVs — an unambiguous, verifiable identity for the NNUEU an engine loaded. Same
    rule as version_test_results/NNUEU_KOTH_matches.md, so the hashes are comparable."""
    net_dir = Path(net_dir)
    if not net_dir.is_dir():
        return None
    h = hashlib.sha256()
    for name in sorted(p.name for p in net_dir.iterdir() if p.suffix == ".csv"):
        h.update(name.encode())
        h.update((net_dir / name).read_bytes())
    return h.hexdigest()[:12]


def read_net_config(net_dir: Path) -> dict | None:
    """Shape summary from a net dir's config.json (None if absent — e.g. the older
    champion net, which predates config.json)."""
    p = Path(net_dir) / "config.json"
    if not p.exists():
        return None
    try:
        c = json.loads(p.read_text())
    except Exception:
        return None
    return {"shape": f"{c.get('width')}/{c.get('second_out')}/{c.get('third_out')}",
            "phase_buckets": c.get("phase_buckets", 1), "dual_act": c.get("dual_act"),
            "attacks": c.get("attacks"), "input_planes": c.get("input_planes")}


def _net_dir_for(bin_path: Path, explicit: str | None) -> str | None:
    """Find the NNUEU_NET a launched engine will use: an explicit override, else the
    NNUEU_NET= line of a shell-wrapper binary, else the current process environment."""
    if explicit:
        return explicit
    try:
        txt = Path(bin_path).read_text(errors="ignore")
        if len(txt) < 4096:  # only sniff small text wrappers, never a real binary
            m = re.search(r'NNUEU_NET\s*=\s*["\']?([^"\'\n]+)', txt)
            if m:
                return m.group(1).strip()
    except Exception:
        pass
    return os.environ.get("NNUEU_NET")


def engine_identity(bin_path: Path, explicit_net: str | None = None) -> dict:
    """Self-describing identity for one engine: the launched binary plus, when
    resolvable, the NNUEU net it loads (dir, name, content-hash, config). This is
    what makes a saved match traceable to the exact net that played it, so a
    stopped-mid-match log is never ambiguous about who was playing."""
    ident = {"binary": str(bin_path)}
    net_dir = _net_dir_for(bin_path, explicit_net)
    if net_dir:
        net_dir = os.path.expanduser(net_dir.rstrip("/"))
        ident["net_dir"] = net_dir
        ident["net_name"] = Path(net_dir).name
        h = net_content_hash(Path(net_dir))
        if h:
            ident["net_sha256"] = h
        cfg = read_net_config(Path(net_dir))
        if cfg:
            ident["net_config"] = cfg
    return ident


def check_same_net(new_ident: dict, old_ident: dict) -> dict | None:
    """Warn (loudly) when both engines will load the SAME NNUEU, and say so in the log.

    This is legitimate and common — it is how you A/B a SEARCH change with the eval held
    fixed. It is also the single easiest way to run a worthless match by accident: forget
    `--new-net`/`--old-net` and both sides inherit the same `$NNUEU_NET`, or two tags build
    with the same CMake net defaults. Either way the resulting Elo says nothing about the
    net, so the run must announce which experiment it actually is.

    Returns a dict describing the shared identity (recorded into meta), or None."""
    nh, oh = new_ident.get("net_sha256"), old_ident.get("net_sha256")
    if not (nh and oh and nh == oh):
        return None
    src = "both sides resolved to the same net"
    if not new_ident.get("net_dir") or new_ident.get("net_dir") == old_ident.get("net_dir"):
        src += " (same net_dir — check whether $NNUEU_NET leaked to both engines)"
    print(f"\n[match] !! WARNING: NEW and OLD load the IDENTICAL net "
          f"{new_ident.get('net_name')} ({nh}).", flush=True)
    print("[match] !! This match therefore measures the SEARCH ONLY, not the eval. "
          "If you meant to compare two nets, pass --new-net/--old-net.\n", flush=True)
    return {"net_sha256": nh, "net_name": new_ident.get("net_name"),
            "note": src + " -> this match isolates the SEARCH, not the eval"}


def engine_label(ident: dict) -> str:
    """A short filesystem-safe label for an engine (its net name if known)."""
    raw = ident.get("net_name") or Path(ident["binary"]).parent.name or "engine"
    return re.sub(r"[^A-Za-z0-9._-]+", "_", raw)[:48]


def atomic_write_json(path: str, obj: dict) -> None:
    """Write JSON via temp-file + atomic rename so a reader — or a kill mid-write —
    never sees a truncated file. os.replace is atomic on POSIX."""
    p = Path(path)
    tmp = p.with_name(p.name + ".tmp")
    tmp.write_text(json.dumps(obj, indent=2))
    os.replace(tmp, p)


def play_game(white: chess.engine.SimpleEngine,
              black: chess.engine.SimpleEngine,
              start_fen: str, base: float, inc: float,
              board_out: list | None = None,
              new_engine: chess.engine.SimpleEngine | None = None,
              move_stats: list | None = None,
              clock_out: list | None = None,
              tc: str | None = None) -> tuple[str, str]:
    """Play one game. Returns (result, reason) where result is from White's POV:
    "1-0", "0-1" or "1/2-1/2". If board_out is given, the (mutated) Board is
    appended to it so the caller can recover the full move stack for a PGN. If
    move_stats is given, one {tc, is_new, move_no, time_ms, depth} record per move is
    appended (move_no = that engine's own move count). If clock_out is given, one
    {tc, is_new, clock_left_ms, base_ms} record per engine is appended at game end (to
    check the engine neither banks its clock nor burns it early).

    Records carry `tc` because think-time differs several-fold between bullet and
    blitz: pooling them would average away exactly the per-control behaviour these
    distributions exist to expose."""
    board = chess.Board(start_fen)
    if board_out is not None:
        board_out.append(board)  # same reference; reflects the final position
    clocks = {chess.WHITE: base, chess.BLACK: base}
    mv_count = {chess.WHITE: 0, chess.BLACK: 0}

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
            result = mover.play(board, limit, info=chess.engine.INFO_ALL)
        except chess.engine.EngineError as exc:
            loser = board.turn
            try:  # record the position that made the engine die (for crash repro)
                with open("/tmp/crash_fens.txt", "a") as fh:
                    fh.write(board.fen() + "\n")
            except Exception:
                pass
            return ("0-1" if loser == chess.WHITE else "1-0", f"engine error: {exc}")
        elapsed = time.monotonic() - t0

        mv_count[board.turn] += 1
        if move_stats is not None:
            move_stats.append({"tc": tc, "is_new": mover is new_engine,
                               "move_no": mv_count[board.turn],
                               "time_ms": elapsed * 1000.0,
                               "depth": result.info.get("depth")})

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

    # Game completed normally: record each engine's leftover clock (banking check).
    if clock_out is not None:
        for col in (chess.WHITE, chess.BLACK):
            clock_out.append({"tc": tc, "is_new": (white if col == chess.WHITE else black) is new_engine,
                              "clock_left_ms": clocks[col] * 1000.0, "base_ms": base * 1000.0})

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


def _dist(vals: list) -> dict | None:
    """Summary stats (count / mean / median / p10 / p90 / max) of a value list."""
    vals = [v for v in vals if v is not None]
    if not vals:
        return None
    s = sorted(vals)
    pct = lambda p: s[min(len(s) - 1, int(p * len(s)))]
    return {"n": len(vals), "mean": round(statistics.mean(vals), 1),
            "median": round(statistics.median(vals), 1),
            "p10": round(pct(0.10), 1), "p90": round(pct(0.90), 1), "max": round(max(vals), 1)}


def _summarize_one(move_stats: list, clock_stats: list | None) -> dict:
    """new-vs-old distributions over ONE already-filtered set of records."""
    out = {}
    for who, is_new in (("new", True), ("old", False)):
        ms = [m for m in move_stats if m["is_new"] == is_new]

        def phase(lo, hi):
            sel = [m for m in ms if lo <= m["move_no"] <= hi]
            d = [m["depth"] for m in sel if m["depth"] is not None]
            return {"n": len(sel),
                    "avg_time_ms": round(statistics.mean([m["time_ms"] for m in sel]), 1) if sel else None,
                    "avg_depth": round(statistics.mean(d), 1) if d else None}

        entry = {"time_ms": _dist([m["time_ms"] for m in ms]),
                 "depth": _dist([m["depth"] for m in ms]),
                 "by_phase": {"opening_1_10": phase(1, 10),
                              "middle_11_30": phase(11, 30),
                              "endgame_31plus": phase(31, 10 ** 9)}}
        if clock_stats is not None:
            cs = [100.0 * c["clock_left_ms"] / c["base_ms"] for c in clock_stats if c["is_new"] == is_new]
            if cs:
                # % of the base clock still unused at game end. High => banks time (plays weak
                # for its clock); ~0/negative => flags. Healthy = a small positive buffer.
                entry["clock_left_pct"] = {"mean": round(statistics.mean(cs), 1),
                                           "median": round(statistics.median(cs), 1)}
        out[who] = entry
    return out


def summarize_moves(move_stats: list, clock_stats: list | None = None) -> dict:
    """Per-version (new vs old): per-move think-time and reached-depth distributions, the
    time/depth split by game phase (opening/middle/endgame — front-loading check), and the
    clock left over at game end (banking check). The lens fixed-depth profiling can't give.

    Reported PER TIME CONTROL under `by_tc`, because a bullet-1+1 move and a blitz-5+2 move
    differ several-fold in think time: pooling them yields a mean that describes no control
    that was actually played. `overall` keeps the pooled view for continuity, but read
    `by_tc` when comparing two engines' clock behaviour."""
    clock_stats = clock_stats if clock_stats is not None else []
    tcs = [tc for tc in dict.fromkeys(m.get("tc") for m in move_stats) if tc]
    out = {"overall": _summarize_one(move_stats, clock_stats),
           "by_tc": {tc: _summarize_one([m for m in move_stats if m.get("tc") == tc],
                                        [c for c in clock_stats if c.get("tc") == tc])
                     for tc in tcs}}
    # Back-compat: older consumers read report["move_distributions"]["new"|"old"].
    out["new"], out["old"] = out["overall"]["new"], out["overall"]["old"]
    return out


def build_report(new_ident: dict, old_ident: dict, meta: dict,
                 per_tc: list[TCResult], move_stats: list, clock_stats: list) -> dict:
    """Assemble the full — possibly PARTIAL — report from the current state. Safe to
    call after every game: a TCResult still in progress simply contributes the games
    played so far. `meta` carries status / timestamps / progress, and the two engine
    identities make the record self-describing (which NNUEU each side played)."""
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
        "meta": meta,
        "new_engine": new_ident, "old_engine": old_ident,
        # kept for backward compatibility with older consumers of this JSON
        "new_binary": new_ident["binary"], "old_binary": old_ident["binary"],
        "per_tc": summary,
        "overall": {"wins": total.wins, "draws": total.draws, "losses": total.losses,
                    "n": total.n, "points": total.points,
                    "score_pct": (100.0 * total.points / total.n) if total.n else 0.0,
                    "elo": elo, "elo_error": err,
                    "new_time_losses": total.new_time_losses,
                    "old_time_losses": total.old_time_losses,
                    "time_mgmt_diff": total.old_time_losses - total.new_time_losses},
        "move_distributions": summarize_moves(move_stats, clock_stats),
    }


def run_match(new_bin: Path, old_bin: Path, tc_map: dict[str, tuple[float, float]],
              openings: list[str], new_ident: dict, old_ident: dict,
              output_path: str | None = None, pgn_path: str | None = None) -> dict:
    print(f"[match] NEW={new_bin}\n[match] OLD={old_bin}", flush=True)
    for tag, ident in (("NEW", new_ident), ("OLD", old_ident)):
        if ident.get("net_name"):
            print(f"[match] {tag} net={ident['net_name']} ({ident.get('net_sha256', '?')})", flush=True)
    same_net = check_same_net(new_ident, old_ident)
    new_label = new_ident.get("net_name") or new_bin.parent.name or "new"
    old_label = old_ident.get("net_name") or old_bin.parent.name or "old"
    if pgn_path:
        open(pgn_path, "w").close()  # truncate
    per_tc: list[TCResult] = []
    move_stats: list = []  # per-move {tc, is_new, move_no, time_ms, depth} across all games
    clock_stats: list = []  # per-engine-per-game {tc, is_new, clock_left_ms, base_ms} at game end
    meta = {"status": "in_progress",
            "started_at": datetime.datetime.now().isoformat(timespec="seconds"),
            "updated_at": None,
            "games_planned": len(tc_map) * len(openings) * 2, "games_played": 0,
            "time_controls": {tc: {"base": b, "inc": i} for tc, (b, i) in tc_map.items()},
            "openings": len(openings)}
    if same_net:
        # Recorded, not just printed: months later the JSON must still say whether this
        # match isolated the SEARCH (same eval both sides) or compared two evals.
        meta["same_net"] = same_net

    def flush(status: str) -> None:
        """Persist the current (partial) report atomically. This is what makes a
        stopped match lose nothing — the file already holds every finished game."""
        if not output_path:
            return
        meta["status"] = status
        meta["updated_at"] = datetime.datetime.now().isoformat(timespec="seconds")
        atomic_write_json(output_path, build_report(new_ident, old_ident, meta,
                                                    per_tc, move_stats, clock_stats))

    flush("in_progress")  # create the file up-front, before any game is played
    if output_path:
        print(f"[match] durable log -> {output_path}", flush=True)

    try:
        for tc, (base, inc) in tc_map.items():
            res = TCResult(tc=tc, base=base, inc=inc)
            per_tc.append(res)  # append the reference NOW so partial results are logged live
            print(f"\n[match] === {tc} ({base}+{inc}) ===", flush=True)
            for op_idx, fen in enumerate(openings):
                # Two games: new as White, then new as Black.
                for new_is_white in (True, False):
                    new_eng = old_eng = None
                    bo: list = []
                    white_result = None
                    try:
                        new_eng = chess.engine.SimpleEngine.popen_uci(str(new_bin))
                        old_eng = chess.engine.SimpleEngine.popen_uci(str(old_bin))
                        if new_is_white:
                            white_result, reason = play_game(new_eng, old_eng, fen, base, inc, bo,
                                                             new_engine=new_eng, move_stats=move_stats,
                                                             clock_out=clock_stats, tc=tc)
                            new_pov = white_result
                        else:
                            white_result, reason = play_game(old_eng, new_eng, fen, base, inc, bo,
                                                             new_engine=new_eng, move_stats=move_stats,
                                                             clock_out=clock_stats, tc=tc)
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
                    meta["games_played"] += 1
                    print(f"  op{op_idx} new_{'W' if new_is_white else 'B'}: "
                          f"{new_pov} ({reason})  running W-D-L={res.wins}-{res.draws}-{res.losses}",
                          flush=True)

                    if pgn_path and bo and white_result is not None:
                        g = chess.pgn.Game.from_board(bo[0])
                        g.headers["Event"] = f"{tc} op{op_idx}"
                        g.headers["White"] = new_label if new_is_white else old_label
                        g.headers["Black"] = old_label if new_is_white else new_label
                        g.headers["Result"] = white_result
                        g.headers["Termination"] = reason
                        g.headers["NewPOV"] = new_pov  # result from the NEW engine's POV
                        with open(pgn_path, "a") as fh:
                            print(g, file=fh, end="\n\n")
                    flush("in_progress")  # persist after every finished game
    except KeyboardInterrupt:
        print("\n[match] interrupted — writing partial result and stopping", flush=True)
        flush("stopped")
        return build_report(new_ident, old_ident, meta, per_tc, move_stats, clock_stats)

    flush("completed")
    return build_report(new_ident, old_ident, meta, per_tc, move_stats, clock_stats)


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
    md = report.get("move_distributions")
    if md and md.get("new") and md["new"].get("time_ms"):
        # Per time control: a bullet move and a blitz move differ several-fold in think
        # time, so the pooled mean describes no control that was actually played.
        by_tc = md.get("by_tc") or {}
        if by_tc:
            print("\n-- per-move time(ms) / depth, BY TIME CONTROL (new vs old) --")
            print(f"{'tc':<14}{'new time':>10}{'new depth':>11} | {'old time':>10}{'old depth':>11}"
                  f"{'Δdepth':>9}")
            for tc, d in by_tc.items():
                nt, nd = d["new"].get("time_ms"), d["new"].get("depth")
                ot, od = d["old"].get("time_ms"), d["old"].get("depth")
                if not (nt and ot):
                    continue
                dd = (f"{nd['mean'] - od['mean']:+.1f}" if nd and od else "-")
                print(f"{tc:<14}{nt['mean']:>10}{(nd['mean'] if nd else '-'):>11} | "
                      f"{ot['mean']:>10}{(od['mean'] if od else '-'):>11}{dd:>9}")
            print("  (Δdepth = new − old; a big negative at equal time means a slower or "
                  "bushier searcher)")
        print("\n-- per-move distributions, ALL time controls pooled (new vs old) --")
        print(f"{'metric':<10}{'new mean':>10}{'new med':>9}{'new p90':>9} | "
              f"{'old mean':>10}{'old med':>9}{'old p90':>9}")
        for metric, lbl in (("time_ms", "time(ms)"), ("depth", "depth")):
            n, o2 = md["new"].get(metric), md["old"].get(metric)
            if n and o2:
                print(f"{lbl:<10}{n['mean']:>10}{n['median']:>9}{n['p90']:>9} | "
                      f"{o2['mean']:>10}{o2['median']:>9}{o2['p90']:>9}")
        # Clock health: time/move by game phase (front-loading?) + clock left at end (banking?).
        if md["new"].get("by_phase"):
            print("\n-- clock usage: avg time(ms)/move by phase, and % base clock left at end --")
            print(f"{'':<10}{'opening':>10}{'middle':>10}{'endgame':>10}{'left@end':>10}")
            for who in ("new", "old"):
                ph = md[who]["by_phase"]
                cl = md[who].get("clock_left_pct", {})
                cells = [ph[k]["avg_time_ms"] for k in ("opening_1_10", "middle_11_30", "endgame_31plus")]
                left = f"{cl['mean']}%" if cl else "-"
                body = "".join(f"{(v if v is not None else '-'):>10}" for v in cells)
                print(f"{who:<10}{body}{left:>10}")
            print("  (healthy: middle >= opening (not front-loaded); left@end small-positive, not banked, not 0)")
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
    ap.add_argument("--output", help="write the durable JSON report to this path "
                    "(default: auto-generated under version_test_results/matches/)")
    ap.add_argument("--new-net", help="NNUEU net dir the NEW engine loads, for the log's "
                    "identity/hash (auto-detected from a wrapper's NNUEU_NET= or $NNUEU_NET if omitted)")
    ap.add_argument("--old-net", help="NNUEU net dir the OLD engine loads (see --new-net)")
    ap.add_argument("--pgn", help="write every game as PGN to this path (for inspecting losses)")
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
    report = None
    output_path = None
    try:
        new_bin = resolve_engine(args.new, built)
        old_bin = resolve_engine(args.old, built)
        new_ident = engine_identity(new_bin, args.new_net)
        old_ident = engine_identity(old_bin, args.old_net)
        # Durable output path: honour --output, else auto-generate a self-describing,
        # timestamped file so every match is recorded even if nobody passed --output.
        if args.output:
            output_path = args.output
            Path(output_path).resolve().parent.mkdir(parents=True, exist_ok=True)
        else:
            outdir = REPO / "version_test_results" / "matches"
            outdir.mkdir(parents=True, exist_ok=True)
            ts = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
            output_path = str(outdir / f"{ts}_{engine_label(new_ident)}_vs_{engine_label(old_ident)}.json")
        report = run_match(new_bin, old_bin, tc_map, openings, new_ident, old_ident,
                           output_path=output_path, pgn_path=args.pgn)
    finally:
        # Clean up any worktrees we created.
        subprocess.run(["git", "worktree", "prune"], cwd=REPO, check=False)

    if report is not None:
        print_summary(report)
        print(f"\n[match] durable log: {output_path}  (status={report['meta']['status']}, "
              f"{report['meta']['games_played']}/{report['meta']['games_planned']} games)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
