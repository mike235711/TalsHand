# La Mano de Miguelito - Architecture and Key Components

The engine's architecture is divided into several main components, all located in the `src/` directory:

## Board Representation and Move Generation

We use "bitboards" for an efficient board representation and fast move generation. The key files are `src/bitposition.cpp`, `src/bitposition.h`, `src/move.h`, and `src/magicmoves.h`. Speed in this area is critical.

Move generation is split into two parts: for alpha-beta search and for quiescence search. In quiescence search, only captures and promotions are generated. Therefore, knowing that only these moves are generated in this part allows for significant optimization of move generation. `makeCapture` and `unmakeCapture` are optimized versions of `makeMove` and `unmakeMove`, all defined in `src/bitposition.cpp`. One more thing is that in quiesence search we dont need to check for threefold repetitions, so we dont need to compute zobrist keys. Hence in makeCapture the zobrist keys are not updated.

To save the state of a position and allow undoing moves, we use `StateInfo` in `src/bitposition.h`. For `makeMove` and `unmakeMove` the following are copied into `StateInfo`: `zobristKey`, `reversibleMovesMade`, and `castlingRights`. For `makeCapture` and `unmakeCapture` nothing is copied into `StateInfo`, as it is not necessary because threefold repetitions do not need to be checked (they cannot occur with captures) and castling is not considered.

To check if we are in check, this is done in `makeMove` and `makeCapture`. To detect discovered checks, `blockersForKing` from `StateInfo` is used. These are computed by calling `setBlockersAndPinsInAB` or `setBlockersPinsAndCheckBitsInQS` (this is called in `worker.cpp`). Direct checks are checked directly in `makeMove` and `makeCapture`.

There are 2 differences between `setBlockersAndPinsInAB` and `setBlockersAndPinsAndCheckBitsInQS`:
    - The first is that in AB, checks on diagonals and ranks/files are generated separately. In QS, they are all generated together. `setBlockersAndPinsInAB` might be slower, but the reason for doing it this way is that having separate diagonal and rank/file pins allows for significant optimization of move generation in AB. The move generators in `bitposition.cpp` can generate illegal moves. But in the case of AB, there are many moves, so generating many illegal moves can be more costly than generating fewer illegal moves, considering that rooks cannot move if they are pinned diagonally and bishops cannot move if they are pinned on a rank or file. In QS, there are few moves, so it is not as important to optimize move generation as much.
    - The second is that in QS, the check bits (`checkBits`) are generated, but not in AB. This is because in AB, we always call `setCheckBits` upon entering AB. In QS, it is only called if a legal or illegal move is found. Because in QS only captures and promotions are generated, it is likely that many times no moves will be generated. In these cases, we do not want to spend time calculating check bits if they are not going to be used.

To see if a move is legal, this is determined by calling `isLegal`/`isCaptureLegal` from `src/bitposition.cpp`. This call is made in `src/move_selectors.h` (`ABMoveSelectorCheck`, `ABMoveSelectorNotCheck`, `QSMoveSelectorCheck`, `QSMoveSelectorNotCheck`). Both `isLegal` and `isCaptureLegal` use `pinnedPieces` from `StateInfo` to see if a piece is pinned. isCaptureLegal is an optimized version of isLegal, using the fact that we know a capture or promotion was made.

## Position Evaluation (NNUEU)
Evaluation is performed by an NNUE (Efficiently Updatable Neural Network) type neural network. We have implemented a custom variant called NNUEU ("Ultra Efficiently Updatable Neural Networks"), which allows for even faster updates, especially for king moves.
  - The implementation is in `src/network.cpp`, and `src/accumulation.cpp`.
  - In order for the NNUEU to work we have to save when making moves some incremental changes information which is stored in NNUEU::NNUEUChange from `src/accumulation.h`. These are created when making moves inside makeMove and unmakeMove from `src/bitposition.cpp`, storing information about the indices of the NNUEU's first layer which are affected by the move.
  - The engine's worker `src/worker.h` has an NNUEU::AccumulatorStack whose deifinition is inside `src/accumulation.h`. This stack stacks up the incremental changes NNUEU::NNUEUChange, and stores the state of the last evaluated position in a NNUEU::AccumulatorState object defined in `src/accumulation.h`. If we want to evaluate a position, the AccumulatorStack will got to the closest previously evaluated position and update the state incrementally based on the NNUEU::NNUEUChange's that had been stored leading to the new position we want to evaluate now.
  - The theory behind NNUEU is detailed in `README.md`. But basically NNUEU::Transformer, NNUEU::AccumulatorStack, NNUEU::AccumulatorState and NNUEUChange are in charge of updating efficiently the output of our NNUEU's first layer. We then have in `network.cpp` and `network.h` defined the rest of the NNUEU. This takes care of performing the forward pass with SIMD instructions (very fast) of the rest of the NNUEU which consists of 3 layers. The output of this will be the static evaluation of the position, where a high value is good for the engine and low is bad for the engine, see NNUEU::Network.evaluate in `src/network.cpp`.
  - The trained neural network models are in the `models/` directory.
  - **Accumulator width (build switch).** The first-layer accumulator width is a
    compile-time constant `NNUEU::FIRST_OUT` (`src/accumulation.h`), driven by the
    CMake cache variable `NNUEU_FIRST_OUT` (default **32**, the v0.3.8 `w32_wdl0`
    baseline; pass `-DNNUEU_FIRST_OUT=8` to rebuild the legacy width-8 net, which
    defaults to the proven v4 net and reproduces v0.3.7's eval). The default net path
    follows the width. Layers 2 and 3 are unchanged across widths — only the first
    accumulator and the layer-1 dot product scale. The forward pass branches on the
    width with `if constexpr (FIRST_OUT == 8)`: width-8 keeps the original int16
    `vmull`/`vaddvq_s16` kernel, while width-32 must accumulate in int32 (32 products
    of ~±8k overflow int16) and uses NEON **SDOT** (`vdotq_s32`, requires
    `-march=armv8.2-a+dotprod`). Both paths are checked bit-for-bit against the scalar
    reference by `forwardPassDebug` (the SIMD==scalar assert on every eval in debug
    builds) and, for width-32, by `NNUEU_Optim/nnueu_bench_w32.cpp`.

## Search Algorithm
The engine uses an alpha-beta search with iterative deepening as its main algorithm.
  - The main search logic is in `src/worker.cpp` and `src/worker.h`. The 4 main functions are Worker::iterativeSearch, Worker::firstMoveSearch, Worker::alphaBetaSearch and Worker::quiesenceSearch.
  - In Worker::iterativeSearch we call Worker::firstMoveSearch at increasing depths untill we either reach a time limit or a max depth limit (maximum depth is set for tests only).
  - In Worker::firstMoveSearch we order the first moves (rootMoves) based on previous lower depth searches and call alphaBetaSearch on the best moves first. There is also some depth penalization for moves with bad scores.
  - alphaBetaSearch calls itself untill depth 0 is reached, which is when we call quisenceSearch. Here are some differences between both searches:
    - In alphaBetaSearch we first have to check if the position is drawn by repetition (currentPos.isDraw()). In quiesenceSearch this doesn't have to be done since we are making always moves which cant lead to repetitions (captures and promotions).
    - In alphaBetaSearch we check in the transposition table if the position is stored already and decide weather to continue searching or not based on the information. In quisenceSearch we cant perform a transposition table lookup because we dont have the zobrist keys updated.
    - Moves in alpha-beta are generated with `ABMoveSelectorCheck` and `ABMoveSelectorNotCheck` in `src/move_selectors.h`. Moves in quiescence search are generated with `QSMoveSelectorCheck` and `QSMoveSelectorNotCheck` in `src/move_selectors.h`. As of v0.3.9, `ABMoveSelectorNotCheck` is **staged** (Stockfish `MovePicker` style): captures + queen promotions are generated/scored/sorted first, and quiets are generated lazily only if no capture caused a beta cutoff (see the profiling notes under Testing).
    - In alphaBetaSearch we know that we will at least have generated a move of the position, unless the position had no moves. If the position had no moves we know that it is either stalemate/mate depending if there is check or not in the position. In quisenceSearch we only look for captures and promotions, so even if we ended up without any captures or promotions we might still have other legal moves. To see this in the case there's a check in the position, there's a function currentPos.isMate(). This function is defined in `src/bitposition.cpp`.

  - It relies on a transposition table (`src/ttable.h`) to cache evaluations and avoid re-calculating previously seen positions, using Zobrist keys (`src/zobrist_keys.h`).
  - **Null-move pruning (v0.3.10).** In `alphaBetaSearch`, when the side to move is not in check, has non-pawn material (zugzwang guard, `BitPosition::hasNonPawnMaterial`), and the static eval is already `>= beta`, the engine passes the turn (`makeNullMove`/`unmakeNullMove` in `bitposition.cpp` — flip side-to-move + zobrist, clear en passant, no-op accumulator) and searches to reduced depth (`R = 2 + depth/6`). A fail-high is confirmed by a verification search (NMP disabled at that node via the `allowNull` flag) before pruning; consecutive null moves are disallowed and mate scores are never returned. This was the #1 lever from the profiling notes below — it prunes large quiet subtrees, giving **~10x** on the fixed-depth tactics suite and **+54.7 Elo** vs 0.3.9 over 64 games (25-24-15, 57.8 %; bullet-1+1 +44, bullet-1+3 +89, blitz-3+2 +89, blitz-5+2 +0). The cost is that very deep tactics need ~1 extra ply at fixed depth (Tactic 3: depth 12 not 11), reached far faster in real time.
  - **Late move reductions (v0.3.11).** In `alphaBetaSearch`'s not-in-check move loop, a late (`movesSearched >= 4`) quiet (`aBMoveValue == 0`) non-checking move at depth >= 3 is first searched at reduced depth (`R = 1`, or 2 for very late / deep nodes) with a **zero window**; if it beats alpha it is re-searched at full depth and window. Good move ordering (TT move, captures, killers first) makes late quiets unlikely to be best, so most reductions stick and the tree shrinks. The reduction is intentionally gentle — an aggressive log-based `R` hid a clearly-winning quiet move (Tactic 3 `b2d4`) past fixed depth 28. Combined with NMP this brings the fixed-depth tactics suite to **~1.9s** (~44x vs 0.3.9) and is worth **+38.2 Elo** vs 0.3.10 over 64 games (17-37-10, 55.5 %, positive in all four time controls; draw-heavy). Tactic 3 resolves at depth 13 (re-calibrated; the time-limited variant confirms it).
  - **Butterfly history ordering + history-aware LMR (v0.3.12).** Quiet moves used to be searched in generation order (`aBMoveValue == 0`); the only quiet-ordering signals were the TT move and killers. A butterfly history table `mainHistory[sideToMove][from][to]` (`src/worker.h`) is now updated on every quiet beta cutoff with the standard history-gravity rule — the cutting move gets a `depth²` bonus, the quiet moves tried before it (collected in `quietsTried[]`) an equal malus, each saturating towards ±16384. The staged selector `ABMoveSelectorNotCheck` takes the side-to-move sub-table and orders quiets by it (killers still rank above; captures stay a separate earlier stage). LMR is made history-aware: a clearly poor-history quiet (`< -4000`) is reduced one extra ply, a strong-history one (`> 8000`) one fewer. This is the first concrete fix for the "bushy tree" finding (LaMano ran ~25× more nodes/sec than Stockfish yet reached <½ the depth — too many quiet moves were un-prioritised). Worth **+38.2 ± 59.7 Elo** vs 0.3.11 over 64 games (19-33-12, 55.5 %; bullet-1+1 −22, bullet-1+3 +112, blitz-3+2 +89, blitz-5+2 −22), zero time losses.
  - **Transposition-table sizing fix (v0.3.13) — the TT was effectively disabled.** `setTTSize()` (`src/engine.cpp`) resized the table in *entries* but was passed the "Hash MB" number, so the default 16 "MB" allocated a **16-entry** (256-byte) table; the TT produced ~16 cutoffs in a 30 M-node search for the whole 0.3.7–0.3.12 history. It now converts MB → entry count (`ttSize * 1024² / sizeof(TTEntry)`). Found via a new search-tree comparison harness (`scripts/search_tree_compare.py`) that records Stockfish's nodes-per-depth / effective branching factor and compares LaMano's: at depth 13 the fix took **TT cutoffs ~16 → 50k–978k, nodes-to-depth 3–17× fewer, mean EBF 3.72 → 3.30** (Stockfish ≈ 2.25). The engine gained `go depth N` + per-depth `info … nodes …` + an `info string` pruning breakdown for the harness. Worth **+49.2 ± 43.4 Elo** vs 0.3.12 over 64 games (13-47-4, 57.0 %, lower bound +5.8), zero time losses. The harness also showed the remaining gap to Stockfish is **no forward pruning** (SF futility/SEE/LMP-skips ~1.5–1.9 moves/node; we skip 0), **timid LMR** (SF reduces ~35–43 % of moves, we ~3 %), and **bloated quiescence** (~52 % of our nodes vs SF's ~30 %) — the roadmap for the next versions.
  - The search is parallelized using a `ThreadPool` (`src/threadpool.h`, `src/worker.h`) to improve performance on multi-core systems; currently, only one thread is implemented, but more will be in a future version.

## User Interface (UCI)
The engine communicates via the Universal Chess Interface (UCI) protocol. The UCI protocol integration is in `src/engine.cpp` and `src/main.cpp`, allowing the engine to integrate with any UCI-compatible chess GUI.

## Testing 
There are 3 build types (Release, Debug&Verbose and Debug). When running tests it is recommended to first build the Debug type. This will run some bit utils tests and perft tests on debug mode, checking for important assertions. If it passes, then it is recommended to build the Debug&Verbose type. This will run the tactics tests, checking for important assertions and printing which moves are being performed internally in the engine, which is useful for debugging. If it passes, then we wan build the Release version and perform the tactics tests, measuring the time taken to do them. If the time taken has decreased we have a new version of TalsHand!

1) To build and test the Debug version:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_VERBOSE_DEBUG=OFF
cmake --build build
cd build && ctest --verbose

### Perft Performance Benchmarking (Nodes / Second)

To track raw move generation and search framework speed, a dedicated performance test suite is built only in the **Release** configuration. It measures the number of nodes per second (NPS) for the canonical perft FEN positions at depth 4 for:

* Standard alpha-beta move generation (`Perft performance (AB)`)
* Quiescence-only move generation (`Perft performance (QS)`)

Each test prints lines like:
```
[Perft-AB] Position 1: depth=4, nodes=197281, time=0.0421s, nps=4688540
```
Where:
* `nodes` is the total number of nodes visited at the specified depth.
* `time` is elapsed wall-clock time in seconds.
* `nps = nodes / time` (higher is better).

#### Running the performance tests
Build in Release mode:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_VERBOSE_DEBUG=OFF
cmake --build build
```
Run only the performance tests:
```
cd build
ctest -R perft_perf --verbose
```
Or run all tests (includes tactics tests):
```
cd build
ctest --verbose
```

#### Interpreting Results
Use the printed NPS to compare across commits or hardware. Significant drops may indicate regressions in:
* Move generation (`bitposition.cpp`, `move_selectors.h`)
* Transposition table probing (`ttable.h`)
* Zobrist or NNUEU incremental update overhead

Minor fluctuations (<5%) can be due to background system load. For stable baselines, run multiple times and take the median.

#### Extending
To add more benchmark depths or positions, edit `tests/test_perft_perf.cpp`. Keep depths modest (≤5) to maintain fast CI runs.
1) To build and test the Debug&Verbose version:
cmake -B build_debug_verbose -DCMAKE_BUILD_TYPE=Debug -DENABLE_VERBOSE_DEBUG=ON
cmake --build build_debug_verbose
cd build_debug_verbose && ctest --verbose
2) To build and test the Debug&Verbose version:
cmake -B build_debug -DCMAKE_BUILD_TYPE=Debug -DENABLE_VERBOSE_DEBUG=OFF
cmake --build build_debug
cd build_debug && ctest --verbose
3) To build and test the Release version:
cmake --B build_release -DCMAKE_BUILD_TYPE=Release -DENABLE_VERBOSE_DEBUG=OFF
cmake --build build_release
cd build_release && ctest --verbose

## Versioning and Releases

This project follows [Semantic Versioning](https://semver.org/). The release process is designed to be straightforward, ensuring that each version is properly tagged and documented.

1.  **Update Changelog**: Before creating a new release, update the `[Unreleased]` section in `CHANGELOG.md` with all notable changes. Once finalized, rename the section to the new version number (e.g., `[0.2.0] - YYYY-MM-DD`) and create a new `[Unreleased]` section above it.

2.  **Commit Changes**: Commit the updated `CHANGELOG.md` and any other final changes for the release.
    ```bash
    git add CHANGELOG.md
    git commit -m "docs: Prepare for release v0.1.0"
    ```

3.  **Tag the Version**: Create an annotated Git tag for the new version.
    ```bash
    git tag -a v0.1.0 -m "Version 0.1.0"
    ```

4.  **Push to GitHub**: Push your commits and the new tag to the remote repository.
    ```bash
    git push origin main --tags
    ```

5.  **Create GitHub Release**: Navigate to the "Releases" section of the GitHub repository. Draft a new release, select the tag you just pushed, and copy the release notes from `CHANGELOG.md` into the description.

### Release quality gate and progression tracking

Before bumping the version (step 1 above), the new build must be shown to be **no weaker than the previous release**, and the result is recorded so improvement can be tracked across versions. The tooling lives in `scripts/` and uses `python-chess`:

* **`scripts/version_match.py`** — plays a match between two builds (binary paths or git refs; a ref is built in a throw-away worktree). Each opening is played twice (one game with each engine as White) over bullet (1+1, 1+3) and blitz (3+2, 5+2) time controls, and reports the score and Elo ± error per time control and overall. Writes a JSON report with `--output`.
* **`scripts/release_gate.py`** — runs `version_match.py` against the previous git tag (auto-detected) and **exits non-zero unless the Elo lower bound is ≥ 0** (i.e. the new build is at least as strong). Use `--min-elo N` to demand proven improvement. This is the gate to run before tagging.
* **`scripts/collect_release_metrics.py`** — records objective per-version metrics (perft NPS, mate-puzzles solved, and optionally the match Elo via `--match <prev tag>`) into `version_test_results/<version>.json`.
* **`scripts/generate_report.py`** — turns all `version_test_results/*.json` into `version_test_results/REPORT.md` plus PNG charts under `version_test_results/charts/` (perft speed, mates solved, Elo gain per version). The Markdown report renders directly on GitHub, so no notebook is needed.

Typical release flow:
```bash
# 1. Quality gate: current build must not be weaker than the previous tag
python3 scripts/release_gate.py            # must pass before bumping the version

# 2. Record this version's metrics (including the match Elo) and refresh the report
python3 scripts/collect_release_metrics.py --match v0.3.1
python3 scripts/generate_report.py
git add version_test_results/ && git commit -m "chore: record vX.Y.Z metrics"

# 3. Proceed with the Versioning steps above (changelog, tag, push, release)
```

## TODO

### Engine strength — search & evaluation (roughly easiest × most-effective first)
These come from a Stockfish-vs-LaManodeMiguelito code comparison and the profiling session of 2026-06-01. **Every task below states what / why (with the measured evidence) / how (with `file:line` pointers) / how to verify**, so it can be picked up cold by someone who has never seen this code.

**How to validate any change.** Run a version match (`scripts/version_match.py`, gate with `scripts/release_gate.py`; reports Elo ± error). *Exception:* for a **numerically identical** change (same eval ⇒ same search tree) measure **NPS at a fixed depth** instead — an identical tree means the wall-clock delta is *pure* speed. That is exactly how the picker / table / legality experiments below were measured: create a clean-HEAD worktree (`git worktree add --detach <dir> HEAD`, so your uncommitted WIP is excluded), build two Release binaries that differ only in the one change, and run `engine.searchFixedDepth(N)` on a basket of midgame positions (the hook `tests/test_tactics.cpp` already times it). Do **not** use an isolated microbenchmark for the accumulator — a tight loop keeps tables warm in cache and misleads.

**Priority (impact × ease).** The search levers **(1) TT best-move + bound → (2) lazy/staged picker → (3) real ordering → (4) cheap legality** are the highest-ROI and are independent of the eval. The eval track **(drop fused-diff table → widen accumulator → output buckets)** is the route to a *stronger* evaluation when you want it (the eval is currently only ~12 % of search time, so it has headroom). The three bullets immediately below are the **profiling evidence** that justifies these priorities; the numbered **tasks** follow.
- **Profiling result (Apple M2, 2026-06-01, midgame Tactic 4 @ depth 10, `sample`-based self-time).** Breakdown: **move generation + ordering + legality ~50 %** (of which `ABMoveSelectorNotCheck::init_all` alone **~28 %** — it eagerly generates *all* moves, `score()`s them, and fully `sort_moves()` at every node, `src/move_selectors.cpp:147`), search control (alphaBeta/quiescence) ~25 %, make/unmake ~10 %, **NNUEU eval ~12 %** (forward pass ~6 %, accumulator update ~5.5 %), SEE ~3 %. Takeaways: (1) the engine is **move-generation-bound, not eval-bound** — accumulator micro-opts (e.g. the `firstW2Indices` table) can't move NPS; (2) **biggest single speed lever = lazy/staged move picking** in `ABMoveSelectorNotCheck`: drop the upfront full `sort_moves` (use lazy partial selection like `QSMoveSelectorNotCheck::select_legal` already does, `src/move_selectors.cpp:66`), try the TT move before generating, and generate quiets only if captures/killers don't already cut off; (3) since eval is only ~12 %, **widening the accumulator for quality is affordable** NPS-wise.
- **Cross-check vs Stockfish (same position, `sample`-profiled): the mirror image.** SF spends **~61 % in NNUE eval** and only **~19 % in move gen + picking + legality** — vs LaMano's ~12 % / ~50 %. Reasons: SF's `MovePicker` is *staged/lazy* (`Stockfish/src/movepick.cpp:213` — `MAIN_TT` returns the TT move before generating anything; captures and quiets are generated in separate stages, quiets only if nothing cut off earlier; `partial_insertion_sort`, never a full sort of every move), and its net is 3072-wide. Two side-notes from the profile: SF's single hottest search function is the 3072-wide accumulator update (~29 % — that is the cost of a wide accumulator, relevant when widening NNUEU), and it pays ~4 % on `update_accumulator_refresh_cache` (king-move Finny refresh) that **NNUEU avoids entirely** — a nice validation of the NNUEU king trick.
- **Perft isolation — is the cost the generator or the picker? (startpos depth 6 = 119 M nodes, single-thread M2).** Original perft **62 Mnps** → without the eager `score()`+`sort_moves()` in `init_all` **78 Mnps** → also without the debug-string building (`perft_recursive` builds `prefix + move.toString() + " "` even when `outfile==nullptr`, `src/engine.cpp:160`) **95 Mnps**; Stockfish `go perft` ≈ **350 Mnps**. Conclusions: (1) the **move generator is fine** — ~95 Mnps clean, only ~3.7× behind SF (headroom, not a crisis), *not* the 4.7 Mnps an old note implied; (2) the **eager picker is the real search cost** — it costs ~20 % even in perft (where ordering is useless) and ~28 % in search (where early cutoffs make front-loaded scoring+sorting mostly wasted), so going staged/lazy is the win; (3) side-finding: the **perft benchmark wastes ~22 % building debug strings** — guard the `prefix + …toString()` concatenations with `if (outfile)` so the NPS regression numbers are honest (does not affect play, since perft is not on the search path).
  - **What the 3.6× perft gap actually is (LaMano ~97 Mnps Release+LTO vs SF ~350): legality filtering, not the generators.** Ruled out by measurement: the picker (search-only), the debug strings, and even the per-node TT probe/save (disabling it changed nothing). Self-time split — **LaMano:** legality/selection (`isLegal` + `select_legal` + `newKingSquareIsSafe`) **~41 %**, per-node recursion/selector ~34 %, raw generators ~18 %, make/unmake ~5 %; **Stockfish:** `generate<>` ~69 %, make/unmake ~15 %, `set_check_info` ~6.5 %, **legality ~2.3 %**. Root cause (code-verified): SF's `generate<LEGAL>` (`Stockfish/src/movegen.cpp`) computes `pinned` once and calls the expensive `pos.legal()` **only** for pinned / king / en-passant moves — every other move is kept with one inline `pinned & from` test; LaMano runs a full `isLegal()` per move via `ABMoveSelectorNotCheck::select_legal` (`src/move_selectors.cpp:109`), which re-decodes each move and checks castling + king + pin before the (cheap) pin test, and for king moves runs `newKingSquareIsSafe` (2 magic lookups). **Note: the raw move generators are competitive — the gap is the per-move legality machinery wrapped around them.** Fix: adopt SF's pattern (skip `isLegal` for non-pinned non-king moves; only check the risky ones) — this speeds up perft *and* search, since every search node also filters legality. Implementation subtleties when adding the skip: (a) **en passant** — a non-pinned pawn's en-passant capture can still be illegal (removing the captured pawn off-destination exposes a rank/diagonal slider) and the pin test misses it; in LaMano this is already safe because `pawnAllMoves` pre-filters en passant via `kingIsSafeAfterPassant` (Bmagic+Rmagic) at generation (`src/bitposition.cpp:923`) and `pawnCapturesAndQueenProms` (QS) doesn't generate en passant at all; (b) **lazy pins in QS** — `state_info->pinnedPieces` only becomes valid after `setBlockersPinsAndCheckBitsInQS`, which runs *inside* `QSMoveSelectorNotCheck::select_legal`, so the skip must live there, not at generation (`src/bitposition.cpp:755`). Prioritise the AB `isLegal` path: the QS `isCaptureLegal` path is small (~0.5 % of search) and already lean (no castling check).
#### Tasks — move ordering & legality (top NPS levers; see the evidence above)

1. **Store the best move in the TT, and store a correct bound** — *one-line change, large gain; prerequisite for tasks 2–3.*
   - *Why:* `alphaBetaSearch` (`src/worker.cpp`) declares `best_move` but never assigns it, so internal nodes call `tt.save(..., Move(0), ...)`. The **TT move — the single most valuable ordering signal — is therefore inoperative** (only the root ever stores a real move). Separately, a fail-low (final value ≤ the original alpha) is currently saved as `BOUND_EXACT`, which is a correctness risk and silently loses cutoffs.
   - *How:* set `best_move = move` on every alpha improvement and pass it to `tt.save`. Store `BOUND_LOWER` on a beta cutoff (fail-high), `BOUND_UPPER` on fail-low (alpha never raised), `BOUND_EXACT` only when alpha was raised strictly inside `(alpha, beta)`.
   - *Verify:* version match (expect a clear Elo gain). Optionally add a "TT move present / caused cutoff" counter to confirm the signal is now live.

2. **Lazy / staged move picking in `ABMoveSelectorNotCheck`** — *the single biggest NPS lever.*
   - *Why:* `init_all` (`src/move_selectors.cpp:147`) eagerly generates **all** moves, `score()`s them and fully `sort_moves()` them at **every** node — but a well-ordered alpha-beta cuts off after 1–2 moves, so the work is proportional to *all* moves while you examine only a few. Measured: deleting `score()`+`sort_moves()` lifts perft 62→78 Mnps *even though perft ignores order*; in real search the waste is ~28 % of total time (profiling above). The QS selector is already lazy; only the AB one front-loads everything.
   - *How:* rewrite the selector as a staged state machine mirroring Stockfish's `MovePicker` (`Stockfish/src/movepick.cpp:213`): (i) emit the **TT move with no generation** (validate it with `ttMoveIsOk`, `src/bitposition.cpp:582`); (ii) generate + score **only captures**, emit the good ones (split good/bad with `see_ge`); (iii) emit killers (HEAD already tracks them); (iv) generate + score quiets **only if** no earlier stage cut off, and pick them with the lazy *find-best-remaining* loop already in `QSMoveSelectorNotCheck::select_legal` (`src/move_selectors.cpp:66`) instead of a full `sort_moves`. Depends on tasks 1 and 3.
   - *Verify:* version match **and** fixed-depth NPS (identical eval ⇒ identical tree ⇒ the time delta is the speedup).

3. **Real move ordering for the AB selectors** — *highest ROI together with tasks 1–2.*
   - *Why:* `aBMoveValue` scores every quiet move `0` (quiets are searched in generation order) and scores captures coarsely — so even with a staged picker the ordering within a stage is weak.
   - *How:* MVV-LVA (victim value × attacker value) for captures; killer moves (**already added at HEAD**, commit `perf(ordering)`) plus a butterfly/history table for quiets; later a capture-history and continuation-history table as in Stockfish.
   - *Verify:* version match.

4. **Cheap legality — skip `isLegal` for non-pinned non-king moves** — *closes most of the 3.6× perft/generator gap and speeds up every search node.*
   - *Why:* legality filtering is ~41 % of LaMano's enumeration vs ~2.3 % in Stockfish; the raw generators are competitive (full analysis, root cause, and the en-passant / lazy-pin subtleties are in the **"What the 3.6× perft gap actually is"** evidence bullet above).
   - *How:* compute pins/checkers once per node; for each generated move call the expensive `isLegal` / `isCaptureLegal` **only** when the origin is pinned, is the king, or is en passant — keep every other move with a single inline `((1ULL<<from) & pinnedPieces) == 0` test (Stockfish's `generate<LEGAL>` pattern). In QS the pin set only becomes valid inside `select_legal` (`src/bitposition.cpp:755`), so the skip must live there. En passant is already safe in LaMano (pre-filtered at generation for AB, not generated at all in QS — see the evidence bullet), so no extra handling is needed.
   - *Verify:* perft node counts must stay **identical** (correctness) while NPS rises; then a version match. Prioritise the AB `isLegal` path; the QS `isCaptureLegal` path is only ~0.5 % of search and already lean (no castling check), so it is low-ROI.
   - **✅ Implemented (v0.3.7 candidate, commits `72a5818` `isLegal` + `1198eea` `isCaptureLegal`).** Both selectors now gate the full legality call behind one inline test `((1ULL<<from) & needLegalityMask) == 0`, where `needLegalityMask = pinnedPieces | (1ULL<<kingSq)` is cached per node via `BitPosition::piecesNeedingLegalityCheck()` (`src/bitposition.h`). En passant needed no special handling (pre-filtered at generation for AB, never generated in QS). Measured on clean-HEAD worktrees (M2, depth-5 perft / depth-5 QS-walk / fixed-depth tactics, thermally interleaved; perft node counts identical, QS-Capture-Consistency green):
     - **perft-AB NPS 77.6M → 89.3M (+15 %)** — the AB move-generation path, where the win shows fully.
     - **QS-walk NPS 25.4M → 27.1M (+7 %)**; the QS `isCaptureLegal` skip itself is a further **~1 %** on the deep tactics suite over the AB-only change (98.9 s vs 100.0 s) — small because the QS path dominates deep search but each capture's legality was already cheap.
     - **deep fixed-depth tactical search ~101 s → ~99 s (~−2 %)** — diluted vs perft because at depth the time is dominated by QS captures + NNUEU eval, not interior-node legality.
     - **Still ~3.9× behind Stockfish's ~350 Mnps `go perft`** (was ~4.5× at 77.6M). Cheap legality narrowed the gap that the *"3.6× perft gap"* bullet identified, but the two larger structural gaps remain: SF's **staged/lazy picker** (task 2 — it never full-sorts every move) and its **wider eval**. The raw generators were never the problem.

#### Tasks — pruning, windows & parallelism (after ordering is in place)

- **Principal Variation Search (PVS).** The AB loop searches every move with the full `(-beta, -alpha)` window. Search the first move full, the rest with a null window `(-alpha-1, -alpha)`, and re-search at full window only on fail-high. Pays off once the ordering above is in place.
- **SEE pruning in the main search.** `see_ge` is currently only used in quiescence; also prune clearly-losing captures (and later quiets) in `alphaBetaSearch`, with a margin scaled by depth.
- **Aspiration windows.** Search the root with a narrow window around the previous iteration's score and widen on fail-high/low, instead of the current ±31000 full window.
- **Late Move Reductions (LMR) in the main search.** Only the root reduces today. Reduce late/quiet moves in `alphaBetaSearch` by an amount driven by depth and move count (later also history/improving), with a full-depth re-search on fail-high.
- **Lazy SMP multithreading.** The `ThreadPool` framework exists but only thread 0 searches. Run N worker threads sharing the global transposition table.
- **Faster NNUEU evaluation.** (Low-priority *speed* lever — the profiling note above shows eval is only ~12 % of search time and the accumulator update ~5.5 %, so this barely moves NPS; it matters mainly for *quality* if you widen the accumulator, which the small eval budget can afford.) Two big wins on the *quality/scaling* side: SIMD (NEON on Apple Silicon) for the accumulator/affine layers, and accumulator caching ("Finny tables") to avoid full accumulator refreshes on king moves.
  - **Drop the `firstW2Indices` / `firstW2IndicesInv` fused-difference tables (measured: no speed change at N=8 on M2 — remove for footprint and to unblock widening).** A non-king move fuses "remove from-square + add to-square" into one vector add by looking up a precomputed `firstW[i] - firstW[j]` in `firstW2Indices[640][640][FIRST_OUT]` (declared in `src/accumulation.h`, built in `Transformer::load`, `src/accumulation.cpp:421`, used in `addAndRemoveOnInput`, `src/accumulation.cpp:160`). The naive alternative — `addOnInput(to)` + `removeOnInput(from)`, both already implemented (`src/accumulation.cpp:209`) — touches only the ~10 KB `firstW`/`firstWInv` tables (L1-resident) instead of the ~13 MB pair. Action: replace the lookup in `addAndRemoveOnInput` with the two ops and delete the tables + their build loop.
    - **Measured (Apple M2, 2026-06-01), midgame Tactic 4 at fixed depth:** baseline (table) vs 2-op is within noise — depth-10 median **11.32 s vs 11.30 s**, depth-9 **1.495 s vs 1.501 s** (2-op marginally *slower*). Correctness: the 2-op accumulator is bit-identical (the 600 assertions in `tests/test_nnueu.cpp` pass). The cache-miss worry does **not** bite at N=8: the ~13 MB tables fit in M2's ~16 MB L2, and the accumulator delta is a negligible slice of per-node cost (movegen / make-unmake / forward pass dominate). So this is **not** an N=8 speed win — earlier "likely a pessimization" was wrong.
    - **Why remove it anyway:** (1) ~13 MB of RAM + a 3.3 M-iteration startup loop for zero measured benefit; (2) the cost is *quadratic in the width* — at N=16/32 the tables are 26/52 MB, which **exceed** L2, so the cache penalty absent at N=8 reappears, on top of a ~13 M-iteration build. Removal is a prerequisite for the widening below, not a standalone speedup. Re-verify any future change with the same controlled experiment (identical eval ⇒ identical fixed-depth tree ⇒ wall-time delta is pure accumulation cost; a tight microbenchmark keeps the table warm and misleads).
  - **Then widen the accumulator from 8 to 16/32 (the single biggest eval-strength lever).** Removing the tables is what unblocks this: they are the only piece whose cost is *quadratic* in the width `N` (memory `640^2 * N * 2` bytes, build `640^2 * N` iterations — ~52 MB / ~13 M iters at N=32), versus `firstW`, which is linear and stays cache-friendly (~40 KB at N=32). Remaining work is mechanical, not architectural: (1) retrain the model with `N` first-layer outputs (the CSVs in `models/` are shaped for N=8); (2) de-hardcode the width-8 assumptions — `AccumulatorState::inputTurn[2][8]`, the `std::memcpy(..., 16)` in `applyIncrementalChanges` (`src/accumulation.cpp:387`, 16 bytes = 8 int16), and the single-register `add_8_int16` / `substract_8_int16` (`src/accumulation.cpp:169`, need `N/8` registers); (3) generalize `Network::forwardPass` (`src/network.cpp:173`), which assumes an 8-wide accumulator throughout (the king-bucketed second layer becomes `N→4`), plus the array dims in the CSV loaders.
  - **Then add output buckets by piece count — the efficient form of "one net per game phase" (supersedes the "build three NNUEUs" idea).** *Why:* specialising the eval by phase does help, but training three *separate* NNUEUs triplicates the expensive incrementally-updated accumulator and switches on a hand-drawn phase boundary. Stockfish instead keeps **one shared feature transformer** and replicates only the tiny head into 8 "layer stacks" (`LayerStacks = 8`, `Stockfish/src/nnue/nnue_architecture.h`) selected by piece count — `bucket = (popcount(all_pieces) - 1) / 4` ∈ 0..7 — so the costly part is computed once, only the cheap readout differs, and the phase boundary is smooth. *How for NNUEU:* keep the shared, king-free `640→N` accumulator; make the head **both** king-bucketed and piece-count-bucketed, i.e. `second1[bucket][kingSq]`, `thirdW[bucket]`, `finalW[bucket]`, indexed by piece count at evaluate time (`src/network.cpp`). The head is tiny, so ×8 storage is negligible. Train it as **one** network end-to-end: a single shared first layer (receives gradient from *all* positions → learns a common representation) plus 8 heads (each receives gradient only from its bucket's positions → each specialises), with a fixed, non-learned gate — mechanically identical to how your king blocks are already trained, just along a second axis. *Caveat:* with a narrow accumulator all heads read the same small summary, so do this **after** widening (task above), or the buckets have little to specialise on. *Verify:* version match.

### Other
- Tune the quiescence SEE-pruning margin. QS currently prunes captures with `see_ge(capture, -120)` in `worker.cpp` (it only skips clearly-losing captures). Experiment with a tighter, position-aware threshold — e.g. `beta + 100`, which in a quick test made the depth-5 Tactic 2 study find the winning `c6c7` and netted +1 on the depth-5 tactics suite — and consider making it depth- or phase-dependent. Validate any change with a version match (Elo): aggressive QS pruning can help tactics in some positions while missing them in others (e.g. winning positions where `beta` is large).
- Create specific tests for Zobrist key generation (e.g., for transpositions and move/unmove symmetry).
- Add a process for creating regression tests for any fixed bugs.
- Try to see if including zobrist key updates and ttable lookup in quiesence is worth it.
- Build three nnueu's, one for openings, one for middle game and another for endgames. → **Superseded** by the **output-buckets** task in the eval track above (one shared accumulator + per-piece-count heads is the same idea, but far cheaper and trained as a single network).
- Mate distance pruning
- If not in check we can perform a static evaluation of the position (enables futility / reverse-futility / null-move pruning).
- Build an ss to save search tree information
- **Move-generator speed vs Stockfish (analysed 2026-06-01).** Clean perft ≈ 97 Mnps vs SF ≈ 350 (3.6×); the gap is **legality filtering, not the generators** — see task 4 and the "What the 3.6× perft gap actually is" evidence bullet. The raw move generators are competitive; do not rewrite them before doing task 4.
- **Fix the perft benchmark's debug-string waste** (benchmark accuracy, not playing strength). `perft_recursive` (`src/engine.cpp:160`) builds `prefix + move.toString() + " "` at every node *even when `outfile == nullptr`* (counting mode) — measured ~22 % of perft time burned on string growth + malloc/free (`to_string`, `__grow_by_and_replace`, `_platform_memmove`, `free_tiny`). Guard each `prefix + …toString()` concatenation with `if (outfile)` (pass `std::string()` otherwise). This does not touch the search path, but makes the perft NPS regression numbers honest.