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

int16_t Worker::alphaBetaSearch(int8_t depth, int16_t alpha, int16_t beta, int ply, bool allowNull)
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
    const int16_t alphaOrig = alpha; // original alpha, to classify the stored TT bound on save
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
        tt_move = ttEntry->getMove();
        assert(tt_move.getData() == 0 || currentPos.ttMoveIsOk(tt_move));
        // If the stored search was at least as deep, the stored bound may let us
        // return immediately: an exact value, a lower bound that already reaches
        // beta (fail-high), or an upper bound that is already <= alpha (fail-low).
        if (ttEntry->getDepth() >= depth)
        {
            const int16_t ttValue = ttEntry->getValue();
            const uint8_t ttBound = ttEntry->getBound();
            if (ttBound == BOUND_EXACT
                || (ttBound == BOUND_LOWER && ttValue >= beta)
                || (ttBound == BOUND_UPPER && ttValue <= alpha))
                return ttValue;
        }
    }

    // Null-move pruning. If we are not in check and have non-pawn material
    // (zugzwang guard), give the opponent a free move and search to reduced depth.
    // If even then the score still fails high, the position is good enough to prune.
    // Gated on a static eval >= beta so we only spend the null search on promising
    // nodes, skipped near mate scores so we never return an unproven mate, and
    // disabled (allowNull) right after a null move and inside verification searches.
    if (allowNull && depth >= 3 && beta < 29000 && !currentPos.getIsCheck() && currentPos.hasNonPawnMaterial())
    {
        const int16_t staticEval = network.evaluate(currentPos, accumulatorStack, *transformer);
        if (staticEval >= beta)
        {
            const int8_t R = static_cast<int8_t>(2 + depth / 6);
            StateInfo null_state;
            makeNullMove(null_state);
            // The opponent may not immediately null back (allowNull = false).
            const int16_t nullValue = -alphaBetaSearch(static_cast<int8_t>(depth - 1 - R),
                                                        static_cast<int16_t>(-beta),
                                                        static_cast<int16_t>(-beta + 1), ply + 1, false);
            unmakeNullMove();
            if (nullValue >= beta)
            {
                // Verification search (NMP disabled at this node) guards against
                // zugzwang and tactical lines where the free move is misleading
                // (e.g. a defender that is up material but actually getting mated).
                const int16_t verify = alphaBetaSearch(static_cast<int8_t>(depth - R), beta - 1, beta, ply, false);
                if (verify >= beta)
                    return beta; // confirmed fail-high prune (never an unproven mate)
            }
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
        child_value = -alphaBetaSearch(depth - 1, -beta, -alpha, ply + 1);
        unmakeMove(tt_move);
        if (child_value > value)
        {
            value = child_value;
            best_move = tt_move;
            if (child_value > alpha)
                alpha = child_value;
        }
        if (child_value >= beta)
        {
            cutoff = true; // Fail high
            storeKiller(ply, tt_move);
        }
    }

    // We only search if tt_move didn't produce a cutoff in the search tree
    if (not cutoff)
    {
        if (not currentPos.getIsCheck()) // Not in check
        {
            Move move;
            ABMoveSelectorNotCheck move_selector(currentPos, tt_move, killers[ply][0], killers[ply][1]);
            move_selector.init_all();
            while ((move = move_selector.select_legal()) != Move(0))
            {
                no_moves = false;
                makeMove(move, state_info);
                child_value = -alphaBetaSearch(depth - 1, -beta, -alpha, ply + 1);
                unmakeMove(move);
                if (child_value > value)
                {
                    value = child_value;
                    best_move = move;
                    if (child_value > alpha)
                        alpha = child_value;
                }
                if (child_value >= beta)
                {
                    cutoff = true;
                    storeKiller(ply, move);
                    break; // Fail high
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
                makeMove(move, state_info);
                child_value = -alphaBetaSearch(depth - 1, -beta, -alpha, ply + 1);
                unmakeMove(move);
                if (child_value > value)
                {
                    value = child_value;
                    best_move = move;
                    if (child_value > alpha)
                        alpha = child_value;
                }
                if (child_value >= beta)
                {
                    cutoff = true;
                    storeKiller(ply, move);
                    break; // Fail high
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
            tt.save(currentPos.getZobristKey(), 0, depth, best_move, BOUND_EXACT);
            return 0;
        }
        // Checkmate against us
        else
        {
            tt.save(currentPos.getZobristKey(), -30000 - depth, depth, best_move, BOUND_EXACT);
            return -30000 - depth;
        }
    }
    // Classify the result for the transposition table: a fail-high (cutoff) is a
    // lower bound; a value that beat the original alpha is exact; otherwise it is
    // an upper bound (fail-low).
    const uint8_t bound = cutoff ? BOUND_LOWER
                                 : (value > alphaOrig ? BOUND_EXACT : BOUND_UPPER);
    tt.save(currentPos.getZobristKey(), value, depth, best_move, bound);

    return value;
}

void Worker::firstMoveSearch(int8_t depth)
// This search is done when depth is more than 0 and considers all moves
// Note that here we have no alpha/beta cutoffs, since we are only applying the first move.
{
    // Keep track of best previous iteration score to decide “penalty”
    // (If a move’s prior score is way below this, we reduce the depth.)
    int16_t bestRootValuePreviousIteration;
    Move bestRootMovePreviousIteration = bestRootMove;

    // Reorder the first moves by last-known scores or first-time ordering
    if (rootScores.empty())
    {
        rootScores.resize(rootMoves.size(), -30001);
        bestRootValuePreviousIteration = -30001;
    }
    else
    {
        std::pair<std::vector<Move>, std::vector<int16_t>> result =
            rootPos.orderAllMovesOnFirstIteration(rootMoves, rootScores);
        rootMoves = result.first;
        rootScores = result.second;
        bestRootValuePreviousIteration = rootScores[0];
    }

    currentPos = rootPos;
    NNUEU::NNUEUChange nnueuChange;
    StateInfo state_info;

    bestRootValue = static_cast<int16_t>(-30001);

    // Main loop over candidate moves
    for (std::size_t i = 0; i < rootMoves.size(); ++i)
    {
        Move currentMove = rootMoves[i];

        makeMove(currentMove, state_info);

        // Decide on “reduction” based on previous iteration’s score
        int reduction = 0;
        if (depth > 1 && !rootScores.empty())
        {
            int16_t prevScore = rootScores[i];
            if (prevScore + 1000 < bestRootValuePreviousIteration)
                reduction = 1;
        }

        // Do the “reduced” (or normal) alpha-beta search:
        int8_t searchDepth = std::max(0, depth - 1 - reduction);

        int16_t child_value = -alphaBetaSearch(searchDepth, -31001, -bestRootValue, 1);

        // If a reduced search beats the best score so far,
        // we re-search at the full depth to avoid missing a good move.
        if (reduction > 0 && child_value > bestRootValue)
        {
            child_value = -alphaBetaSearch(depth - 1, -31001, -bestRootValue, 1);
        }
        unmakeMove(currentMove);

        // Update the move’s new score
        rootScores[i] = child_value;

        // Track if this is the best so far
        if (child_value > bestRootValue)
        {
            bestRootValue = child_value;
            bestRootMove = currentMove;
        }

        moveDepthValues[currentMove].emplace_back(child_value);
    }

    // Save in TT as “exact”
    tt.save(currentPos.getZobristKey(), bestRootValue, depth, bestRootMove, BOUND_EXACT);
}

void Worker::iterativeSearch(int8_t start_depth, int8_t fixed_max_depth)
{
    std::memset(killers, 0, sizeof(killers)); // fresh killer table per search

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

    // Seed a legal move up-front so an early hard-timeout never returns Move(0)
    // (returning the null move forfeits the game). firstMoveSearch overwrites it.
    if (!rootMoves.empty())
        bestRootMove = rootMoves[0];

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

        softTimeLimit = hardTimeLimit / (32 + rootPos.countStartPieces() + rootPos.countAllPieces());

        startTime = std::chrono::high_resolution_clock::now();
        Move bestMovePreviousDepth{};
        bestRootValue = static_cast<int16_t>(-30001);
        int streak = 1;                          // To keep track of the improvement streak

        // Iterative deepening
        for (int8_t depth = start_depth; depth <= fixed_max_depth; ++depth)
        {
            // Search
            firstMoveSearch(depth);

            completedDepth = static_cast<int>(depth);

            // Get max move duration of the root moves to make sure we have time to think for another move
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - startTime);

            // We exceed time limit or we have a mate score
            if (duration >= softTimeLimit || bestRootValue >= 29000)
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