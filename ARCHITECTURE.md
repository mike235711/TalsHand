# Architecture and Key Components

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
2) To build and test the Debug&Verbose version:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_VERBOSE_DEBUG=ON
cmake --build build
cd build && ctest --verbose
3) To build and test the Release version:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_VERBOSE_DEBUG=OFF
cmake --build build
cd build && ctest --verbose

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

## TODO
- We have to include some prediction of the number of moves left in the game so that engine doesn't overthink at the start of the game.
- Build tests for the nnueu loading and accumulation
- Include the final test for version release which is to play several games on different positions and time controls against last oldest version and see the score.
- Simplify castling rights logic in makeMove
- Simplify further `src/bitposition.cpp` code with m_turn logic
- Try to see if including zobrist key updates and ttable lookup in quiesence is worth it.
- Mate distance pruning
- If not in check we can perform a static evaluation of the position
- Build an ss to save search tree information
- MultiThreading