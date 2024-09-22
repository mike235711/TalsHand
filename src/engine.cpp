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


int16_t quiesenceSearch(BitPosition &position, int16_t alpha, int16_t beta, bool our_turn)
// This search is done when depth is less than or equal to 0 and considers only captures and promotions
{
    // If we are in quiescence, we have a baseline evaluation as if no captures happened
    int16_t value{NNUEU::evaluationFunction(our_turn)};
    Move best_move;
    bool no_captures{true};
    if (not position.getIsCheck()) // Not in check
    {
        ScoredMove moves[64];
        ScoredMove *current_move = moves;
        ScoredMove *end_move = position.setCapturesAndScores(current_move);
        ScoredMove capture = position.nextCapture(current_move, end_move);
        
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
                capture = position.nextCapture(current_move, end_move);
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
                capture = position.nextCapture(current_move, end_move);
            }
        }
    }
    else // In check
    {
        // Before generating in check moves we need the checks info
        Move moves[32];
        Move *current_move = moves;
        Move *end_move = position.setCapturesInCheck(current_move);
        Move capture = position.nextCaptureInCheck(current_move, end_move);
        
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
                capture = position.nextCaptureInCheck(current_move, end_move);
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
                capture = position.nextCaptureInCheck(current_move, end_move);
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
                    return -30000;
                else
                    return 30000;
            }
            // In check quiet position
            else
                return NNUEU::evaluationFunction(our_turn);
        }
        // If there is no check we check for bad captures
        else
        {
            // Stalemate
            if (position.isStalemate())
                return 0;
            // Quiet position
            else
                return NNUEU::evaluationFunction(our_turn);
        }
    }
    return value;
}

int16_t alphaBetaSearch(BitPosition &position, int8_t depth, int16_t alpha, int16_t beta, bool our_turn)
// This search is done when depth is more than 0 and considers all moves and stores positions in the transposition table
{
    // Threefold repetition
    if (position.isThreeFold())
        return 0;

    bool no_moves{true};
    bool cutoff{false};

    // Baseline evaluation
    int16_t value{our_turn ? static_cast<int16_t>(-30001) : static_cast<int16_t>(30001)};
    Move best_move;

    // Check if we have stored this position in ttable
    TTEntry *ttEntry = globalTT.probe(position.getZobristKey());
    Move tt_move;
    // If position is stored in ttable
    if (ttEntry != nullptr)
    {
        no_moves = false;
        // At ttable's shalower depth we get the best move
        if (ttEntry->getDepth() < depth)
            tt_move = ttEntry->getMove();
        // Exact value at deeper depth
        else if (ttEntry->getDepth() >= depth && ttEntry->getIsExact())
            return ttEntry->getValue();
        // Lower bound at deeper depth
        else if (ttEntry->getDepth() >= depth && our_turn)
        {
            tt_move = ttEntry->getMove();
            alpha = ttEntry->getValue();
        }
        // Upper bound at deeper depth
        else
        {
            tt_move = ttEntry->getMove();
            beta = ttEntry->getValue();
        }
    }
    // At depths <= 0 we enter quiesence search
    if (depth <= 0)
        return quiesenceSearch(position, alpha, beta, our_turn);

    // We only search if tt_move didn't produce a cutoff in the search tree
    if (not cutoff)
    {
        if (not position.getIsCheck()) // Not in check
        {
            ScoredMove moves[256];
            ScoredMove *current_move = moves;
            ScoredMove *end_move = position.setMovesAndScores(current_move);
            ScoredMove move = position.nextMove(current_move, end_move);
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
                    move = position.nextMove(current_move, end_move);
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
                    move = position.nextMove(current_move, end_move);
                }
            }
        }
        else // In check
        {
            // Before generating in check moves we need the checks info
            Move moves[64];
            Move *current_move = moves;
            Move *end_move = position.setMovesInCheck(current_move);
            Move move = position.nextMoveInCheck(current_move, end_move);
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
                    move = position.nextMoveInCheck(current_move, end_move);
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
                    move = position.nextMoveInCheck(current_move, end_move);
                }
            }
        }
    }
    // Game finished since there are no legal moves
    if (no_moves)
    {
        // Stalemate
        if (not position.getIsCheck())
            return 0;
        // Checkmate against us
        else if (our_turn)
            return -30000;
        // Checkmate against opponent
        else
            return 30000;
    }
    // Saving a tt value
    if (depth >= 2)
        globalTT.save(position.getZobristKey(), value, depth, best_move, not cutoff);

    return value;
}

std::tuple<Move, int16_t, std::vector<int16_t>> firstMoveSearch(BitPosition &position, int8_t depth, int16_t alpha, int16_t beta, std::vector<Move> &first_moves, std::vector<int16_t> &first_moves_scores, std::chrono::milliseconds timeForMoveMS)
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
    Move best_move;

    // Maximize (it's our move)
    for (std::size_t i = 0; i < first_moves.size(); ++i)
    {
        Move move{first_moves[i]};
        position.makeMove(move);
        int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, false)};
        first_moves_scores[i] = child_value;
        if (child_value > value)
        {
            value = child_value;
            best_move = move;
        }
        position.unmakeMove(move);
        alpha = std::max(alpha, value);

        // Calculate the elapsed time in milliseconds
        std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() - STARTTIME;
        // Check if the duration has been exceeded
        if (duration >= timeForMoveMS)
            break;
    }

    // Saving an exact value
    if (depth >= 2)
        globalTT.save(position.getZobristKey(), value, depth, best_move, true);

    return std::tuple<Move, int16_t, std::vector<int16_t>>(best_move, value, first_moves_scores);
}

std::pair<Move, int16_t> iterativeSearch(BitPosition position, int8_t fixed_max_depth)
{ 
    std::vector<Move> first_moves;

    if (position.getIsCheck())
        first_moves = position.inCheckAllMoves();
    else
        first_moves = position.allMoves();

    // If there is only one move in the position, we make it
    if (first_moves.size() == 1) 
        return std::pair<Move, int16_t>(first_moves[0], 0);
    
    
    std::chrono::milliseconds timeForMoveMS{(OURTIME + OURINC) / 20};
    Move bestMove{};
    int16_t bestValue;
    std::tuple<Move, int16_t, std::vector<int16_t>> tuple;
    std::vector<int16_t> first_moves_scores; // For first move ordering
    int8_t last_depth;
    
    // Iterative deepening
    for (int8_t depth = 1; depth <= fixed_max_depth; ++depth)
    {
        int16_t alpha{-30002};
        int16_t beta{30002};

        // Search
        tuple = firstMoveSearch(position, depth, alpha, beta, first_moves, first_moves_scores, timeForMoveMS);
        bestMove = std::get<0>(tuple);
        bestValue = std::get<1>(tuple);
        first_moves_scores = std::get<2>(tuple);

        last_depth = depth;

        // Calculate the elapsed time in milliseconds
        std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() - STARTTIME;
        // Check if the duration has been exceeded
        if (duration >= timeForMoveMS)
            break;
    }
    std::cout << "Depth: " << unsigned(last_depth) << "\n";
    return std::pair<Move, int16_t>(bestMove, bestValue);
}