#include "worker.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <utility>

#include "accumulation.h"
#include "engine.h"
#include "move_selectors.h"
#include "ttable.h"

//  Constructor (only *definition* lives here; prototype in header)
Worker::Worker(TranspositionTable &ttable,
               ThreadPool &threadpool,
               NNUEU::Network networkIn,
               const NNUEU::Transformer &transformerIn,
               size_t idx)
    : lastFirstMoveTimeTakenMS(0),
      timeForMoveMS(0),
      timeLimit(0),
      ponder(false),
      isEndgame(false),
      completedDepth(0),
      threadIdx(idx),
      threads(threadpool),
      tt(ttable),
      network(std::move(networkIn)),
      transformer(&transformerIn)
{
    // rootPos / rootState are filled just before the search starts
}

//  startSearching – wrapper around iterative deepening
std::pair<Move, int16_t> Worker::startSearching()
{
    auto result = iterativeSearch(); // result = {bestMove, score}

    // only thread-0 is the “main” UCI thread → tell the GUI our move
    if (isMainThread())
    {
        std::cout << "bestmove "
                  << result.first.toString() // e.g. e2e4, e7e8q …
                  << '\n'                    // newline required by protocol
                  << std::flush;             // be sure it reaches the GUI
    }
    return result;
}

bool Worker::stopSearch(const std::vector<int16_t> &values, int streak, int depth, BitPosition &position)
{
    // If not endgame
    if (not isEndgame)
    {
        if (streak > 8 && depth > 9)
            return true;
        // Check if the move's score has just increased over time
        for (std::size_t i = 1; i < values.size(); ++i)
        {
            if (values[i] <= values[i - 1])
                return false;
        }

        if (streak > 7 && depth > 8)
            return true;
    }
    // If endgame
    else
    {
        if (streak > 11 && depth > 12)
            return true;
        // Check if the move's score has just increased over time
        for (std::size_t i = 1; i < values.size(); ++i)
        {
            if (values[i] <= values[i - 1])
                return false;
        }

        if (streak > 10 && depth > 11)
            return true;
    }
    return false;
}

int16_t Worker::quiesenceSearch(int16_t alpha, int16_t beta, bool our_turn)
// This search is done when depth is less than or equal to 0 and considers only captures and promotions
{
    // If we are in quiescence, we have a baseline evaluation as if no captures happened
    // Stand pat
    int16_t value = network.evaluate(currentPos, our_turn, accumulatorStack, *transformer);

    if (value >= beta)
        return value; // beta-cutoff

    alpha = std::max(alpha, value);

    int16_t child_value;
    Move best_move;
    bool no_captures = true;
    StateInfo state_info;
    NNUEU::NNUEUChange nnueuChange;

    if (not currentPos.getIsCheck()) // Not in check
    {
        Move capture;
        QSMoveSelectorNotCheck move_selector(currentPos);
        move_selector.init();
        while ((capture = move_selector.select_legal()) != Move(0))
        {
            no_captures = false;
            // Do not search moves with bad enough SEE values
            if (!currentPos.see_ge(capture, -120))
                continue;

            nnueuChange = currentPos.makeCapture(capture, state_info);
            accumulatorStack.push(nnueuChange);
            if (our_turn) // Maximize
            {
                child_value = quiesenceSearch(alpha, beta, false);
                if (child_value > value)
                {
                    value = child_value;
                    best_move = capture;
                }
                currentPos.unmakeCapture(capture);
                accumulatorStack.pop();
                if (value >= beta)
                    break;

                alpha = std::max(alpha, value);
            }
            else // Minimize
            {
                child_value = quiesenceSearch(alpha, beta, true);
                if (child_value < value)
                {
                    value = child_value;
                    best_move = capture;
                }
                currentPos.unmakeCapture(capture);
                accumulatorStack.pop();
                if (value <= alpha)
                    break;

                beta = std::min(beta, value);
            }
        }
    }
    else // In check
    {
        Move capture;
        currentPos.setCheckInfo();
        QSMoveSelectorCheck move_selector(currentPos);
        move_selector.init();
        while ((capture = move_selector.select_legal()) != Move(0))
        {
            no_captures = false;
            nnueuChange = currentPos.makeCapture(capture, state_info);
            accumulatorStack.push(nnueuChange);
            if (our_turn) // Maximize
            {
                child_value = quiesenceSearch(alpha, beta, false);
                if (child_value > value)
                {
                    value = child_value;
                    best_move = capture;
                }
                currentPos.unmakeCapture(capture);
                accumulatorStack.pop();
                if (value >= beta)
                    break;

                alpha = std::max(alpha, value);
            }
            else // Minimize
            {
                child_value = quiesenceSearch(alpha, beta, true);
                if (child_value < value)
                {
                    value = child_value;
                    best_move = capture;
                }
                currentPos.unmakeCapture(capture);
                accumulatorStack.pop();
                if (value <= alpha)
                    break;

                beta = std::min(beta, value);
            }
        }
    }
    // If there are no captures we return a game ending eval
    if (no_captures && currentPos.getIsCheck() && currentPos.isMate())
    {
        if (our_turn)
        {
            // globalTT.save(position.getZobristKey(), 0, 0, Move(0), false);
            return 0;
        }
        else
        {
            // globalTT.save(position.getZobristKey(), 30000, 0, Move(0), false);
            return 30000;
        }
    }
    // Saving a tt value
    // globalTT.save(position.getZobristKey(), value, 0, best_move, false);
    return value;
}

int16_t Worker::alphaBetaSearch(int8_t depth, int16_t alpha, int16_t beta, bool our_turn)
// This search is done when depth is more than 0 and considers all moves and stores positions in the transposition table
{
    assert(alpha <= beta);
    if (currentPos.isDraw())
        return 2048;

    // At depths <= 0 we enter quiesence search
    if (depth <= 0)
        return quiesenceSearch(alpha, beta, our_turn);

    bool no_moves{true};
    bool cutoff{false};

    // Baseline eval
    int16_t child_value;
    int16_t value{our_turn ? static_cast<int16_t>(-31000) : static_cast<int16_t>(31000)};
    Move best_move;
    StateInfo state_info;

    currentPos.setBlockersAndPinsInAB(); // For discovered checks and move generators
    currentPos.setCheckBits();           // For direct checks

    // Check if we have stored this position in ttable
    TTEntry *ttEntry = tt.probe(currentPos.getZobristKey());
    Move tt_move{0};
    bool is_pv_node{false};
    // If position is stored in ttable
    if (ttEntry != nullptr)
    {
        // We are in a PV-Node
        if (ttEntry->getIsExact())
        {
            if (ttEntry->getDepth() >= depth)
                return ttEntry->getValue();

            is_pv_node = true;
            tt_move = ttEntry->getMove();
            assert(tt_move.getData() == 0 || currentPos.ttMoveIsOk(tt_move));
        }
        // We are not in a PV-Node
        else
        {
            tt_move = ttEntry->getMove();
            // if (ttEntry->getDepth() >= depth)
            // {
            //     // Lower bound at deeper depth
            //     if (our_turn)
            //         alpha = ttEntry->getValue();
            //     // Upper bound at deeper depth
            //     else
            //         beta = ttEntry->getValue();
            // }
        }
    }
    NNUEU::NNUEUChange nnueuChange;

    // Transposition table move search
    if (tt_move.getData() != 0)
    {
        no_moves = false;

        if (our_turn) // Maximize
        {
            nnueuChange = currentPos.makeMove(tt_move, state_info);
            accumulatorStack.push(nnueuChange);
            child_value = alphaBetaSearch(depth - 1, alpha, beta, false);
            currentPos.unmakeMove(tt_move);
            accumulatorStack.pop();
            if (child_value > value)
            {
                value = child_value;
                best_move = tt_move;
                if (value >= beta)
                    cutoff = true;
            }
            alpha = std::max(alpha, value);
        }
        else // Minimize
        {
            nnueuChange = currentPos.makeMove(tt_move, state_info);
            accumulatorStack.push(nnueuChange);
            child_value = alphaBetaSearch(depth - 1, alpha, beta, true);
            currentPos.unmakeMove(tt_move);
            accumulatorStack.pop();
            if (child_value < value)
            {
                value = child_value;
                best_move = tt_move;
                if (value <= alpha)
                    cutoff = true;
            }
            beta = std::min(beta, value);
        }
    }

    // We only search if tt_move didn't produce a cutoff in the search tree
    if (not cutoff)
    {
        if (not currentPos.getIsCheck()) // Not in check
        {
            Move move;
            ABMoveSelectorNotCheck move_selector(currentPos, tt_move);
            move_selector.init_all();
            while ((move = move_selector.select_legal()) != Move(0))
            {
                no_moves = false;
                if (our_turn) // Maximize
                {
                    nnueuChange = currentPos.makeMove(move, state_info);
                    accumulatorStack.push(nnueuChange);
                    child_value = alphaBetaSearch(depth - 1, alpha, beta, false);
                    currentPos.unmakeMove(move);
                    accumulatorStack.pop();
                    if (child_value > value)
                    {
                        value = child_value;
                        best_move = move;
                        if (value >= beta)
                        {
                            cutoff = true;
                            break;
                        }
                    }
                    alpha = std::max(alpha, value);
                }
                else // Minimize
                {
                    nnueuChange = currentPos.makeMove(move, state_info);
                    accumulatorStack.push(nnueuChange);
                    child_value = alphaBetaSearch(depth - 1, alpha, beta, true);
                    currentPos.unmakeMove(move);
                    accumulatorStack.pop();
                    if (child_value < value)
                    {
                        value = child_value;
                        best_move = move;
                        if (value <= alpha)
                        {
                            cutoff = true;
                            break;
                        }
                    }
                    beta = std::min(beta, value);
                }
            }
        }
        else // In check
        {
            currentPos.setCheckInfo();
            Move move;
            ABMoveSelectorCheck move_selector(currentPos, tt_move);
            move_selector.init();
            while ((move = move_selector.select_legal()) != Move(0))
            {
                no_moves = false;
                if (our_turn) // Maximize
                {
                    nnueuChange = currentPos.makeMove(move, state_info);
                    accumulatorStack.push(nnueuChange);
                    child_value = alphaBetaSearch(depth - 1, alpha, beta, false);
                    currentPos.unmakeMove(move);
                    accumulatorStack.pop();
                    if (child_value > value)
                    {
                        value = child_value;
                        best_move = move;
                        if (value >= beta)
                        {
                            cutoff = true;
                            break;
                        }
                    }
                    alpha = std::max(alpha, value);
                }
                else // Minimize
                {
                    nnueuChange = currentPos.makeMove(move, state_info);
                    accumulatorStack.push(nnueuChange);
                    child_value = alphaBetaSearch(depth - 1, alpha, beta, true);
                    currentPos.unmakeMove(move);
                    accumulatorStack.pop();
                    if (child_value < value)
                    {
                        value = child_value;
                        best_move = move;
                        if (value <= alpha)
                        {
                            cutoff = true;
                            break;
                        }
                    }
                    beta = std::min(beta, value);
                }
            }
        }
    }
    // Game finished since there are no legal moves
    if (no_moves)
    {
        // Stalemate
        if (not currentPos.getIsCheck())
        {
            tt.save(currentPos.getZobristKey(), 2048, depth, best_move, true);
            return 2048;
        }
        // Checkmate against us
        else if (our_turn)
        {
            tt.save(currentPos.getZobristKey(), 0, depth, best_move, true);
            return -depth;
        }
        // Checkmate against opponent
        else
        {
            tt.save(currentPos.getZobristKey(), 30000, depth, best_move, true);
            return 30000 + depth;
        }
    }
    // Saving a tt value
    tt.save(currentPos.getZobristKey(), value, depth, best_move, not cutoff);

    return value;
}

std::pair<Move, int16_t> Worker::firstMoveSearch(int8_t depth, int16_t alpha, int16_t beta, std::chrono::milliseconds predictedTimeTakenMs)
// This search is done when depth is more than 0 and considers all moves
// Note that here we have no alpha/beta cutoffs, since we are only applying the first move.
{
    // Reorder the first moves by last-known scores or first-time ordering
    if (rootScores.empty())
    {
        rootScores.resize(rootMoves.size(), -30001);
    }
    else
    {
        std::pair<std::vector<Move>, std::vector<int16_t>> result =
            rootPos.orderAllMovesOnFirstIteration(rootMoves, rootScores);
        rootMoves = result.first;
        rootScores = result.second;
    }

    // Baseline initialization
    int16_t value = static_cast<int16_t>(-30001);
    Move best_move{0};

    // Keep track of best previous iteration score to decide “penalty”
    // (If a move’s prior score is way below this, we reduce the depth.)
    int16_t bestScoreFromPreviousIteration = -30001;
    for (auto sc : rootScores)
        if (sc > bestScoreFromPreviousIteration)
            bestScoreFromPreviousIteration = sc;

    auto first_move_start_time = std::chrono::high_resolution_clock::now();

    currentPos = rootPos;
    NNUEU::NNUEUChange nnueuChange;

    // Main loop over candidate moves
    for (std::size_t i = 0; i < rootMoves.size(); ++i)
    {
        Move currentMove = rootMoves[i];

        StateInfo state_info;
        nnueuChange = rootPos.makeMove(currentMove, state_info);
        accumulatorStack.push(nnueuChange);

        // Decide on “reduction” based on previous iteration’s score
        int reduction = 0;
        if (depth > 1 && !rootScores.empty())
        {
            int16_t prevScore = rootScores[i];
            if (prevScore + 1000 < bestScoreFromPreviousIteration)
                reduction = 1;
        }

        // Do the “reduced” (or normal) alpha-beta search:
        int8_t searchDepth = std::max(0, depth - 1 - reduction);

        int16_t child_value = alphaBetaSearch(searchDepth, alpha, beta, false);

        // If a reduced search "fails high" (beats alpha),
        // we re-search at the full depth to avoid missing a good move.
        if (reduction > 0 && child_value > alpha)
        {
            child_value = alphaBetaSearch(depth - 1, alpha, beta, false);
        }

        // Update the move’s new score
        rootScores[i] = child_value;

        // Track if this is the best so far
        if (child_value > value)
        {
            value = child_value;
            best_move = currentMove;
        }

        currentPos.unmakeMove(currentMove);
        accumulatorStack.pop();
        alpha = std::max(alpha, value);

        moveDepthValues[currentMove].emplace_back(value);

        // Check time
        auto duration = std::chrono::high_resolution_clock::now() - startTime;
        if (duration >= timeForMoveMS)
            break;
    }

    // Time taken for this entire loop
    lastFirstMoveTimeTakenMS = static_cast<int>(
                                   std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::high_resolution_clock::now() - first_move_start_time)
                                       .count()) +
                               1;

    // Save in TT as “exact”
    tt.save(currentPos.getZobristKey(), value, depth, best_move, true);

    return std::pair<Move, int16_t>(best_move, value);
}

std::pair<Move, int16_t> Worker::iterativeSearch(int8_t start_depth, int8_t fixed_max_depth)
{
    isEndgame = rootPos.isEndgame();
    moveDepthValues = {};

    lastFirstMoveTimeTakenMS = 1;
    timeForMoveMS = timeLimit / 4;

    rootPos.setBlockersAndPinsInAB(); // For discovered checks and move generators
    rootPos.setCheckBits();           // For direct checks

    // Populate rootMoves
    if (rootPos.getIsCheck())
    {
        rootPos.setCheckInfo();
        ABMoveSelectorCheck msel(rootPos, Move(0));
        msel.init();
        Move candidate;
        while ((candidate = msel.select_legal()) != Move(0))
            rootMoves.push_back(candidate);
    }
    else
    {
        ABMoveSelectorNotCheck msel(rootPos, Move(0));
        msel.init_all();
        Move candidate;
        while ((candidate = msel.select_legal()) != Move(0))
            rootMoves.push_back(candidate);
    }
    // If there is only one move in the position, we make it
    if (rootMoves.size() == 1)
        return std::pair<Move, int16_t>(rootMoves[0], 0);

    Move bestMove{};
    Move bestMovePreviousDepth{};
    int16_t bestValue;
    std::pair<Move, int16_t> tuple;
    int streak = 1;                          // To keep track of the improvement streak

    // Iterative deepening
    for (int8_t depth = start_depth; depth <= fixed_max_depth; ++depth)
    {
        // We are going to perform N searches of time T, where:
        // N is first_moves.size() and T is lastFirstMoveTimeTakenMS
        // Hence we can predict the time taken of this new search to be N * T
        std::chrono::milliseconds predictedTimeTakenMs{17 * lastFirstMoveTimeTakenMS};

        if (predictedTimeTakenMs >= timeForMoveMS)
        {
            break;
        }

        // Set best current values to worse possible ones (so that we try to improve them)
        int16_t alpha{-31001};
        int16_t beta{31001};

        // Search
        tuple = firstMoveSearch(depth, alpha, beta, predictedTimeTakenMs);
        bestMove = tuple.first;
        bestValue = tuple.second;

        completedDepth = static_cast<int>(depth);

        if (bestMove.getData() == bestMovePreviousDepth.getData())
            streak++;
        else
        {
            bestMovePreviousDepth = bestMove;
            streak = 1;
        }
        // Check stop condition based on streak and improvement pattern
        if (stopSearch(moveDepthValues[bestMove], streak, depth, rootPos))
        {
            break;
        }

        // Calculate the elapsed time in milliseconds
        std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() - startTime;
        // Check if the duration has been exceeded
        if (duration >= timeForMoveMS)
        {
            break;
        }
    }
    return std::pair<Move, int16_t>(bestMove, bestValue);
}