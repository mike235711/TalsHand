#ifndef ENGINE_H
#define ENGINE_H

#include <cstdint>

#include "bitposition.h"
#include "threadpool.h"
#include "network.h"
#include "ttable.h"

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

    // Test move generation
    std::uint64_t perftTest(const std::string &fen, int depth);

    // uci functions
    void readUci(); // Reads uci loop (options, position and time)
    void setPosition(const std::string &fen, const std::vector<std::string> &moves);

    // non blocking call to start searching
    void goSearch();
    // non blocking call to stop searching
    void stopSearch();

    // blocking call to wait for search to finish
    void waitToFinishSearch();

    // modifiers
    void setTimeLimit(const int ourTime, const int ourInc);
    void setTTSize();
    void resizeThreads();
    void setPonderHit(bool);
    void searchClear();

    // network related
    void loadNNUEU();

private:
    BitPosition pos;
    // If a std::vector were used, adding new elements beyond its current capacity could 
    // cause it to reallocate its memory, which would invalidate any pointers to its existing 
    // elements. With a std::deque, pointers to the elements remain stable, which is essential 
    // for safely using the previous pointer within the StateInfo struct.
    std::unique_ptr<std::deque<StateInfo>> stateInfos;
    // timeLimit is ourInc + ourTime, the Worker will then manage the time based on improving strikes
    int timeLimit;

    // Manages threads but for the moment we will keep it simple only with the main_thread()
    ThreadPool threadpool;
    // Transposition table with size given in the configuration
    TranspositionTable tt;
    // second, third and fourth layers of the NNUEU
    NNUEU::Network network;

    // Stuff to read from the configuration at initialization
    int numThreads; // Number of threads to use, for the moment we use 1 to keep it simple
    size_t ttSize; // Transposition table size
    bool ponder; // If the engine will think in opponent's time or not, for the moment we dont ponder to keep it simple
    std::string NNUEUFile; // Location of where the NNUEU weights are stored to be loaded
};

#endif // #ifndef ENGINE_H