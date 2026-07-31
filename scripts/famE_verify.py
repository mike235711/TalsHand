#!/usr/bin/env python3
"""famE (HEAD_SUM / HEAD_SKIP) numpy reference + bit-exact verification harness.

This is PURE ENGINE-VERIFICATION tooling (analogous to the UCI `eval` command's own doc comment
in src/engine.cpp: "used to verify the C++ net against the PyTorch/numpy reference oracle"). There
is no trained model for famE yet -- every weight used here is SYNTHETIC (deterministic, seeded
random integers, already living directly on the engine's quantization grids -- see "On-grid
weights" below), generated fresh by this script and fed to the real C++ engine binary through a
temporary model directory + the UCI `eval` command.

famE's fixed architecture (see accumulation.h / network.cpp for the authoritative C++):
  - Input: kings_in, F_MAP=768 (both kings as feature planes), always.
  - SPLIT_FT=1, DUAL_ACT=1, THIRD_PHASE=1 (8 phase-bucketed 3rd-layer stacks), always.
  - psqt_sf (Weight::hasPsqt / psqtTerm/psqtRaw): always on for famE, so this reference also
    implements it -- it is a real additive term in every evaluate() call.
  - 2nd layer: SECOND_OUT_W (H1) = 16, always, king-bucketed (64 buckets), split-ft (each
    perspective reads its own half of the FIRST_OUT-wide accumulator).
  - NEW (famE): HEAD_SUM sums the two perspectives' RAW (pre-activation) 2nd-layer outputs into
    ONE vector and activates ONLY the sum (CReLU + SqrCReLU, dual-act). Optionally HEAD_SKIP
    carves the LAST lane (index 15) of each perspective out of that sum as a "skip scalar" that
    bypasses the summed/activated path AND the 3rd/final layers, added straight into the final
    output at a fixed-point scale derived in network.cpp's `headSkipToOutputScale()` comment
    (repeated below).
  - Varies: N (NNUEU_FIRST_OUT) in {128, 256}, H2 (NNUEU_THIRD_OUT) in {16, 32}, HEAD_SKIP in {0,1}.

On-grid weights
----------------
The established convention in this codebase (ConstrainedLinear / STE on the training side, not
present yet for famE) is: FT weights x127, hidden-layer (2nd/3rd) weights x64, hidden-layer biases
x8128 (=64*127), final weights x(4096/127), final bias x4096, output centered by -2048. Every one
of those integers IS the "on-grid" representation directly -- there is no separate float weight in
this pipeline to round. So "on-grid synthetic weights" here simply means: generate the INTEGERS
the C++ engine consumes directly (bounded to sane per-tensor ranges, see `gen_weights()`), with no
intermediate float rounding step to confound the comparison. That is exactly what this script does.

Usage:
    python3 scripts/famE_verify.py --root /path/to/worktree \
        --configs 128:16:0 128:16:1 256:32:1 128:32:0 128:32:1 256:16:0 256:16:1 256:32:0

Each prebuilt engine binary is expected at
    <root>/build_famE/N<N>_H<H2>_SK<SKIP>/src/talshand_exe
(built with -DNNUEU_FIRST_OUT=<N> -DNNUEU_SECOND_OUT=16 -DNNUEU_THIRD_OUT=<H2> -DNNUEU_F_MAP=768
 -DNNUEU_SPLIT_FT=1 -DNNUEU_DUAL_ACT=1 -DNNUEU_THIRD_PHASE=1 -DNNUEU_HEAD_SUM=1
 -DNNUEU_HEAD_SKIP=<SKIP> -DCMAKE_BUILD_TYPE=Release).
"""
import argparse
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import chess

F_MAP = 768
H1 = 16  # NNUEU_SECOND_OUT, fixed for every famE arm
THIRD_STACKS = 8
KING_OWN_BASE = 640  # white king plane (F_MAP=768: always the ABSOLUTE white king)
KING_OPP_BASE = 704  # black king plane (F_MAP=768: always the ABSOLUTE black king)

PIECE_TYPES = [chess.PAWN, chess.KNIGHT, chess.BISHOP, chess.ROOK, chess.QUEEN]  # t = 0..4


# ---------------------------------------------------------------------------
# Bit-level helpers that must match the C++ engine's integer semantics EXACTLY
# ---------------------------------------------------------------------------

def mirror_plane(p: int) -> int:
    """accumulation.h::mirrorPlane, specialised to KINGS_IN=true (F_MAP=768, famE always)."""
    if p < 10:
        return (p + 5) % 10
    return 21 - p  # 10 (white king) <-> 11 (black king)


def invert_index(sq: int) -> int:
    """bit_utils.h::invertIndex -- vertical (rank) mirror. Equivalent to sq ^ 56."""
    row = 7 - (sq // 8)
    return row * 8 + (sq % 8)


def trunc_div(a: int, b: int) -> int:
    """C++ int division: truncate toward zero (Python's // floors -- NOT the same for a<0)."""
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q


def to_i16(x: int) -> int:
    """C++20 well-defined narrowing to int16_t: 2's-complement modular wraparound."""
    x &= 0xFFFF
    return x - 0x10000 if x >= 0x8000 else x


def head_skip_to_output_scale(skip_raw_x8128: int) -> int:
    """Mirrors network.cpp's headSkipToOutputScale() EXACTLY: floor(skip_raw * 64 / 127).

    Derivation (repeated from the C++ comment so this file is self-contained): the skip lane is
    an ordinary 2nd-layer neuron's raw `dot + bias`, at the SAME x8128 fixed point every other
    2nd-layer neuron's pre->6-shift value lives at. It must land in evaluate()'s x4096-scale final
    sum, but it BYPASSES the 3rd and final layers -- the only two places such a conversion is
    normally learned -- and the spec requires it be a straight, UNCLAMPED linear residual (no
    CReLU), matching nnue-pytorch's factorizer pattern (l1c_out/l1f_out folded straight into the
    final sum). Design choice: treat it as an IDENTITY-weighted residual, i.e. give it the same
    per-unit output contribution an ordinary neuron gets from a final-layer weight of exactly 1.0:
    (1/64) [x8128->x127, the ordinary >>6] * (4096/127) [x127->x4096 at unit weight] = 64/127
    exactly. Python's `//` is already floor division for a positive divisor, matching the C++ side's
    explicit floor-division adjustment (needed there only because skip values, unlike everything
    else in this file, are never ReLU'd and so can be negative).
    """
    return (skip_raw_x8128 * 64) // 127


def phase_bucket(n_pieces: int) -> int:
    """network.cpp::Network::phaseBucket -- SF's PSQT phase bucket, ALL pieces both colours incl. king."""
    return min(7, max(0, (n_pieces - 1) // 4))


# ---------------------------------------------------------------------------
# famE build configuration
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class FamEConfig:
    n: int          # NNUEU_FIRST_OUT: 128 or 256
    h2: int         # NNUEU_THIRD_OUT: 16 or 32
    head_skip: int  # NNUEU_HEAD_SKIP: 0 or 1

    @property
    def split_read(self) -> int:
        return self.n // 2  # SPLIT_FT=1 always for famE

    @property
    def head_sum_raw_w(self) -> int:
        return H1 - (1 if self.head_skip else 0)

    @property
    def head_out_w(self) -> int:
        return 2 * self.head_sum_raw_w  # DUAL_ACT=1 always for famE

    @property
    def third_in(self) -> int:
        return self.head_out_w  # PSQT_L3=0 always for famE, so THIRD_STRIDE == THIRD_IN == HEAD_OUT_W

    def name(self) -> str:
        return f"N{self.n}_H{self.h2}_SK{self.head_skip}"

    def cmake_args(self) -> list:
        return [
            f"-DNNUEU_FIRST_OUT={self.n}", "-DNNUEU_SECOND_OUT=16", f"-DNNUEU_THIRD_OUT={self.h2}",
            "-DNNUEU_F_MAP=768", "-DNNUEU_SPLIT_FT=1", "-DNNUEU_DUAL_ACT=1",
            "-DNNUEU_THIRD_PHASE=1", "-DNNUEU_HEAD_SUM=1", f"-DNNUEU_HEAD_SKIP={self.head_skip}",
        ]


# ---------------------------------------------------------------------------
# Synthetic on-grid weight generation
# ---------------------------------------------------------------------------

@dataclass
class Weights:
    first_w: np.ndarray        # [N, F_MAP]      int, CSV/nn.Linear layout (out, in)
    first_bias: np.ndarray     # [N]
    second1: np.ndarray        # [H1, 64, SPLIT_READ]
    second2: np.ndarray        # [H1, 64, SPLIT_READ]
    second_bias_turn: np.ndarray   # [H1]
    second_bias_nott: np.ndarray   # [H1]
    third_w: np.ndarray        # [8, H2, THIRD_IN]
    third_bias: np.ndarray     # [8, H2]
    final_w: np.ndarray        # [H2]
    final_bias: int
    psqt_w: np.ndarray         # [8, F_MAP]


def gen_weights(cfg: FamEConfig, seed: int) -> Weights:
    rng = np.random.default_rng(seed)
    # Ranges are chosen so that (a) every intermediate int32 dot product stays comfortably inside
    # int32 range for realistic test positions (checked at comparison time too), and (b) the
    # accumulator (int16, unclamped add) does not silently wrap for realistic active-feature
    # counts (~30-34). Where a value legitimately DOES need to wrap (the final int16 narrowing
    # casts), to_i16() reproduces C++'s well-defined modular truncation exactly, so overflow there
    # is not a correctness risk -- just something both sides must agree on, which they do.
    return Weights(
        first_w=rng.integers(-180, 181, size=(cfg.n, F_MAP), dtype=np.int64),
        first_bias=rng.integers(-300, 301, size=(cfg.n,), dtype=np.int64),
        second1=rng.integers(-100, 101, size=(H1, 64, cfg.split_read), dtype=np.int64),
        second2=rng.integers(-100, 101, size=(H1, 64, cfg.split_read), dtype=np.int64),
        second_bias_turn=rng.integers(-3000, 3001, size=(H1,), dtype=np.int64),
        second_bias_nott=rng.integers(-3000, 3001, size=(H1,), dtype=np.int64),
        third_w=rng.integers(-100, 101, size=(THIRD_STACKS, cfg.h2, cfg.third_in), dtype=np.int64),
        third_bias=rng.integers(-3000, 3001, size=(THIRD_STACKS, cfg.h2), dtype=np.int64),
        final_w=rng.integers(-100, 101, size=(cfg.h2,), dtype=np.int64),
        final_bias=int(rng.integers(-3000, 3001)),
        psqt_w=rng.integers(-150, 151, size=(8, F_MAP), dtype=np.int64),
    )


def _write_rows(path: Path, rows: np.ndarray) -> None:
    with open(path, "w") as f:
        for row in np.atleast_2d(rows):
            f.write(",".join(str(int(v)) for v in row) + "\n")


def _write_lines(path: Path, values) -> None:
    with open(path, "w") as f:
        for v in values:
            f.write(f"{int(v)}\n")


def write_model_dir(out_dir: Path, cfg: FamEConfig, w: Weights) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    _write_rows(out_dir / "first_linear_weights.csv", w.first_w)             # [N, F_MAP]
    _write_lines(out_dir / "first_linear_biases.csv", w.first_bias)          # N lines
    _write_rows(out_dir / "second_layer_turn_weights.csv",
                w.second1.reshape(H1, 64 * cfg.split_read))                  # H1 rows
    _write_rows(out_dir / "second_layer_not_turn_weights.csv",
                w.second2.reshape(H1, 64 * cfg.split_read))
    _write_lines(out_dir / "second_layer_turn_biases.csv", w.second_bias_turn)
    _write_lines(out_dir / "second_layer_not_turn_biases.csv", w.second_bias_nott)
    _write_rows(out_dir / "third_layer_weights.csv",
                w.third_w.reshape(THIRD_STACKS * cfg.h2, cfg.third_in))      # bucket-major
    _write_lines(out_dir / "third_layer_biases.csv", w.third_bias.reshape(-1))
    _write_rows(out_dir / "final_layer_weights.csv", w.final_w.reshape(1, -1))
    _write_lines(out_dir / "final_layer_biases.csv", [w.final_bias])
    _write_rows(out_dir / "psqt_weights.csv", w.psqt_w)                      # [8, F_MAP]


# ---------------------------------------------------------------------------
# numpy reference forward path (mirrors network.cpp's evaluate()/forwardPass() exactly)
# ---------------------------------------------------------------------------

def _active_columns(board: chess.Board):
    """Yield (t, colour, square, plane, col) for every active king-free feature, absolute-colour
    plane assignment (white P..Q -> planes 0-4, black P..Q -> planes 5-9), matching
    accumulation.cpp::initialize()."""
    for t, pt in enumerate(PIECE_TYPES):
        for colour, base in ((chess.WHITE, 0), (chess.BLACK, 5)):
            plane = base + t
            for sq in board.pieces(pt, colour):
                yield plane, sq


def numpy_forward_eval(fen: str, cfg: FamEConfig, w: Weights) -> int:
    board = chess.Board(fen)
    turn_white = board.turn == chess.WHITE
    wk = board.king(chess.WHITE)
    bk = board.king(chess.BLACK)
    assert wk is not None and bk is not None

    active = list(_active_columns(board))                      # [(plane, sq), ...] king-free
    active_cols = [(plane * 64 + sq) for plane, sq in active]
    active_cols.append(KING_OWN_BASE + wk)                      # plane 10: absolute white king
    active_cols.append(KING_OPP_BASE + bk)                      # plane 11: absolute black king

    first_w_mem = w.first_w.T                                  # [F_MAP, N] -- firstW[col][k]
    acc_w = w.first_bias.copy()
    acc_b = w.first_bias.copy()
    for col in active_cols:
        acc_w = acc_w + first_w_mem[col]
        acc_b = acc_b + first_w_mem[mirror_plane(col // 64) * 64 + invert_index(col % 64)]
    acc_w = np.array([to_i16(int(v)) for v in acc_w], dtype=np.int64)
    acc_b = np.array([to_i16(int(v)) for v in acc_b], dtype=np.int64)

    p_input = acc_w if turn_white else acc_b

    if turn_white:
        own_king_idx, opp_king_idx = wk, bk
    else:
        own_king_idx, opp_king_idx = invert_index(bk), invert_index(wk)

    a = np.clip(p_input, 0, 127)  # CReLU/narrow-to-int8 of the accumulator (a[FIRST_OUT])
    sr = cfg.split_read
    a_blk = [a[:sr], a[sr:2 * sr]]  # SPLIT_FT=1 always: blk0 = low half, blk1 = high half

    raw = [np.zeros(H1, dtype=np.int64), np.zeros(H1, dtype=np.int64)]
    for blk in range(2):
        king_idx = own_king_idx if blk == 0 else opp_king_idx
        table = w.second1 if blk == 0 else w.second2
        bias = w.second_bias_turn if blk == 0 else w.second_bias_nott
        for o in range(H1):
            s = int(bias[o]) + int(np.dot(a_blk[blk], table[o, king_idx, :]))
            raw[blk][o] = s

    skip_turn_raw = int(raw[0][H1 - 1])
    skip_nott_raw = int(raw[1][H1 - 1])

    raw_w = cfg.head_sum_raw_w
    combined = raw[0][:raw_w] + raw[1][:raw_w]  # elementwise int add, x8128 scale

    c = np.clip(combined >> 6, 0, 127)          # CReLU(combined); Python >> is floor, matches C++20
    sq_c = (c * c) >> 7                         # SqrCReLU(combined)
    l1 = np.concatenate([c, sq_c]).astype(np.int64)  # width HEAD_OUT_W == THIRD_IN == THIRD_STRIDE

    n_pieces = len(board.piece_map())
    bucket = phase_bucket(n_pieces)

    l2 = np.zeros(cfg.h2, dtype=np.int64)
    for o2 in range(cfg.h2):
        s3 = int(w.third_bias[bucket, o2]) + int(np.dot(l1, w.third_w[bucket, o2]))
        l2[o2] = np.clip(s3 >> 6, 0, 127)

    final_raw = int(w.final_bias) + int(np.dot(l2, w.final_w))
    if cfg.head_skip:
        final_raw += head_skip_to_output_scale(skip_turn_raw) + head_skip_to_output_scale(skip_nott_raw)
    forward_pass_result = to_i16(final_raw)

    # psqt_sf (always on for famE)
    wp = bp = 0
    for plane, sq in active:
        wp += int(w.psqt_w[bucket, plane * 64 + sq])
        bp += int(w.psqt_w[bucket, mirror_plane(plane) * 64 + invert_index(sq)])
    wp += int(w.psqt_w[bucket, KING_OWN_BASE + wk]) + int(w.psqt_w[bucket, KING_OPP_BASE + bk])
    bp += int(w.psqt_w[bucket, KING_OPP_BASE + invert_index(wk)]) + int(w.psqt_w[bucket, KING_OWN_BASE + invert_index(bk)])
    psqt_raw_stm = (wp - bp) if turn_white else (bp - wp)
    psqt_add = trunc_div(psqt_raw_stm, 2)

    final_eval = to_i16(forward_pass_result - 2048 + 0 + psqt_add)  # materialTerm == 0 (not used by famE)
    return final_eval


# ---------------------------------------------------------------------------
# Test positions -- varying piece counts to exercise every phase bucket (0..7)
# ---------------------------------------------------------------------------

def build_test_positions():
    """Deterministic hand-built positions spanning phase buckets 0..7 (bucket = (nPieces-1)//4,
    clamped 0..7, nPieces counting BOTH colours and the two kings)."""
    positions = []

    # bucket 7 (>=29 pieces): the real start position (32 pieces).
    positions.append(("startpos_32pc", chess.STARTING_FEN))

    # A real, densely-populated middlegame (Kiwipete) as a second bucket-7-ish/6-ish sample.
    positions.append(("kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"))

    # Hand-built boards with a controlled piece count, deterministic RNG placement.
    rng = np.random.default_rng(12345)
    target_counts = [26, 22, 18, 14, 10, 6, 3]  # -> buckets 6,5,4,3,2,1,0
    for idx, n_extra_target in enumerate(target_counts):
        board = chess.Board(None)  # empty board
        board.turn = chess.WHITE if idx % 2 == 0 else chess.BLACK
        squares = list(chess.SQUARES)
        rng.shuffle(squares)
        it = iter(squares)
        wk = next(it)
        board.set_piece_at(wk, chess.Piece(chess.KING, chess.WHITE))
        bk = next(it)
        while chess.square_distance(bk, wk) < 2:  # kings cannot be adjacent
            bk = next(it)
        board.set_piece_at(bk, chess.Piece(chess.KING, chess.BLACK))

        n_extra = n_extra_target - 2  # minus the two kings already placed
        piece_pool = [chess.PAWN, chess.KNIGHT, chess.BISHOP, chess.ROOK, chess.QUEEN]
        placed = 0
        for sq in it:
            if placed >= n_extra:
                break
            if chess.square_rank(sq) in (0, 7) and True:
                pt = piece_pool[placed % len(piece_pool)]
                if pt == chess.PAWN:
                    continue  # keep pawns off the back ranks, cosmetic only
            else:
                pt = piece_pool[placed % len(piece_pool)]
            colour = chess.WHITE if placed % 2 == 0 else chess.BLACK
            board.set_piece_at(sq, chess.Piece(pt, colour))
            placed += 1

        fen = board.fen()
        positions.append((f"synthetic_{n_extra_target}pc", fen))

    return positions


# ---------------------------------------------------------------------------
# C++ engine driver (UCI) + comparison harness
# ---------------------------------------------------------------------------

def cpp_eval_batch(binary: Path, model_dir: Path, fens):
    """Drive one talshand_exe process through `setoption EvalFile`, `position fen`, `eval` for
    every FEN in `fens`, returning a list of ints in the same order."""
    cmds = ["uci", f"setoption name EvalFile value {model_dir}/"]
    for fen in fens:
        cmds.append(f"position fen {fen}")
        cmds.append("eval")
    cmds.append("quit")
    proc = subprocess.run([str(binary)], input="\n".join(cmds) + "\n",
                           capture_output=True, text=True, timeout=60)
    evals = [int(line.split()[1]) for line in proc.stdout.splitlines() if line.startswith("eval ")]
    if len(evals) != len(fens):
        print(proc.stdout, file=sys.stderr)
        print(proc.stderr, file=sys.stderr)
        raise RuntimeError(f"expected {len(fens)} eval lines, got {len(evals)}")
    return evals


def run_config(root: Path, cfg: FamEConfig, positions, seed: int, out_root: Path):
    binary = root / "build_famE" / cfg.name() / "src" / "talshand_exe"
    if not binary.exists():
        raise FileNotFoundError(f"missing prebuilt binary: {binary}")

    w = gen_weights(cfg, seed)
    model_dir = out_root / cfg.name()
    write_model_dir(model_dir, cfg, w)

    fens = [fen for _, fen in positions]
    cpp_evals = cpp_eval_batch(binary, model_dir, fens)

    max_abs_diff = 0
    rows = []
    for (name, fen), cpp_v in zip(positions, cpp_evals):
        py_v = numpy_forward_eval(fen, cfg, w)
        diff = cpp_v - py_v
        max_abs_diff = max(max_abs_diff, abs(diff))
        rows.append((name, cpp_v, py_v, diff))

    return max_abs_diff, rows


def parse_config(spec: str) -> FamEConfig:
    n, h2, sk = spec.split(":")
    return FamEConfig(n=int(n), h2=int(h2), head_skip=int(sk))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1],
                     help="worktree root containing build_famE/<name>/src/talshand_exe")
    ap.add_argument("--configs", nargs="+", default=["128:16:0", "128:16:1", "256:32:1"],
                     help="N:H2:HEAD_SKIP triples")
    ap.add_argument("--seed", type=int, default=20260731)
    ap.add_argument("--out-dir", type=Path, default=None,
                     help="where to write synthetic model dirs (default: <root>/scratch_famE_models)")
    args = ap.parse_args()

    out_root = args.out_dir or (args.root / "scratch_famE_models")
    positions = build_test_positions()
    print(f"Test positions ({len(positions)}):")
    for name, fen in positions:
        b = chess.Board(fen)
        print(f"  {name:20s} nPieces={len(b.piece_map()):2d} bucket={phase_bucket(len(b.piece_map()))} fen={fen}")
    print()

    overall_ok = True
    for spec in args.configs:
        cfg = parse_config(spec)
        try:
            max_diff, rows = run_config(args.root, cfg, positions, args.seed, out_root)
        except Exception as e:
            print(f"[{cfg.name()}] ERROR: {e}")
            overall_ok = False
            continue
        status = "PASS (bit-exact)" if max_diff == 0 else "FAIL"
        if max_diff != 0:
            overall_ok = False
        print(f"=== {cfg.name()}  max|diff|={max_diff}  {status} ===")
        for name, cpp_v, py_v, diff in rows:
            marker = "" if diff == 0 else "  <-- MISMATCH"
            print(f"    {name:20s} cpp={cpp_v:7d}  numpy={py_v:7d}  diff={diff:4d}{marker}")
        print()

    sys.exit(0 if overall_ok else 1)


if __name__ == "__main__":
    main()
