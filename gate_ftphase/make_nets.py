#!/usr/bin/env python3
"""Build synthetic nets that pin down what NNUEU_FT_PHASE must do in the engine.

No trained net is needed, and no numpy replica of the forward pass either -- a replica that
shared the exporter's assumptions could not validate them (that is the standing lesson from the
export-verification work). Instead the test uses the ENGINE ITSELF twice, against an invariant:

  NET A (uniform):  all 8 phase buckets carry IDENTICAL weights.
                    Then the bucketing is a no-op BY CONSTRUCTION, so an FT_PHASE=1 build loading
                    net A must produce BIT-IDENTICAL evals to an FT_PHASE=0 build loading the
                    single-bucket net it was tiled from. That one comparison exercises the index
                    offset, the per-bucket plane mirroring in the inverted loader, the shared
                    bias, and the single-row psqt layout all at once. Any of them wrong and the
                    numbers diverge.

  NETS B (bucket 3 scaled up, and scaled down): identical to A except bucket 3's FT block.
                    Every eval that MOVES must belong to bucket 3 ((n_pieces-1)//4 == 3, i.e.
                    13-16 pieces) -- that is the bucket SELECTION test, which net A alone cannot
                    make: A would pass on a build that always read bucket 0.

                    The assertion is "changed positions are a SUBSET of bucket 3", not "all of
                    bucket 3 changes". A position whose head output already sits on a rail
                    (the head is bounded to +-2048 before psqt) does not move when the weights
                    feeding it are scaled further INTO that rail -- measured here on a 16-piece
                    position pinned at -2023. Hence two perturbations, up and down: a rail can
                    only pin one direction, so every bucket-3 position must move in at least one.
"""
import os, sys, numpy as np

SRC = sys.argv[1]      # a single-bucket (F_MAP-row) net dir to tile from
DST = sys.argv[2]      # destination root
F_MAP = 768
rng = np.random.default_rng(4)

os.makedirs(DST, exist_ok=True)


def read_rows(p):
    return [l.strip() for l in open(p) if l.strip()]


def tile_ft(src_path, dst_path, scale_bucket=None, scale=1.0):
    """first_linear_weights.csv is [FIRST_OUT rows] x [F_MAP cols]; the ft_phase form is the same
    rows with 8*F_MAP cols, bucket-major."""
    out = []
    for line in read_rows(src_path):
        v = np.array([int(float(x)) for x in line.split(",")], dtype=np.int64)
        assert v.size == F_MAP, v.size
        blocks = []
        for b in range(8):
            blk = v.copy()
            if scale_bucket is not None and b == scale_bucket:
                blk = np.round(blk * scale).astype(np.int64)
            blocks.append(blk)
        out.append(",".join(str(int(x)) for x in np.concatenate(blocks)))
    open(dst_path, "w").write("\n".join(out) + "\n")


def tile_psqt(src_path, dst_path):
    """classic psqt is 8 rows x F_MAP (one per bucket); the ft_phase form is ONE row of
    8*F_MAP in the same bucket-major order -- so it is a flatten, not a tile."""
    rows = read_rows(src_path)
    assert len(rows) == 8, len(rows)
    flat = []
    for r in rows:
        v = [int(float(x)) for x in r.split(",")]
        assert len(v) == F_MAP, len(v)
        flat.extend(v)
    open(dst_path, "w").write(",".join(str(x) for x in flat) + "\n")


for name, sb, sc in (("uniform", None, 1.0), ("bucket3_up", 3, 1.5), ("bucket3_down", 3, 0.5)):
    d = os.path.join(DST, name)
    os.makedirs(d, exist_ok=True)
    for fn in os.listdir(SRC):
        if not fn.endswith(".csv"):
            continue
        s, t = os.path.join(SRC, fn), os.path.join(d, fn)
        if fn == "first_linear_weights.csv":
            tile_ft(s, t, sb, sc)
        elif fn == "psqt_weights.csv":
            tile_psqt(s, t)
        else:
            open(t, "w").write(open(s).read())
    print(f"  {name}: escrito en {d}")
