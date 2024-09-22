#ifndef DATAGENERATOR_H
#define DATAGENERATOR_H

#include "bitposition.h"
#include <vector>
#include <utility> // For std::pair
#include <limits>  // For numeric limits
#include <array>
#include <cstdint>
#include <chrono>
#include <algorithm> // For std::max
#include "ttable.h"
#include <memory>
#include "position_eval.h"
#include "nnue_ttable.h"

// We are going to make the engine play against itself on some initial positions.
// At some points in the search the engine will reach positions and evaluate them with the NNUE,
// we store the zobrist keys and evaluations of these positions in a Table.
// Then when we perform another search if these positions show up at a depth higher than or equal to minDepthSave,
// if the new evaluations we get differ by the NNUE evaluations by more than minEvalDiff, we store the positions in a file.

extern TranspositionTable globalTT;
extern TranspositionTableNNUE nnueTT;

// Globals for this file
int16_t MIN_EVAL_DIFF;
int MIN_DEPTH_SAVE;
std::string OUT_FILE_NAME;

bool valuesDiffer(int16_t evalSearch, int evalSearchType, int16_t evalNNUE)
// Return true if we want to save the position and value.
// evalSearchType = 0 : exact, 1 : lower bound, 2 : upper bound.
{
    // If evalSearch is exact or a lower bound, and evalSearch is higher than evallNNUE by more than the minEvalDiff
    if ((evalSearchType == 0 || evalSearchType == 1) && ((evalSearch - evalNNUE) >= MIN_EVAL_DIFF))
        return true;
    // If evalSearch is exact or a upper bound, and evalSearch is lower than evallNNUE by more than the minEvalDiff
    else if ((evalSearchType == 0 || evalSearchType == 1) && ((evalNNUE - evalSearch) >= MIN_EVAL_DIFF))
        return true;
    return false;
}

void saveFenAndNNUEValue(BitPosition &position, int16_t evalNNUE)
{
    // Open the file in append mode
    std::ofstream outFile(OUT_FILE_NAME, std::ios::app);
    // Check if the file opened successfully
    if (!outFile)
    {
        std::cerr << "Error opening file: " << OUT_FILE_NAME << std::endl;
        return;
    }
    // Write the position and evaluation value to the file
    outFile << position.toFenString() << "," << evalNNUE << "\n";
    // Close the file
    outFile.close();
}

int16_t quiesenceSearchGen(BitPosition &position, int16_t alpha, int16_t beta, bool our_turn)
// This search is done when depth is less than or equal to 0 and considers only captures and promotions
{
    // std::vector<Capture> captures;
    // bool isCheck;
    // position.setAttackedSquaresAfterMove();
    // if (position.isCheck())
    // {
    //     isCheck = true;
    //     position.setChecksAndPinsBits();
    //     captures = position.inCheckOrderedGoodCaptures();
    // }
    // else
    // {
    //     isCheck = false;
    //     position.setPinsBits();
    //     captures = position.orderedGoodCaptures();
    // }
    // bool noGoodCaptures{false};
    // // If there are no captures we return an eval
    // if (captures.size() == 0)
    // {
    //     noGoodCaptures = true;
    //     // If there is a check
    //     if (isCheck)
    //     {
    //         captures = position.inCheckOrderedBadCaptures();
    //         if (captures.size() == 0)
    //         {   
    //             // Mate
    //             if (position.isMate())
    //             {
    //                 if (our_turn)
    //                     return -30000;
    //                 else
    //                     return 30000;
    //             }
    //             // In check quiet position
    //             else
    //             {
    //                 int16_t eval{NNUE::evaluationFunction(our_turn)};
    //                 nnueTT.save(position.getZobristKey(), eval); // Save eval given by NNUE
    //                 return eval;
    //             }
    //         }
    //     }
    //     // If there is no check
    //     else
    //     {
    //         captures = position.orderedBadCaptures();
    //         if (captures.size() == 0)
    //         {
    //             // Stalemate
    //             if (position.isStalemate())
    //                 return 0;
    //             // Quiet position
    //             else
    //             {
    //                 int16_t eval{NNUE::evaluationFunction(our_turn)};
    //                 nnueTT.save(position.getZobristKey(), eval); // Save eval given by NNUE
    //                 return eval;
    //             }
    //         }
    //     }
    // }
    // // If we are in quiescence, we have a baseline evaluation as if no captures happened
    // int16_t value{NNUE::evaluationFunction(our_turn)};
    // nnueTT.save(position.getZobristKey(), value); // Save eval given by NNUE
    // Capture best_move;
    // bool cutoff{false};
    // if (our_turn) // Maximize
    // {
    //     for (Capture capture : captures)
    //     {
    //         position.makeCapture(capture);
    //         int16_t child_value{quiesenceSearchGen(position, alpha, beta, false)};
    //         if (child_value > value)
    //         {
    //             value = child_value;
    //             best_move = capture;
    //         }
    //         position.unmakeCapture(capture);
    //         if (value >= beta)
    //         {
    //             cutoff = true;
    //             break;
    //         }
    //         alpha = std::max(alpha, value);
    //     }
    // }
    // else // Minimize
    // {
    //     for (Capture capture : captures)
    //     {
    //         position.makeCapture(capture);
    //         int16_t child_value{quiesenceSearchGen(position, alpha, beta, true)};
    //         if (child_value < value)
    //         {
    //             value = child_value;
    //             best_move = capture;
    //         }
    //         position.unmakeCapture(capture);
    //         if (value <= alpha)
    //         {
    //             cutoff = true;
    //             break;
    //         }
    //         beta = std::min(beta, value);
    //     }
    // }
    // // Search bad captures if no alpha beta cutoff has been made on the good ones
    // if (not cutoff && not noGoodCaptures)
    // {
    //     captures = position.orderedBadCaptures();

    //     if (our_turn) // Maximize
    //     {
    //         for (Capture capture : captures)
    //         {
    //             position.makeCapture(capture);
    //             int16_t child_value{quiesenceSearchGen(position, alpha, beta, false)};
    //             if (child_value > value)
    //             {
    //                 value = child_value;
    //                 best_move = capture;
    //             }
    //             position.unmakeCapture(capture);
    //             if (value >= beta)
    //             {
    //                 break;
    //             }
    //             alpha = std::max(alpha, value);
    //         }
    //     }
    //     else // Minimize
    //     {
    //         for (Capture capture : captures)
    //         {
    //             position.makeCapture(capture);
    //             int16_t child_value{quiesenceSearchGen(position, alpha, beta, true)};
    //             if (child_value < value)
    //             {
    //                 value = child_value;
    //                 best_move = capture;
    //             }
    //             position.unmakeCapture(capture);
    //             if (value <= alpha)
    //             {
    //                 break;
    //             }
    //             beta = std::min(beta, value);
    //         }
    //     }
    // }
    // return value;
}

int16_t alphaBetaSearchGen(BitPosition &position, int8_t depth, int16_t alpha, int16_t beta, bool our_turn)
// This search is done when depth is more than 0 and considers all moves and stores positions in the transposition table
{
    // // Threefold repetition
    // if (position.isThreeFold())
    //     return 0;
    // // Check if we have stored this position in ttable
    // TTEntry *ttEntry = globalTT.probe(position.getZobristKey());
    // Move tt_move;
    // // If position is stored in ttable
    // if (ttEntry != nullptr)
    // {
    //     // At ttable's shalower depth we get the best move
    //     if (ttEntry->getDepth() < depth)
    //         tt_move = ttEntry->getMove();
    //     // Exact value at deeper depth 
    //     else if (ttEntry->getDepth() >= depth && ttEntry->getIsExact())
    //         return ttEntry->getValue();
    //     // Lower bound at deeper depth
    //     else if (ttEntry->getDepth() >= depth && our_turn)
    //     {
    //         tt_move = ttEntry->getMove();
    //         alpha = ttEntry->getValue();
    //     }
    //     // Upper bound at deeper depth
    //     else
    //     {
    //         tt_move = ttEntry->getMove();
    //         beta = ttEntry->getValue();
    //     }
    // }
    // // At depths <= 0 we enter quiscence search
    // if (depth <= 0)
    //     return quiesenceSearchGen(position, alpha, beta, our_turn);
    // // Get the legal moves
    // position.setAttackedSquaresAfterMove();
    // std::vector<Move> moves;
    // if (position.isCheck())
    // {
    //     position.setChecksAndPinsBits();
    //     moves = position.inCheckAllMoves();
    //     moves = position.orderAllMoves(moves, tt_move);
    // }
    // else
    // {
    //     position.setPinsBits();
    //     moves = position.allMoves();
    //     moves = position.orderAllMoves(moves, tt_move);
    // }
    // // Game finished since there are no legal moves
    // if (moves.size() == 0)
    // {
    //     // Stalemate
    //     if (not position.getIsCheck())
    //         return 0;
    //     // Checkmate against us
    //     else if (our_turn)
    //         return -30000;
    //     // Checkmate against opponent
    //     else
    //         return 30000;
    // }
    // // Baseline evaluation
    // int16_t value{our_turn ? static_cast <int16_t>(-30001) : static_cast <int16_t>(30001)};
    // Move best_move;
    // bool cutoff{false};
    // // Maximize
    // if (our_turn)
    // {
    //     for (Move move : moves)
    //     {
    //         position.makeNormalMove(move);
    //         int16_t child_value{alphaBetaSearchGen(position, depth - 1, alpha, beta, false)};
    //         if (child_value > value)
    //         {
    //             value = child_value;
    //             best_move = move;
    //         }
    //         position.unmakeNormalMove(move);
    //         if (value >= beta)
    //         {
    //             cutoff = true;
    //             if (depth >= 2)
    //                 globalTT.save(position.getZobristKey(), value, depth, best_move, false);
    //             break;
    //         }
    //         alpha = std::max(alpha, value);
    //     }
    // }
    // // Minimize
    // else
    // {
    //     for (Move move : moves)
    //     {
    //         position.makeNormalMove(move);
    //         int16_t child_value{alphaBetaSearchGen(position, depth - 1, alpha, beta, true)};
    //         if (child_value < value)
    //         {
    //             value = child_value;
    //             best_move = move;
    //         }
    //         position.unmakeNormalMove(move);
    //         if (value <= alpha)
    //         {
    //             cutoff = true;
    //             if (depth >= 2)
    //                 globalTT.save(position.getZobristKey(), value, depth, best_move, false);
    //             break;
    //         }
    //         beta = std::min(beta, value);
    //     }
    // }
    // // Saving an exact value
    // if (not cutoff && depth >= 2)
    //     globalTT.save(position.getZobristKey(), value, depth, best_move, true);

    // // Check if this position has been evaluated by the NNUE and if the eval was far from the one we got, save fen and eval
    // if (depth >= MIN_DEPTH_SAVE)
    // {
    //     TTNNUEEntry *ttnnueEntry = nnueTT.probe(position.getZobristKey());
    //     // If position is stored in NNUE transposition table
    //     if (ttnnueEntry != nullptr)
    //     {
    //         if (our_turn && cutoff) // Lower bound
    //         {
    //             if (valuesDiffer(value, 0, ttnnueEntry->getValue()))
    //                 saveFenAndNNUEValue(position, value);
    //         }
    //         else if (not our_turn && cutoff) // Upper bound
    //         {
    //             if (valuesDiffer(value, 0, ttnnueEntry->getValue()))
    //                 saveFenAndNNUEValue(position, value);
    //         }
    //         else // Exact value
    //         {   
    //             if (valuesDiffer(value, 0, ttnnueEntry->getValue()))
    //                 saveFenAndNNUEValue(position, value);
    //         };
    //     }
    // }
    // return value;
}

std::tuple<Move, int16_t, std::vector<int16_t>> firstMoveSearchGen(BitPosition &position, int8_t depth, int16_t alpha, int16_t beta, bool our_turn, std::vector<int16_t> &first_moves_scores, std::chrono::milliseconds timeForMoveMS, std::chrono::high_resolution_clock::time_point startTime)
// This search is done when depth is more than 0 and considers all moves
{
    // position.setAttackedSquaresAfterMove();
    
    // TTEntry *ttEntry = globalTT.probe(position.getZobristKey());
    // Move tt_move;
    // // If position is stored in transposition table
    // if (ttEntry != nullptr)
    // {
    //     // If depth in ttable is lower than the one we are going to search, we just use the tt_move
    //     if (ttEntry->getDepth() < depth)
    //         tt_move = ttEntry->getMove();

    //     // If depth in ttable is higher or equal than the one we are going to search:
    //     // 1) Exact value, we just return it (no need to search at a lower depth)
    //     else if (ttEntry->getDepth() >= depth && ttEntry->getIsExact())
    //         return std::tuple<Move, int16_t, std::vector<int16_t>> (ttEntry->getMove(), ttEntry->getValue(), first_moves_scores);
    //     // 2) Lower bound at deeper depth and best move found
    //     else if (ttEntry->getDepth() >= depth && our_turn)
    //     {
    //         tt_move = ttEntry->getMove();
    //         alpha = ttEntry->getValue();
    //     }
    //     // 3) Upper bound at deeper depth and best move found
    //     else
    //     {
    //         tt_move = ttEntry->getMove();
    //         beta = ttEntry->getValue();
    //     }
    // }
    // // Get legal moves
    // std::vector<Move> moves;
    // if (position.isCheck())
    // {
    //     position.setChecksAndPinsBits();
    //     moves = position.inCheckAllMoves();
    // }
    // else
    // {
    //     position.setPinsBits();
    //     moves = position.allMoves();
    // }
    // // Order the moves based on scores
    // if (first_moves_scores.empty())
    // {
    //     moves = position.orderAllMovesOnFirstIterationFirstTime(moves, tt_move);
    //     first_moves_scores.reserve(moves.size());
    // }
    // else
    // {
    //     std::pair<std::vector<Move>, std::vector<int16_t>> result = position.orderAllMovesOnFirstIteration(moves, first_moves_scores);
    //     moves = result.first;
    //     first_moves_scores = result.second;
    // }

    // // Baseline evaluation
    // int16_t value{our_turn ? static_cast <int16_t>(-30001) : static_cast <int16_t>(30001)};
    // Move best_move;

    // // Maximize
    // if (our_turn)
    // {
    //     for (std::size_t i = 0; i < moves.size(); ++i)
    //     {
    //         Move move{moves[i]};
    //         position.makeNormalMove(move);
    //         int16_t child_value{alphaBetaSearchGen(position, depth - 1, alpha, beta, false)};
    //         first_moves_scores[i] = child_value;
    //         if (child_value > value)
    //         {
    //             value = child_value;
    //             best_move = move;
    //         }
    //         position.unmakeNormalMove(move);
    //         alpha = std::max(alpha, value);

    //         // Calculate the elapsed time in milliseconds
    //         std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() - startTime;
    //         // Check if the duration has been exceeded
    //         if (duration >= timeForMoveMS)
    //             break;
    //     }
    // }
    // // Minimize
    // else
    // {
    //     for (std::size_t i = 0; i < moves.size(); ++i)
    //     {
    //         Move move{moves[i]};
    //         position.makeNormalMove(move);
    //         int16_t child_value{alphaBetaSearchGen(position, depth - 1, alpha, beta, true)};
    //         first_moves_scores[i] = child_value;
    //         if (child_value < value)
    //         {
    //             value = child_value;
    //             best_move = move;
    //         }
    //         position.unmakeNormalMove(move);
    //         beta = std::min(beta, value);
    //         // Calculate the elapsed time in milliseconds
    //         std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() - startTime;
    //         // Check if the duration has been exceeded
    //         if (duration >= timeForMoveMS)
    //             break;
    //     }
    // }
    // // Saving an exact value in ttable
    // if (depth >= 2)
    //     globalTT.save(position.getZobristKey(), value, depth, best_move, true);

    // // Check if this position has been evaluated by the NNUE and if the eval was far from the one we got, save fen and eval
    // if (depth >= MIN_DEPTH_SAVE)
    // {
    //     TTNNUEEntry *ttnnueEntry = nnueTT.probe(position.getZobristKey());
    //     // If position is stored in NNUE transposition table
    //     if (ttnnueEntry != nullptr)
    //     {
    //         // If the NNUE failed 
    //         if (valuesDiffer(value, 0, ttnnueEntry->getValue()))
    //             saveFenAndNNUEValue(position, value);
    //     }
    // }

    // return std::tuple<Move, int16_t, std::vector<int16_t>>(best_move, value, first_moves_scores);
}

std::pair<Move, int16_t> iterativeSearchGen(BitPosition position, int timeForMoveMilliseconds, int16_t minEvalDiff, int minDepthSave, std::string outFileName, int8_t fixed_max_depth = 100)
{
    // std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    // startTime = std::chrono::high_resolution_clock::now();
    // std::chrono::milliseconds timeForMoveMS(timeForMoveMilliseconds);
    // MIN_EVAL_DIFF = minEvalDiff;
    // MIN_DEPTH_SAVE = minDepthSave;
    // OUT_FILE_NAME = outFileName;
    // Move bestMove{};
    // int16_t bestValue;
    // std::tuple<Move, int16_t, std::vector<int16_t>> tuple;
    // std::vector<int16_t> first_moves_scores; // For first move ordering
    // for (int8_t depth = 1; depth <= fixed_max_depth; ++depth)
    // {
    //     int16_t alpha{-30002};
    //     int16_t beta{30002};

    //     // Search
    //     tuple = firstMoveSearchGen(position, depth, alpha, beta, true, first_moves_scores, timeForMoveMS, startTime);
    //     bestMove = std::get<0>(tuple);
    //     bestValue = std::get<1>(tuple);
    //     first_moves_scores = std::get<2>(tuple);

    //     // Calculate the elapsed time in milliseconds
    //     std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() - startTime;
    //     // Check if the duration has been exceeded
    //     if (duration >= timeForMoveMS)
    //     {
    //         nnueTT.printTableMemory();
    //         std::cout << "Depth: " << unsigned(depth) << "\n";
    //         break;
    //     }
    // }
    // return std::pair<Move, int16_t>(bestMove, bestValue);
}

#endif // DATAGENERATOR_H
