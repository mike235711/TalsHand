# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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