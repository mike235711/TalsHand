# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.3.13] - 2026-06-19

Search-strength release: **transposition-table sizing fix** (the TT was effectively
disabled) plus a search-tree comparison harness. Same evaluation net as 0.3.8–0.3.12
(`w32_wdl0`) — a pure search change.

### Fixed
- **Transposition table was allocating 16 *entries*, not 16 MB.** `setTTSize()` sized the
  table in entries but was fed the "Hash MB" number, so the default 16 "MB" produced a
  16-entry (256-byte) table. The TT was effectively off for the entire 0.3.7–0.3.12 history
  (~16 cutoffs in a 30 M-node search). `setTTSize()` now converts MB → entry count. Measured
  at depth 13: TT cutoffs **~16 → 50k–978k**, nodes-to-depth **3–17× fewer**, mean effective
  branching factor **3.72 → 3.30** (Stockfish ≈ 2.25).

### Added
- **Search-tree comparison harness** (`scripts/search_tree_compare.py` +
  `version_test_results/sf_search_tree_ref.json`): records Stockfish's nodes-per-depth / EBF
  and compares LaMano's. The engine now supports `go depth N` (fixed-depth, no early-stop)
  and prints per-depth `info depth … nodes …` plus an end-of-search `info string … betafirst …`
  pruning breakdown (ttcut/nmp/lmr/beta counters). Fixed-depth searches clear the TT per call
  so tactic tests are reproducible. Includes a patch instrumenting Stockfish with the same
  counter set (`version_test_results/stockfish_instrumentation.patch`).

### Result
- **+49.2 ± 43.4 Elo** vs 0.3.12 over 64 games (13-47-4, 57.0 %; lower bound +5.8), zero time
  losses. Per TC: bullet-1+1 +112, bullet-1+3 −22, blitz-3+2 +112, blitz-5+2 ±0.

## [0.3.12] - 2026-06-18

Search-strength release: butterfly history move ordering + history-aware LMR.
Same evaluation net as 0.3.8–0.3.11 (`w32_wdl0`) — a pure search change on top
of LMR (0.3.11).

### Added
- **Butterfly history heuristic** for quiet-move ordering. A
  `mainHistory[sideToMove][from][to]` table is updated on every quiet beta cutoff
  with the standard history-gravity rule (the cutting move gets a `depth²` bonus,
  the quiet moves tried before it an equal malus, saturating towards ±16384). The
  staged AB move selector (`ABMoveSelectorNotCheck`) now orders quiets by this
  table — killers still rank above, captures remain a separate earlier stage —
  replacing the old generation-order fallback. This is the first concrete fix for
  the "bushy tree" finding: LaMano searched ~25× more nodes/sec than Stockfish yet
  reached less than half the depth, i.e. its move ordering left too many quiet
  moves un-prioritised.
- **History-aware LMR.** The late-move reduction is nudged by the move's history
  score: a clearly poor-history quiet (`< -4000`) is reduced one extra ply, a
  strong-history one (`> 8000`) one fewer.

### Result
- **+38.2 ± 59.7 Elo** vs 0.3.11 over 64 games (19-33-12, 55.5 %), zero time
  losses on either side. Per time control: bullet-1+1 −21.7, bullet-1+3 +112.3,
  blitz-3+2 +88.7, blitz-5+2 −21.7. Correctness gate green (nnueu eval, mate,
  repetition, 14/14 tactics).

## [0.3.11] - 2026-06-18

Search-strength release: late move reductions (LMR). Same evaluation net as
0.3.8–0.3.10 (`w32_wdl0`) — a pure search change on top of null-move pruning.

### Added
- **Late move reductions** (`alphaBetaSearch`). In the not-in-check move loop, late
  (`movesSearched >= 4`) quiet (`aBMoveValue == 0`) non-checking moves at depth >= 3
  are first searched at reduced depth with a **zero window** (R = 1, or 2 for very
  late / deep nodes); if the reduced search beats alpha the move is **re-searched at
  full depth and window**. The reduction is deliberately conservative — an aggressive
  log-based R left a clearly-winning quiet move (Tactic 3 `b2d4`) unfindable even at
  fixed depth 28, so a gentle reduction is used instead.
  - Combined with NMP (0.3.10): fixed-depth tactics suite **~1.9s** (was ~85s at
    0.3.9, ~8s at 0.3.10) — ~44x faster than 0.3.9.
  - Worth **+38.2 Elo** over 0.3.10 in a 64-game match with the **same** net
    (17-37-10, 55.5 %), positive in **all four** time controls: bullet-1+1 +22,
    bullet-1+3 +44, blitz-3+2 +66, blitz-5+2 +22. (Draw-heavy — 37 of 64 — as
    expected for an increment layered on an already-strong NMP engine.)
  - Deep Tactic 3 resolves at depth 13 (NMP needed 12); the fixed-depth test was
    re-calibrated and the time-limited variant confirms `b2d4` under real time
    control. All 30 correctness tests pass.

## [0.3.10] - 2026-06-17

Search-strength release: null-move pruning. Same evaluation net as 0.3.8/0.3.9
(`w32_wdl0`) — a pure search change that lets the engine reach much greater depth
in the same time.

### Added
- **Null-move pruning** (`alphaBetaSearch`). When the side to move is not in
  check, has non-pawn material (zugzwang guard), and the static eval is already
  `>= beta`, the opponent is given a free move and the position is searched to
  reduced depth (`R = 2 + depth/6`); if that still fails high, a **verification
  search** (NMP disabled at that node) confirms it before pruning.
  `makeNullMove`/`unmakeNullMove` pass the turn (flip side-to-move + zobrist key,
  clear en passant, no-op accumulator change). Consecutive null moves are
  disallowed and mate scores are never returned from a prune.
  - **~10x faster** on the fixed-depth tactics suite — it prunes large quiet
    subtrees, so far fewer nodes per depth (the #1 lever from the v0.3.9 profile).
  - Worth **+54.7 Elo** over 0.3.9 in a 64-game match with the **same** net
    (25-24-15, 57.8 %), positive in three of four time controls: bullet-1+1 **+44**,
    bullet-1+3 **+89**, blitz-3+2 **+89**; blitz-5+2 even (+0).
  - Deep Tactic 3 (a quiet win, `b2d4`, vs a materially-up defender) now resolves
    at depth 12 instead of 11 — the reduced-depth null search briefly hides White's
    mating attack; the fixed-depth regression test was re-calibrated to depth 12,
    and the time-limited variant confirms it under real time control. All 30
    correctness tests pass.

## [0.3.9] - 2026-06-15

Search-speed release: the staged move picker, now the default. Same evaluation net
as 0.3.8 (`w32_wdl0`); this is a pure search-efficiency change.

### Changed
- **Staged move picker** (`ABMoveSelectorNotCheck`) — captures + queen promotions are
  generated/scored/sorted first; the quiets are generated, scored and sorted lazily
  **only if no capture produced a beta cutoff** (Stockfish `MovePicker` style). A node
  that cuts off on a capture — very common in tactical search — never generates/scores/
  sorts its ~30+ quiet moves. Profiling 0.3.8 showed eager move generation
  (`init_all`, 18.5 %) was the single largest hotspot (move-gen ≈ 34 % of search time
  vs Stockfish's ≈ 5 %); this targets exactly that.
  - **~26 % faster** on the fixed-depth tactics suite (115 s → 85 s, order-independent
    across thermal interleaving), perft node counts identical, all 32 tests pass.
  - Worth **≈ +55 Elo** over 0.3.8 in a 64-game match with the **same** net
    (23-28-13, 57.8 %), positive in three of four time controls:
    bullet-1+1 **+160** (71.9 %), blitz-3+2 +44, blitz-5+2 +66; bullet-1+3 −44 (a noisy
    outlier, ±153). The gain scales with node-starvation, so it is largest at 1+1.
  - This change was Elo-neutral when measured on the older width-8 net (50.0 % vs
    0.3.6) — the speed gain only became visible against the slower, more
    node-starved width-32 net of 0.3.8.

## [0.3.8] - 2026-06-11

NNUEU evaluation upgrade: a wider network (width-32 accumulator, `w32_wdl0`) plus a
null-move forfeit fix. `w32_wdl0` becomes the new evaluation baseline, replacing the
width-8 `v4` net.

### Added
- **Width-32 NNUEU** (`w32_wdl0`) — the first hidden accumulator is widened from 8 to
  32 neurons, giving the evaluation more capacity (layers 2/3 are unchanged). The
  accumulator width is now a build-time switch: `NNUEU_FIRST_OUT` (default 32; CMake
  `-DNNUEU_FIRST_OUT=8` rebuilds the legacy width-8 net), and both the default net
  path and the SIMD kernel follow it. The net is trained on quiet **and** non-quiet
  positions (WDL-blended target). Worth **+54.7 ± 61 Elo** over the 0.3.7 net in a
  64-game rolling-baseline match (21-32-11, 57.8 %, positive in all four time
  controls, 0 time losses), strongest at the longer controls (blitz-3+2 +112,
  blitz-5+2 +66) where the stronger eval has time to pay off.

### Fixed
- **Move(0) forfeit** — `iterativeSearch` never seeded `bestRootMove` in the
  multi-move branch; it relied on `firstMoveSearch` completing to set it. If the hard
  time limit fired first (e.g. the slower width-32 net in bullet), the engine returned
  the null move `a1a1` and forfeited the game. `bestRootMove` is now seeded with the
  first root move up-front, so an early timeout always returns a legal move.

### Performance
- The width-32 layer-1 dot product accumulates in int32: the width-8 int16
  `vmull`/`vaddvq_s16` path overflows over 32 products (~±8k each), so the wide kernel
  uses NEON SDOT (`vdotq_s32`, requires `-march=armv8.2-a+dotprod`). Validated against
  a scalar int32 reference (bit-identical over 100k random inputs, ~3× faster than
  scalar in the NNUEU_Optim micro-bench) and in-engine via the `forwardPassDebug`
  SIMD==scalar assertion on every evaluation in debug builds. The width-8 path is
  unchanged (`if constexpr (FIRST_OUT == 8)`), so width-8 0.3.8 builds remain
  bit-identical to 0.3.7.

## [0.3.7] - 2026-06-08

Correctness + search-speed release: cheap legality filtering, and a repetition-counter
fix that makes the engine actually claim/avoid perpetual-check draws.

### Fixed
- Threefold repetition involving king moves — `reversibleMovesMade` (the window
  `isDraw` scans) was not incremented on normal king moves, only on other pieces.
  A perpetual-check repetition (queen-check + king-shuffle) therefore reached the
  3rd occurrence with the counter still below `isDraw`'s `< 8` guard, so the engine
  failed to claim it: it would play on out of a drawn-by-repetition position and
  lose, or could miss avoiding one. King moves now increment the counter like every
  other reversible move (the Zobrist key already encodes castling-rights changes, so
  no false matches). New `tests/test_repetition.cpp` guards both directions (a winning
  side must avoid the draw; a losing side must force the perpetual).

### Changed
- Cheap legality — in a non-check node a pseudo-legal move is illegal only if its
  origin is the king (king-safety / castling) or a pinned piece, so the AB and QS
  selectors gate the full `isLegal` / `isCaptureLegal` behind one inline
  `((1ULL<<from) & (pinnedPieces | kingBB)) == 0` test (en passant is pre-filtered at
  generation). Aggregate perft NPS **+15 %** (77.6M → 89.3M at depth 5); the search
  tree is unchanged (perft node counts identical across all 6 positions). Elo-neutral
  but not weaker over a 64-game match vs 0.3.6 (a 64-game match cannot resolve an
  NPS-only gain). Still ~3.9× behind Stockfish on raw `go perft` (was ~4.5×).

## [0.3.6] - 2026-06-02

Search-strength release: real move ordering at interior nodes. Quiet moves are now
ordered with a killer-move heuristic and captures with MVV-LVA, on top of the TT
move. Worth roughly **+77 Elo** over 0.3.5 in a 64-game match (60.9%, positive in
all 4 time controls), with zero time losses.

### Added
- MVV-LVA capture ordering — captures are scored by victim value minus a small
  attacker term (`CAPTURE_SCORE + MVV_LVA_VALUE[victim]*16 - MVV_LVA_VALUE[attacker]`),
  so a pawn-takes-queen is tried before queen-takes-pawn. En-passant and castling
  are handled explicitly. The per-move ordering score was widened from `int8_t` to
  `int32_t` to hold the new score bands (captures > castling > quiets).
- Killer-move heuristic — two quiet moves per ply that produced a beta cutoff are
  remembered (`killers[ply][2]`) and ordered just below captures and above the
  remaining quiets, so a refutation found in one sibling is tried early in the next.

### Notes
- A continuation-history experiment (search stack + `[prevPiece][prevTo][curPiece][curTo]`
  table with bonus/malus) was tested separately and did **not** show a net gain over
  this release (−11 ± 57 Elo, 64 games): it loses badly at bullet-1+1 (−89, where the
  590 KB table's overhead dominates ~1 s/move) and is mildly positive at intermediate
  controls. It is kept on the `history-experiment` branch for future work (overhead
  reduction / time-gated activation), not merged. See `version_test_results/`.

## [0.3.5] - 2026-06-01

Search-strength release: the transposition table now actually drives move ordering
and stores correct bounds. Worth roughly **+38 Elo** over 0.3.4 in a 64-game match
(55.5%, positive in 3 of the 4 time controls), with zero time losses.

### Fixed
- Transposition table — `alphaBetaSearch` never assigned `best_move`, so internal nodes stored `Move(0)` and TT-move ordering (the single most valuable move-ordering signal) was inoperative; only the root stored a real move. The best move is now recorded on each improvement. The single `isExact` flag is replaced by a 3-valued bound (exact / lower / upper): a fail-low (value ≤ the original alpha) was previously stored as "exact", which is incorrect — it is now an *upper* bound, and the probe returns on exact, lower ≥ beta, or upper ≤ alpha. The full 64-bit Zobrist key is stored and matched on probe, so the recovered TT move is always legal for the same position.

## [0.3.4] - 2026-05-31

Correctness and codebase-cleanup release. Strength-neutral vs 0.3.3 (≈51.6% over a
32-game bullet match, within noise). Fixes a quiescence in-check move-generation bug,
replaces the QS perft test with a lighter QS-capture-consistency check (removing a
large amount of test-only move-generation code), and adds architecture-specific
Release build flags.

### Fixed
- `inCheckOrderedCaptures`: removed a stale-pin pre-filter on the knight captures (`& ~state_info->pinnedPieces`). In quiescence search the pins are computed lazily in `QSMoveSelectorCheck::select_legal()`, i.e. *after* `init()` has already generated the captures, so `pinnedPieces` was stale when `inCheckOrderedCaptures` read it and a legal knight capture of the checker could be wrongly dropped (the engine could miss capturing the checking piece with a knight in QS). This mirrors the same fix applied to `knightCaptures` in 0.3.2; pinned knights are still filtered correctly by `isCaptureLegal`. Surfaced by the new QS-capture-consistency test (below) at depth ≥ 5.

### Changed
- Replaced the QS perft test with a direct QS-capture-vs-AB-legal consistency check (`THEngine::qsCaptureConsistencyTest`): it walks the alpha-beta legal-move tree and asserts that, at every node, the quiescence capture selectors emit exactly the AB-legal moves that are captures or queen promotions (`BitPosition::isQSCaptureOrQueenProm`). This removed the test-only non-capture QS generators and selectors (`QSMoveSelector{NotCheck,Check}NonCaptures`, `pawn/knight/bishop/rook/queen/kingNonCaptures`, `kingNonCapturesInCheck`, `inCheckPawnBlocksNonQueenProms`, `inCheckPawnCapturesNonQueenProms`, `inCheckPassantCaptures`) plus the quiescent branch of the perft driver and the dead `tests.h`, shrinking the codebase. UCI move parsing (`findMoveFromString`) now enumerates moves with the AB selectors, which also fixes parsing of under-promotions while in check.
- `setBlockersPinsAndCheckBitsInQS` / `setBlockersAndPinsInAB`: discovered-check blockers are now restricted to own pieces (`& m_bitboard_by_color[not m_turn]`), a semantic cleanup (behaviourally inert: the only readers test own-piece origins).
- Release builds add `-funroll-loops` and, on arm64, `-march=armv8.2-a+dotprod`.

## [0.3.3] - 2026-05-31

Bug-fix release: corrects two strength/stability regressions introduced by the
2D→1D `m_pieces` refactor that shipped in 0.3.2. The refactor's ~8x move-generation
speedup is kept; with these fixes the engine plays on par with 0.3.1 (no longer
weaker) and no longer crashes in real games.

### Fixed
- `isDiscoverCheck`: discovered checks were missed on king moves. The 2D→1D refactor rewrote `m_pieces[m_turn][5]` (a clean enemy-king bitboard) as `m_pieces[5] & m_bitboard_by_color[m_turn]`, which during a king move (called before the colour bitboards are consistent) could include the just-moved king and miss the discovered check. A wrong `isCheck` then drove the in-check quiescence path to an out-of-bounds read (`m_check_square` left at 65), causing SIGSEGV in real games, and also weakened play. Now uses the enemy king position `m_king_position[m_turn]`.
- `see_ge` (Static Exchange Evaluation): operator-precedence bug in the attacker computation. `BmagicNOMASK(sq, occ) & m_pieces[2] | m_pieces[4]` parses as `(... & m_pieces[2]) | m_pieces[4]`, treating *every* queen on the board as an attacker of every square (same on the rook line). This corrupted SEE, which quiescence uses to prune captures, weakening tactical play. Parenthesized: `... & (m_pieces[2] | m_pieces[4])`.

### Added
- Tests: NNUEU loading/accumulation, "Mate in X" puzzles, and UCI protocol handshake.
- Release tooling under `scripts/`: a version-match harness (Elo and time-management metrics vs a previous build), a release gate, per-version metric collection, and a Markdown progression report under `version_test_results/`.

## [0.3.2] - 2026-05-29

### Fixed
- `isQueenCheck`: fixed an operator-precedence bug (`a & b & c != 0` was parsed as `a & b & (c != 0)`) that prevented detection of direct checks from queen promotions and tripped the `isCheck` assertions in `makeCapture`.
- `knightCaptures`: removed the stale `pinnedPieces` pre-filter. In quiescence search the pins are computed lazily after the captures are generated, so legal knight captures could be incorrectly dropped. Pinned knights are still filtered by `isCaptureLegal`.
- `kingNonCaptures` (test-only QS path): castling is now only generated when the relevant rook is still on its corner square, preventing an illegal castle in QS perft (where `makeCapture` does not maintain castling rights) that corrupted the bitboards and caused a depth-6 crash.

## [0.3.1] - 2025-10-25

### Fixed
- Improved transposition table cutoff logic at non-PV nodes.


## [0.3.0] - 2025-10-24

### Changed
- Simplified and improved time management logic to prevent time extensions.
- Simplified `firstMoveSearch` and `iterativeSearch` functions.

### Added
- Added time-limited tests for release builds.
- Added methods to count pieces on the board.

### Fixed
- We check for time after depth is finished, so that unfinished searches dont corrupt result.
- The engine now immediately plays a move when a mate is found.


## [0.2.0] - 2025-10-22

### Changed
- Reworked time management to use a `softTimeLimit` and `hardTimeLimit` system, allowing for more dynamic time allocation during search.
- Simplified the `firstMoveSearch` logic.
- Refactored quiescence search move generation to correctly use pinning information.

### Removed
- Removed the unused `MAX_TACTICS_DEPTH` compile definition.

### Added
- Added the ability to save data from tactics tests for version comparison.

## [0.1.0] - 2023-10-27

### Added
- Initial project structure with CMake build system.
- Core engine components: Bitboard representation, move generation, alpha-beta search, quiscence search and transposition table implementation.
- Custom NNUEU implementation for fast and efficient position evaluation.
- UCI protocol support for integration with standard chess GUIs.
- Multi-stage testing process (Debug, Debug&Verbose, Release) using Catch2.
- Architectural documentation (`ARCHITECTURE.md`) and project overview (`README.md`).