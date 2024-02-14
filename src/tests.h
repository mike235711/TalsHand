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

unsigned long long runCapturesPerftTest(BitPosition& position, int depth, int currentDepth = 0)
{

    if (depth == 0)
        return 1;

    std::vector<Move> captures;
    std::vector<Move> non_captures;
    position.setAttackedSquaresAfterMove();
    if (position.isCheck())
    {
        position.setChecksAndPinsBits();
        captures = position.inCheckOrderedCaptures();
        non_captures = position.inCheckMoves();
    }
    else
    {
        position.setPinsBits();
        captures = position.orderedCaptures();
        non_captures = position.nonCaptureMoves();
    }

    unsigned long long moveCount = 0;
    for (const Move &move : captures)
    {
        if (currentDepth == 0)
        {
            printMove(move);
        }
        position.makeCapture(move);

        unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);
        
        position.unmakeMove();

        if (currentDepth == 0)
        {
            std::cout << subCount << std::endl; // Print the number of moves leading from this move
        }

        moveCount += subCount;
    }
    for (const Move &move : non_captures)
    {
        if (currentDepth == 0)
        {
            printMove(move);
        }
        position.makeNormalMove(move);

        unsigned long long subCount = runCapturesPerftTest(position, depth - 1, currentDepth + 1);

        position.unmakeMove();

        if (currentDepth == 0)
        {
            std::cout << subCount << std::endl; // Print the number of moves leading from this move
        }

        moveCount += subCount;
    }
    return moveCount;
}
unsigned long long runNormalPerftTest(BitPosition &position, int depth, int currentDepth = 0)
{

    if (depth == 0)
        return 1;

    std::vector<Move> moves;
    position.setAttackedSquaresAfterMove();
    if (position.isCheck())
    {
        position.setChecksAndPinsBits();
        moves = position.inCheckAllMoves();
    }
    else
    {
        position.setPinsBits();
        moves = position.allMoves();
    }

    unsigned long long moveCount = 0;
    for (const Move &move : moves)
    {
        if (currentDepth == 0)
        {
            printMove(move);
        }
        position.makeNormalMove(move);

        unsigned long long subCount = runNormalPerftTest(position, depth - 1, currentDepth + 1);

        position.unmakeMove();

        if (currentDepth == 0)
        {
            std::cout << subCount << std::endl; // Print the number of moves leading from this move
        }

        moveCount += subCount;
    }
    return moveCount;
}
#endif
