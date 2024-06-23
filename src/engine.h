#ifndef ENGINE_H
#define ENGINE_H
#include "bitposition.h"
#include <vector>
#include <utility> // For std::pair
#include <limits>  // For numeric limits
#include <array>
#include <cstdint>
#include <chrono>
#include <algorithm> // For std::max
#include "ttable.h"
#include <torch/script.h>
#include <memory>
#include "position_eval.h"


extern TranspositionTable globalTT;

float quiesenceSearch(BitPosition &position, float alpha, float beta, bool our_turn)
// This search is done when depth is less than or equal to 0 and considers only captures and promotions
{
    std::vector<Capture> moves;
    position.setAttackedSquaresAfterMove();
    if (position.isCheck())
    {
        position.setChecksAndPinsBits();
        moves = position.inCheckOrderedCaptures();
    }
    else
    {
        position.setPinsBits();
        moves = position.orderedCaptures();
    }
    // If we have reached quisence search and there are no captures
    if (moves.size() == 0)
    {
        if (not position.isCheck())
            return evaluationFunction(position, our_turn);
        else if (position.inCheckAllMoves().size() == 0) // Mate
        {
            if (our_turn)
                return -32764;
            else
                return 32764;
        }
    }
    // If we are in quiescence, we have a baseline evaluation as if no captures happened
    float value{evaluationFunction(position, our_turn)};
    Capture best_move;
    if (our_turn) // Maximize
    {
        for (Capture move : moves)
        {
            position.makeCapture(move);
            float child_value{quiesenceSearch(position, alpha, beta, false)};
            if (child_value > value)
            {
                value = child_value;
                best_move = move;
            }
            position.unmakeCapture(move);
            if (value >= beta)
                break;
            alpha = std::max(alpha, value);
        }
    }
    else // Minimize
    {
        for (Capture move : moves)
        {
            position.makeCapture(move);
            float child_value{quiesenceSearch(position, alpha, beta, true)};
            if (child_value < value)
            {
                value = child_value;
                best_move = move;
            }
            position.unmakeCapture(move);
            if (value <= alpha)
                break;
            beta = std::min(beta, value);
        }
    }
    return value;
}

float alphaBetaSearch(BitPosition &position, int8_t depth, float alpha, float beta, bool our_turn)
// This search is done when depth is more than 0 and considers all moves and stores positions in the transposition table
{
    if (position.isThreeFold())
        return 0;

    TTEntry *ttEntry = globalTT.probe(position.getZobristKey());

    Move tt_move;
    // If position is stored
    if (ttEntry != nullptr)
    {
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

    if (depth <= 0)
        return quiesenceSearch(position, alpha, beta, our_turn);

    position.setAttackedSquaresAfterMove();
    std::vector<Move> moves;
    if (position.isCheck())
    {
        position.setChecksAndPinsBits();
        moves = position.inCheckAllMoves();
        moves = position.orderAllMoves(moves, tt_move);
    }
    else
    {
        position.setPinsBits();
        moves = position.allMoves();
        moves = position.orderAllMoves(moves, tt_move);
    }
    if (moves.size() == 0)
    {
        // Stalemate
        if (not position.getIsCheck())
            return 0;
        // Checkmate against us
        else if (our_turn)
            return -32764;
        // Checkmate against opponent
        else
            return 32764;
    }
    // If we are in quiescence, we have a baseline evaluation as if no captures happened
    float value{our_turn ? static_cast <float>(-32765) : static_cast <float>(32765)};
    Move best_move;
    bool cutoff{false};
    // Maximize
    if (our_turn)
    {
        for (Move move : moves)
        {
            position.makeNormalMove(move);
            float child_value{alphaBetaSearch(position, depth - 1, alpha, beta, false)};
            if (child_value > value)
            {
                value = child_value;
                best_move = move;
            }
            position.unmakeNormalMove(move);
            if (value >= beta)
            {
                cutoff = true;
                if (depth >= 2)
                    globalTT.save(position.getZobristKey(), value, depth, best_move, false);
                break;
            }
            alpha = std::max(alpha, value);
        }
    }
    // Minimize
    else
    {
        for (Move move : moves)
        {
            position.makeNormalMove(move);
            float child_value{alphaBetaSearch(position, depth - 1, alpha, beta, true)};
            if (child_value < value)
            {
                value = child_value;
                best_move = move;
            }
            position.unmakeNormalMove(move);
            if (value <= alpha)
            {
                cutoff = true;
                if (depth >= 2)
                    globalTT.save(position.getZobristKey(), value, depth, best_move, false);
                break;
            }
            beta = std::min(beta, value);
        }
    }
    // Saving an exact value
    if (not cutoff && depth >= 2)
        globalTT.save(position.getZobristKey(), value, depth, best_move, true);
    return value;
}

std::pair<Move, float> firstMoveSearch(BitPosition &position, int8_t depth, float alpha, float beta, bool our_turn, Move last_best_move)
// This search is done when depth is more than 0 and considers all moves
{
    position.setAttackedSquaresAfterMove();
    
    TTEntry *ttEntry = globalTT.probe(position.getZobristKey());

    Move tt_move;
    // If position is stored
    if (ttEntry != nullptr)
    {
        // At ttable's shalower depth we get the best move
        if (ttEntry->getDepth() < depth)
            tt_move = ttEntry->getMove();
        // Exact value at deeper depth
        else if (ttEntry->getDepth() >= depth && ttEntry->getIsExact())
            return std::pair<Move, float> (ttEntry->getMove(), ttEntry->getValue());
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
    std::vector<Move> moves;
    if (position.isCheck())
    {
        position.setChecksAndPinsBits();
        moves = position.inCheckAllMoves();
        moves = position.orderAllMovesOnFirstIteration(moves, last_best_move, tt_move);
    }
    else
    {
        position.setPinsBits();
        moves = position.allMoves();
        moves = position.orderAllMovesOnFirstIteration(moves, last_best_move, tt_move);
    }
    // If we are in quiescence, we have a baseline evaluation as if no captures happened
    float value{our_turn ? static_cast <float>(-32765) : static_cast <float>(32765)};
    Move best_move;
    bool cutoff{false};
    // Maximize
    if (our_turn)
    {
        for (Move move : moves)
        {
            position.makeNormalMove(move);
            float child_value{alphaBetaSearch(position, depth - 1, alpha, beta, false)};
            if (child_value > value)
            {
                value = child_value;
                best_move = move;
            }
            position.unmakeNormalMove(move);
            if (value >= beta)
            {
                cutoff = true;
                if (depth >= 2)
                    globalTT.save(position.getZobristKey(), value, depth, best_move, false);
                break;
            }
            alpha = std::max(alpha, value);
        }
    }
    // Minimize
    else
    {
        for (Move move : moves)
        {
            position.makeNormalMove(move);
            float child_value{alphaBetaSearch(position, depth - 1, alpha, beta, true)};
            if (child_value < value)
            {
                value = child_value;
                best_move = move;
            }
            position.unmakeNormalMove(move);
            if (value <= alpha)
            {
                cutoff = true;
                if (depth >= 2)
                    globalTT.save(position.getZobristKey(), value, depth, best_move, false);
                break;
            }
            beta = std::min(beta, value);
        }
    }
    // Saving an exact value
    if (not cutoff && depth >= 2)
        globalTT.save(position.getZobristKey(), value, depth, best_move, true);

    return std::pair<Move, float>(best_move, value);
}

std::pair <Move, float> iterativeSearch(BitPosition position, int time_for_move, int8_t fixed_max_depth = 100)
{
    std::chrono::milliseconds timeForMoveMS(time_for_move);
    auto start_time = std::chrono::high_resolution_clock::now(); // Start timing
    Move bestMove{};
    float bestValue;
    std::pair<Move, float> pair;
    for (int8_t depth = 1; depth <= fixed_max_depth; ++depth)
    {
        float alpha{-32766};
        float beta{32766};
        // Get the current time
        auto now = std::chrono::steady_clock::now();
        pair = firstMoveSearch(position, depth, alpha, beta, true, bestMove);
        bestMove = pair.first;
        bestValue = pair.second;
        // Calculate the elapsed time
        std::chrono::duration<double> duration = std::chrono::high_resolution_clock::now() - start_time; // Calculate duration

        // Check if the duration has been exceeded
        if (duration >= timeForMoveMS)
        {
            std::cout << "Depth: " << unsigned(depth) << "\n";
            break;
        }
    }
    return pair;
}
#endif
