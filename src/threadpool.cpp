#include "threadpool.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <cstring>     // std::memcpy
#include <type_traits> // std::is_trivially_copyable_v

// Constructor (To finish)
Thread::Thread()
{
    // IDontKnow
}

// Destructor wakes up the thread in idle_loop() and waits
// for its termination. Thread should be already waiting.
Thread::~Thread()
{
    exit = true;
    stdThread.join();
}

// Wakes up the thread that will start the search
void Thread::run_custom_job(std::function<void()> f)
{
    if (f)
        f(); // for now we run it synchronously
}

void Thread::startSearching()
{
    assert(worker);
    run_custom_job([this]
                   { worker->startSearching(); });
}

// Blocks on the condition variable until the thread has finished searching
void Thread::waitToFinishSearch()
{
    // std::unique_lock<std::mutex> lk(mutex);
    // cv.wait(lk, [&]
    //         { return !searching; });
}

void ThreadPool::set(int numThreads, TranspositionTable &tt, NNUEU::Network &network, const NNUEU::Transformer &transformer)
{
    threads.clear();
    if (numThreads < 1)
        numThreads = 1;

    threads.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i)
    {
        auto t = std::make_unique<Thread>();
        t->worker = std::make_unique<Worker>(tt, *this,
                                             network, transformer,
                                             static_cast<size_t>(i));
        threads.emplace_back(std::move(t));
    }
}

void ThreadPool::clear()
{
}

void ThreadPool::waitToFinishSearch()
{
}

/// Fast, thread‑safe clone: raw‑copy BitPosition + deep copy of root StateInfo
inline void clonePositionPerThread(const BitPosition &src,
                                   BitPosition &dst,
                                   StateInfo &dstRoot,
                                   StateInfo *sharedTail)
{
    static_assert(std::is_trivially_copyable_v<BitPosition>,
                  "BitPosition stopped being trivially-copyable – update this clone!");

    std::memcpy(&dst, &src, sizeof(BitPosition)); // raw copy of ~450 B
    dst.setState(&dstRoot);                       // re-wire root pointer

    dstRoot = *src.getState();     // deep copy the root
    dstRoot.previous = sharedTail; // share older history
    dstRoot.next = nullptr;
}

void ThreadPool::startThinking(BitPosition &pos,
                               std::unique_ptr<std::deque<StateInfo>> &stateInfos,
                               int timeLimit,
                               bool pondering)
{
    main_thread()->waitToFinishSearch();
    main_thread()->worker->ponder = pondering;

    // If we received a fresh move list, take ownership of its history
    assert(stateInfos || setupStates);
    if (stateInfos)
        setupStates = std::move(stateInfos); // stateInfos becomes nullptr

    for (auto &thPtr : threads)
    {
        StateInfo *const sharedTail = &setupStates->back(); // immutable tail

        thPtr->run_custom_job([&, sharedTail, timeLimit]
                              {
            thPtr->worker->rootPos.fromFen(pos.toFenString(), &thPtr->worker->rootState);
            thPtr->worker->rootState = setupStates->back();

            thPtr->worker->timeLimit = std::chrono::milliseconds(timeLimit); });
    }

    for (auto &thPtr : threads)
        thPtr->waitToFinishSearch();

    main_thread()->startSearching();
}