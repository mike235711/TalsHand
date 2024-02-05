#include <iostream>
#include <string>
#include <sstream>
#include "precomputed_moves.h"
#include "magics.h"
#include "bitposition.h"
#include "tests.h"
#include <chrono>
#include "magicmoves.h"
#include "engine.h"

// We have to declare them since they where made extern in precomputed_moves.h
/*
namespace precomputed_moves{
    std::vector<std::map<uint64_t, uint64_t>> precomputedBishopMovesTable;
    std::vector<std::map<uint64_t, uint64_t>> precomputedRookMovesTable;
}
*/
// Function to convert a FEN string to a BitPosition object
BitPosition fenToBitPosition(const std::string &fen)
{
    std::istringstream fenStream(fen);
    std::string board, turn, castling, enPassant;
    fenStream >> board >> turn >> castling >> enPassant;

    // Initialize all bitboards to 0
    uint64_t white_pawns = 0, white_knights = 0, white_bishops = 0, white_rooks = 0, white_queens = 0, white_king = 0;
    uint64_t black_pawns = 0, black_knights = 0, black_bishops = 0, black_rooks = 0, black_queens = 0, black_king = 0;

    int square = 56; // Start from the top-left corner of the chess board
    for (char c : board)
    {
        if (c == '/')
        {
            square -= 16; // Move to the next row
        }
        else if (isdigit(c))
        {
            square += c - '0'; // Skip empty squares
        }
        else
        {
            uint64_t bit = 1ULL << square;
            switch (c)
            {
            case 'P':
                white_pawns |= bit;
                break;
            case 'N':
                white_knights |= bit;
                break;
            case 'B':
                white_bishops |= bit;
                break;
            case 'R':
                white_rooks |= bit;
                break;
            case 'Q':
                white_queens |= bit;
                break;
            case 'K':
                white_king |= bit;
                break;
            case 'p':
                black_pawns |= bit;
                break;
            case 'n':
                black_knights |= bit;
                break;
            case 'b':
                black_bishops |= bit;
                break;
            case 'r':
                black_rooks |= bit;
                break;
            case 'q':
                black_queens |= bit;
                break;
            case 'k':
                black_king |= bit;
                break;
            }
            square++;
        }
    }

    // Determine which side is to move
    bool whiteToMove = (turn == "w");

    // Parse castling rights
    bool whiteKingsideCastling = castling.find('K') != std::string::npos;
    bool whiteQueensideCastling = castling.find('Q') != std::string::npos;
    bool blackKingsideCastling = castling.find('k') != std::string::npos;
    bool blackQueensideCastling = castling.find('q') != std::string::npos;
    // Create and return a BitPosition object
    BitPosition position{BitPosition(white_pawns, white_knights, white_bishops, white_rooks, white_queens, white_king,
                       black_pawns, black_knights, black_bishops, black_rooks, black_queens, black_king,
                       whiteToMove,
                       whiteKingsideCastling, whiteQueensideCastling, blackKingsideCastling, blackQueensideCastling)};
    position.setAllPiecesBits();
    position.setKingPosition();
    return position;
}

Move findMoveFromString(std::string moveString, BitPosition position)
{
    if (position.isCheck())
    {
        position.setChecksAndPinsBits();
        for (Move move : position.inCheckMoves())
        {
            if (move.toString() == moveString)
            {
                return move;
            }
        }
        for (Move move : position.inCheckCaptures())
        {
            if (move.toString() == moveString)
            {
                return move;
            }
        }
    }
    else
    {
        position.setPinsBits();
        for (Move move : position.nonCaptureMoves())
        {
            if (move.toString() == moveString)
            {
                return move;
            }
        }
        for (Move move : position.captureMoves())
        {
            if (move.toString() == moveString)
            {
                return move;
            }
        }
    }
    return Move(0);
}

int main()
{
    initmagicmoves();
    // Initializing precomputed data
    /*
    precomputed_moves::precomputedBishopMovesTable = getBishopLongPrecomputedTable();
    precomputed_moves::precomputedRookMovesTable = getRookLongPrecomputedTable();
    */

    // Initialize position
    std::string inputLine;
    std::string lastFen; // Variable to store the last FEN string
    BitPosition position {fenToBitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")};
    bool position_initialized{false};

    // Simple loop to read commands from the Python GUI
    while (std::getline(std::cin, inputLine))
    {
        std::istringstream iss(inputLine);
        std::string command;
        iss >> command;
        // Process input from GUI
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
        else if (command == "quit")
        {
            break;
        }
        else if (command == "position" && position_initialized == false)
        {
            iss >> command;
            std::string fen;
            if (command == "startpos")
            {
                fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
                iss >> command; // Consume the 'moves' token if it's there
                position_initialized = true;
            }
            else if (command == "fen")
            {
                getline(iss, fen);   // Read the rest of the line as the FEN string
                fen = fen.substr(1); // Remove the leading space
                position_initialized = true;
            }

            position = fenToBitPosition(fen);
            std::string moveUci;
            while (iss >> command)
            {
                // Apply each move in the moves list
                moveUci = command;
            }
            Move move{findMoveFromString(moveUci, position)};
            if (move.getData() != 0)
                position.makeMove(move);
        }
        else if (command == "position" && position_initialized == true)
        // Get the last move and make it (move made from other player)
        {
            std::string moveUci;
            while (iss >> command)
            {
                moveUci = command;
            }
            Move move{findMoveFromString(moveUci, position)};
            if (move.getData() != 0)
                position.makeMove(move);
        }
        else if (inputLine.substr(0,2) == "go")
        {
            // Implement your move generation and evaluation logic here
            // Placeholder for first non-capture move
            Move bestMove = iterativeSearch(position,  800);
            position.makeMove(bestMove);
            std::cout << "bestmove " << bestMove.toString() << "\n" << std::flush;
        }
        else if (inputLine == "perftTests")
        {
            std::cout << "Starting test\n";

            auto start = std::chrono::high_resolution_clock::now(); // Start timing

            for (int depth = 1; depth <= 4; ++depth)
            {
                // It's important to output the results of the tests to verify correctness
                std::cout << "Depth " << depth << ":\n";
                std::cout << "Position 1: \n"
                            << runPerftTest(fenToBitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"), depth) << " moves\n";
                std::cout << "Position 2: \n"
                            << runPerftTest(fenToBitPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"), depth) << " moves\n";
                std::cout << "Position 3: \n"
                            << runPerftTest(fenToBitPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"), depth) << " moves\n";
                std::cout << "Position 4: \n"
                            << runPerftTest(fenToBitPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"), depth) << " moves\n";
                std::cout << "Position 5: \n"
                            << runPerftTest(fenToBitPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"), depth) << " moves\n";
                std::cout << "Position 6: \n"
                            << runPerftTest(fenToBitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 "), depth) << " moves\n";
            }

            auto end = std::chrono::high_resolution_clock::now(); // End timing
            std::chrono::duration<double> duration = end - start; // Calculate duration

            std::cout << "Time taken: " << duration.count() << " seconds\n";
        }
        }
    return 0;
}
