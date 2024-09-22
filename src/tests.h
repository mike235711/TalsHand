#ifndef TESTS_H
#define TESTS_H
#include "bitposition.h"
#include <vector>
#include <iostream> // For printing


// Additional helper function for printing moves
void printMove(const Move &move)
{
    // Assuming you have a way to convert 'Move' to a string or standard notation
    std::cout << move.toString() << ": ";
}

unsigned long long runCapturesPerftTest(BitPosition &position, int depth, int currentDepth = 0)
// Function to test the captures and non captures move generators, it outputs the number of
// final moves leading from each of the first legal moves (Perft Test)
{
    if (depth == 0)
        return 1;
    unsigned long long moveCount = 0;
    if (currentDepth == 0 && position.getIsCheck()) // First move search in check
    {
        // Generate all moves
        Move moves[32];
        Move *currMove = moves;
        Move *endMove = position.setCapturesInCheckTest(currMove);
        Move move = position.nextCaptureInCheck(currMove, endMove);

        Move nonCaptures[64];
        Move *currNonCapture = nonCaptures;
        Move *endNonCapture = position.setNonCapturesInCheck(currNonCapture);
        Move nonCapture = position.nextNonCapture(currNonCapture, endNonCapture);

        // Captures in check
        while (move.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeCapture(move);

            unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeCapture(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            move = position.nextCaptureInCheck(currMove, endMove);
        }
        // Non captures in check
        while (nonCapture.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(nonCapture);
            }
            position.makeMove(nonCapture);

            unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(nonCapture);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            nonCapture = position.nextNonCapture(currNonCapture, endNonCapture);
        }
    }
    else if (not position.getIsCheck()) // Non first move search not in check
    {
        // Generate all moves
        ScoredMove moves[64];
        ScoredMove *currMove = moves;
        ScoredMove* endMove = position.setCapturesAndScores(currMove);
        ScoredMove move = position.nextCapture(currMove, endMove);

        Move nonCaptures[128];
        Move *currNonCapture = nonCaptures;
        Move *endNonCapture = position.setNonCaptures(currNonCapture);
        Move nonCapture = position.nextNonCapture(currNonCapture, endNonCapture);

        // Captures not in check
        while (move.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeCapture(move);

            unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeCapture(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            move = position.nextCapture(currMove, endMove);
        }
        // Non captures not in check
        while (nonCapture.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(nonCapture);
            }
            position.makeMove(nonCapture);

            unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(nonCapture);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            nonCapture = position.nextNonCapture(currNonCapture, endNonCapture);
        }
    }
    else // Non first move search in check
    {
        // Generate all moves
        Move moves[32];
        Move *currMove = moves;
        Move *endMove = position.setCapturesInCheck(currMove);
        Move move = position.nextCaptureInCheck(currMove, endMove);

        Move nonCaptures[64];
        Move *currNonCapture = nonCaptures;
        Move *endNonCapture = position.setNonCapturesInCheck(currNonCapture);
        Move nonCapture = position.nextNonCapture(currNonCapture, endNonCapture);
        
        // Captures in check
        while (move.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeCapture(move);

            unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeCapture(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            move = position.nextCaptureInCheck(currMove, endMove);
        }
        // Non captures in check
        while (nonCapture.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(nonCapture);
            }
            position.makeMove(nonCapture);

            unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(nonCapture);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            nonCapture = position.nextNonCapture(currNonCapture, endNonCapture);
        }
    }
    return moveCount;
}

unsigned long long runNormalPerftTest(BitPosition &position, int depth, int currentDepth = 0)
// Function to test the allMoves move generator, it outputs the number of
// final moves leading from each of the first legal moves (Perft Test)
{

    if (depth == 0)
        return 1;
    unsigned long long moveCount = 0;
    if (currentDepth == 0 && position.getIsCheck()) // In check first moves
    {
        Move moves[64];
        Move *currMove = moves;
        // Set moves without score already ordered
        Move *endMove = position.setMovesInCheckTest(currMove);
        Move move = position.nextMoveInCheck(currMove, endMove);
        while (move.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move);

            unsigned long long subCount = runNormalPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            move = position.nextMoveInCheck(currMove, endMove);
        }
    }
    else if (not position.getIsCheck()) // Not in check
    {
        ScoredMove moves[256];
        ScoredMove *currMove = moves;
        // Add scored moves and set starting pointer and ending pointer
        ScoredMove *endMove = position.setMovesAndScores(currMove);
        // Go to the next move with highest score
        ScoredMove move = position.nextMove(currMove, endMove);
        while (move.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move);

            unsigned long long subCount = runNormalPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            move = position.nextMove(currMove, endMove);
        }
    }
    else // In check
    {
        Move moves[64];
        Move *currMove = moves;
        // Set moves without score already ordered
        Move *endMove = position.setMovesInCheck(currMove);
        Move move = position.nextMoveInCheck(currMove, endMove);
        while (move.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move);

            unsigned long long subCount = runNormalPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            move = position.nextMoveInCheck(currMove, endMove);
        }
    }
    return moveCount;
}

unsigned long long runFirstMovesPerftTest(BitPosition &position, int depth, int currentDepth = 0)
// Function to test the allMoves move generator, it outputs the number of
// final moves leading from each of the first legal moves (Perft Test)
{

    if (depth == 0)
        return 1;
    unsigned long long moveCount = 0;
    if (position.getIsCheck()) // In check first moves
    {
        // All moves
        std::vector<Move> first_moves = position.inCheckAllMoves();
        for (Move move : first_moves)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move);

            unsigned long long subCount = runNormalPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
        }
    }
    else // Not in check first moves
    {
        // All moves
        std::vector<Move> first_moves = position.allMoves();
        for (Move move : first_moves)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move);

            unsigned long long subCount = runNormalPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
        }
    }
    return moveCount;
}

#endif
