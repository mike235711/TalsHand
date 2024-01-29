#ifndef ENGINE_H
#define ENGINE_H
#include "bitposition.h"
#include <vector>
#include <utility> // For std::pair
#include <limits>  // For numeric limits

// Assuming Move is a defined type in your project.
// Assuming BitPosition has methods like makeMove, unmakeMove, etc.

const int MAX_SCORE = 10000;
const int MIN_SCORE = -10000;
Move lastBestMove{0};

int evaluationFunction(BitPosition position)
{
    return 0;
}

std::pair<int, Move> alphaBeta(BitPosition &position, int depth, int alpha, int beta, bool ourTurn, int currentDepth = 0)
{
    std::vector<Move> captures, non_captures;
    // Logic to populate captures and non_captures based on the position
    // Quiesence search (Only captures)
    bool quiesence{false};
    if (position.isCheck() && depth <= 0)
    {
        position.setChecksAndPinsBits();
        captures = position.inCheckCaptures();
        quiesence = true;
    }
    else if (not position.isCheck() && depth <= 0)
    {
        position.setPinsBits();
        captures = position.captureMoves();
        quiesence = true;
    }
    // Normal search
    else if (position.isCheck())
    {
        position.setChecksAndPinsBits();
        captures = position.inCheckCaptures();
        non_captures = position.inCheckMoves();
    }
    else
    {
        position.setPinsBits();
        captures = position.captureMoves();
        non_captures = position.nonCaptureMoves();
    }

    int value = ourTurn ? MIN_SCORE : MAX_SCORE;

    if (depth <= 0 && ourTurn)
        value = evaluationFunction(position);
    
    Move bestMove;
    if (depth == currentDepth && lastBestMove.getData() != 0) 
    // If we are starting an alpha beta search and we have already a previous best we will start with the previous best
        captures.emplace_back(lastBestMove);

    for (const Move &move : captures)
    {
        position.makeMove(move);
        auto childValue = alphaBeta(position, depth - 1, alpha, beta, !ourTurn, currentDepth + 1).first;
        position.unmakeMove();

        if (ourTurn)
        {
            if (childValue > value)
            {
                value = childValue;
                bestMove = move;
            }
            alpha = std::max(alpha, value);
            if (alpha >= beta)
                break;
        }
        else
        {
            if (childValue < value)
            {
                value = childValue;
                bestMove = move;
            }
            beta = std::min(beta, value);
            if (beta <= alpha)
                break;
        }
    }

    // Repeat for non-capture moves if no cutoff occurred
    if (ourTurn ? alpha < beta : beta > alpha)
    {
        for (const Move &move : non_captures)
        {
            position.makeMove(move);
            auto childValue = alphaBeta(position, depth - 1, alpha, beta, !ourTurn, currentDepth + 1).first;
            position.unmakeMove();

            if (ourTurn)
            {
                if (childValue > value)
                {
                    value = childValue;
                    bestMove = move;
                }
                alpha = std::max(alpha, value);
                if (alpha >= beta)
                    break;
            }
            else
            {
                if (childValue < value)
                {
                    value = childValue;
                    bestMove = move;
                }
                beta = std::min(beta, value);
                if (beta <= alpha)
                    break;
            }
        }
    }

    return std::make_pair(value, bestMove);
}

#endif
