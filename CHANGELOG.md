# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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