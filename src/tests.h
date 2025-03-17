#ifndef TESTS_H
#define TESTS_H
#include "bitposition.h"
#include "move_selectors.h"
#include "ttable.h"
#include <vector>
#include <iostream> // For printing

extern TranspositionTable globalTT;

// Additional helper function for printing moves
void printMove(const Move &move)
{
    // Assuming you have a way to convert 'Move' to a string or standard notation
    std::cout << move.toString() << ": ";
}

unsigned long long runFirstMovesPerftTest(BitPosition &position, int depth, int currentDepth = 0)
// Function to test the allMoves move generator, it outputs the number of
// final moves leading from each of the first legal moves (Perft Test)
{

    if (depth == 0)
        return 1;
    unsigned long long moveCount = 0;

    StateInfo state_info;
    position.setIsCheckOnInitialization();

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
            position.makeMove(move, state_info);

            unsigned long long subCount = runFirstMovesPerftTest(position, depth - 1, currentDepth + 1);

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
            position.makeMove(move, state_info);

            unsigned long long subCount = runFirstMovesPerftTest(position, depth - 1, currentDepth + 1);

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

unsigned long long runQSPerftTest(BitPosition &position, int depth, int currentDepth = 0)
// Function to test the captures and non captures move generators, it outputs the number of
// final moves leading from each of the first legal moves (Perft Test)
{
    if (depth == 0)
        return 1;
    unsigned long long moveCount = 0;
    StateInfo state_info;
    position.setCheckBits();

    if (not position.getIsCheck()) // Non first move search not in check
    {
        // Generate refutation moves if not first move
        Move refutation = Move(0);
        if (currentDepth)
            refutation = position.getBestRefutation();

        // Refutations not in check
        if (refutation.getData() != 0)
        {
            position.setBlockersAndPinsInQS();
            if (position.isRefutationLegal(refutation))
            {
                if (currentDepth == 0)
                {
                    printMove(refutation);
                }
                position.makeCaptureTest(refutation, state_info);

                unsigned long long subCount = runQSPerftTest(position, depth - 1, currentDepth + 1);

                position.unmakeCapture(refutation);

                if (currentDepth == 0)
                {
                    std::cout << subCount << std::endl; // Print the number of moves leading from this move
                }
                moveCount += subCount;
            }
        }

        // Captures not in check
        Move move;
        QSMoveSelectorNotCheck move_selector(position, refutation);
        move_selector.init();
        while ((move = move_selector.select_legal()) != Move(0))
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeCaptureTest(move, state_info);

            unsigned long long subCount = runQSPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeCapture(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
        }
        // Non captures not in check
        position.setBlockersAndPinsInAB();
        QSMoveSelectorNotCheckNonCaptures move_selector_non_capture(position, refutation);
        move_selector_non_capture.init();
        while ((move = move_selector_non_capture.select_legal()) != Move(0))
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move, state_info);

            unsigned long long subCount = runQSPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
        }
    }
    else // Move search in check
    {       
        // Captures in check
        Move move;
        position.setCheckInfo();
        QSMoveSelectorCheck move_selector(position);
        move_selector.init();
        while ((move = move_selector.select_legal()) != Move(0))
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeCaptureTest(move, state_info);

            unsigned long long subCount = runQSPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeCapture(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
        }
        // Non captures in check
        position.setBlockersAndPinsInAB();
        position.setCheckInfo();
        QSMoveSelectorCheckNonCaptures move_selector_non_capture(position);
        move_selector_non_capture.init();
        while ((move = move_selector_non_capture.select_legal()) != Move(0))
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move, state_info);

            unsigned long long subCount = runQSPerftTest(position, depth - 1, currentDepth + 1);

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

unsigned long long runPVPerftTest(BitPosition &position, int depth, int currentDepth = 0)
{
    if (depth == 0)
        return 1;
    unsigned long long moveCount = 0;
    Move lastMove = Move(0); // For ttTable 
    position.setBlockersAndPinsInAB(); // For discovered checks and move generators
    position.setCheckBits();
    // TTmove
    Move tt_move = Move(0);
    TTEntry *ttEntry = globalTT.probe(position.getZobristKey());
    StateInfo state_info;
    // If position is stored in ttable
    if (ttEntry != nullptr)
        tt_move = ttEntry->getMove();
    if (tt_move.getData() != 0 && position.ttMoveIsOk(tt_move))
    {
        if (currentDepth == 0)
        {
            printMove(tt_move);
        }
        position.makeMove(tt_move, state_info);

        unsigned long long subCount = runPVPerftTest(position, depth - 1, currentDepth + 1);

        position.unmakeMove(tt_move);

        if (currentDepth == 0)
        {
            std::cout << subCount << std::endl; // Print the number of moves leading from this move
        }
        moveCount += subCount;
    }

    if (position.getIsCheck()) // In check moves
    {
        position.setCheckInfo();
        Move move;
        ABMoveSelectorCheck move_selector(position, tt_move);
        move_selector.init();
        while ((move = move_selector.select_legal()) != Move(0))
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move, state_info);

            unsigned long long subCount = runPVPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
        }
    }
    else // Not in check
    {
        Move move;
        ABMoveSelectorNotCheck move_selector(position, tt_move);
        move_selector.init_all();
        while ((move = move_selector.select_legal()) != Move(0))
        {
            // if (depth == 4 && move.toString() == "g5c1")
            //     std::cout << "klk\n";
            // if (depth == 3 && move.toString() == "e7e6")
            //     std::cout << "klk\n";
            // if (depth == 2 && move.toString() == "c4e6")
            //     std::cout << "klk\n";
            lastMove = move;
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move, state_info);

            unsigned long long subCount = runPVPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
        }
    }
    // if (position.getZobristKey() == 10095184848992382700)
    //     std::cout << "klk\n";
    // Saving a tt value
    globalTT.save(position.getZobristKey(), 0, depth, lastMove, false);
    return moveCount;
}

unsigned long long runNonPVPerftTest(BitPosition &position, int depth, int currentDepth = 0)
{
    if (depth == 0)
        return 1;
    unsigned long long moveCount = 0;
    Move lastMove = Move(0);           // For ttTable
    position.setBlockersAndPinsInAB(); // For discovered checks and move generators
    position.setCheckBits();
    // TTmove
    Move tt_move = Move(0);
    TTEntry *ttEntry = globalTT.probe(position.getZobristKey());
    StateInfo state_info;
    // If position is stored in ttable
    if (ttEntry != nullptr)
        tt_move = ttEntry->getMove();
    if (tt_move.getData() != 0 && position.ttMoveIsOk(tt_move))
    {
        if (currentDepth == 0)
        {
            printMove(tt_move);
        }
        position.makeMove(tt_move, state_info);

        unsigned long long subCount = runPVPerftTest(position, depth - 1, currentDepth + 1);

        position.unmakeMove(tt_move);

        if (currentDepth == 0)
        {
            std::cout << subCount << std::endl; // Print the number of moves leading from this move
        }
        moveCount += subCount;
    }

    if (position.getIsCheck()) // In check first moves
    {
        position.setCheckInfo();
        Move move;
        ABMoveSelectorCheck move_selector(position, tt_move);
        move_selector.init();
        while ((move = move_selector.select_legal()) != Move(0))
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move, state_info);

            unsigned long long subCount = runPVPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
        }
    }
    else // Not in check
    {
        Move move;
        ABMoveSelectorNotCheck move_selector(position, tt_move);
        if (currentDepth)
        {
            move_selector.init_refutations();
            while ((move = move_selector.select_legal()) != Move(0))
            {
                lastMove = move;
                if (currentDepth == 0)
                {
                    printMove(move);
                }
                position.makeMove(move, state_info);

                unsigned long long subCount = runPVPerftTest(position, depth - 1, currentDepth + 1);

                position.unmakeMove(move);

                if (currentDepth == 0)
                {
                    std::cout << subCount << std::endl; // Print the number of moves leading from this move
                }

                moveCount += subCount;
            }
        }
        move_selector.init_good_captures();
        while ((move = move_selector.select_legal()) != Move(0))
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move, state_info);

            unsigned long long subCount = runPVPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
        }
        move_selector.init_rest();
        while ((move = move_selector.select_legal()) != Move(0))
        {
            if (currentDepth == 0)
            {
                printMove(move);
            }
            position.makeMove(move, state_info);

            unsigned long long subCount = runPVPerftTest(position, depth - 1, currentDepth + 1);

            position.unmakeMove(move);

            if (currentDepth == 0)
            {
                std::cout << subCount << std::endl; // Print the number of moves leading from this move
            }

            moveCount += subCount;
        }
    }
    // Saving a tt value
    globalTT.save(position.getZobristKey(), 0, depth, lastMove, false);
    return moveCount;
}
#endif
