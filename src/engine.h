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

bool ENGINEISWHITE;


// Constants for material values
const int MATERIAL_VALUES[] = {10, 32, 33, 50, 90, 100, -10, -32, -33, -50, -90, -100};

// Constants for positional values
const std::array<int, 64> WHITE_PAWNS = {0, 0, 0, 0, 0, 0, 0, 0,
                                         5, 5, 5, 5, 5, 5, 5, 5,
                                         1, 1, 2, 4, 4, 2, 1, 1,
                                         1, 1, 1, 3, 3, 1, 1, 1,
                                         1, 1, 1, 3, 3, 1, 1, 1,
                                         1, 1, 1, 2, 2, 1, 1, 1,
                                         5, 5, 1, 1, 1, 1, 1, 1,
                                         0, 0, 0, 0, 0, 0, 0, 0}; 
const std::array<int, 64> WHITE_KNIGHTS = {1, 2, 3, 3, 3, 3, 2, 1,
                                           2, 3, 4, 4, 4, 4, 3, 2,
                                           3, 4, 5, 5, 5, 5, 4, 3,
                                           3, 4, 5, 5, 5, 5, 4, 3,
                                           3, 4, 5, 5, 5, 5, 4, 3,
                                           3, 4, 5, 5, 5, 5, 4, 3,
                                           2, 3, 4, 4, 4, 4, 3, 2,
                                           1, 2, 3, 3, 3, 3, 2, 1};
const std::array<int, 64> WHITE_BISHOPS = {2, 3, 3, 3, 3, 3, 3, 2,
                                           3, 4, 4, 4, 4, 4, 4, 3,
                                           3, 4, 5, 5, 5, 5, 4, 3,
                                           3, 4, 5, 5, 5, 5, 4, 3,
                                           3, 4, 5, 5, 5, 5, 4, 3,
                                           3, 4, 5, 5, 5, 5, 4, 3,
                                           3, 4, 4, 4, 4, 4, 4, 3,
                                           2, 3, 3, 3, 3, 3, 3, 2};
const std::array<int, 64> WHITE_ROOKS = {1, 1, 1, 2, 2, 1, 1, 1,
                                           2, 2, 2, 2, 2, 2, 2, 2,
                                           1, 2, 2, 2, 2, 2, 2, 1,
                                           1, 2, 2, 2, 2, 2, 2, 1,
                                           1, 2, 2, 2, 2, 2, 2, 1,
                                           1, 2, 2, 2, 2, 2, 2, 1,
                                           1, 2, 2, 2, 2, 2, 2, 1,
                                           1, 1, 2, 4, 4, 3, 1, 1};
const std::array<int, 64> WHITE_QUEENS = {1, 1, 1, 1, 1, 1, 1, 1,
                                           1, 1, 1, 1, 1, 1, 1, 1,
                                           1, 1, 1, 1, 1, 1, 1, 1,
                                           1, 1, 1, 1, 1, 1, 1, 1,
                                           1, 1, 1, 1, 1, 1, 1, 1,
                                           1, 1, 1, 1, 1, 1, 1, 1,
                                           1, 1, 1, 1, 1, 1, 1, 1,
                                           1, 1, 1, 1, 1, 1, 1, 1};
const std::array<int, 64> WHITE_KING = {1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        4, 5, 5, 3, 3, 5, 5, 4};
const std::array<int, 64> BLACK_PAWNS = {0, 0, 0, 0, 0, 0, 0, 0, 
                                         1, 1, 1, 1, 1, 1, 5, 5, 
                                         1, 1, 1, 2, 2, 1, 1, 1, 
                                         1, 1, 1, 3, 3, 1, 1, 1, 
                                         1, 1, 1, 3, 3, 1, 1, 1, 
                                         1, 1, 2, 4, 4, 2, 1, 1, 
                                         5, 5, 5, 5, 5, 5, 5, 5, 
                                         0, 0, 0, 0, 0, 0, 0, 0};
const std::array<int, 64> BLACK_KNIGHTS = {1, 2, 3, 3, 3, 3, 2, 1, 
                                           2, 3, 4, 4, 4, 4, 3, 2, 
                                           3, 4, 5, 5, 5, 5, 4, 3, 
                                           3, 4, 5, 5, 5, 5, 4, 3, 
                                           3, 4, 5, 5, 5, 5, 4, 3, 
                                           3, 4, 5, 5, 5, 5, 4, 3, 
                                           2, 3, 4, 4, 4, 4, 3, 2, 
                                           1, 2, 3, 3, 3, 3, 2, 1};
const std::array<int, 64> BLACK_BISHOPS = {2, 3, 3, 3, 3, 3, 3, 2, 
                                           3, 4, 4, 4, 4, 4, 4, 3, 
                                           3, 4, 5, 5, 5, 5, 4, 3, 
                                           3, 4, 5, 5, 5, 5, 4, 3, 
                                           3, 4, 5, 5, 5, 5, 4, 3, 
                                           3, 4, 5, 5, 5, 5, 4, 3, 
                                           3, 4, 4, 4, 4, 4, 4, 3, 
                                           2, 3, 3, 3, 3, 3, 3, 2};
const std::array<int, 64> BLACK_ROOKS = {1, 1, 3, 4, 4, 2, 1, 1, 
                                         1, 2, 2, 2, 2, 2, 2, 1, 
                                         1, 2, 2, 2, 2, 2, 2, 1, 
                                         1, 2, 2, 2, 2, 2, 2, 1, 
                                         1, 2, 2, 2, 2, 2, 2, 1, 
                                         1, 2, 2, 2, 2, 2, 2, 1, 
                                         2, 2, 2, 2, 2, 2, 2, 2, 
                                         1, 1, 1, 2, 2, 1, 1, 1};
const std::array<int, 64> BLACK_QUEENS = {1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1};
const std::array<int, 64> BLACK_KING = {4, 5, 5, 3, 3, 5, 5, 4,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1};

// Convert a bitboard to an array of positions
std::array<bool, 64> bitboardToArray(uint64_t bitboard)
{
    std::array<bool, 64> result{};
    for (int i = 0; i < 64; ++i)
    {
        result[i] = (bitboard & (1ULL << i)) != 0;
    }
    return result;
}

// Evaluation function for white
int16_t evaluationFunctionWhite(const BitPosition &position)
{
    int16_t totalEval = 0;

    for (unsigned short square : getBitIndices(position.getWhitePawnsBits()))
    {
        totalEval += MATERIAL_VALUES[0] + WHITE_PAWNS[square];
    }
    for (unsigned short square : getBitIndices(position.getWhiteKnightsBits()))
    {
        totalEval += MATERIAL_VALUES[1] + WHITE_KNIGHTS[square];
    }
    for (unsigned short square : getBitIndices(position.getWhiteBishopsBits()))
    {
        totalEval += MATERIAL_VALUES[2] + WHITE_BISHOPS[square];
    }
    for (unsigned short square : getBitIndices(position.getWhiteRooksBits()))
    {
        totalEval += MATERIAL_VALUES[3] + WHITE_ROOKS[square];
    }
    for (unsigned short square : getBitIndices(position.getWhiteQueensBits()))
    {
        totalEval += MATERIAL_VALUES[4] + WHITE_QUEENS[square];
    }
    for (unsigned short square : getBitIndices(position.getWhiteKingBits()))
    {
        totalEval += MATERIAL_VALUES[5] + WHITE_KING[square];
    }

    for (unsigned short square : getBitIndices(position.getBlackPawnsBits()))
    {
        totalEval += MATERIAL_VALUES[6] - BLACK_PAWNS[square];
    }
    for (unsigned short square : getBitIndices(position.getBlackKnightsBits()))
    {
        totalEval += MATERIAL_VALUES[7] - BLACK_KNIGHTS[square];
    }
    for (unsigned short square : getBitIndices(position.getBlackBishopsBits()))
    {
        totalEval += MATERIAL_VALUES[8] - BLACK_BISHOPS[square];
    }
    for (unsigned short square : getBitIndices(position.getBlackRooksBits()))
    {
        totalEval += MATERIAL_VALUES[9] - BLACK_ROOKS[square];
    }
    for (unsigned short square : getBitIndices(position.getBlackQueensBits()))
    {
        totalEval += MATERIAL_VALUES[10] - BLACK_QUEENS[square];
    }
    for (unsigned short square : getBitIndices(position.getBlackKingBits()))
    {
        totalEval += MATERIAL_VALUES[11] - BLACK_KING[square];
    }
    return totalEval;
}

// Evaluation function for black
int16_t evaluationFunctionBlack(const BitPosition &position)
{
    int16_t totalEval = 0;

    for (unsigned short square : getBitIndices(position.getWhitePawnsBits()))
    {
        totalEval += MATERIAL_VALUES[6] - WHITE_PAWNS[square];
    }
    for (unsigned short square : getBitIndices(position.getWhiteKnightsBits()))
    {
        totalEval += MATERIAL_VALUES[7] - WHITE_KNIGHTS[square];
    }
    for (unsigned short square : getBitIndices(position.getWhiteBishopsBits()))
    {
        totalEval += MATERIAL_VALUES[8] - WHITE_BISHOPS[square];
    }
    for (unsigned short square : getBitIndices(position.getWhiteRooksBits()))
    {
        totalEval += MATERIAL_VALUES[9] - WHITE_ROOKS[square];
    }
    for (unsigned short square : getBitIndices(position.getWhiteQueensBits()))
    {
        totalEval += MATERIAL_VALUES[10] - WHITE_QUEENS[square];
    }
    for (unsigned short square : getBitIndices(position.getWhiteKingBits()))
    {
        totalEval += MATERIAL_VALUES[11] - WHITE_KING[square];
    }

    for (unsigned short square : getBitIndices(position.getBlackPawnsBits()))
    {
        totalEval += MATERIAL_VALUES[0] + BLACK_PAWNS[square];
    }
    for (unsigned short square : getBitIndices(position.getBlackKnightsBits()))
    {
        totalEval += MATERIAL_VALUES[1] + BLACK_KNIGHTS[square];
    }
    for (unsigned short square : getBitIndices(position.getBlackBishopsBits()))
    {
        totalEval += MATERIAL_VALUES[2] + BLACK_BISHOPS[square];
    }
    for (unsigned short square : getBitIndices(position.getBlackRooksBits()))
    {
        totalEval += MATERIAL_VALUES[3] + BLACK_ROOKS[square];
    }
    for (unsigned short square : getBitIndices(position.getBlackQueensBits()))
    {
        totalEval += MATERIAL_VALUES[4] + BLACK_QUEENS[square];
    }
    for (unsigned short square : getBitIndices(position.getBlackKingBits()))
    {
        totalEval += MATERIAL_VALUES[5] + BLACK_KING[square];
    }

    return totalEval;
}
int16_t simpleEvaluationFunctionWhite(BitPosition position)
{
    int16_t totalEval = 0;

    for (unsigned short i : getBitIndices(position.getWhitePawnsBits()))
        totalEval++;
    for (unsigned short i : getBitIndices(position.getWhiteKnightsBits()))
        totalEval+=2;
    for (unsigned short i : getBitIndices(position.getWhiteBishopsBits()))
        totalEval+=2;
    for (unsigned short i : getBitIndices(position.getWhiteRooksBits()))
        totalEval+=5;
    for (unsigned short i : getBitIndices(position.getWhiteQueensBits()))
        totalEval+=10;
    for (unsigned short i : getBitIndices(position.getBlackPawnsBits()))
        totalEval--;
    for (unsigned short i : getBitIndices(position.getBlackKnightsBits()))
        totalEval-=2;
    for (unsigned short i : getBitIndices(position.getBlackBishopsBits()))
        totalEval-=2;
    for (unsigned short i : getBitIndices(position.getBlackRooksBits()))
        totalEval-=5;
    for (unsigned short i : getBitIndices(position.getBlackQueensBits()))
        totalEval-=10;
    return totalEval;
}
int16_t simpleEvaluationFunctionBlack(BitPosition position)
{
    int16_t totalEval = 0;

    for (unsigned short i : getBitIndices(position.getWhitePawnsBits()))
        totalEval--;
    for (unsigned short i : getBitIndices(position.getWhiteKnightsBits()))
        totalEval-=2;
    for (unsigned short i : getBitIndices(position.getWhiteBishopsBits()))
        totalEval-=2;
    for (unsigned short i : getBitIndices(position.getWhiteRooksBits()))
        totalEval-=5;
    for (unsigned short i : getBitIndices(position.getWhiteQueensBits()))
        totalEval-=10;
    for (unsigned short i : getBitIndices(position.getBlackPawnsBits()))
        totalEval++;
    for (unsigned short i : getBitIndices(position.getBlackKnightsBits()))
        totalEval+=2;
    for (unsigned short i : getBitIndices(position.getBlackBishopsBits()))
        totalEval+=2;
    for (unsigned short i : getBitIndices(position.getBlackRooksBits()))
        totalEval+=5;
    for (unsigned short i : getBitIndices(position.getBlackQueensBits()))
        totalEval+=10;
    return totalEval;
}
// Main evaluation function
int16_t evaluationFunction(const BitPosition &position)
{
    if (ENGINEISWHITE)
    {
        return evaluationFunctionWhite(position);
    }
    else
    {
        return evaluationFunctionBlack(position);
    }
}

int16_t quiesenceSearch(BitPosition &position, int16_t alpha, int16_t beta, bool our_turn)
// This search is done when depth is less than or equal to 0 and considers only captures and promotions
{
    std::vector<Move> moves;
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
            return evaluationFunction(position);
        else if (position.inCheckAllMoves().size() == 0) // Mate
        {
            if (our_turn)
                return -32764;
            else
                return 32764;
        }
    }
    // If we are in quiescence, we have a baseline evaluation as if no captures happened
    int16_t value{evaluationFunction(position)};
    Move best_move;
    if (our_turn) // Maximize
    {
        for (Move move : moves)
        {
            position.makeCapture(move);
            int16_t child_value{quiesenceSearch(position, alpha, beta, false)};
            if (child_value > value)
            {
                value = child_value;
                best_move = move;
            }
            position.unmakeMove();
            if (value >= beta)
                break;
            alpha = std::max(alpha, value);
        }
    }
    else // Minimize
    {
        for (Move move : moves)
        {
            position.makeCapture(move);
            int16_t child_value{quiesenceSearch(position, alpha, beta, true)};
            if (child_value < value)
            {
                value = child_value;
                best_move = move;
            }
            position.unmakeMove();
            if (value <= alpha)
                break;
            beta = std::min(beta, value);
        }
    }

    return value;
}

int16_t alphaBetaSearch(BitPosition &position, int8_t depth, int16_t alpha, int16_t beta, bool our_turn)
// This search is done when depth is more than 0 and considers all moves
{
    if (position.isThreeFold())
        return 0;

    if (depth <= 0)
        return quiesenceSearch(position, alpha, beta, our_turn);

    position.setAttackedSquaresAfterMove();
    std::vector<Move> moves;
    if (position.isCheck())
    {
        position.setChecksAndPinsBits();
        moves = position.inCheckAllMoves();
        moves = position.orderAllMoves(moves);
    }
    else
    {
        position.setPinsBits();
        moves = position.allMoves();
        moves = position.orderAllMoves(moves);
    }
    if (moves.size() == 0)
    {
        if (not position.getIsCheck()) // Stalemate
            return 0;
        else if (our_turn) // Checkmate against us
            return -32764;
        else // Checkmate against opponent
            return 32764;
    }
    // If we are in quiescence, we have a baseline evaluation as if no captures happened
    int16_t value{our_turn ? static_cast<int16_t>(-32765) : static_cast<int16_t>(32765)};
    Move best_move;
    if (our_turn) // Maximize
    {
        for (Move move : moves)
        {
            position.makeNormalMove(move);
            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, false)};
            if (child_value > value)
            {
                value = child_value;
                best_move = move;
            }
            position.unmakeMove();
            if (value >= beta)
            {
                break;
            }
            alpha = std::max(alpha, value);
        }
    }
    else // Minimize
    {
        for (Move move : moves)
        {
            position.makeNormalMove(move);
            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, true)};
            if (child_value < value)
            {
                value = child_value;
                best_move = move;
            }
            position.unmakeMove();
            if (value <= alpha)
            {
                break;
            }
            beta = std::min(beta, value);
        }
    }

    return value;
}

std::pair<Move, int16_t> firstMoveSearch(BitPosition &position, int8_t depth, int16_t alpha, int16_t beta, bool our_turn, Move last_best_move)
// This search is done when depth is more than 0 and considers all moves
{
    position.setAttackedSquaresAfterMove();
    std::vector<Move> moves;
    if (position.isCheck())
    {
        position.setChecksAndPinsBits();
        moves = position.inCheckAllMoves();
        moves = position.orderAllMovesOnFirstIteration(moves, last_best_move);
    }
    else
    {
        position.setPinsBits();
        moves = position.allMoves();
        moves = position.orderAllMovesOnFirstIteration(moves, last_best_move);
    }
    // If we are in quiescence, we have a baseline evaluation as if no captures happened
    int16_t value{our_turn ? static_cast<int16_t>(-32765) : static_cast<int16_t>(32765)};
    Move best_move;
    if (our_turn) // Maximize
    {
        for (Move move : moves)
        {
            position.makeNormalMove(move);
            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, false)};
            if (child_value > value)
            {
                value = child_value;
                best_move = move;
            }
            position.unmakeMove();
            if (value >= beta)
            {
                break;
            }
            alpha = std::max(alpha, value);
        }
    }
    else // Minimize
    {
        for (Move move : moves)
        {
            position.makeNormalMove(move);
            int16_t child_value{alphaBetaSearch(position, depth - 1, alpha, beta, true)};
            if (child_value < value)
            {
                value = child_value;
                best_move = move;
            }
            position.unmakeMove();
            if (value <= alpha)
            {
                break;
            }
            beta = std::min(beta, value);
        }
    }

    return std::pair<Move, int16_t>(best_move, value);
}

Move iterativeSearch(BitPosition position, int time_for_move, int8_t fixed_max_depth = 100)
{
    std::chrono::milliseconds timeForMoveMS(time_for_move);
    auto start_time = std::chrono::high_resolution_clock::now(); // Start timing
    ENGINEISWHITE = position.getTurn();
    Move bestMove{};
    int16_t bestValue;
    std::pair<Move, int> pair;
    for (int8_t depth = 1; depth <= fixed_max_depth; ++depth)
    {
        int16_t alpha{-32766};
        int16_t beta{32766};
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
            std::cout << "Depth " << unsigned(depth) << "\n";
            std::cout << evaluationFunction(position) << "\n" << std::flush;
            break;
        }
    }
    return bestMove;
}
#endif
