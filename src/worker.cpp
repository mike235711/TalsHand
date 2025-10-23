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
               NNUEU::Network &networkIn,
               const NNUEU::Transformer &transformerIn,
               size_t idx)
    : softTimeLimit(0),
      hardTimeLimit(0),
      ponder(false),
      isEndgame(false),
      completedDepth(0),
      threadIdx(idx),
      threads(threadpool),
      tt(ttable),
      network(networkIn),
      transformer(&transformerIn)
{
    // rootPos / rootState are filled just before the search starts
}

bool Worker::stopSearch(const std::vector<int16_t> &values, int streak, int depth)
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

int16_t Worker::quiesenceSearch(int16_t alpha, int16_t beta)
// This search is done when depth is less than or equal to 0 and considers only captures and promotions
{
    // If we are in quiescence, we have a baseline evaluation as if no captures happened
    // Stand pat
    int16_t value = network.evaluate(currentPos, accumulatorStack, *transformer);

    // Fail high when making no moves
    if (value >= beta)
        return value;

    alpha = std::max(alpha, value);

    int16_t child_value;
    bool no_captures = true;
    StateInfo state_info;

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

            makeCapture(capture, state_info);
            child_value = -quiesenceSearch(-beta, -alpha);
            unmakeCapture(capture);

            if (child_value > value)
            {
                value = child_value;
                if (child_value > alpha)
                    alpha = child_value;
            }
            if (child_value >= beta)
                break; // Fail high
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
            makeCapture(capture, state_info);
            child_value = -quiesenceSearch(-beta, -alpha);
            unmakeCapture(capture);

            if (child_value > value)
            {
                value = child_value;
                if (child_value > alpha)
                    alpha = child_value;
            }
            if (child_value >= beta)
                break; // Fail high
        }
    }
    // If there are no captures we return a game ending eval
    if (no_captures && currentPos.getIsCheck() && currentPos.isMate())
        return -30000;

    // Saving a tt value
    // globalTT.save(position.getZobristKey(), value, 0, best_move, false);
    return value;
}

int16_t Worker::alphaBetaSearch(int8_t depth, int16_t alpha, int16_t beta)
// This search is done when depth is more than 0 and considers all moves and stores positions in the transposition table
{
    assert(alpha <= beta);
    if (currentPos.isDraw())
        return 0;

    // At depths <= 0 we enter quiesence search
    if (depth <= 0)
        return quiesenceSearch(alpha, beta);

    bool no_moves{true};
    bool cutoff{false};

    // Baseline eval
    int16_t child_value;
    int16_t value{static_cast<int16_t>(-31000)};
    Move best_move;
    StateInfo state_info;

    currentPos.setBlockersAndPinsInAB(); // For discovered checks and move generators
    currentPos.setCheckBits();           // For direct checks

    // Check if we have stored this position in ttable
    TTEntry *ttEntry = tt.probe(currentPos.getZobristKey());
    Move tt_move{0};

    // If position is stored in ttable
    if (ttEntry != nullptr)
    {
        // We are in a PV-Node
        if (ttEntry->getIsExact())
        {
            if (ttEntry->getDepth() >= depth)
                return ttEntry->getValue();

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

    // Transposition table move search
    if (tt_move.getData() != 0)
    {
#ifndef NDEBUG // DEBUG
    #ifdef VERBOSE_DEBUG
        std::cout << "Transposition Table move: " << tt_move.toString() << "\n";
    #endif
#endif
        no_moves = false;
        makeMove(tt_move, state_info);
        child_value = -alphaBetaSearch(depth - 1, -beta, -alpha);
        unmakeMove(tt_move);
        if (child_value > value)
        {
            value = child_value;
            if (child_value > alpha)
                alpha = child_value;
        }
        if (child_value >= beta)
            cutoff = true; // Fail high
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
                makeMove(move, state_info);
                child_value = -alphaBetaSearch(depth - 1, -beta, -alpha);
                unmakeMove(move);
                if (child_value > value)
                {
                    value = child_value;
                    if (child_value > alpha)
                        alpha = child_value;
                }
                if (child_value >= beta)
                    break; // Fail high
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
                makeMove(move, state_info);
                child_value = -alphaBetaSearch(depth - 1, -beta, -alpha);
                unmakeMove(move);
                if (child_value > value)
                {
                    value = child_value;
                    if (child_value > alpha)
                        alpha = child_value;
                }
                if (child_value >= beta)
                    break; // Fail high
            }
        }
    }
    // Game finished since there are no legal moves
    if (no_moves)
    {
        // Stalemate
        if (not currentPos.getIsCheck())
        {
            tt.save(currentPos.getZobristKey(), 0, depth, best_move, true);
            return 0;
        }
        // Checkmate against us
        else
        {
            tt.save(currentPos.getZobristKey(), -30000 - depth, depth, best_move, true);
            return -30000 - depth;
        }
    }
    // Saving a tt value
    tt.save(currentPos.getZobristKey(), value, depth, best_move, not cutoff);

    return value;
}

bool Worker::firstMoveSearch(int8_t depth, int16_t alpha, int16_t beta)
// This search is done when depth is more than 0 and considers all moves
// Note that here we have no alpha/beta cutoffs, since we are only applying the first move.
{
    // Keep track of best previous iteration score to decide “penalty”
    // (If a move’s prior score is way below this, we reduce the depth.)
    int16_t bestScoreFromPreviousIteration;
    Move bestMovePreviousIteration = bestRootMove;

    // Reorder the first moves by last-known scores or first-time ordering
    if (rootScores.empty())
    {
        rootScores.resize(rootMoves.size(), -30001);
        bestScoreFromPreviousIteration = -30001;
    }
    else
    {
        std::pair<std::vector<Move>, std::vector<int16_t>> result =
            rootPos.orderAllMovesOnFirstIteration(rootMoves, rootScores);
        rootMoves = result.first;
        rootScores = result.second;
        bestScoreFromPreviousIteration = rootScores[0];
    }

    currentPos = rootPos;
    NNUEU::NNUEUChange nnueuChange;
    StateInfo state_info;

    std::chrono::milliseconds max_move_duration(0);
    bestRootValue = static_cast<int16_t>(-30001);

    // Main loop over candidate moves
    for (std::size_t i = 0; i < rootMoves.size(); ++i)
    {
        auto move_start_time = std::chrono::high_resolution_clock::now();
        Move currentMove = rootMoves[i];

        makeMove(currentMove, state_info);

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

        int16_t child_value = -alphaBetaSearch(searchDepth, -beta, -alpha);

        // If a reduced search "fails high" (beats alpha),
        // we re-search at the full depth to avoid missing a good move.
        if (reduction > 0 && child_value > alpha)
        {
            child_value = -alphaBetaSearch(depth - 1, -beta, -alpha);
        }
        unmakeMove(currentMove);

        // Update the move’s new score
        rootScores[i] = child_value;

        // Track if this is the best so far
        if (child_value > bestRootValue)
        {
            bestRootValue = child_value;
            bestRootMove = currentMove;

            // If the best move changes, we might need more time
            if (bestRootMove.getData() != bestMovePreviousIteration.getData())
            {
                softTimeLimit += softTimeLimit / 8;
            }
        }
        // If score drops suddenly for the best move, extend time
        else if (currentMove.getData() == bestRootMove.getData() && child_value < bestRootValue - 20)
        {
            softTimeLimit += softTimeLimit / 16;
        }
        alpha = std::max(alpha, bestRootValue);

        moveDepthValues[currentMove].emplace_back(child_value);

        auto move_end_time = std::chrono::high_resolution_clock::now();
        auto move_duration = std::chrono::duration_cast<std::chrono::milliseconds>(move_end_time - move_start_time);
        if (move_duration > max_move_duration)
        {
            max_move_duration = move_duration;
        }

        // Check time
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(move_end_time - startTime);
        if (duration >= (softTimeLimit - max_move_duration) || duration >= (hardTimeLimit - max_move_duration))
            return true;
    }

    // Save in TT as “exact”
    tt.save(currentPos.getZobristKey(), bestRootValue, depth, bestRootMove, true);

    return false;
}

void Worker::iterativeSearch(int8_t start_depth, int8_t fixed_max_depth)
{
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
    {
        bestRootMove = rootMoves[0];
        bestRootValue = 0;
    }
    else
    {
        isEndgame = rootPos.isEndgame();
        moveDepthValues = {};

        softTimeLimit = hardTimeLimit / (24 + rootPos.countStartPieces() + rootPos.countAllPieces());

        startTime = std::chrono::high_resolution_clock::now();
        Move bestMovePreviousDepth{};
        bestRootValue = static_cast<int16_t>(-30001);
        int streak = 1;                          // To keep track of the improvement streak

        // Iterative deepening
        for (int8_t depth = start_depth; depth <= fixed_max_depth; ++depth)
        {
            // Set best current values to worse possible ones (so that we try to improve them)
            int16_t alpha{-31001};
            int16_t beta{31001};

            // Search
            bool stop_search = firstMoveSearch(depth, alpha, beta);

            completedDepth = static_cast<int>(depth);
            
            if (stop_search)
                break;
            // Check if the best move at this depth is still the same, and adjust its streak
            else if (bestRootMove.getData() == bestMovePreviousDepth.getData())
            {
                streak++;
                // Check stop condition based on streak and improvement pattern
                if (stopSearch(moveDepthValues[bestRootMove], streak, depth))
                    break;
            }
            else
            {
                bestMovePreviousDepth = bestRootMove;
                streak = 1;
            }
        }
        // std::cout << "Depth: " << completedDepth << "\n";
    }
}

//  startSearching – wrapper around iterative deepening
std::pair<Move, int16_t> Worker::startSearching(int8_t max_depth)
{
    rootMoves.clear();
    rootScores.clear();
    accumulatorStack.reset(rootPos, *transformer);
    iterativeSearch(2, max_depth);

    // only thread-0 is the “main” UCI thread → tell the GUI our move
    if (isMainThread())
    {
        std::cout << "bestmove "
                  << bestRootMove.toString()
                  << '\n'                    // newline required by protocol
                  << std::flush;             // be sure it reaches the GUI
    }
    return {bestRootMove, bestRootValue};
}