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
        Move move = position.nextMove(currMove, endMove);

        Move nonCaptures[64];
        Move *currNonCapture = nonCaptures;
        Move *endNonCapture = position.setNonCapturesInCheck(currNonCapture);
        Move nonCapture = position.nextMove(currNonCapture, endNonCapture);

        // Captures in check
        while (move.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeCaptureWithoutNNUE(move);

            unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeCaptureWithoutNNUE(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            move = position.nextMove(currMove, endMove);
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
            nonCapture = position.nextMove(currNonCapture, endNonCapture);
        }
    }
    else if (not position.getIsCheck() && currentDepth == 0) // First move search not in check
    {
        ScoredMove moves[256];
        ScoredMove *currMove = moves;
        // Add scored moves and set starting pointer and ending pointer
        ScoredMove *endMove = position.setMovesAndScores(currMove);
        // Go to the next move with highest score
        ScoredMove move = position.nextScoredMove(currMove, endMove, Move(0));
        while (move.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move);

            unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            move = position.nextScoredMove(currMove, endMove, Move(0));
        }
    }
    else if (not position.getIsCheck()) // Non first move search not in check
    {
        // Generate pawn refutation moves
        Move refutationMove = position.getBestRefutation();
        // Pawn captures not in check
        if (refutationMove.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(refutationMove);
            }
            // else
            // {
            //     printMove(refutationMove);
            // }
            position.makeCaptureWithoutNNUE(refutationMove);

            unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeCaptureWithoutNNUE(refutationMove);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }
            moveCount += subCount;
        }

        // Generate all moves
        ScoredMove moves[64];
        ScoredMove *currMove = moves;
        ScoredMove* endMove = position.setCapturesAndScores(currMove);
        ScoredMove move = position.nextScoredMove(currMove, endMove, refutationMove);

        Move nonCaptures[128];
        Move *currNonCapture = nonCaptures;
        Move *endNonCapture = position.setNonCaptures(currNonCapture);
        Move nonCapture = position.nextMove(currNonCapture, endNonCapture);

        // Captures not in check
        while (move.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            // else
            // {
            //     printMove(move);
            // }
            position.makeCaptureWithoutNNUE(move);

            unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeCaptureWithoutNNUE(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            move = position.nextScoredMove(currMove, endMove, refutationMove);
        }
        // Non captures not in check
        while (nonCapture.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(nonCapture);
            }
            // else
            // {
            //     printMove(nonCapture);
            //     std::cout << "Depth: " << currentDepth << "\n";
            // }
            position.makeMove(nonCapture);

            unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(nonCapture);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            nonCapture = position.nextMove(currNonCapture, endNonCapture);
        }
    }
    else // Non first move search in check
    {
        // Generate all moves
        Move moves[32];
        Move *currMove = moves;
        Move *endMove = position.setOrderedCapturesInCheck(currMove);
        Move move = position.nextMove(currMove, endMove);

        Move nonCaptures[64];
        Move *currNonCapture = nonCaptures;
        Move *endNonCapture = position.setNonCapturesInCheck(currNonCapture);
        Move nonCapture = position.nextMove(currNonCapture, endNonCapture);
        
        // Captures in check
        while (move.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeCaptureWithoutNNUE(move);

            unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeCaptureWithoutNNUE(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            move = position.nextMove(currMove, endMove);
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
            nonCapture = position.nextMove(currNonCapture, endNonCapture);
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
        Move move = position.nextMove(currMove, endMove, Move(0));
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
            move = position.nextMove(currMove, endMove, Move(0));
        }
    }
    else if (not position.getIsCheck()) // Not in check
    {
        ScoredMove moves[256];
        ScoredMove *currMove = moves;
        // Add scored moves and set starting pointer and ending pointer
        ScoredMove *endMove = position.setMovesAndScores(currMove);
        // Go to the next move with highest score
        ScoredMove move = position.nextScoredMove(currMove, endMove, Move(0));
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
            move = position.nextScoredMove(currMove, endMove, Move(0));
        }
    }
    else // In check
    {
        Move moves[64];
        Move *currMove = moves;
        // Set moves without score already ordered
        Move *endMove = position.setMovesInCheck(currMove);
        Move move = position.nextMove(currMove, endMove, Move(0));
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
            move = position.nextMove(currMove, endMove, Move(0));
        }
    }
    return moveCount;
}

unsigned long long runTTPerftTest(BitPosition &position, int depth, int currentDepth = 0)
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
        Move move = position.nextMove(currMove, endMove, Move(0));
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
            move = position.nextMove(currMove, endMove, Move(0));
        }
    }
    else if (not position.getIsCheck()) // Not in check
    {
        ScoredMove moves[256];
        ScoredMove *currMove = moves;
        // Add scored moves and set starting pointer and ending pointer
        ScoredMove *endMove = position.setMovesAndScores(currMove);
        // TTMove 
        ScoredMove ttmove = position.nextScoredMove(currMove, endMove, Move(0));
        if (ttmove.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(ttmove);
            }
            position.makeTTMove(ttmove);

            unsigned long long subCount = runNormalPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeTTMove(ttmove);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }
            moveCount += subCount;
        }
        // Go to the next move with highest score
        ScoredMove move = position.nextScoredMove(currMove, endMove, ttmove);
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
            move = position.nextScoredMove(currMove, endMove, ttmove);
        }
    }
    else // In check
    {
        Move moves[64];
        Move *currMove = moves;
        // Set moves without score already ordered
        Move *endMove = position.setMovesInCheck(currMove);
        // TTMove
        Move ttmove = position.nextMove(currMove, endMove, Move(0));
        if (ttmove.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(ttmove);
            }
            position.makeTTMove(ttmove);

            unsigned long long subCount = runNormalPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeTTMove(ttmove);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
        }
        Move move = position.nextMove(currMove, endMove, ttmove);
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
            move = position.nextMove(currMove, endMove, ttmove);
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

unsigned long long runNonPVMovesPerftTest(BitPosition &position, int depth, int currentDepth = 0)
// Function to test the captures and non captures move generators, it outputs the number of
// final moves leading from each of the first legal moves (Perft Test)
{
    if (depth == 0)
        return 1;
    unsigned long long moveCount = 0;
    if (currentDepth == 0 && position.getIsCheck()) // First move search in check
    {
        Move moves[64];
        Move *currMove = moves;
        // Set moves without score already ordered
        Move *endMove = position.setMovesInCheckTest(currMove);
        Move move = position.nextMove(currMove, endMove, Move(0));
        while (move.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move);

            unsigned long long subCount = runNonPVMovesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            move = position.nextMove(currMove, endMove, Move(0));
        }
    }
    else if (not position.getIsCheck() && currentDepth == 0)
    {
        ScoredMove moves[256];
        ScoredMove *currMove = moves;
        // Add scored moves and set starting pointer and ending pointer
        ScoredMove *endMove = position.setMovesAndScores(currMove);
        // Go to the next move with highest score
        ScoredMove move = position.nextScoredMove(currMove, endMove, Move(0));
        while (move.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move);

            unsigned long long subCount = runNonPVMovesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            move = position.nextScoredMove(currMove, endMove, Move(0));
        }
    }
    else if (not position.getIsCheck()) // Non first move search not in check
    {
        // std::cout << "Refutation Moves: \n";
        // Refutation moves
        Move refutationMoves[64];
        Move *currRefutationMove = refutationMoves;
        Move *endRefutationMove = position.setRefutationMovesOrdered(currRefutationMove);
        Move refutationMove = position.nextMove(currRefutationMove, endRefutationMove);

        while (refutationMove.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(refutationMove);
            }
            // else
            // {
            //     printMove(refutationMove);
            //     std::cout << "Depth: " << currentDepth << "\n";
            // }
            position.makeMove(refutationMove);

            unsigned long long subCount = runNonPVMovesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(refutationMove);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            refutationMove = position.nextMove(currRefutationMove, endRefutationMove);
        }

        // std::cout << "Good Captures: \n";
        // Good captures not in check
        Move goodCaptures[64];
        Move *currGoodCapture = goodCaptures;
        Move *endGoodCapture = position.setGoodCapturesOrdered(currGoodCapture);
        Move goodCapture = position.nextMove(currGoodCapture, endGoodCapture);
        while (goodCapture.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(goodCapture);
            }
            // else
            // {
            //     printMove(goodCapture);
            //     std::cout << "Depth: " << currentDepth << "\n";
            // }
            position.makeMove(goodCapture);

            unsigned long long subCount = runNonPVMovesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(goodCapture);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            goodCapture = position.nextMove(currGoodCapture, endGoodCapture);
        }
        // std::cout << "Safe Moves: \n";
        // Safe moves
        ScoredMove safeMoves[128];
        ScoredMove *currSafeMove = safeMoves;
        ScoredMove *endSafeMove = position.setSafeMovesAndScores(currSafeMove);
        ScoredMove safeMove = position.nextScoredMove(currSafeMove, endSafeMove);
        while (safeMove.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(safeMove);
            }
            // else
            // {
            //     printMove(safeMove);
            //     std::cout << "Depth: " << currentDepth << "\n";
            // }
            position.makeMove(safeMove);

            unsigned long long subCount = runNonPVMovesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(safeMove);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            safeMove = position.nextScoredMove(currSafeMove, endSafeMove);
        }
        // std::cout << "Bad Moves: \n";
        // Bad captures and unsafe moves
        Move badCaptures[64];
        Move *currBadCapture = badCaptures;
        Move *endBadCapture = position.setBadCapturesOrUnsafeMoves(currBadCapture);
        Move badCapture = position.nextMove(currBadCapture, endBadCapture);
        while (badCapture.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(badCapture);
            }
            // else
            // {
            //     printMove(badCapture);
            //     std::cout << "Depth: " << currentDepth << "\n";
            // }
            position.makeMove(badCapture);

            unsigned long long subCount = runNonPVMovesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(badCapture);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            badCapture = position.nextMove(currBadCapture, endBadCapture);
        }
    }
    else // Non first move search in check
    {
        Move moves[64];
        Move *currMove = moves;
        // Set moves without score already ordered
        Move *endMove = position.setMovesInCheck(currMove);
        Move move = position.nextMove(currMove, endMove, Move(0));
        while (move.getData() != 0)
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move);

            unsigned long long subCount = runNonPVMovesPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
            move = position.nextMove(currMove, endMove, Move(0));
        }
    }
    return moveCount;
}
#endif
