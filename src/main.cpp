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



TranspositionTable globalTT;
TranspositionTableNNUE nnueTT; // For position generator
bool ENGINEISWHITE;
int OURTIME;
int OURINC;
std::chrono::time_point<std::chrono::high_resolution_clock> STARTTIME;
int TTSIZE{20};

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
        std::vector<Move> moves{position.inCheckAllMoves()};
        for (Move move : moves)
        {
            if (move.toString() == moveString)
            {
                return move;
            }
        }
    }
    else
    {
        std::vector<Move> moves{position.allMoves()};
        for (Move move : moves)
        {
            if (move.toString() == moveString)
            {
                return move;
            }
        }
    }
    return Move(0);
}
Move findCaptureMoveFromString(std::string moveString, BitPosition position)
// Find a capture or non capture move from a string
{
    if (position.getIsCheck())
    {
        position.setCheckInfoAfterMove();
        Move captures[256];
        Move *currCapture = captures;
        Move *endCapture = position.setCapturesInCheck(currCapture);
        Move capture{position.nextMoveInCheck(currCapture, endCapture)};
        
        Move nonCaptures[256];
        Move *currNonCapture = nonCaptures;
        Move *endNonCapture = position.setNonCapturesInCheck(currNonCapture);
        Move nonCapture{position.nextNonCapture(currNonCapture, endNonCapture)};
        while (capture.getData() != 0)
        {
            if (capture.toString() == moveString)
            {
                return capture;
            }
            capture = position.nextCaptureInCheck(currCapture, endCapture);
        }
        while (nonCapture.getData() != 0)
        {
            if (nonCapture.toString() == moveString)
            {
                return nonCapture;
            }
            nonCapture = position.nextNonCapture(currNonCapture, endNonCapture);
        }
    }
    else
    {
        ScoredMove captures[256];
        ScoredMove *currCapture = captures;
        ScoredMove *endCapture = position.setCapturesAndScores(currCapture);
        ScoredMove capture{position.nextCapture(currCapture, endCapture)};
        
        Move nonCaptures[256];
        Move *currNonCapture = nonCaptures;
        Move *endNonCapture = position.setNonCaptures(currNonCapture);
        Move nonCapture{position.nextNonCapture(currNonCapture, endNonCapture)};
        while (capture.getData() != 0)
        {
            if (capture.toString() == moveString)
            {
                return capture;
            }
            capture = position.nextCapture(currCapture, endCapture);
        }
        while (nonCapture.getData() != 0)
        {
            if (nonCapture.toString() == moveString)
            {
                return nonCapture;
            }
            nonCapture = position.nextNonCapture(currNonCapture, endNonCapture);
        }
    }
    return Move(0);
}

int main()
{
    // Initialize std::vectors of NNUEInput layers as global variables
    NNUEU::initNNUEParameters();

    // Initialize magic numbers and zobrist numbers
    initmagicmoves();
    zobrist_keys::initializeZobristNumbers();

    // Initialize position object
    std::string inputLine;
    std::string lastFen; // Variable to store the last FEN string
    BitPosition position {BitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")};

    globalTT.resize(1 << TTSIZE);
    // Simple loop to read commands from the Python GUI following UCI communication protocol
    while (std::getline(std::cin, inputLine))
    {
        // Process initial input from GUI
        std::istringstream iss(inputLine);
        std::string command;
        iss >> command;
        if (command == "uci")
        {
            std::cout << "id name La_Mano_de_Tahl\n" << std::flush;
            std::cout << "id author Miguel_Cordoba\n" << std::flush;
            std::cout << "uciok\n" << std::flush;
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
            iss >> command;
            std::string fen;
            if (command == "startpos")
            {
                fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
                iss >> command; // Consume the 'moves' token if it's there
            }
            else if (command == "fen")
            {
                getline(iss, fen);   // Read the rest of the line as the FEN string
                fen = fen.substr(1); // Remove the leading space
            }

            position = BitPosition(fen);
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position);
            
            std::string moveUci;
            while (iss >> command)
            {
                // Apply each move in the moves list
                moveUci = command;
                Move move{findNormalMoveFromString(moveUci, position)};
                bool reseterMove{false};
                if (move.getData() != 0)
                    position.makeMove(move);
            }
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
                else if (command == "binc" && (ENGINEISWHITE==false))
                    iss >> OURINC;
            }
            std::cout << "Static Eval Before Move: " << NNUEU::evaluationFunction(true) << "\n";
            // Call the engine
            STARTTIME = std::chrono::high_resolution_clock::now();
            auto [bestMove, bestValue]{iterativeSearch(position)};

            // Send our best move through a UCI command
            std::cout << "Eval: " << bestValue << "\n";
            std::cout << "bestmove " << bestMove.toString() << "\n"
                      << std::flush;

            bool reseterMove{position.moveIsReseter(bestMove)};

            // Static eval after making move (testing purposes)
            position.makeMove(bestMove);
            std::cout << "Static Eval After Move: " << NNUEU::evaluationFunction(false) << "\n";
            NNUEU::initializeNNUEInput(position);
            std::cout << "Static Eval After Move: " << NNUEU::evaluationFunction(false) << "\n";

            // If we make a capture we cant go back to previous positions so we empty
            // the plyInfo (for threefold checking) in position and the TransPosition table
            if (reseterMove)
            {
                position.resetPlyInfo();
                globalTT.resize(1 << TTSIZE);
            }
            // Check transposition table memory
            globalTT.printTableMemory();
        }

        ////////////////////////////////////////////////////////////
        // Some tests to see efficiency and algorithm correctness
        ////////////////////////////////////////////////////////////
        
        // Test allMoves and inCheckAllMoves generators efficiency
        else if (inputLine == "firstMovesPerftTests")
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
            NNUEU::initializeNNUEInput(position_1);
            auto start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_1 = BitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); // This is because we are searching moves from start again
                // position_1.makeMove(findNormalMoveFromString("e2e3", position_1));
                std::cout << runFirstMovesPerftTest(position_1, depth) << " moves\n";
            }
            auto end = std::chrono::high_resolution_clock::now(); // End timing
            duration = end - start;                               // Calculate duration

            // Position 2
            std::cout << "Position 2 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_2);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_2 = BitPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"); // This is because we are searching moves from start again
                std::cout << runFirstMovesPerftTest(position_2, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 3
            std::cout << "Position 3 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_3);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_3 = BitPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"); // This is because we are searching moves from start again
                std::cout << runFirstMovesPerftTest(position_3, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 4
            std::cout << "Position 4 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_4);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_4 = BitPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"); // This is because we are searching moves from start again
                // position_4.makeMove(findNormalMoveFromString("c4c5", position_4));
                std::cout << runFirstMovesPerftTest(position_4, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 5
            std::cout << "Position 5 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_5);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_5 = BitPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"); // This is because we are searching moves from start again
                // position_5.makeMove(findNormalMoveFromString("d7c8n", position_5));
                std::cout << runFirstMovesPerftTest(position_5, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 6
            std::cout << "Position 6 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_6);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_6 = BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 "); // This is because we are searching moves from start again
                // position_6.makeMove(findNormalMoveFromString("c3b1", position_6));
                std::cout << runFirstMovesPerftTest(position_6, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            std::cout << "Time taken: " << duration.count() << " seconds\n";
        }

        // Test setMovesAndScores and setMovesInCheck generators efficiency
        else if (inputLine == "normalPerftTests")
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
            NNUEU::initializeNNUEInput(position_1);
            auto start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_1 = BitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); // This is because we are searching moves from start again
                // position_1.makeMove(findNormalMoveFromString("e2e3", position_1));
                std::cout << runNormalPerftTest(position_1, depth) << " moves\n";
            }
            auto end = std::chrono::high_resolution_clock::now(); // End timing
            duration = end - start;                               // Calculate duration

            // Position 2
            std::cout << "Position 2 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_2);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_2 = BitPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"); // This is because we are searching moves from start again
                std::cout << runNormalPerftTest(position_2, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 3
            std::cout << "Position 3 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_3);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_3 = BitPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"); // This is because we are searching moves from start again
                // position_3.makeMoveWithoutNNUE(findNormalMoveFromString("b4f4", position_3));
                std::cout << runNormalPerftTest(position_3, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 4
            std::cout << "Position 4 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_4);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_4 = BitPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"); // This is because we are searching moves from start again
                // position_4.makeMove(findNormalMoveFromString("c4c5", position_4));
                std::cout << runNormalPerftTest(position_4, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 5
            std::cout << "Position 5 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_5);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_5 = BitPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"); // This is because we are searching moves from start again
                // position_5.makeMove(findNormalMoveFromString("d7c8n", position_5));
                std::cout << runNormalPerftTest(position_5, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            // Position 6
            std::cout << "Position 6 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_6);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_6 = BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 "); // This is because we are searching moves from start again
                // position_6.makeMove(findNormalMoveFromString("c3b1", position_6));
                std::cout << runNormalPerftTest(position_6, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                         // Calculate duration

            std::cout << "Time taken: " << duration.count() << " seconds\n";
        }

        // Test setCapturesAndScores and setCapturesInCheck generators efficiency
        else if (inputLine == "capturesPerftTests")
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
            NNUEU::initializeNNUEInput(position_1);
            auto start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_1 = BitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); // This is because we are searching moves from start again
                //position_1.makeMoveWithoutNNUE(findCaptureMoveFromString("e2e3", position_1));
                std::cout << runCapturesPerftTest(position_1, depth) << " moves\n";
            }
            auto end = std::chrono::high_resolution_clock::now(); // End timing
            duration = end - start;                               // Calculate duration

            // Position 2
            std::cout << "Position 2 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_2);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_2 = BitPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"); // This is because we are searching moves from start again
                // position_2.makeCaptureWithoutNNUE(findCaptureMoveFromString("d5e6", position_2));
                std::cout << runCapturesPerftTest(position_2, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                               // Calculate duration

            // Position 3
            std::cout << "Position 3 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_3);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_3 = BitPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"); // This is because we are searching moves from start again
                // position_3.makeCaptureWithoutNNUE(findCaptureMoveFromString("b4b1", position_3));
                std::cout << runCapturesPerftTest(position_3, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                               // Calculate duration

            // Position 4
            std::cout << "Position 4 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_4);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_4 = BitPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"); // This is because we are searching moves from start again
                // position_4.makeMove(findNormalMoveFromString("d2d4", position_4));
                std::cout << runCapturesPerftTest(position_4, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                               // Calculate duration

            // Position 5
            std::cout << "Position 5 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_5);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_5 = BitPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"); // This is because we are searching moves from start again
                // position_5.makeMoveWithoutNNUE(findCaptureMoveFromString("a2a3", position_5));
                std::cout << runCapturesPerftTest(position_5, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                               // Calculate duration

            // Position 6
            std::cout << "Position 6 \n";
            // Initialize NNUE input std::vec
            NNUEU::initializeNNUEInput(position_6);
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                position_6 = BitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 "); // This is because we are searching moves from start again
                std::cout << runCapturesPerftTest(position_6, depth) << " moves\n";
            }
            end = std::chrono::high_resolution_clock::now(); // End timing
            duration += end - start;                               // Calculate duration

            std::cout << "Time taken: " << duration.count() << " seconds\n";
        }

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
            NNUEU::initializeNNUEInput(position_1);
            globalTT.resize(1 << 20);
            std::cout << "Position 1: \n";
            std::cout << "Best move should be a1a6 \n";
            auto start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_1 = BitPosition("kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_1, depth).first.toString() << "\n";
            }
            auto end = std::chrono::high_resolution_clock::now(); // End timing
            duration += (end - start); // Calculate duration

            // Position 2
            NNUEU::initializeNNUEInput(position_2);
            globalTT.resize(1 << 20);
            std::cout << "Position 2: \n";
            std::cout << "Best move should be c6c7 \n";
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_2 = BitPosition("rR6/p7/KnPk4/P7/8/8/8/8 w - - 0 1"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_2, depth).first.toString() << "\n";
            }
            end = std::chrono::high_resolution_clock::now();    // End timing
            duration += (end - start); // Calculate duration

            // Position 3
            NNUEU::initializeNNUEInput(position_3);
            globalTT.resize(1 << 20);
            std::cout << "Position 3: \n";
            std::cout << "Best move should be b2b4 \n";
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_3 = BitPosition("1b1q4/8/P2p4/1N1Pp2p/5P1k/7P/1B1P3K/8 w - - 0 1"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_3, depth).first.toString() << "\n";
            }
            end = std::chrono::high_resolution_clock::now();    // End timing
            duration += (end - start); // Calculate duration

            // Position 4
            NNUEU::initializeNNUEInput(position_4);
            globalTT.resize(1 << 20);
            std::cout << "Position 4: \n";
            std::cout << "Best move should be c6b6 \n";
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_4 = BitPosition("2r2rk1/1b3ppp/p1qpp3/1P6/1Pn1P2b/2NB1P1P/1BP1R1P1/R2Q2K1 b - - 0 19"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_4, depth).first.toString() << "\n";
            }
            end = std::chrono::high_resolution_clock::now();    // End timing
            duration += (end - start); // Calculate duration

            // Position 5
            NNUEU::initializeNNUEInput(position_5);
            globalTT.resize(1 << 20);
            std::cout << "Position 5: \n";
            std::cout << "Best move should be f4e5 \n";
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_5 = BitPosition("rn2kb1r/1bq2pp1/pp3n1p/4p3/2PQ1B1P/2N3P1/PP2PPB1/2KR3R w kq - 0 12"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_5, depth).first.toString() << "\n";
            }
            end = std::chrono::high_resolution_clock::now();    // End timing
            duration += (end - start); // Calculate duration

            // Position 6
            NNUEU::initializeNNUEInput(position_6);
            globalTT.resize(1 << 20);
            std::cout << "Position 6: \n";
            std::cout << "Best move should be h8h2 \n";
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_6 = BitPosition("3k2rr/4b3/p3Qpq1/P2pn3/1p1Nb3/6B1/1PP1B2P/3R1RK1 b - - 0 25"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_6, depth).first.toString() << "\n";
            }
            end = std::chrono::high_resolution_clock::now();    // End timing
            duration += (end - start); // Calculate duration

            // Position 7
            NNUEU::initializeNNUEInput(position_7);
            globalTT.resize(1 << 20);
            std::cout << "Position 7: \n";
            std::cout << "Best move should be b2b8 \n";
            start = std::chrono::high_resolution_clock::now(); // Start timing
            for (int8_t depth = 1; depth <= maxDepth; ++depth)
            {
                STARTTIME = std::chrono::high_resolution_clock::now();
                position_7 = BitPosition("4k3/Q6n/8/8/8/8/PR5P/4K1NR w K - 0 1"); // This is because we are searching moves from start again
                std::cout << iterativeSearch(position_7, depth).first.toString() << "\n";
            }
            end = std::chrono::high_resolution_clock::now();    // End timing
            duration += (end - start); // Calculate duration

            std::cout << "Time taken: " << duration.count() << " seconds\n";
        }
        else if (inputLine == "nNTests")
        {
            // Position at initialization
            BitPosition positionAfter_e2e3{BitPosition("rnbqkbnr/pppppppp/8/8/8/4P3/PPPP1PPP/RNBQKBNR b KQkq - 0 1")};
            NNUEU::initializeNNUEInput(positionAfter_e2e3);
            std::cout << "Eval our turn: " << NNUEU::evaluationFunction(true) << "\n";
            std::cout << "Eval not our turn: " << NNUEU::evaluationFunction(false) << "\n";
            // printArray("White turn Accumulator", NNUEU::inputWhiteTurn, 8);
            // printArray("Black turn Accumulator", NNUEU::inputBlackTurn, 8);

            // Position accumulated
            BitPosition position_1{BitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")};
            NNUEU::initializeNNUEInput(position_1);
            position_1.makeMove(findNormalMoveFromString("e2e3", position_1));
            std::cout << "Eval our turn: " << NNUEU::evaluationFunction(true) << "\n";
            std::cout << "Eval not our turn: " << NNUEU::evaluationFunction(false) << "\n";
            // printArray("White turn Accumulator", NNUEU::inputWhiteTurn, 8);
            // printArray("Black turn Accumulator", NNUEU::inputBlackTurn, 8);

            // Position at initialization
            BitPosition position2{BitPosition("7r/3qrpbk/1p1p1np1/p1nP3p/P1P1pP2/1P2B2P/3NBRP1/3Q1R1K b - - 2 24")};
            NNUEU::initializeNNUEInput(position2);
            std::cout << "Eval our turn: " << NNUEU::evaluationFunction(true) << "\n";
            std::cout << "Eval not our turn: " << NNUEU::evaluationFunction(false) << "\n";
            // printArray("White turn Accumulator", NNUEU::inputWhiteTurn, 8);
            // printArray("Black turn Accumulator", NNUEU::inputBlackTurn, 8);

            // Position at initialization
            BitPosition position3{BitPosition("4q2k/4rp2/1p1p2p1/p2P2Pn/P1P5/1P1p1Q2/3N1R2/5RK1 b - - 1 33")};
            NNUEU::initializeNNUEInput(position3);
            std::cout << "Eval our turn: " << NNUEU::evaluationFunction(true) << "\n";
            std::cout << "Eval not our turn: " << NNUEU::evaluationFunction(false) << "\n";
            // printArray("White turn Accumulator", NNUEU::inputWhiteTurn, 8);
            // printArray("Black turn Accumulator", NNUEU::inputBlackTurn, 8);
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
                NNUEU::initializeNNUEInput(position);
                bool gameEnded{false};
                while (not gameEnded)
                {
                    Move move = iterativeSearchGen(position, timeForMoveMilliseconds, minEvalDiff, minDepthSave, outFileName).first;
                    position.makeMove(move);
                    // Check if game ended
                    if (position.getIsCheck())
                    {
                        position.setCheckInfoAfterMove();
                        if (position.inCheckAllMoves().size() == 0 && position.isMate())
                            gameEnded = true;
                    }
                    else
                    {
                        if (position.allMoves().size() == 0 && position.isStalemate())
                            gameEnded = true;
                    }
                    if (position.isThreeFold())
                        gameEnded = true;
                }
            }
        }
    }
    return 0;
}
