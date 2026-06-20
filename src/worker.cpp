#include "worker.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <utility>

#include "accumulation.h"
#include "engine.h"
#include "move_selectors.h"
#include "ttable.h"

// Late-move-reduction table (Stockfish-style): Reductions[i] ~ K*log(i). The per-move
// reduction is R = Reductions[depth] * Reductions[movesSearched] / LMR_SCALE, so it grows
// with both depth and move count. Lower LMR_SCALE = more aggressive reductions.
static int Reductions[256];
static constexpr int LMR_SCALE = 1024;
static const bool reductionsInit = [] {
    Reductions[0] = 0;
    for (int i = 1; i < 256; ++i)
        Reductions[i] = static_cast<int>(23.0 * std::log(static_cast<double>(i)));
    return true;
}();

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
    ++nodes;
    ++qnodes;
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
    ++nodes;
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

    const bool stm = currentPos.getTurn(); // side to move: indexes the butterfly history
    Move quietsTried[64];                  // quiet moves searched at this node (history malus on cutoff)
    int nQuiets = 0;

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
            {
                ++cntTTcut;
                return ttValue;
            }
        }
    }

    // Static evaluation of this node, computed once (when not in check) — the basis for
    // reverse futility, the null-move gate, and later forward pruning. Eval is meaningless
    // in check, so it is left at 0 there and all eval-based pruning is skipped.
    const bool inCheck = currentPos.getIsCheck();
    const int16_t staticEval = inCheck ? static_cast<int16_t>(0)
                                       : network.evaluate(currentPos, accumulatorStack, *transformer);

    // Reverse futility (static null move): if the static eval clears beta by a generous
    // depth-scaled margin, the node almost certainly fails high, so return it without
    // searching. Skipped in check and near mate scores.
    if (!inCheck && depth <= 3 && beta < 29000 && beta > -29000
        && staticEval < 20000 && staticEval >= beta + 175 * depth)
        return staticEval;

    // Null-move pruning. If we are not in check and have non-pawn material
    // (zugzwang guard), give the opponent a free move and search to reduced depth.
    // If even then the score still fails high, the position is good enough to prune.
    // Gated on the static eval >= beta so we only spend the null search on promising
    // nodes, skipped near mate scores so we never return an unproven mate, and
    // disabled (allowNull) right after a null move and inside verification searches.
    if (allowNull && depth >= 3 && beta < 29000 && !inCheck && currentPos.hasNonPawnMaterial()
        && staticEval >= beta)
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
            {
                ++cntNMP;
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
        const bool ttQuiet = (currentPos.aBMoveValue(tt_move) == 0);
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
            ++cntBeta;
            ++cntBetaFirst; // the TT move is the first move searched at this node
            storeKiller(ply, tt_move);
            if (ttQuiet)
                updateHistory(stm, tt_move, depth, quietsTried, nQuiets); // nQuiets == 0: bonus only
        }
        else if (ttQuiet)
            quietsTried[nQuiets++] = tt_move;
    }

    // We only search if tt_move didn't produce a cutoff in the search tree
    if (not cutoff)
    {
        if (not currentPos.getIsCheck()) // Not in check
        {
            Move move;
            ABMoveSelectorNotCheck move_selector(currentPos, tt_move, killers[ply][0], killers[ply][1], mainHistory[stm]);
            move_selector.init_all();
            // Count of moves already searched at this node (the TT move, if any, was
            // searched at full depth above). Used for late-move reductions.
            int movesSearched = (tt_move.getData() != 0) ? 1 : 0;
            while ((move = move_selector.select_legal()) != Move(0))
            {
                no_moves = false;
                ++movesSearched;
                // Quiet moves score 0 in aBMoveValue (captures/promotions score != 0).
                const bool isQuiet = (currentPos.aBMoveValue(move) == 0);
                // SEE pruning: at shallow depth, skip clearly-losing captures (the
                // exchange drops material badly) — they rarely justify themselves before
                // quiescence. Conservative threshold, scaled with depth; guarded so we
                // never prune when the best score so far is still a near-mate loss.
                if (depth <= 6 && currentPos.isCaptureStageMove(move) && value > -29000
                    && !currentPos.see_ge(move, -75 * depth))
                    continue;
                makeMove(move, state_info);
                // Principal variation search. The first move is searched full depth + full
                // window; every later move is searched first with a zero window — reduced
                // (formula LMR) when it is a late quiet non-checking move — and re-searched
                // at full depth and window only if the scout beats alpha (an LMR-reduced
                // scout that beats alpha is always confirmed at full depth).
                if (movesSearched == 1)
                {
                    child_value = -alphaBetaSearch(static_cast<int8_t>(depth - 1), -beta, -alpha, ply + 1);
                }
                else
                {
                    int R = 0;
                    if (depth >= 2 && isQuiet && !currentPos.getIsCheck())
                    {
                        R = (Reductions[depth < 256 ? depth : 255]
                             * Reductions[movesSearched < 256 ? movesSearched : 255]) / LMR_SCALE;
                        R -= historyScore(stm, move) / 8000; // +/- up to ~2 plies
                        if (R < 0)
                            R = 0;
                        if (R > depth - 1)
                            R = depth - 1; // keep the reduced depth >= 1
                        if (R > 0)
                            ++cntLMR;
                    }
                    child_value = -alphaBetaSearch(static_cast<int8_t>(depth - 1 - R),
                                                   static_cast<int16_t>(-(alpha + 1)),
                                                   static_cast<int16_t>(-alpha), ply + 1);
                    if (child_value > alpha && (R > 0 || child_value < beta))
                        child_value = -alphaBetaSearch(static_cast<int8_t>(depth - 1), -beta, -alpha, ply + 1);
                }
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
                    ++cntBeta;
                    if (movesSearched == 1)
                        ++cntBetaFirst;
                    storeKiller(ply, move);
                    if (isQuiet)
                        updateHistory(stm, move, depth, quietsTried, nQuiets);
                    break; // Fail high
                }
                if (isQuiet && nQuiets < 64)
                    quietsTried[nQuiets++] = move;
            }
        }
        else // In check
        {
            currentPos.setCheckInfo();
            Move move;
            ABMoveSelectorCheck move_selector(currentPos, tt_move);
            move_selector.init();
            int movesSearched = (tt_move.getData() != 0) ? 1 : 0;
            while ((move = move_selector.select_legal()) != Move(0))
            {
                no_moves = false;
                ++movesSearched;
                makeMove(move, state_info);
                // PVS for check evasions (no reduction in check): first move full window,
                // later moves a zero-window scout re-searched on alpha < value < beta.
                if (movesSearched == 1)
                    child_value = -alphaBetaSearch(depth - 1, -beta, -alpha, ply + 1);
                else
                {
                    child_value = -alphaBetaSearch(static_cast<int8_t>(depth - 1),
                                                   static_cast<int16_t>(-(alpha + 1)),
                                                   static_cast<int16_t>(-alpha), ply + 1);
                    if (child_value > alpha && child_value < beta)
                        child_value = -alphaBetaSearch(depth - 1, -beta, -alpha, ply + 1);
                }
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
                    ++cntBeta;
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
    std::memset(killers, 0, sizeof(killers));         // fresh killer table per search
    std::memset(mainHistory, 0, sizeof(mainHistory)); // fresh butterfly history per search
    nodes = qnodes = cntTTcut = cntNMP = cntLMR = cntBeta = cntBetaFirst = 0; // search-introspection counters

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

        // A finite target depth (fixed_max_depth < 99, i.e. searchFixedDepth / "go depth N")
        // or the introspection flag means: run every depth to the target with no time- or
        // streak-based early stop — otherwise a fixed-depth search can quit early on a stable
        // (and sometimes wrong) move before the target depth is ever reached.
        const bool noEarlyStop = infoNoEarlyStop || (fixed_max_depth < 99);

        // Iterative deepening
        for (int8_t depth = start_depth; depth <= fixed_max_depth; ++depth)
        {
            // Search
            firstMoveSearch(depth);

            completedDepth = static_cast<int>(depth);

            // Get max move duration of the root moves to make sure we have time to think for another move
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - startTime);

            // Per-depth UCI info — also the data source for the search-tree / EBF harness.
            if (isMainThread())
            {
                const long long ms = duration.count();
                const long long nps = ms > 0 ? static_cast<long long>(nodes) * 1000 / ms : 0;
                std::cout << "info depth " << static_cast<int>(depth)
                          << " score cp " << bestRootValue
                          << " nodes " << nodes
                          << " nps " << nps
                          << " time " << ms
                          << " pv " << bestRootMove.toString()
                          << '\n' << std::flush;
            }

            // We exceed time limit or we have a mate score (both skipped in fixed-depth
            // "go depth N" introspection mode, which always runs every depth up to N).
            if (!noEarlyStop && (duration >= softTimeLimit || bestRootValue >= 29000))
                break;
            // Check if the best move at this depth is still the same, and adjust its streak
            else if (bestRootMove.getData() == bestMovePreviousDepth.getData())
            {
                streak++;
                // Check stop condition based on streak and improvement pattern
                if (!noEarlyStop && stopSearch(moveDepthValues[bestRootMove], streak, depth))
                    break;
            }
            else
            {
                bestMovePreviousDepth = bestRootMove;
                streak = 1;
            }
        }
        // End-of-search pruning breakdown (the "why" side of the comparison).
        if (isMainThread())
            std::cout << "info string nodes " << nodes << " qnodes " << qnodes
                      << " ttcut " << cntTTcut << " nmp " << cntNMP << " lmr " << cntLMR
                      << " betacut " << cntBeta << " betafirst " << cntBetaFirst
                      << '\n' << std::flush;
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