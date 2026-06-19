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
           NNUEU::Network &networkIn,
           const NNUEU::Transformer &transformerIn,
           size_t idx);

    // Entry point called by Thread::startSearching()
    std::pair<Move, int16_t> startSearching(int8_t max_depth);

    bool isMainThread() const { return threadIdx == 0; }

    // --- search introspection (for the search-tree / EBF comparison harness) ---
    // Reset at the start of every search; counted in alphaBetaSearch / quiesenceSearch.
    uint64_t nodes = 0, qnodes = 0;                 // total / quiescence nodes this search
    uint64_t cntTTcut = 0, cntNMP = 0, cntLMR = 0;  // TT-cutoffs / null-move prunes / LMR reductions
    uint64_t cntBeta = 0, cntBetaFirst = 0;         // beta cutoffs total / on the first move (ordering quality)
    bool infoNoEarlyStop = false;                   // "go depth N": run every depth to N + print per-depth info

private:
    // Makes move and pushes on the accumulator
    inline void makeMove(Move move, StateInfo &st)
    {
        accumulatorStack.push(currentPos.makeMove(move, st));
    }
    // Makes capture and pushes on the accumulator
    inline void makeCapture(Move move, StateInfo &st)
    {
        accumulatorStack.push(currentPos.makeCapture(move, st));
    }
    // Unmakes move and pops on the accumulator
    inline void unmakeMove(Move move)
    {
        currentPos.unmakeMove(move);
        accumulatorStack.pop();
    }
    // Unmakes move and pops on the accumulator
    inline void unmakeCapture(Move move)
    {
        currentPos.unmakeCapture(move);
        accumulatorStack.pop();
    }
    // Null move (for null-move pruning): pushes a no-op accumulator change.
    inline void makeNullMove(StateInfo &st)
    {
        accumulatorStack.push(currentPos.makeNullMove(st));
    }
    inline void unmakeNullMove()
    {
        currentPos.unmakeNullMove();
        accumulatorStack.pop();
    }
    // Record a quiet beta-cutoff move as a killer for this ply (captures/promotions,
    // which score != 0 in aBMoveValue, are ignored).
    inline void storeKiller(int ply, Move m)
    {
        if (ply < MAX_SEARCH_PLY && currentPos.aBMoveValue(m) == 0 && !(killers[ply][0] == m))
        {
            killers[ply][1] = killers[ply][0];
            killers[ply][0] = m;
        }
    }

    // Calls first move search iteratively
    void iterativeSearch(int8_t start_depth = 2, int8_t fixed_max_depth = 99);

    // Calls alphaBetaSearch after each root move
    void firstMoveSearch(int8_t depth);

    // Calls quisence when depth 0 is reached
    int16_t alphaBetaSearch(int8_t depth, int16_t alpha, int16_t beta, int ply, bool allowNull = true);

    // Only captures are searched making sure there is no mate in the end
    int16_t quiesenceSearch(int16_t alpha, int16_t beta);

    // Stop search if certain criteria is met
    bool stopSearch(const std::vector<int16_t> &values,
                    int streak,
                    int depth);

    // Time control
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    std::chrono::milliseconds softTimeLimit;
    std::chrono::milliseconds hardTimeLimit;

    // Root‑level bookkeeping
    bool ponder;
    int completedDepth;

    // Stopping condition utilities
    bool isEndgame;
    std::unordered_map<Move, std::vector<int16_t>> moveDepthValues;

    BitPosition rootPos; // cloned before each search
    StateInfo rootState; // thread‑local mutable root
    std::vector<Move> rootMoves;
    std::vector<int16_t> rootScores;
    Move bestRootMove;
    int16_t bestRootValue;
    // BitPosition object within search
    BitPosition currentPos;

    // Move ordering: 2 killer moves (quiet moves that produced a beta cutoff) per ply.
    static constexpr int MAX_SEARCH_PLY = 128;
    Move killers[MAX_SEARCH_PLY][2];

    // Butterfly history: [sideToMove][from][to]. "How often has this quiet move produced
    // a beta cutoff." Orders quiet moves (only killers rank above) and scales LMR. Reset
    // to 0 at the start of every search.
    int32_t mainHistory[2][64][64];
    static constexpr int HISTORY_MAX = 16384;

    inline int historyScore(bool stm, Move m) const
    {
        return mainHistory[stm][m.getOriginSquare()][m.getDestinationSquare()];
    }

    // On a quiet beta cutoff: reward the cutting move and penalise the quiet moves tried
    // before it that failed to cut. Uses the standard "history gravity" update so entries
    // saturate towards +/-HISTORY_MAX instead of growing without bound.
    inline void updateHistory(bool stm, Move cutMove, int depth, const Move *tried, int nTried)
    {
        const int bonus = depth * depth < 400 ? depth * depth : 400;
        auto bump = [&](Move m, int b)
        {
            int32_t &e = mainHistory[stm][m.getOriginSquare()][m.getDestinationSquare()];
            const int ab = b < 0 ? -b : b;
            e += b - e * ab / HISTORY_MAX;
        };
        bump(cutMove, bonus);
        for (int i = 0; i < nTried; ++i)
            if (!(tried[i] == cutMove))
                bump(tried[i], -bonus);
    }

    // Threading (to implement)
    size_t threadIdx;
    ThreadPool &threads;

    // Transposition table
    TranspositionTable &tt;

    // NNUEU
    NNUEU::AccumulatorStack accumulatorStack;
    NNUEU::Network &network;
    const NNUEU::Transformer *transformer;

    friend class ThreadPool;
};

#endif // WORKER_H