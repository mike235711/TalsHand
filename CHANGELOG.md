# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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