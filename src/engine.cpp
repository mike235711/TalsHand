#include "bitposition.h"
#include <vector>
#include <utility> // For std::pair
#include <limits>  // For numeric limits
#include <array>
#include <cstdint>
#include <chrono>
#include <algorithm> // For std::max
#include "ttable.h"
#include <memory>
#include "position_eval.h"
#include "engine.h"

extern TranspositionTable globalTT;
extern int OURTIME;
extern int OURINC;
extern std::chrono::time_point<std::chrono::high_resolution_clock> STARTTIME;

int DEPTH;
Move ourMoveMade;

std::unordered_map<Move, std::vector<int16_t>> moveDepthValues;

bool stopSearch(const std::vector<int16_t> &values, int streak, int depth, BitPosition position)
{
    // If not endgame
    if (not position.isEndgame())
    {
        if (streak > 6 && depth > 7)
            return true;
        // Check if the move's score has just increased over time
        for (std::size_t i = 1; i < values.size(); ++i)
        {
            if (values[i] <= values[i - 1])
                return false;
        }

        if (streak > 5 && depth > 7)
            return true;
    }
    // If endgame
    else
    {
        if (streak > 8 && depth > 9)
            return true;
        // Check if the move's score has just increased over time
        for (std::size_t i = 1; i < values.size(); ++i)
        {
            if (values[i] <= values[i - 1])
                return false;
        }

        if (streak > 6 && depth > 8)
            return true;
    }
    return false;
}

int16_t quiesenceSearch(BitPosition &position, int16_t alpha, int16_t beta, bool our_turn)
// This search is done when depth is less than or equal to 0 and considers only captures and promotions
{
    // If we are in quiescence, we have a baseline evaluation as if no captures happened
    int16_t value{NNUE::evaluationFunction(our_turn)};
    Move best_move;
    bool no_captures{true};
    bool cutoff{false};
    Move refutation{};

    if (not position.getIsCheck()) // Not in check
    {
        refutation = position.getBestRefutation();
        // Refutation
        if (refutation.getData() != 0)
        {
            no_captures = false;
            if (our_turn) // Maximize
            {
                position.makeCapture(refutation);
                int16_t child_value{quiesenceSearch(position, alpha, beta, false)};
                if (child_value > value)
                {
                    value = child_value;
                    best_move = refutation;
                }
                position.unmakeCapture(refutation);
                if (value >= beta)
                    cutoff = true;

                alpha = std::max(alpha, value);
            }
            else // Minimize
            {
                position.makeCapture(refutation);
                int16_t child_value{quiesenceSearch(position, alpha, beta, true)};
                if (child_value < value)
                {
                    value = child_value;
                    best_move = refutation;
                }
                position.unmakeCapture(refutation);
                if (value <= alpha)
                    cutoff = true;

                beta = std::min(beta, value);
            }
        }
        if (not cutoff)
        {
            // All non refutations
            ScoredMove captures[64];
            ScoredMove *current_move = captures;
            ScoredMove *end_move = position.setCapturesAndScores(current_move);
            ScoredMove capture = position.nextScoredMove(current_move, end_move, refutation);
            
            if (capture.getData() != 0)
                no_captures = false;

            if (our_turn) // Maximize
            {
                while (capture.getData() != 0)
                {
                    position.makeCapture(capture);
                    int16_t child_value{quiesenceSearch(position, alpha, beta, false)};
                    if (child_value > value)
                    {
                        value = child_value;
                        best_move = capture;
                    }
                    position.unmakeCapture(capture);
                    if (value >= beta)
                        break;

                    alpha = std::max(alpha, value);
                    capture = position.nextScoredMove(current_move, end_move, refutation);
                }
            }
            else // Minimize
            {
                while (capture.getData() != 0)
                {
                    position.makeCapture(capture);
                    int16_t child_value{quiesenceSearch(position, alpha, beta, true)};
                    if (child_value < value)
                    {
                        value = child_value;
                        best_move = capture;
                    }
                    position.unmakeCapture(capture);
                    if (value <= alpha)
                        break;

                    beta = std::min(beta, value);
                    capture = position.nextScoredMove(current_move, end_move, refutation);
                }
            }
        }
    }
    else // In check
    {
        // Before generating in check moves we need the checks info
        Move moves[32];
        Move *current_move = moves;
        Move *end_move = position.setOrderedCapturesInCheck(current_move);
        Move capture = position.nextMove(current_move, end_move);
        
        if (capture.getData() != 0)
            no_captures = false;

        if (our_turn) // Maximize
        {
            while (capture.getData() != 0)
            {
                position.makeCapture(capture);
                int16_t child_value{quiesenceSearch(position, alpha, beta, false)};
                if (child_value > value)
                {
                    value = child_value;
                    best_move = capture;
                }
                position.unmakeCapture(capture);
                if (value >= beta)
                    break;

                alpha = std::max(alpha, value);
                capture = position.nextMove(current_move, end_move);
            }
        }
        else // Minimize
        {
            while (capture.getData() != 0)
            {
                position.makeCapture(capture);
                int16_t child_value{quiesenceSearch(position, alpha, beta, true)};
                if (child_value < value)
                {
                    value = child_value;
                    best_move = capture;
                }
                position.unmakeCapture(capture);
                if (value <= alpha)
                    break;

                beta = std::min(beta, value);
                capture = position.nextMove(current_move, end_move);
            }
        }
    }

    // If there are no captures we return an eval
    if (no_captures)
    {
        // If there is a check
        if (position.getIsCheck())
        {
            // Mate
            if (position.isMate())
            {
                if (our_turn)
                    return 0;
                else
                    return 30000;
            }
            // In check quiet position
            else
                return NNUE::evaluationFunction(our_turn);
        }
        // If there is no check we check for bad captures
        else
        {
            // Stalemate
            if (position.isStalemate())
                return 2048;
            // Quiet position
            else
                return NNUE::evaluationFunction(our_turn);
        }
    }
    return value;
}

int16_t alphaBetaSearch(BitPosition &position, int8_t depth, int16_t alpha, int16_t beta, bool our_turn)
// This search is done when depth is more than 0 and considers all moves and stores positions in the transposition table
{
    // Threefold repetition
    if (position.isThreeFoldOr50MoveRule())
        return 2048;

    bool no_moves{true};
    bool cutoff{false};
    bool is_check{position.getIsCheck()};

    // Baseline evaluation
    int16_t value{our_turn ? static_cast<int16_t>(-31000) : static_cast<int16_t>(31000)};
    Move best_move;

    // At depths <= 0 we enter quiesence search
    if (depth <= 0)
        return quiesenceSearch(position, alpha, beta, our_turn);

    // Check if we have stored this position in ttable
    TTEntry *ttEntry = globalTT.probe(position.getZobristKey());
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
        }
        // We are not in a PV-Node
        else
        {
            tt_move = ttEntry->getMove();
            if (ttEntry->getDepth() >= depth)
            {
                // Lower bound at deeper depth
                if (our_turn)
                    alpha = ttEntry->getValue();
                // Upper bound at deeper depth
                else
                    beta = ttEntry->getValue();
            }
        }
    }

    // Transposition table move search
    if (tt_move.getData() != 0)
    {
        no_moves = false;

        position.setBlockers(); // For discovered checks
        if (our_turn) // Maximize
        {
            position.makeTTMove(tt_move);
            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, false)};
            if (child_value > value)
            {
                value = child_value;
                best_move = tt_move;
            }
            position.unmakeTTMove(tt_move);
            if (value >= beta)
                cutoff = true;
            alpha = std::max(alpha, value);
        }
        else // Minimize
        {
            position.makeTTMove(tt_move);
            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, true)};
            if (child_value < value)
            {
                value = child_value;
                best_move = tt_move;
            }
            position.unmakeTTMove(tt_move);
            if (value <= alpha)
                cutoff = true;
            beta = std::min(beta, value);
        }
    }

    // We only search if tt_move didn't produce a cutoff in the search tree
    if (not cutoff)
    {
        if (not is_check) // Not in check
        {
            if (is_pv_node) // PV Nodes usually dont produce cutoffs so we generate all moves in one
            {
                if (not cutoff)
                {
                    // Refutation moves ordered
                    Move refutationMoves[64];
                    Move *current_move = refutationMoves;
                    Move *end_move = position.setRefutationMovesOrdered(current_move);
                    Move move = position.nextMove(current_move, end_move, tt_move);
                    if (move.getData() != 0)
                        no_moves = false;
                    if (our_turn) // Maximize
                    {
                        while (move.getData() != 0)
                        {
                            position.makeMove(move);
                            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, false)};
                            if (child_value > value)
                            {
                                value = child_value;
                                best_move = move;
                            }
                            position.unmakeMove(move);
                            if (value >= beta)
                            {
                                cutoff = true;
                                break;
                            }

                            alpha = std::max(alpha, value);
                            move = position.nextMove(current_move, end_move, tt_move);
                        }
                    }
                    else // Minimize
                    {
                        while (move.getData() != 0)
                        {
                            position.makeMove(move);
                            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, true)};
                            if (child_value < value)
                            {
                                value = child_value;
                                best_move = move;
                            }
                            position.unmakeMove(move);
                            if (value <= alpha)
                            {
                                cutoff = true;
                                break;
                            }

                            beta = std::min(beta, value);
                            move = position.nextMove(current_move, end_move, tt_move);
                        }
                    }
                }
                if (not cutoff)
                {
                    // Good captures ordered
                    Move goodCaptures[64];
                    Move *current_move = goodCaptures;
                    Move *end_move = position.setGoodCapturesOrdered(current_move);
                    Move move = position.nextMove(current_move, end_move);
                    if (move.getData() != 0)
                        no_moves = false;
                    if (our_turn) // Maximize
                    {
                        while (move.getData() != 0)
                        {
                            position.makeMove(move);
                            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, false)};
                            if (child_value > value)
                            {
                                value = child_value;
                                best_move = move;
                            }
                            position.unmakeMove(move);
                            if (value >= beta)
                            {
                                cutoff = true;
                                break;
                            }

                            alpha = std::max(alpha, value);
                            move = position.nextMove(current_move, end_move, tt_move);
                        }
                    }
                    else // Minimize
                    {
                        while (move.getData() != 0)
                        {
                            position.makeMove(move);
                            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, true)};
                            if (child_value < value)
                            {
                                value = child_value;
                                best_move = move;
                            }
                            position.unmakeMove(move);
                            if (value <= alpha)
                            {
                                cutoff = true;
                                break;
                            }

                            beta = std::min(beta, value);
                            move = position.nextMove(current_move, end_move, tt_move);
                        }
                    }
                }
                if (not cutoff)
                {
                    // Safe moves
                    ScoredMove safeMoves[128];
                    ScoredMove *currSafeMove = safeMoves;
                    ScoredMove *endSafeMove = position.setSafeMovesAndScores(currSafeMove);
                    ScoredMove safe_move = position.nextScoredMove(currSafeMove, endSafeMove);
                    if (safe_move.getData() != 0)
                        no_moves = false;
                    if (our_turn) // Maximize
                    {
                        while (safe_move.getData() != 0)
                        {
                            position.makeMove(safe_move);
                            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, false)};
                            if (child_value > value)
                            {
                                value = child_value;
                                best_move = safe_move;
                            }
                            position.unmakeMove(safe_move);
                            if (value >= beta)
                            {
                                cutoff = true;
                                break;
                            }

                            alpha = std::max(alpha, value);
                            safe_move = position.nextScoredMove(currSafeMove, endSafeMove, tt_move);
                        }
                    }
                    else // Minimize
                    {
                        while (safe_move.getData() != 0)
                        {
                            position.makeMove(safe_move);
                            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, true)};
                            if (child_value < value)
                            {
                                value = child_value;
                                best_move = safe_move;
                            }
                            position.unmakeMove(safe_move);
                            if (value <= alpha)
                            {
                                cutoff = true;
                                break;
                            }

                            beta = std::min(beta, value);
                            safe_move = position.nextScoredMove(currSafeMove, endSafeMove, tt_move);
                        }
                    }
                }
                if (not cutoff)
                {
                    // Bad captures
                    Move badCaptures[64];
                    Move *current_move = badCaptures;
                    Move *end_move = position.setBadCapturesOrUnsafeMoves(current_move);
                    Move move = position.nextMove(current_move, end_move);
                    if (move.getData() != 0)
                        no_moves = false;
                    if (our_turn) // Maximize
                    {
                        while (move.getData() != 0)
                        {
                            position.makeMove(move);
                            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, false)};
                            if (child_value > value)
                            {
                                value = child_value;
                                best_move = move;
                            }
                            position.unmakeMove(move);
                            if (value >= beta)
                            {
                                cutoff = true;
                                break;
                            }

                            alpha = std::max(alpha, value);
                            move = position.nextMove(current_move, end_move, tt_move);
                        }
                    }
                    else // Minimize
                    {
                        while (move.getData() != 0)
                        {
                            position.makeMove(move);
                            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, true)};
                            if (child_value < value)
                            {
                                value = child_value;
                                best_move = move;
                            }
                            position.unmakeMove(move);
                            if (value <= alpha)
                            {
                                cutoff = true;
                                break;
                            }

                            beta = std::min(beta, value);
                            move = position.nextMove(current_move, end_move, tt_move);
                        }
                    }
                }
            }
            else // Non PV nodes
            {
                ScoredMove moves[256];
                ScoredMove *current_move = moves;
                ScoredMove *end_move = position.setMovesAndScores(current_move);
                ScoredMove move = position.nextScoredMove(current_move, end_move, tt_move);
                if (move.getData() != 0)
                    no_moves = false;
                if (our_turn) // Maximize
                {
                    while (move.getData() != 0)
                    {
                        position.makeMove(move);
                        int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, false)};
                        if (child_value > value)
                        {
                            value = child_value;
                            best_move = move;
                        }
                        position.unmakeMove(move);
                        if (value >= beta)
                        {
                            cutoff = true;
                            break;
                        }

                        alpha = std::max(alpha, value);
                        move = position.nextScoredMove(current_move, end_move, tt_move);
                    }
                }
                else // Minimize
                {
                    while (move.getData() != 0)
                    {
                        position.makeMove(move);
                        int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, true)};
                        if (child_value < value)
                        {
                            value = child_value;
                            best_move = move;
                        }
                        position.unmakeMove(move);
                        if (value <= alpha)
                        {
                            cutoff = true;
                            break;
                        }

                        beta = std::min(beta, value);
                        move = position.nextScoredMove(current_move, end_move, tt_move);
                    }
                }
            }
        }
        else // In check
        {
            // Before generating in check moves we need the checks info
            Move moves[64];
            Move *current_move = moves;
            Move *end_move = position.setMovesInCheck(current_move);
            Move move = position.nextMove(current_move, end_move, tt_move);
            if (move.getData() != 0)
                no_moves = false;
            if (our_turn) // Maximize
            {
                while (move.getData() != 0)
                {
                    position.makeMove(move);
                    int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, false)};
                    if (child_value > value)
                    {
                        value = child_value;
                        best_move = move;
                    }
                    position.unmakeMove(move);
                    if (value >= beta)
                    {
                        cutoff = true;
                        break;
                    }

                    alpha = std::max(alpha, value);
                    move = position.nextMove(current_move, end_move, tt_move);
                }
            }
            else // Minimize
            {
                while (move.getData() != 0)
                {
                    position.makeMove(move);
                    int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, true)};
                    if (child_value < value)
                    {
                        value = child_value;
                        best_move = move;
                    }
                    position.unmakeMove(move);
                    if (value <= alpha)
                    {
                        cutoff = true;
                        break;
                    }

                    beta = std::min(beta, value);
                    move = position.nextMove(current_move, end_move, tt_move);
                }
            }
        }
    }
    // Game finished since there are no legal moves
    if (no_moves)
    {
        // Stalemate
        if (not is_check)
            return 2048;
        // Checkmate against us
        else if (our_turn)
            return - depth;
        // Checkmate against opponent
        else
            return 30000 + depth;
    }
    // Saving a tt value
    globalTT.save(position.getZobristKey(), value, depth, best_move, not cutoff);

    return value;
}

std::tuple<Move, int16_t, std::vector<int16_t>> firstMoveSearch(BitPosition &position, int8_t depth, int16_t alpha, int16_t beta, std::vector<Move> &first_moves, std::vector<int16_t> &first_moves_scores, std::chrono::milliseconds timeForMoveMS, std::chrono::milliseconds predictedTimeTakenMs, int &lastFirstMoveTimeTakenMS)
// This search is done when depth is more than 0 and considers all moves
// Note that here we have no alpha/beta cutoffs, since we are only applying the first move.
{
    TTEntry *ttEntry = globalTT.probe(position.getZobristKey());
    Move tt_move;
    // If position is stored in transposition table
    if (ttEntry != nullptr)
    {
        // If depth in ttable is lower than the one we are going to search, we just use the tt_move
        if (ttEntry->getDepth() < depth)
            tt_move = ttEntry->getMove();

        // If depth in ttable is higher or equal than the one we are going to search:
        // 1) Exact value, we just return it (no need to search at a lower depth)
        else if (ttEntry->getDepth() >= depth && ttEntry->getIsExact())
            return std::tuple<Move, int16_t, std::vector<int16_t>>(ttEntry->getMove(), ttEntry->getValue(), first_moves_scores);
        // 2) Lower bound at deeper depth and best move found
        else if (ttEntry->getDepth() >= depth)
        {
            tt_move = ttEntry->getMove();
            alpha = ttEntry->getValue();
        }
    }
    // Order the moves based on scores
    if (first_moves_scores.empty())
    {
        first_moves = position.orderAllMovesOnFirstIterationFirstTime(first_moves, tt_move);
        first_moves_scores.reserve(first_moves.size());
    }
    else
    {
        std::pair<std::vector<Move>, std::vector<int16_t>> result = position.orderAllMovesOnFirstIteration(first_moves, first_moves_scores);
        first_moves = result.first;
        first_moves_scores = result.second;
    }

    // Baseline evaluation
    int16_t value{static_cast<int16_t>(-30001)};
    Move best_move{0};

    std::chrono::time_point<std::chrono::high_resolution_clock> first_move_start_time{std::chrono::high_resolution_clock::now()};
    Move newKiller{};
    // Maximize (it's our move)
    for (std::size_t i = 0; i < first_moves.size(); ++i)
    {
        ourMoveMade = first_moves[i];
        position.makeMove(ourMoveMade);
        int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, false)};
        first_moves_scores[i] = child_value;
        if (child_value > value)
        {
            value = child_value;
            best_move = ourMoveMade;
        }
        
        position.unmakeMove(ourMoveMade);
        alpha = std::max(alpha, value);
        moveDepthValues[ourMoveMade].emplace_back(value);
        // Calculate the elapsed time in milliseconds
        std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() - STARTTIME;
        // Check if the duration has been exceeded
        if (duration >= timeForMoveMS)
            break;
    }

    // Find the time taken (milliseconds) to perform search at this depth, to predict the depth + 1 search time taken (lastFirstMoveTimeTaken is an int)
    lastFirstMoveTimeTakenMS = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::high_resolution_clock::now() - first_move_start_time)
                                   .count() + 1;

    // Saving an exact value
    globalTT.save(position.getZobristKey(), value, depth, best_move, true);

    return std::tuple<Move, int16_t, std::vector<int16_t>>(best_move, value, first_moves_scores);
}

std::pair<Move, int16_t> iterativeSearch(BitPosition position, int8_t start_depth, int8_t fixed_max_depth)
{
    moveDepthValues = {};
    std::vector<Move> first_moves;
    int lastFirstMoveTimeTakenMS {1};
    std::chrono::milliseconds timeForMoveMS{(OURTIME + OURINC) / 6};

    if (position.getIsCheck())
        first_moves = position.inCheckAllMoves();
    else
        first_moves = position.allMoves();

    // If there is only one move in the position, we make it
    if (first_moves.size() == 1) 
        return std::pair<Move, int16_t>(first_moves[0], 0);

    Move bestMove{};
    Move bestMovePreviousDepth{};
    int16_t bestValue;
    std::tuple<Move, int16_t, std::vector<int16_t>> tuple;
    std::vector<int16_t> first_moves_scores; // For first move ordering
    int streak = 1;                          // To keep track of the improvement streak

    // Iterative deepening
    for (int8_t depth = start_depth; depth <= fixed_max_depth; ++depth)
    {
        // We are going to perform N searches of time T, where:
        // N is first_moves.size() and T is lastFirstMoveTimeTakenMS
        // Hence we can predict the time taken of this new search to be N * T
        std::chrono::milliseconds predictedTimeTakenMs{first_moves.size() * lastFirstMoveTimeTakenMS};
        
        if (predictedTimeTakenMs >= timeForMoveMS)
            break;

        // Set best current values to worse possible ones (so that we try to improve them)
        int16_t alpha{-31001};
        int16_t beta{31001};

        // Search
        tuple = firstMoveSearch(position, depth, alpha, beta, first_moves, first_moves_scores, timeForMoveMS, predictedTimeTakenMs, lastFirstMoveTimeTakenMS);
        bestMove = std::get<0>(tuple);
        bestValue = std::get<1>(tuple);
        first_moves_scores = std::get<2>(tuple);

        DEPTH = static_cast<int>(depth);
        
        if (bestMove.getData() == bestMovePreviousDepth.getData())
            streak++;
        else
        {
            bestMovePreviousDepth = bestMove;
            streak = 1;
        }
        // Check stop condition based on streak and improvement pattern
        if (stopSearch(moveDepthValues[bestMove], streak, depth, position))
            break;

        // Calculate the elapsed time in milliseconds
        std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() - STARTTIME;
        // Check if the duration has been exceeded
        if (duration >= timeForMoveMS)
            break;

    }

    std::cout << "Depth: " << DEPTH << "\n";
    return std::pair<Move, int16_t>(bestMove, bestValue);
}
