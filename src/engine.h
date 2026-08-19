#ifndef ENGINE_H
#define ENGINE_H

#include <cstdint>
#include <memory>
#include <deque>
#include <filesystem>
#include <string>
#include <utility>

#include <optional>

#include "bitposition.h"
#include "threadpool.h"
#include "network.h"
#include "ttable.h"

namespace NNUEU
{
    class Transformer;
}

class THEngine
{
public:
    THEngine(std::optional<std::string> path = std::nullopt);

    // Cannot be movable due to components holding backreferences to fields
    THEngine(const THEngine &) = delete;
    THEngine(THEngine &&) = delete;
    THEngine &operator=(const THEngine &) = delete;
    THEngine &operator=(THEngine &&) = delete;

    ~THEngine() { waitToFinishSearch(); }

    // Test move generation (alpha-beta legal-move perft).
    std::uint64_t perftTest(int depth, const std::optional<std::string>& filename = std::nullopt);

    // Walks the full AB-legal move tree to `depth` and, at every node, checks that
    // the quiescence capture selectors emit exactly the AB-legal moves that are
    // captures or queen promotions (see BitPosition::isQSCaptureOrQueenProm).
    // Returns the perft node count and the number of nodes whose QS capture set
    // disagreed with the reference (0 == fully consistent).
    struct QSConsistencyResult { std::uint64_t nodes; std::uint64_t mismatches; };
    QSConsistencyResult qsCaptureConsistencyTest(int depth);

    // uci functions
    void readUci(); // Reads uci loop (options, position and time)
    void setPosition(const std::string &fen, const std::vector<std::string> &moves);

    // non blocking call to start searching
    void goSearch();
    // "go movetime N": think for exactly N ms (no early stop; hard abort at N).
    void goSearchMovetime(int ms);
    // fixed-depth search ("go depth N") for the search-tree / EBF comparison harness
    void goSearchDepth(int depth);
    // non blocking call to stop searching
    void stopSearch();

    // blocking call to wait for search to finish
    void waitToFinishSearch();

    // modifiers
    void settimeLeft(const int ourTime, const int ourInc);
    void setTTSize();
    void resizeThreads();
    void setPonderHit(bool);
    void searchClear();

    // network related
    void loadNNUEU();

    // Performance testing utilities
    std::string searchFixedDepth(int8_t depth);
    std::string searchWithTimeConstraint(int timeLimitMs);
    // Like searchFixedDepth but also returns the root score (useful for mate
    // detection in tests: a forced mate yields a score of large magnitude).
    std::pair<std::string, int16_t> searchFixedDepthWithScore(int8_t depth);

private:
    BitPosition pos;
    // If a std::vector were used, adding new elements beyond its current capacity could 
    // cause it to reallocate its memory, which would invalidate any pointers to its existing 
    // elements. With a std::deque, pointers to the elements remain stable, which is essential 
    // for safely using the previous pointer within the StateInfo struct.
    std::unique_ptr<std::deque<StateInfo>> stateInfos;
    // timeLeft is ourInc + ourTime, the Worker will then manage the time based on improving strikes
    int timeLeft;
    int ourClock = 0; // our remaining base clock (ms); used to set the safe per-move hard cap

    // Manages threads but for the moment we will keep it simple only with the main_thread()
    ThreadPool threadpool;
    // Transposition table with size given in the configuration
    TranspositionTable tt;
    // second, third and fourth layers of the NNUEU
    NNUEU::Network network;
    // Used to transform the accumulator, it is heavy so we use a pointer
    std::unique_ptr<NNUEU::Transformer> transformer;
    

    // Stuff to read from the configuration at initialization
    int numThreads; // Number of threads to use, for the moment we use 1 to keep it simple
    size_t ttSize; // Transposition table size
    bool ponder; // If the engine will think in opponent's time or not, for the moment we dont ponder to keep it simple
    std::string NNUEUFile; // Location of where the NNUEU weights are stored to be loaded
    std::filesystem::path execDir; // Executable directory
};

#endif // #ifndef ENGINE_H