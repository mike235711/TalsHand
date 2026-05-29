#!/usr/bin/env python3
"""Release gate: require the current build to be no weaker than the previous one.

Intended to run before bumping the version / tagging a release. It plays a match
between the current build and the previous released version (auto-detected from
the git tags) and passes only if the measured Elo is not significantly negative
-- i.e. the new version is at least as strong as the old one.

    python3 scripts/release_gate.py                 # full default match (slow)
    python3 scripts/release_gate.py --min-elo 5      # demand proven improvement
    python3 scripts/release_gate.py --base 5 --inc 0.1 --max-openings 4   # quick

Exit code 0 = gate passed (safe to bump), non-zero = gate failed.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


def changelog_version() -> str:
    m = re.search(r"^##\s*\[(\d+\.\d+\.\d+)\]", (REPO / "CHANGELOG.md").read_text(),
                  re.MULTILINE)
    return m.group(1) if m else "0.0.0"


def semver(v: str):
    return tuple(int(x) for x in v.lstrip("v").split(".")[:3])


def previous_tag(current: str) -> str | None:
    out = subprocess.run(["git", "tag", "--list"], cwd=REPO,
                         capture_output=True, text=True, check=True)
    tags = [t for t in out.stdout.split() if re.fullmatch(r"v\d+\.\d+\.\d+", t)]
    below = [t for t in tags if semver(t) < semver(current)]
    if not below:
        return None
    return max(below, key=semver)


def main() -> int:
    ap = argparse.ArgumentParser(description="TalsHand release gate")
    ap.add_argument("--new", default=None, help="new engine (path or ref); default current Release build")
    ap.add_argument("--previous", default=None, help="opponent (path or ref); default: previous git tag")
    ap.add_argument("--min-elo", type=float, default=0.0,
                    help="require Elo lower-bound (elo - error) >= this (default 0: not weaker)")
    # Pass-through match controls.
    ap.add_argument("--tc", action="append")
    ap.add_argument("--base", type=float)
    ap.add_argument("--inc", type=float, default=0.0)
    ap.add_argument("--max-openings", type=int)
    args = ap.parse_args()

    current = changelog_version()
    prev = args.previous or previous_tag(current)
    if prev is None:
        print(f"[gate] no previous tag below {current}; nothing to compare against -> PASS")
        return 0
    print(f"[gate] current {current} vs previous {prev}")

    out = Path(tempfile.mktemp(suffix=".json"))
    cmd = [sys.executable, str(REPO / "scripts" / "version_match.py"),
           "--old", prev, "--output", str(out)]
    if args.new:
        cmd += ["--new", args.new]
    if args.base is not None:
        cmd += ["--base", str(args.base), "--inc", str(args.inc)]
    elif args.tc:
        for tc in args.tc:
            cmd += ["--tc", tc]
    if args.max_openings:
        cmd += ["--max-openings", str(args.max_openings)]

    subprocess.run(cmd, check=True)
    report = json.loads(out.read_text())
    out.unlink(missing_ok=True)

    o = report["overall"]
    lower = o["elo"] - o["elo_error"]
    print(f"\n[gate] overall: W-D-L={o['wins']}-{o['draws']}-{o['losses']} "
          f"score={o['score_pct']:.1f}% Elo={o['elo']:+.1f} ± {o['elo_error']:.1f} "
          f"(lower bound {lower:+.1f})")

    if lower >= args.min_elo:
        print(f"[gate] PASS: Elo lower bound {lower:+.1f} >= {args.min_elo:+.1f}")
        return 0
    print(f"[gate] FAIL: Elo lower bound {lower:+.1f} < {args.min_elo:+.1f} "
          f"(new build not proven {'stronger' if args.min_elo > 0 else 'as strong'})")
    return 1


if __name__ == "__main__":
    sys.exit(main())
