#!/usr/bin/env python3
"""Write a config.json beside every net in models/ — Miguel's idea (2026-07-27):
give each net a self-describing record INCLUDING its content hash, so match logs
(which now embed net_sha256 via version_match.py) join to the net's config exactly.

Design constraints honoured:
  - The hash rule is THE SAME as version_match.net_content_hash and
    NNUEU_KOTH_matches.md: sha256 over the name-sorted concatenation of the dir's
    *.csv files, first 12 hex. config.json itself is NOT hashed (only .csv), so
    writing it does not change the net's identity. Verified: the rule reproduces
    the champion's documented 849499aee760.
  - Keys match what version_match.read_net_config expects (width / second_out /
    third_out / phase_buckets / dual_act / attacks / input_planes).
  - Arch is INFERRED FROM THE CSV SHAPES (ground truth), never guessed.
  - Training provenance is merged in from the recovered record (/tmp/provenance.json)
    when available, tagged with its confidence. Absent fields stay null — a wrong
    recorded hyperparameter is worse than a missing one.
  - Existing config.json files are only updated if --force; otherwise merged
    (existing keys win, hash is always refreshed).

Usage:  python3 scripts/gen_net_configs.py [--models-dir models] [--provenance /tmp/provenance.json]
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parent.parent


def net_content_hash(net_dir: Path) -> str:
    """Same rule as version_match.py / NNUEU_KOTH_matches.md — .csv files only."""
    h = hashlib.sha256()
    for name in sorted(p.name for p in net_dir.iterdir() if p.suffix == ".csv"):
        h.update(name.encode())
        h.update((net_dir / name).read_bytes())
    return h.hexdigest()[:12]


def infer_arch(d: Path) -> dict | None:
    """Arch from CSV shapes (works for king-free 640 and kings-in 768 inputs)."""
    def csv(name):
        p = d / name
        return np.loadtxt(p, delimiter=",", ndmin=2) if p.exists() else None

    fw = csv("first_linear_weights.csv")
    sw = csv("second_layer_turn_weights.csv")
    tw = csv("third_layer_weights.csv")
    fl = csv("final_layer_weights.csv")
    if fw is None or sw is None or tw is None or fl is None:
        return None
    N, in_planes = fw.shape
    H1 = sw.shape[0]
    if sw.shape[1] != 64 * N:      # not the king-bucketed layout we know
        return None
    out_buckets, H2 = fl.shape
    if tw.shape[0] != out_buckets * H2:
        return None
    act_mult = tw.shape[1] // (2 * H1)
    if act_mult not in (1, 2):
        return None
    n_params = sum(int(np.prod(csv(f).shape)) for f in
                   ("first_linear_weights.csv", "first_linear_biases.csv",
                    "second_layer_turn_weights.csv", "second_layer_turn_biases.csv",
                    "second_layer_not_turn_weights.csv", "second_layer_not_turn_biases.csv",
                    "third_layer_weights.csv", "third_layer_biases.csv",
                    "final_layer_weights.csv", "final_layer_biases.csv")
                   if (d / f).exists())
    return {"width": int(N), "second_out": int(H1), "third_out": int(H2),
            "dual_act": act_mult == 2, "phase_buckets": int(out_buckets),
            "attacks": False, "input_planes": int(in_planes),
            "kings_in_input": in_planes == 768, "n_params_int8": n_params}


def provenance_for(net: str, prov: dict) -> dict | None:
    """Best recovered record for this net (keys may carry descriptive suffixes)."""
    rank = {"high": 3, "medium": 2, "low": 1}
    best = None
    for k, v in prov.items():
        if k.startswith("[DATASET]"):
            continue
        if k.split("  (")[0].strip() == net:
            if best is None or rank.get(v.get("confidence"), 0) > rank.get(best.get("confidence"), 0):
                best = v
    if not best:
        return None
    out = {"confidence": best.get("confidence"),
           "training_script": best.get("training_script"),
           "training": best.get("training"), "data": best.get("data"),
           "recovered": "2026-07-27 archaeology (MODEL_REGISTRY.md); sources in the registry"}
    ver = best.get("_verify", {})
    if ver.get("verdict"):
        out["source_verification"] = ver["verdict"]
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--models-dir", default=str(REPO / "models"))
    ap.add_argument("--provenance", default="/tmp/provenance.json")
    ap.add_argument("--force", action="store_true", help="overwrite existing config.json keys")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()

    prov = {}
    if Path(a.provenance).exists():
        prov = json.load(open(a.provenance))

    written = skipped = 0
    for d in sorted(Path(a.models_dir).iterdir()):
        if not d.is_dir() or not (d / "first_linear_weights.csv").exists():
            continue
        arch = infer_arch(d)
        if arch is None:
            print(f"SKIP {d.name}: unrecognised CSV layout")
            skipped += 1
            continue
        cfg = {
            "net_name": d.name,
            # THE JOIN KEY: same rule as version_match.py net_content_hash and
            # NNUEU_KOTH_matches.md — match logs carrying net_sha256 join here.
            "weights_sha256_12": net_content_hash(d),
            "hash_rule": "sha256 over name-sorted *.csv (name bytes + content), first 12 hex; config.json excluded",
            **arch,
            "generated": datetime.datetime.now().isoformat(timespec="seconds"),
            "generated_by": "scripts/gen_net_configs.py",
        }
        p = provenance_for(d.name, prov)
        if p:
            cfg["provenance"] = p
        out = d / "config.json"
        if out.exists() and not a.force:
            old = json.loads(out.read_text())
            old["weights_sha256_12"] = cfg["weights_sha256_12"]  # always refresh the hash
            for k, v in cfg.items():
                old.setdefault(k, v)
            cfg = old
        if a.dry_run:
            print(f"DRY {d.name}: {cfg['weights_sha256_12']} {arch['width']}/{arch['second_out']}/{arch['third_out']}"
                  f" da={int(arch['dual_act'])} pb={arch['phase_buckets']} in={arch['input_planes']}"
                  + (" +provenance" if p else ""))
        else:
            out.write_text(json.dumps(cfg, indent=2) + "\n")
            print(f"wrote {out.relative_to(REPO)}  ({cfg['weights_sha256_12']}, "
                  f"{arch['width']}/{arch['second_out']}/{arch['third_out']}, "
                  f"da={int(arch['dual_act'])}, pb={arch['phase_buckets']}, in={arch['input_planes']}"
                  + (", +provenance" if p else "") + ")")
            written += 1
    print(f"\n{written} written, {skipped} skipped")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
