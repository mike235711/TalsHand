#ifndef WORKER_H
#define WORKER_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "network.h"

class TranspositionTable;
class ThreadPool;


// Stack struct keeps track of the information we need to remember from nodes
// shallower and deeper in the tree during the search. Each search thread has
// its own array of Stack objects, indexed by the current ply.
// This is not implemented yet, but could be useful later
struct Stack
{
    Move *pv;
    int ply;
    Move currentMove;
    int staticEval;
    int statScore;
    int moveCount;
    bool inCheck;
    bool ttPv;
    bool ttHit;
    int cutoffCnt;
    int reduction;
    bool isPvNode;
};

// Worker is the class that performs the search. In a future version I will implement a threaded version
class Worker
{
public:
    Worker(TranspositionTable &ttable, ThreadPool &threadpool, NNUEU::Network network, size_t);

    // Called at instantiation to initialize reductions tables.
    // Reset histories, usually before a new game.
    void clear();

    // This calls iterativeSearch, in a future version it will handle threads
    std::pair<Move, int16_t> startSearching(int8_t start_depth, int8_t fixed_max_depth) { return iterativeSearch(start_depth, fixed_max_depth); };

    bool isMainThread() const { return threadIdx == 0; }

private:
    std::pair<Move, int16_t> iterativeSearch(int8_t start_depth, int8_t fixed_max_depth);
    std::pair<Move, int16_t> firstMoveSearch(int8_t depth, int16_t alpha, int16_t beta, std::chrono::milliseconds predictedTimeTakenMs);

    // This is the main search function, for both PV and non-PV nodes
    int16_t alphaBetaSearch(int8_t depth, int16_t alpha, int16_t beta, bool our_turn);

    // Quiescence search function, which is called by the main search
    int16_t quiesenceSearch(int16_t alpha, int16_t beta, bool our_turn);

    // Stop search based on some criteria
    bool stopSearch(const std::vector<int16_t> &values, int streak, int depth, BitPosition &position);

    // Time point at which we start searching
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    int lastFirstMoveTimeTakenMS;
    std::chrono::milliseconds timeForMoveMS;

    // Time limit
    std::chrono::milliseconds timeLimit;

    // If pondering or not (not implemented pondering yet)
    bool ponder;

    // If we are in endgame or not a root position
    bool isEndgame;

    // To find streaks of moves for several depths
    std::unordered_map<Move, std::vector<int16_t>> moveDepthValues;

    BitPosition currentPos;
    BitPosition rootPos;
    StateInfo rootState;
    std::vector<Move> rootMoves;
    std::vector<int16_t> rootScores;

    // Deepest depth searched so far on current position
    int completedDepth;

    size_t threadIdx; // Indicates which thread is using the worker (However only main thread is implemented yet)

    ThreadPool &threads;
    TranspositionTable &tt;

    // Used by NNUE
    NNUEU::AccumulatorStack accumulatorStack;
    NNUEU::Network network;

    friend class ThreadPool;
};

#endif // #ifndef WORKER_H