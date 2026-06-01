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

## Search Algorithm
The engine uses an alpha-beta search with iterative deepening as its main algorithm.
  - The main search logic is in `src/worker.cpp` and `src/worker.h`. The 4 main functions are Worker::iterativeSearch, Worker::firstMoveSearch, Worker::alphaBetaSearch and Worker::quiesenceSearch.
  - In Worker::iterativeSearch we call Worker::firstMoveSearch at increasing depths untill we either reach a time limit or a max depth limit (maximum depth is set for tests only).
  - In Worker::firstMoveSearch we order the first moves (rootMoves) based on previous lower depth searches and call alphaBetaSearch on the best moves first. There is also some depth penalization for moves with bad scores.
  - alphaBetaSearch calls itself untill depth 0 is reached, which is when we call quisenceSearch. Here are some differences between both searches:
    - In alphaBetaSearch we first have to check if the position is drawn by repetition (currentPos.isDraw()). In quiesenceSearch this doesn't have to be done since we are making always moves which cant lead to repetitions (captures and promotions).
    - In alphaBetaSearch we check in the transposition table if the position is stored already and decide weather to continue searching or not based on the information. In quisenceSearch we cant perform a transposition table lookup because we dont have the zobrist keys updated.
    - Moves in alpha-beta are generated with `ABMoveSelectorCheck` and `ABMoveSelectorNotCheck` in `src/move_selectors.h`. Moves in quiescence search are generated with `QSMoveSelectorCheck` and `QSMoveSelectorNotCheck` in `src/move_selectors.h`.
    - In alphaBetaSearch we know that we will at least have generated a move of the position, unless the position had no moves. If the position had no moves we know that it is either stalemate/mate depending if there is check or not in the position. In quisenceSearch we only look for captures and promotions, so even if we ended up without any captures or promotions we might still have other legal moves. To see this in the case there's a check in the position, there's a function currentPos.isMate(). This function is defined in `src/bitposition.cpp`.

  - It relies on a transposition table (`src/ttable.h`) to cache evaluations and avoid re-calculating previously seen positions, using Zobrist keys (`src/zobrist_keys.h`).
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
These come from a Stockfish-vs-LaManodeMiguelito code comparison. Validate each with a version match (Elo).
- **Profiling result (Apple M2, 2026-06-01, midgame Tactic 4 @ depth 10, `sample`-based self-time).** Breakdown: **move generation + ordering + legality ~50 %** (of which `ABMoveSelectorNotCheck::init_all` alone **~28 %** — it eagerly generates *all* moves, `score()`s them, and fully `sort_moves()` at every node, `src/move_selectors.cpp:147`), search control (alphaBeta/quiescence) ~25 %, make/unmake ~10 %, **NNUEU eval ~12 %** (forward pass ~6 %, accumulator update ~5.5 %), SEE ~3 %. Takeaways: (1) the engine is **move-generation-bound, not eval-bound** — accumulator micro-opts (e.g. the `firstW2Indices` table) can't move NPS; (2) **biggest single speed lever = lazy/staged move picking** in `ABMoveSelectorNotCheck`: drop the upfront full `sort_moves` (use lazy partial selection like `QSMoveSelectorNotCheck::select_legal` already does, `src/move_selectors.cpp:66`), try the TT move before generating, and generate quiets only if captures/killers don't already cut off; (3) since eval is only ~12 %, **widening the accumulator for quality is affordable** NPS-wise.
- **Cross-check vs Stockfish (same position, `sample`-profiled): the mirror image.** SF spends **~61 % in NNUE eval** and only **~19 % in move gen + picking + legality** — vs LaMano's ~12 % / ~50 %. Reasons: SF's `MovePicker` is *staged/lazy* (`Stockfish/src/movepick.cpp:213` — `MAIN_TT` returns the TT move before generating anything; captures and quiets are generated in separate stages, quiets only if nothing cut off earlier; `partial_insertion_sort`, never a full sort of every move), and its net is 3072-wide. Two side-notes from the profile: SF's single hottest search function is the 3072-wide accumulator update (~29 % — that is the cost of a wide accumulator, relevant when widening NNUEU), and it pays ~4 % on `update_accumulator_refresh_cache` (king-move Finny refresh) that **NNUEU avoids entirely** — a nice validation of the NNUEU king trick.
- **Perft isolation — is the cost the generator or the picker? (startpos depth 6 = 119 M nodes, single-thread M2).** Original perft **62 Mnps** → without the eager `score()`+`sort_moves()` in `init_all` **78 Mnps** → also without the debug-string building (`perft_recursive` builds `prefix + move.toString() + " "` even when `outfile==nullptr`, `src/engine.cpp:160`) **95 Mnps**; Stockfish `go perft` ≈ **350 Mnps**. Conclusions: (1) the **move generator is fine** — ~95 Mnps clean, only ~3.7× behind SF (headroom, not a crisis), *not* the 4.7 Mnps an old note implied; (2) the **eager picker is the real search cost** — it costs ~20 % even in perft (where ordering is useless) and ~28 % in search (where early cutoffs make front-loaded scoring+sorting mostly wasted), so going staged/lazy is the win; (3) side-finding: the **perft benchmark wastes ~22 % building debug strings** — guard the `prefix + …toString()` concatenations with `if (outfile)` so the NPS regression numbers are honest (does not affect play, since perft is not on the search path).
  - **What the 3.6× perft gap actually is (LaMano ~97 Mnps Release+LTO vs SF ~350): legality filtering, not the generators.** Ruled out by measurement: the picker (search-only), the debug strings, and even the per-node TT probe/save (disabling it changed nothing). Self-time split — **LaMano:** legality/selection (`isLegal` + `select_legal` + `newKingSquareIsSafe`) **~41 %**, per-node recursion/selector ~34 %, raw generators ~18 %, make/unmake ~5 %; **Stockfish:** `generate<>` ~69 %, make/unmake ~15 %, `set_check_info` ~6.5 %, **legality ~2.3 %**. Root cause (code-verified): SF's `generate<LEGAL>` (`Stockfish/src/movegen.cpp`) computes `pinned` once and calls the expensive `pos.legal()` **only** for pinned / king / en-passant moves — every other move is kept with one inline `pinned & from` test; LaMano runs a full `isLegal()` per move via `ABMoveSelectorNotCheck::select_legal` (`src/move_selectors.cpp:109`), which re-decodes each move and checks castling + king + pin before the (cheap) pin test, and for king moves runs `newKingSquareIsSafe` (2 magic lookups). **Note: the raw move generators are competitive — the gap is the per-move legality machinery wrapped around them.** Fix: adopt SF's pattern (skip `isLegal` for non-pinned non-king moves; only check the risky ones) — this speeds up perft *and* search, since every search node also filters legality.
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

### Other
- Tune the quiescence SEE-pruning margin. QS currently prunes captures with `see_ge(capture, -120)` in `worker.cpp` (it only skips clearly-losing captures). Experiment with a tighter, position-aware threshold — e.g. `beta + 100`, which in a quick test made the depth-5 Tactic 2 study find the winning `c6c7` and netted +1 on the depth-5 tactics suite — and consider making it depth- or phase-dependent. Validate any change with a version match (Elo): aggressive QS pruning can help tactics in some positions while missing them in others (e.g. winning positions where `beta` is large).
- Create specific tests for Zobrist key generation (e.g., for transpositions and move/unmove symmetry).
- Add a process for creating regression tests for any fixed bugs.
- Try to see if including zobrist key updates and ttable lookup in quiesence is worth it.
- Build three nnueu's, one for openings, one for middle game and another for endgames.
- Mate distance pruning
- If not in check we can perform a static evaluation of the position (enables futility / reverse-futility / null-move pruning).
- Build an ss to save search tree information
- Try to improve the move generator and see if we can reach stockfish's nodes per second.