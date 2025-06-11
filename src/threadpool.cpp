#include "threadpool.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>



// Constructor (To finish)
Thread::Thread()
{
    // IDontKnow
}

void runCustomJob(std::function<void()> f)
{
    if (f)
        f();
}

// Destructor wakes up the thread in idle_loop() and waits
// for its termination. Thread should be already waiting.
Thread::~Thread()
{
    exit = true;
    startSearching();
    stdThread.join();
}

// Wakes up the thread that will start the search
void Thread::startSearching(BitPosition &pos, std::unique_ptr<std::deque<StateInfo>> &stateInfos)
{
    assert(worker != nullptr);
    runCustomJob([this]()
                    { worker->startSearching(pos, stateInfos, 1, 99); });
}

// Blocks on the condition variable until the thread has finished searching
void Thread::waitToFinishSearch()
{

    std::unique_lock<std::mutex> lk(mutex);
    cv.wait(lk, [&]
            { return !searching; });
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
        t->worker = std::make_unique<Worker>(tt, *this, network, transformer, static_cast<size_t>(i), time_left);
        threads.emplace_back(std::move(t));
    }
}

void ThreadPool::clear()
{
}

void ThreadPool::waitToFinishSearch()
{
}

// Wakes up main thread waiting in idle_loop() and returns immediately.
// Main thread will wake up other threads and start the search.
void ThreadPool::startThinking(BitPosition &pos, std::unique_ptr<std::deque<StateInfo>> &stateInfos, int timeLimit, bool pondering)
{

    main_thread()->waitToFinishSearch();
    main_thread()->worker->ponder = pondering;

    time_left = timeLimit;

    // After ownership transfer 'states' becomes empty, so if we stop the search
    // and call 'go' again without setting a new position states.get() == nullptr.
    assert(stateInfos.get() || setupStates.get());

    if (stateInfos.get())
        setupStates = std::move(stateInfos); // Ownership transfer, states is now empty

    main_thread()->startSearching(pos, stateInfos);
}
