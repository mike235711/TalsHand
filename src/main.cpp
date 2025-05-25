#include <iostream>
#include <string>
#include <sstream>
#include "precomputed_moves.h"
#include "bitposition.h"
#include "tests.h"
#include <chrono>
#include <random>
#include "magicmoves.h"
#include "engine.h"
#include "zobrist_keys.h"
#include "ttable.h"
#include "position_eval.h"
#include "data_generator.h"
#include "nnue_ttable.h"
#include <fstream>
#include <vector>
#include <cstdlib>
#include "memory.h"
#include "move_selectors.h"

TranspositionTable globalTT;
TranspositionTableNNUE nnueTT; // For position generator

bool ENGINEISWHITE;
int OURTIME{1200};                                                     // Time left
int OURINC{1200};                                                      // Increment per move
std::chrono::time_point<std::chrono::high_resolution_clock> STARTTIME; // Starting thinking time point
int TTSIZE{22};
std::array<StateInfo, 256> state_info_array;

void printArray(const char *name, const int16_t *array, size_t size)
{
    std::cout << name << ": ";
    for (size_t i = 0; i < size; ++i)
    {
        std::cout << array[i] << " ";
    }
    std::cout << std::endl;
}

Move findNormalMoveFromString(std::string moveString, BitPosition position)
{
    if (position.getIsCheck())
    {
        position.setCheckInfo();
        position.setBlockersPinsAndCheckBitsInQS();
        // Captures
        Move move;
        QSMoveSelectorCheck move_selector_1(position);
        move_selector_1.init();
        while ((move = move_selector_1.select_legal()) != Move(0))
        {
            if (move.toString() == moveString)
                return move;
        }
        // Non Captures
        QSMoveSelectorCheckNonCaptures move_selector_2(position);
        move_selector_2.init();
        while ((move = move_selector_2.select_legal()) != Move(0))
        {
            if (move.toString() == moveString)
                return move;
        }
    }
    else
    {
        position.setBlockersPinsAndCheckBitsInQS();
        // Captures
        Move move;
        QSMoveSelectorNotCheck move_selector_1(position);
        move_selector_1.init();
        while ((move = move_selector_1.select_legal()) != Move(0))
        {
            if (move.toString() == moveString)
                return move;
        }
        // Non Captures
        QSMoveSelectorNotCheckNonCaptures move_selector_2(position, Move(0));
        move_selector_2.init();
        while ((move = move_selector_2.select_legal()) != Move(0))
        {
            if (move.toString() == moveString)
                return move;
        }
    }
    return Move(0);
}
Move findCaptureMoveFromString(std::string moveString, BitPosition position)
// Find a capture or non capture move from a string
{
    if (position.getIsCheck())
    {
        position.setCheckInfo();
        position.setBlockersPinsAndCheckBitsInQS();
        // Captures
        Move move;
        QSMoveSelectorCheck move_selector_1(position);
        move_selector_1.init();
        while ((move = move_selector_1.select_legal()) != Move(0))
        {
            if (move.toString() == moveString)
                return move;
        }
        // Non Captures
        QSMoveSelectorCheckNonCaptures move_selector_2(position);
        move_selector_2.init();
        while ((move = move_selector_2.select_legal()) != Move(0))
        {
            if (move.toString() == moveString)
                return move;
        }
    }
    else
    {
        position.setBlockersPinsAndCheckBitsInQS();
        // Captures
        Move move;
        QSMoveSelectorNotCheck move_selector_1(position);
        move_selector_1.init();
        while ((move = move_selector_1.select_legal()) != Move(0))
        {
            if (move.toString() == moveString)
                return move;
        }
        // Non Captures
        QSMoveSelectorNotCheckNonCaptures move_selector_2(position, Move(0));
        move_selector_2.init();
        while ((move = move_selector_2.select_legal()) != Move(0))
        {
            if (move.toString() == moveString)
                return move;
        }
    }
    return Move(0);
}

int main()
{
    int startDepth = 1;

    // Initialize std::vectors of NNUEInput layers as global variables
    NNUEU::initNNUEParameters();
    // precomputed_moves::init_precomputed_moves();

    // precomputed_moves::pretty_print_all();

    // Initialize magic numbers and zobrist numbers
    initmagicmoves();
    zobrist_keys::initializeZobristNumbers();

    // zobrist_keys::printAllZobristKeys();

    // Initialize position object
    std::string inputLine;
    std::string lastFen; // Variable to store the last FEN string
    BitPosition position{BitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")};

    globalTT.resize(1 << TTSIZE);

    bool fromStart;
    int movesMade = 0;
    Move lastMove;

    // Simple loop to read commands from the Python GUI following UCI communication protocol
    while (std::getline(std::cin, inputLine))
    {
        // Process initial input from GUI
        std::istringstream iss(inputLine);
        std::string command;
        iss >> command;
        if (command == "uci")
        {
            std::cout << "id name La_Mano_de_Tahl\n"
                      << std::flush;
            std::cout << "id author Miguel_Cordoba\n"
                      << std::flush;
            std::cout << "uciok\n"
                      << std::flush;
        }
        else if (command == "isready")
        {
            std::cout << "readyok\n";
        }
        // End process if GUI asks kindly
        else if (command == "quit")
        {
            break;
        }
        // Setting up position
        else if (command == "position")
        {
            state_info_array = std::array<StateInfo, 256>{};
            iss >> command;
            std::string fen;
            std::string line;
            if (command == "startpos")
            {
                fromStart = true;
                fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
                iss >> command; // Consume the 'moves' token if it's there
            }
            else if (command == "fen")
            {
                fromStart = false;
                std::string line;
                std::getline(iss, line);

                // Remove the leading space if any
                if (!line.empty() && line[0] == ' ')
                    line.erase(0, 1);

                // Now find where " moves" begins, if it exists
                // so we can separate the FEN part from the moves part.
                size_t pos = line.find(" moves");
                if (pos != std::string::npos)
                {
                    // Extract the pure FEN substring
                    fen = line.substr(0, pos);

                    // Extract the moves substring, e.g. " moves c2c4 e2e4"
                    std::string movesPart = line.substr(pos); // => " moves c2c4 e2e4"

                    // Remove the initial " moves"
                    movesPart.erase(0, 6);

                    // Now we clear the old stream and replace its buffer with movesPart
                    // so that while(iss >> command) can parse "c2c4", "e2e4", etc.
                    iss.clear();
                    iss.str(movesPart);
                }
                else
                {
                    // No " moves" found; the whole line is the FEN
                    fen = line;
                }
            }

            position = BitPosition(fen);
            movesMade = 0;
            Move move;
            bool reseterMove{false};
            while (iss >> command)
            {
                // Apply each move in the moves list
                std::string moveUci{command};
                move = findNormalMoveFromString(moveUci, position);

                if (move.getData() != 0)
                {
                    lastMove = move;
                    if (position.moveIsReseter(move))
                        reseterMove = true;
                    else
                        reseterMove = false;

                    position.setBlockersAndPinsInAB();
                    position.setCheckBits(); // For direct checks
                    position.makeMove(move, state_info_array[movesMade++]);
                }
            }
            // If the last move made was a reseter we reset the ttable
            if (reseterMove)
                globalTT.resize(1 << TTSIZE);
        }
        // Thinking after opponent made move
        else if (inputLine.substr(0, 2) == "go")
        {
            ENGINEISWHITE = position.getTurn();
            // Get our time left and increment
            while (iss >> command)
            {
                if (command == "wtime" && ENGINEISWHITE)
                    iss >> OURTIME;
                else if (command == "btime" && (ENGINEISWHITE == false))
                    iss >> OURTIME;
                else if (command == "winc" && ENGINEISWHITE)
                    iss >> OURINC;
                else if (command == "binc" && (ENGINEISWHITE == false))
                    iss >> OURINC;
            }
            if (fromStart && movesMade == 0)
            {
                // Return a random move
                std::array<std::string, 2> opening_moves = {"e2e4", "d2d4"};
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<int> dist(0, 1);

                std::string random_move = opening_moves[dist(gen)];
                std::cout << "bestmove " << random_move << "\n"
                          << std::flush;
            }
            else if ((fromStart) && (movesMade == 1) && (lastMove.toString() == "e2e4"))
            {
                // Return a random move
                std::array<std::string, 2> opening_moves = {"e7e5", "c7c6"};
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<int> dist(0, 1);

                std::string random_move = opening_moves[dist(gen)];
                std::cout << "bestmove " << random_move << "\n"
                          << std::flush;
            }
            else if ((fromStart) && (movesMade == 1) && (lastMove.toString() == "d2d4"))
            {
                // Return a random move
                std::array<std::string, 2> opening_moves = {"d7d5", "g8f6"};
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<int> dist(0, 1);

                std::string random_move = opening_moves[dist(gen)];
                std::cout << "bestmove " << random_move << "\n"
                          << std::flush;
            }
            else
            {
                NNUEU::globalAccumulatorStack.reset(position);
                std::cout << position.toFenString() << "\n";
                // Call the engine
                STARTTIME = std::chrono::high_resolution_clock::now();
                startDepth = 2;
                auto [bestMove, bestValue]{iterativeSearch(position, startDepth)};

                // Send our best move through a UCI command
                std::cout << "Eval: " << bestValue << "\n";
                std::cout << "bestmove " << bestMove.toString() << "\n"
                          << std::flush;

                // Check transposition table memory
                // globalTT.printTableMemory();
            }
        }

        ////////////////////////////////////////////////////////////
        // Some tests to see efficiency and algorithm correctness
        ////////////////////////////////////////////////////////////

        // Test setMovesAndScores and setMovesInCheck generators efficiency
        else if (inputLine == "pVPerftTests")
        {
            int maxDepth;
            // Prompt for minimum evaluation difference
            std::cout << "Max depth: \n";
            while (!(std::cin >> maxDepth))
            {
                std::cin.clear();                                                   // clear the error flag
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // discard invalid input
                std::cout << "Invalid input. Please enter a integer: \n";
            }
            std::cout << "Starting test\n";

            // Initialize positions
            BitPosition position_1{BitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")};
            BitPosition position_2{BitPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1")};
            BitPosition position_3{BitPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1")};
            BitPosition position_4{BitPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1")};
            BitPosition position_5{BitPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8")};
            BitPosition position_6{BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ")};

            std::chrono::duration<double> duration{0};

            // Position 1
            std::cout << "Position 1 \n";
            // Initialize NNUE input std::vec
            globalTT.resize(1 << TTSIZE);
            auto start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_1 = BitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); // This is because we are searching moves from start again
                // position_1.makeMove(findNormalMoveFromString("e2e3", position_1));
                std::cout << runPVPerftTest(position_1, depth) << " moves\n";
            }
            auto end = std::chrono::high_resolution_clock::now(); // End timing
            duration = end - start;                               // Calculate duration

            // Position 2
            std::cout << "Position 2 \n";
            // Initialize NNUE input std::vec
            globalTT.resize(1 << TTSIZE);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_2 = BitPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"); // This is because we are searching moves from start again
                std::cout << runPVPerftTest(position_2, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 3
            std::cout << "Position 3 \n";
            // Initialize NNUE input std::vec
            globalTT.resize(1 << TTSIZE);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_3 = BitPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"); // This is because we are searching moves from start again
                // position_3.makeMove(findNormalMoveFromString("b4d4", position_3), state_info);
                std::cout << runPVPerftTest(position_3, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 4
            std::cout << "Position 4 \n";
            // Initialize NNUE input std::vec
            globalTT.resize(1 << TTSIZE);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_4 = BitPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"); // This is because we are searching moves from start again
                // position_4.makeMove(findNormalMoveFromString("d2d4", position_4), state_info);
                // position_4.makeMove(findNormalMoveFromString("a3a2", position_4), state_info);
                // position_4.makeMove(findNormalMoveFromString("g1f2", position_4), state_info);
                // position_4.printChecksInfo();
                std::cout << runPVPerftTest(position_4, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 5
            std::cout << "Position 5 \n";
            // Initialize NNUE input std::vec
            StateInfo state_info;
            globalTT.resize(1 << TTSIZE);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_5 = BitPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"); // This is because we are searching moves from start again
                // position_5.makeMove(findNormalMoveFromString("c4f7", position_5), state_info);
                // position_5.makeMove(findNormalMoveFromString("d8e8", position_5), state_info);
                std::cout << runPVPerftTest(position_5, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 6
            std::cout << "Position 6 \n";
            // Initialize NNUE input std::vec
            globalTT.resize(1 << TTSIZE);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                StateInfo state_info;
                position_6 = BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 "); // This is because we are searching moves from start again
                // position_6.makeMove(findNormalMoveFromString("e2d2", position_6), state_info);
                // position_6.setBlockersAndPinsInAB();
                // position_6.makeMove(findNormalMoveFromString("g4h3", position_6), state_info);
                // position_6.printPinsInfo();
                std::cout << runPVPerftTest(position_6, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            std::cout << "Time taken: " << duration.count() << " seconds\n";
        }

        // Test setCapturesAndScores and setCapturesInCheck generators efficiency
        else if (inputLine == "qSPerftTests")
        {
            int maxDepth;
            std::cout << "Max depth: \n";
            while (!(std::cin >> maxDepth))
            {
                std::cin.clear();                                                   // clear the error flag
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // discard invalid input
                std::cout << "Invalid input. Please enter a integer: \n";
            }
            std::cout << "Starting test\n";

            // Initialize positions
            BitPosition position_1{BitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")};
            BitPosition position_2{BitPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1")};
            BitPosition position_3{BitPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1")};
            BitPosition position_4{BitPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1")};
            BitPosition position_5{BitPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8")};
            BitPosition position_6{BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ")};

            std::chrono::duration<double> duration{0};

            // Position 1
            std::cout << "Position 1 \n";
            // Initialize NNUE input std::vec
            auto start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_1 = BitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); // This is because we are searching moves from start again
                // position_1.makeMove(findNormalMoveFromString("c2c3", position_1));
                std::cout << runQSPerftTest(position_1, depth) << " moves\n";
            }
            auto end = std::chrono::high_resolution_clock::now(); // End timing
            duration = end - start;                               // Calculate duration

            // Position 2
            std::cout << "Position 2 \n";
            // Initialize NNUE input std::vec
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_2 = BitPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"); // This is because we are searching moves from start again
                // position_2.makeMove(findNormalMoveFromString("e5g6", position_2), state_info);
                // position_2.makeMove(findNormalMoveFromString("b4c3", position_2), state_info);
                // position_2.makeMove(findNormalMoveFromString("g6h8", position_2), state_info);
                std::cout << runQSPerftTest(position_2, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 3
            std::cout << "Position 3 \n";
            // Initialize NNUE input std::vec
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_3 = BitPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"); // This is because we are searching moves from start again
                // position_3.makeMove(findCaptureMoveFromString("b4b1", position_3));
                std::cout << runQSPerftTest(position_3, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 4
            std::cout << "Position 4 \n";
            // Initialize NNUE input std::vec
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                StateInfo state_info;
                position_4 = BitPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"); // This is because we are searching moves from start again
                // position_4.makeMove(findNormalMoveFromString("f3d4", position_4), state_info);
                // position_4.makeMove(findNormalMoveFromString("a5b3", position_4), state_info);
                std::cout << runQSPerftTest(position_4, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 5
            std::cout << "Position 5 \n";
            // Initialize NNUE input std::vec
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                StateInfo state_info;
                position_5 = BitPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"); // This is because we are searching moves from start again
                // position_5.makeMove(findNormalMoveFromString("e1f2", position_5), state_info);
                // position_5.makeMove(findNormalMoveFromString("f2d3", position_5), state_info);
                std::cout << runQSPerftTest(position_5, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 6
            std::cout << "Position 6 \n";
            // Initialize NNUE input std::vec
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                StateInfo state_info;
                position_6 = BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 "); // This is because we are searching moves from start again
                // position_6.makeMove(findNormalMoveFromString("c4f7", position_6), state_info);
                std::cout << runQSPerftTest(position_6, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            std::cout << "Time taken: " << duration.count() << " seconds\n";
        }

        // Tactics tests to see how engine thinks
        else if (inputLine == "tacticsTests")
        {
            int maxDepth;
            // Prompt for minimum evaluation difference
            std::cout << "Max depth: \n";
            while (!(std::cin >> maxDepth))
            {
                std::cin.clear();                                                   // clear the error flag
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // discard invalid input
                std::cout << "Invalid input. Please enter a integer: \n";
            }
            std::cout << "Starting test\n";
            // Initialize positions
            BitPosition position_1{BitPosition("kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1")};
            BitPosition position_2{BitPosition("rR6/p7/KnPk4/P7/8/8/8/8 w - - 0 1")};
            BitPosition position_3{BitPosition("1b1q4/8/P2p4/1N1Pp2p/5P1k/7P/1B1P3K/8 w - - 0 1")};
            BitPosition position_4{BitPosition("2r2rk1/1b3ppp/p1qpp3/1P6/1Pn1P2b/2NB1P1P/1BP1R1P1/R2Q2K1 b - - 0 19")};
            BitPosition position_5{BitPosition("rn2kb1r/1bq2pp1/pp3n1p/4p3/2PQ1B1P/2N3P1/PP2PPB1/2KR3R w kq - 0 12")};
            BitPosition position_6{BitPosition("3k2rr/4b3/p3Qpq1/P2pn3/1p1Nb3/6B1/1PP1B2P/3R1RK1 b - - 0 25")};
            BitPosition position_7{BitPosition("4k3/Q6n/8/8/8/8/PR5P/4K1NR w K - 0 1")};

            // Setting the time to not be the limit
            OURTIME = 8000000;
            OURINC = 0;

            // Time duration of test
            std::chrono::duration<double> duration{0};

            // Position 1
            globalTT.resize(1 << 20);
            std::cout << "Position 1: \n";
            std::cout << "Best move should be a1a6 \n";
            auto start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_1 = BitPosition("kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_1, 1, depth).first.toString() << "\n";
            }
            auto end = std::chrono::high_resolution_clock::now(); // End timing
            duration += (end - start);                            // Calculate duration

            // Position 2
            globalTT.resize(1 << 20);
            std::cout << "Position 2: \n";
            std::cout << "Best move should be c6c7 \n";
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_2 = BitPosition("rR6/p7/KnPk4/P7/8/8/8/8 w - - 0 1"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_2, 1, depth).first.toString() << "\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += (end - start);                       // Calculate duration

            // Position 3
            globalTT.resize(1 << 20);
            std::cout << "Position 3: \n";
            std::cout << "Best move should be b2b4 \n";
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_3 = BitPosition("1b1q4/8/P2p4/1N1Pp2p/5P1k/7P/1B1P3K/8 w - - 0 1"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_3, 1, depth).first.toString() << "\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += (end - start);                       // Calculate duration

            // Position 4
            globalTT.resize(1 << 20);
            std::cout << "Position 4: \n";
            std::cout << "Best move should be c6b6 \n";
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_4 = BitPosition("2r2rk1/1b3ppp/p1qpp3/1P6/1Pn1P2b/2NB1P1P/1BP1R1P1/R2Q2K1 b - - 0 19"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_4, 1, depth).first.toString() << "\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += (end - start);                       // Calculate duration

            // Position 5
            globalTT.resize(1 << 20);
            std::cout << "Position 5: \n";
            std::cout << "Best move should be f4e5 \n";
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_5 = BitPosition("rn2kb1r/1bq2pp1/pp3n1p/4p3/2PQ1B1P/2N3P1/PP2PPB1/2KR3R w kq - 0 12"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_5, 1, depth).first.toString() << "\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += (end - start);                       // Calculate duration

            // Position 6
            globalTT.resize(1 << 20);
            std::cout << "Position 6: \n";
            std::cout << "Best move should be h8h2 \n";
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_6 = BitPosition("3k2rr/4b3/p3Qpq1/P2pn3/1p1Nb3/6B1/1PP1B2P/3R1RK1 b - - 0 25"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_6, 1, depth).first.toString() << "\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += (end - start);                       // Calculate duration

            // Position 7
            globalTT.resize(1 << 20);
            std::cout << "Position 7: \n";
            std::cout << "Best move should be b2b8 \n";
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_7 = BitPosition("4k3/Q6n/8/8/8/8/PR5P/4K1NR w K - 0 1"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_7, 1, depth).first.toString() << "\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += (end - start);                       // Calculate duration

            std::cout << "Time taken: " << duration.count() << " seconds\n";
        }

        else if (inputLine == "nNTests")
        {
            StateInfo state_info;
            // Test 1 (capture)
            std::cout << "\n Test 1 (Capture) \n";
            // Position1 at initialization
            BitPosition positionAfter_g5f6{BitPosition("r4rk1/1pp1qppp/p1np1B2/2b1p3/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 b - - 0 10")};
            printArray("White turn Accumulator", NNUEU::globalAccumulatorStack.top().inputTurn[0], 8);
            printArray("Black turn Accumulator", NNUEU::globalAccumulatorStack.top().inputTurn[1], 8);
            std::cout << "Eval our turn: " << NNUEU::evaluationFunction(positionAfter_g5f6, true) << "\n";
            positionAfter_g5f6 = BitPosition("r4rk1/1pp1qppp/p1np1B2/2b1p3/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 b - - 0 10");
            std::cout << "Eval not our turn: " << NNUEU::evaluationFunction(positionAfter_g5f6, false) << "\n";

            // Position1 accumulated
            BitPosition position_2{BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10")};
            position_2.makeCapture(findNormalMoveFromString("g5f6", position_2), state_info);
            std::cout << "Eval our turn: " << NNUEU::evaluationFunction(position_2, true) << "\n";
            printArray("White turn Accumulator", NNUEU::globalAccumulatorStack.top().inputTurn[0], 8);
            printArray("Black turn Accumulator", NNUEU::globalAccumulatorStack.top().inputTurn[1], 8);
            position_2 = BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
            position_2.makeCapture(findNormalMoveFromString("g5f6", position_2), state_info);
            std::cout << "Eval not our turn: " << NNUEU::evaluationFunction(position_2, false) << "\n";

            // Test 2 (really bad normal move)
            std::cout << "\n Test 2 (Bad Normal move) \n";
            // Position2 at initialization
            BitPosition positionAfter_f3h4{BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1bN/P1NP4/1PP1QPPP/R4RK1 b - - 1 10")};
            printArray("White turn Accumulator", NNUEU::globalAccumulatorStack.top().inputTurn[0], 8);
            printArray("Black turn Accumulator", NNUEU::globalAccumulatorStack.top().inputTurn[1], 8);
            std::cout << "Eval our turn: " << NNUEU::evaluationFunction(positionAfter_f3h4, true) << "\n";
            positionAfter_f3h4 = BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1bN/P1NP4/1PP1QPPP/R4RK1 b - - 1 10");
            std::cout << "Eval not our turn: " << NNUEU::evaluationFunction(positionAfter_f3h4, false) << "\n";

            // Position2 accumulated
            BitPosition position_3{BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10")};
            position_3.makeMove(findNormalMoveFromString("f3h4", position_3), state_info);
            std::cout << "Eval our turn: " << NNUEU::evaluationFunction(position_3, true) << "\n";
            printArray("White turn Accumulator", NNUEU::globalAccumulatorStack.top().inputTurn[0], 8);
            printArray("Black turn Accumulator", NNUEU::globalAccumulatorStack.top().inputTurn[1], 8);
            position_3 = BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
            position_3.makeMove(findNormalMoveFromString("f3h4", position_3), state_info);
            std::cout << "Eval not our turn: " << NNUEU::evaluationFunction(position_3, false) << "\n";

            // Test 3 (king move)
            std::cout << "\n Test 3 (King move) \n";
            // Position3 at initialization
            BitPosition positionAfter_g1h1{BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4R1K b - - 1 10")};
            printArray("White turn Accumulator", NNUEU::globalAccumulatorStack.top().inputTurn[0], 8);
            printArray("Black turn Accumulator", NNUEU::globalAccumulatorStack.top().inputTurn[1], 8);
            std::cout << "Eval our turn: " << NNUEU::evaluationFunction(positionAfter_g1h1, true) << "\n";
            positionAfter_g1h1 = BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4R1K b - - 1 10");
            std::cout << "Eval not our turn: " << NNUEU::evaluationFunction(positionAfter_g1h1, false) << "\n";

            // Position3 accumulated
            BitPosition position_4{BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10")};
            position_4.makeMove(findNormalMoveFromString("g1h1", position_4), state_info);
            std::cout << "Eval our turn: " << NNUEU::evaluationFunction(position_4, true) << "\n";
            printArray("White turn Accumulator", NNUEU::globalAccumulatorStack.top().inputTurn[0], 8);
            printArray("Black turn Accumulator", NNUEU::globalAccumulatorStack.top().inputTurn[1], 8);
            position_4 = BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
            position_4.makeMove(findNormalMoveFromString("g1h1", position_4), state_info);
            std::cout << "Eval not our turn: " << NNUEU::evaluationFunction(position_4, false) << "\n";

            // Test 4
            std::cout << "\n Test 4 (Normal move) \n";
            // Position4 at initialization
            BitPosition positionBad{BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P2N/P1NP2Pb/1PP2P1P/R6K w - - 2 13")};
            std::cout << "Eval our turn: " << NNUEU::evaluationFunction(positionBad, true) << "\n";
            printArray("White turn Accumulator", NNUEU::globalAccumulatorStack.top().inputTurn[0], 8);
        }

        // Test threefold check
        else if (inputLine == "threefoldTest")
        {
            BitPosition position_1{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};

            auto checkAndMakeMove = [&](const std::string &moveStr)
            {
                StateInfo state_info;
                Move move = findNormalMoveFromString(moveStr, position_1);
                position_1.makeMove(move, state_info);
                if (position_1.isDraw())
                    std::cout << "Draw\n";
                else
                    std::cout << "Not Draw\n";
            };

            checkAndMakeMove("b1a3"); // Not draw
            checkAndMakeMove("b8a6"); // Not draw
            checkAndMakeMove("a3b1"); // Not draw
            checkAndMakeMove("a6b8"); // Not draw

            checkAndMakeMove("b1a3"); // Not draw
            checkAndMakeMove("b8a6"); // Not draw
            checkAndMakeMove("a3b1"); // Not draw
            checkAndMakeMove("a6b8"); // Not draw

            checkAndMakeMove("b1a3"); // Draw
            checkAndMakeMove("b8a6");
            checkAndMakeMove("a3b1");
            checkAndMakeMove("a6b8");

            checkAndMakeMove("b1a3");
            checkAndMakeMove("b8a6");
            checkAndMakeMove("a3b1");
            checkAndMakeMove("a6b8");

            // position_1.printZobristKeys();
        }

        // Generate data for NNUE further training
        else if (inputLine == "generateData")
        {
            int timeForMoveMilliseconds;
            int numGames;
            float minEvalDiff;
            int minDepthSave;
            std::string outFileName;

            // Prompt for minimum evaluation difference
            std::cout << "Minimum difference of evals from NNUE and depth search to save a position: \n";
            while (!(std::cin >> minEvalDiff))
            {
                std::cin.clear();                                                   // clear the error flag
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // discard invalid input
                std::cout << "Invalid input. Please enter a floating-point number for the minimum evaluation difference: \n";
            }
            // Prompt for minimum depth to save positions
            std::cout << "Minimum depth to save positions if evaluation differs by more than the minimum difference: \n";
            while (!(std::cin >> minDepthSave))
            {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Invalid input. Please enter an integer for the minimum depth: \n";
            }
            // Prompt for file of positions to be played
            std::cout << "Number of games to be played: \n";
            while (!(std::cin >> numGames))
            {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Invalid input. Please enter an integer for the minimum depth: \n";
            }
            // Prompt for time spent per move in milliseconds
            std::cout << "Time spent per move in milliseconds: \n";
            while (!(std::cin >> timeForMoveMilliseconds))
            {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Invalid input. Please enter an integer for the time per move in milliseconds: \n";
            }
            // Prompt for output file name
            std::cout << "Name of file in which to save data: \n";
            std::cin.ignore();
            std::getline(std::cin, outFileName);

            // Check if the file already exists
            if (FILE *file = fopen(outFileName.c_str(), "r"))
            {
                // File exists
                fclose(file);
                std::cout << "File already exists.\n";
            }
            else
            {
                // File doesn't exist, create it
                std::ofstream outfile(outFileName.c_str());
                if (outfile)
                {
                    std::cout << "File created successfully.\n";
                }
                else
                {
                    std::cerr << "Error creating file.\n";
                }
                outfile.close();
            }

            // Output the collected inputs (for verification)
            std::cout << "Collected inputs:\n";
            std::cout << "Number of games to play: " << numGames << "\n";
            std::cout << "Minimum evaluation difference: " << minEvalDiff << "\n";
            std::cout << "Minimum depth: " << minDepthSave << "\n";
            std::cout << "Time per move in milliseconds: " << timeForMoveMilliseconds << "\n";
            std::cout << "Output file name: " << outFileName << "\n";

            for (int i = 0; i < numGames; ++i)
            {
                std::cout << "New initial position \n";
                nnueTT.resize(1 << 21);
                BitPosition position{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
                StateInfo state_info;
                bool gameEnded{false};
                while (not gameEnded)
                {
                    Move move = iterativeSearchGen(position, timeForMoveMilliseconds, minEvalDiff, minDepthSave, outFileName).first;
                    position.makeMove(move, state_info);
                    // // Check if game ended
                    // if (position.getIsCheck())
                    // {
                    //     position.setCheckInfo();
                    //     if (position.inCheckAllMoves().size() == 0 && position.isMate())
                    //         gameEnded = true;
                    // }
                    // else
                    // {
                    //     if (position.allMoves().size() == 0)
                    //         gameEnded = true;
                    // }
                }
            }
        }
    }
    return 0;
}
