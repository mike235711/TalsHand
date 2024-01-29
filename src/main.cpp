#include <iostream>
#include <string>
#include <sstream>
#include "precomputed_moves.h"
#include "magics.h"
#include "bitposition.h"
#include "tests.h"
#include <chrono>
#include "magicmoves.h"

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

    // Simple loop to read commands from the Python GUI
    while (std::getline(std::cin, inputLine))
    {
        BitPosition position{fenToBitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")};
        // Process input from GUI
        if (inputLine == "uci")
        {
            std::cout << "id name La_Mano_de_Tahl\n";
            std::cout << "id author Miguel_Cordoba\n";
            std::cout << "uciok\n";
        }
        else if (inputLine == "isready")
        {
            std::cout << "readyok\n";
        }
        else if (inputLine == "quit")
        {
            break;
        }
        if (inputLine.substr(0, 8) == "position")
        {
            // Extract and store the FEN string from the command
            std::string fen;
            if (inputLine.substr(9, 8) == "startpos")
            {
                fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
            }
            else
            {
                fen = inputLine.substr(13);
            }
            position = fenToBitPosition(fen);
        }
        else if (inputLine == "go")
        {
            // The previous command contained the position in FEN format
            // Now you can use lastFen to get the board position
            // and start calculating the move
            Move uci_move{};
            uci_move = position.nonCaptureMoves()[0];
            std::cout << "best move " << uci_move.toString() << "\n";
        }
        else if (inputLine == "perftTests")
        {
            std::cout << "Starting test\n";

            auto start = std::chrono::high_resolution_clock::now(); // Start timing

            for (int depth = 1; depth <= 4; ++depth)
            {
                // It's important to output the results of the tests to verify correctness
                std::cout << "Depth " << depth << ":\n";
                std::cout << "Position 1: \n"<< runPerftTest(fenToBitPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"), depth) << " moves\n";
                std::cout << "Position 2: \n"<< runPerftTest(fenToBitPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"), depth) << " moves\n";
                std::cout << "Position 3: \n"<< runPerftTest(fenToBitPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"), depth) << " moves\n";
                std::cout << "Position 4: \n"<< runPerftTest(fenToBitPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"), depth) << " moves\n";
                std::cout << "Position 5: \n"<< runPerftTest(fenToBitPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"), depth) << " moves\n";
                std::cout << "Position 6: \n"<< runPerftTest(fenToBitPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 "), depth) << " moves\n";
            }

            auto end = std::chrono::high_resolution_clock::now(); // End timing
            std::chrono::duration<double> duration = end - start; // Calculate duration

            std::cout << "Time taken: " << duration.count() << " seconds\n";
        }
    }

    return 0;
}
