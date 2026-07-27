#!/usr/bin/env python3
"""Build a git ref with the NNUEU net that ref actually shipped with.

CMake's NNUEU defaults (`NNUEU_FIRST_OUT=32`, head 4/4, `NNUEU_DUAL_ACT=0` -> `models/w32_wdl0`)
are unchanged at every tag, but from v0.4.0 on the *release* binaries were built with explicit
width flags (v0.4.0-v0.4.2: `n512_h32`; v0.4.3-v0.4.4: dual-act `n256_h16x16_sq`). So

    cmake -S <worktree of v0.4.3> -B build -DCMAKE_BUILD_TYPE=Release

does not build v0.4.3: it builds v0.4.3's *search* with the v0.3.8 net — and it does so silently,
because `w32_wdl0` is committed at every tag, so nothing fails to load. Two versions built that way
share one eval, and a match between them measures nothing.

This module is the fix: `config_for_ref()` looks the ref's widths up in scripts/nnueu_versions.json,
`cmake_flags()` turns them into -D flags the ref's CMakeLists actually understands, and
`verify_binary_net()` reads the model path back out of the built binary so a wrong net is a hard
error instead of a quiet one.

Run it directly to see what a ref would be built with:

    python3 scripts/nnueu_build_config.py v0.4.3 v0.4.2      # print the configs
    python3 scripts/nnueu_build_config.py --check bin_v043/talshand_exe v0.4.3   # audit a binary
"""
from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TABLE_PATH = REPO / "scripts" / "nnueu_versions.json"
TABLE_REL = "scripts/nnueu_versions.json"

# CMake option name -> key in a config entry. Only options the ref's CMakeLists declares are
# passed (tags <= v0.3.7 have none at all, and cmake warns about unused -D variables).
OPTION_KEYS = {
    "NNUEU_FIRST_OUT": "first_out",
    "NNUEU_SECOND_OUT": "second_out",
    "NNUEU_THIRD_OUT": "third_out",
    "NNUEU_DUAL_ACT": "dual_act",
}


class NetConfigError(RuntimeError):
    """The net a build would load could not be determined, or is not the expected one."""


def _git(*args: str, check: bool = True) -> str:
    res = subprocess.run(["git", "-C", str(REPO), *args],
                         capture_output=True, text=True, check=False)
    if check and res.returncode != 0:
        raise NetConfigError(f"git {' '.join(args)} failed: {res.stderr.strip()}")
    return res.stdout.strip()


def _load_table(path: Path = TABLE_PATH) -> dict:
    with open(path) as fh:
        return json.load(fh)


def _table_at_ref(ref: str) -> dict | None:
    """The table as committed at `ref` (None if that ref predates the file)."""
    out = subprocess.run(["git", "-C", str(REPO), "show", f"{ref}:{TABLE_REL}"],
                         capture_output=True, text=True, check=False)
    if out.returncode != 0:
        return None
    try:
        return json.loads(out.stdout)
    except json.JSONDecodeError:
        return None


def _as_tag(ref: str) -> str:
    """'0.4.3' and 'v0.4.3' are the same tag; anything else is left alone."""
    return f"v{ref}" if re.fullmatch(r"\d+\.\d+\.\d+", ref) else ref


def config_for_ref(ref: str) -> tuple[dict, str]:
    """Return (config, where-it-came-from) for a git ref.

    Resolution order: this checkout's table (authoritative for released tags) -> the table the ref
    itself carries (a newer tag, or a branch, described by its own 'current') -> the newest tag the
    ref descends from (a branch that predates the table). No silent default: an unresolvable ref
    raises, because "assume the CMake defaults" is exactly the bug this module exists to prevent.
    """
    tag = _as_tag(ref)
    table = _load_table()
    if tag == "current":  # the working tree, i.e. how the next release is meant to be built
        return table["current"], f"{TABLE_REL} [current]"
    versions = table["versions"]
    if tag in versions:
        return versions[tag], f"{TABLE_REL} [{tag}]"

    own = _table_at_ref(ref)
    if own:
        if tag in own.get("versions", {}):
            return own["versions"][tag], f"{ref}:{TABLE_REL} [{tag}]"
        if own.get("current"):
            return own["current"], f"{ref}:{TABLE_REL} [current]"

    nearest = _git("describe", "--tags", "--abbrev=0", ref, check=False)
    if nearest and _as_tag(nearest) in versions:
        return versions[_as_tag(nearest)], f"{TABLE_REL} [{_as_tag(nearest)}] (newest tag behind {ref})"

    raise NetConfigError(
        f"no NNUEU build config for ref '{ref}'. Add an entry to {TABLE_REL} (widths + the net "
        f"that build must load) — building it on the CMake defaults would silently give you the "
        f"width-32 w32_wdl0 net.")


def _declared_options(ref: str) -> set[str]:
    """The NNUEU_* cache variables `ref`'s CMakeLists declares (empty for tags <= v0.3.7)."""
    if ref == "current":
        cmakelists = (REPO / "CMakeLists.txt").read_text()
    else:
        cmakelists = subprocess.run(["git", "-C", str(REPO), "show", f"{ref}:CMakeLists.txt"],
                                    capture_output=True, text=True, check=False).stdout
    return set(re.findall(r"set\((NNUEU_\w+)\s+\S+\s+CACHE", cmakelists))


def cmake_flags(config: dict, ref: str | None = None) -> list[str]:
    """-D flags that make a build of `ref` load `config['model_dir']`.

    Options the ref does not declare are dropped: passing them would only earn a
    "Manually-specified variables were not used" warning (pre-v0.4.0 tags have no width switch —
    their single hard-coded width is already the right one).
    """
    declared = _declared_options(ref) if ref else set(OPTION_KEYS)
    return [f"-D{opt}={config[key]}" for opt, key in OPTION_KEYS.items()
            if opt in declared and key in config]


def describe(config: dict, source: str = "") -> str:
    return (f"net={config['model_dir']} "
            f"widths={config['first_out']}/{config['second_out']}/{config['third_out']} "
            f"dual_act={config['dual_act']}" + (f"  (from {source})" if source else ""))


def ensure_model_dir(workdir: Path, config: dict) -> str:
    """Make sure the net exists in a freshly added worktree, and say where it came from.

    The release nets are committed from v0.4.5 on, but the v0.4.0-v0.4.4 tag trees predate that,
    so a worktree of those tags has no n512_h32 / n256_h16x16_sq to copy into the build. Fall back
    to this checkout (working tree first, then the current commit).
    """
    rel = config["model_dir"].rstrip("/")
    dest = workdir / rel
    if dest.is_dir() and any(dest.iterdir()):
        return "in the ref's tree"

    src = REPO / rel
    if src.is_dir() and any(src.iterdir()):
        shutil.copytree(src, dest, dirs_exist_ok=True)
        return f"copied from {src}"

    dest.parent.mkdir(parents=True, exist_ok=True)
    archive = subprocess.run(["git", "-C", str(REPO), "archive", "HEAD", "--", rel],
                             capture_output=True, check=False)
    if archive.returncode == 0 and archive.stdout:
        untar = subprocess.run(["tar", "-x", "-C", str(workdir)], input=archive.stdout, check=False)
        if untar.returncode == 0 and dest.is_dir():
            return "extracted from HEAD"

    raise NetConfigError(
        f"net {rel} is needed to build this ref but exists neither in the ref's tree, nor in "
        f"{REPO}, nor in HEAD. Restore it before building (it is what makes that version that "
        f"version).")


def embedded_model_dirs(binary: Path) -> set[str]:
    """The models/... paths baked into a built binary (the compile-time net choice)."""
    data = Path(binary).read_bytes()
    return {m.decode() for m in re.findall(rb"models/[A-Za-z0-9_.+-]+/", data)}


def verify_binary_net(binary: Path, config: dict) -> str:
    """Hard-fail unless the binary really embeds the net the config asked for."""
    want = config["model_dir"]
    found = embedded_model_dirs(binary)
    if want not in found:
        raise NetConfigError(
            f"{binary} was built with the wrong net: expected {want}, binary embeds "
            f"{sorted(found) or '(no models/ path at all)'}. Check the -D flags for this ref.")
    if len(found) > 1:
        print(f"[net] warning: {binary} embeds several model paths {sorted(found)}; "
              f"expected {want} is among them", flush=True)
    return want


def smoke_check(binary: Path, timeout: float = 60.0) -> None:
    """A UCI handshake, so a net that fails to load is caught here and not mid-match.

    The engine exits with EXIT_FAILURE when its net directory is missing, and python-chess would
    otherwise report that as an opaque engine error partway through the first game.
    """
    proc = subprocess.run([str(binary)], input="uci\nisready\nquit\n",
                          capture_output=True, text=True, timeout=timeout, check=False)
    if "readyok" not in proc.stdout:
        raise NetConfigError(
            f"{binary} did not answer the UCI handshake (exit {proc.returncode}). "
            f"stderr: {proc.stderr.strip()[:400]}")


def main(argv: list[str]) -> int:
    if len(argv) == 2 and argv[0] == "--flags":
        # For build commands: cmake ... $(python3 scripts/nnueu_build_config.py --flags current)
        config, _ = config_for_ref(argv[1])
        print(" ".join(cmake_flags(config, argv[1])))
        return 0

    if len(argv) >= 3 and argv[0] == "--check":
        binary, ref = Path(argv[1]), argv[2]
        config, source = config_for_ref(ref)
        print(f"{ref}: {describe(config, source)}")
        print(f"  binary embeds: {sorted(embedded_model_dirs(binary)) or '(none)'}")
        try:
            verify_binary_net(binary, config)
        except NetConfigError as exc:
            print(f"  MISMATCH: {exc}")
            return 1
        print("  OK: binary matches the version's net")
        return 0

    if not argv:
        argv = sorted(_load_table()["versions"], key=lambda t: [int(x) for x in t.lstrip("v").split(".")])
    for ref in argv:
        config, source = config_for_ref(ref)
        print(f"{ref:10s} {describe(config, source)}")
        print(f"{'':10s} cmake flags: {' '.join(cmake_flags(config, ref)) or '(none — no width switch at this ref)'}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except NetConfigError as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(2)
