#ifndef WORKER_H
#define WORKER_H

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "network.h"
#include "bitposition.h" // Move, BitPosition, StateInfo

class TranspositionTable;
class ThreadPool;

//  Worker — performs the search (one per thread)
class Worker
{
public:
    Worker(TranspositionTable &ttable,
           ThreadPool &threadpool,
           NNUEU::Network networkIn,
           const NNUEU::Transformer &transformerIn,
           size_t idx);

    // Entry point called by Thread::startSearching()
    std::pair<Move, int16_t> startSearching();

    bool isMainThread() const { return threadIdx == 0; }

private:
    // —— core search routines ——
    std::pair<Move, int16_t> iterativeSearch(int8_t start_depth = 1,
                                             int8_t fixed_max_depth = 99);
    std::pair<Move, int16_t> firstMoveSearch(int8_t depth,
                                             int16_t alpha,
                                             int16_t beta,
                                             std::chrono::milliseconds predictedTimeTakenMs);
    int16_t alphaBetaSearch(int8_t depth, int16_t alpha, int16_t beta, bool our_turn);
    int16_t quiesenceSearch(int16_t alpha, int16_t beta, bool our_turn);
    bool stopSearch(const std::vector<int16_t> &values,
                    int streak,
                    int depth,
                    BitPosition &position);

    // —— time / limits ——
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    int lastFirstMoveTimeTakenMS;
    std::chrono::milliseconds timeForMoveMS;
    std::chrono::milliseconds timeLimit;

    // —— root‑level bookkeeping ——
    bool ponder;
    bool isEndgame;
    int completedDepth;
    std::unordered_map<Move, std::vector<int16_t>> moveDepthValues;

    BitPosition rootPos; // cloned before each search
    StateInfo rootState; // thread‑local mutable root
    std::vector<Move> rootMoves;
    std::vector<int16_t> rootScores;
    BitPosition currentPos;

    // —— thread / TT / NNUEU context ——
    size_t threadIdx;
    ThreadPool &threads;
    TranspositionTable &tt;
    NNUEU::AccumulatorStack accumulatorStack;
    NNUEU::Network network;
    const NNUEU::Transformer *transformer;

    friend class ThreadPool;
};

#endif // WORKER_H