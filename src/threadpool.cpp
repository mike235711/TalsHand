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
{}

// Destructor wakes up the thread in idle_loop() and waits
// for its termination. Thread should be already waiting.
Thread::~Thread()
{
    assert(!searching);

    exit = true;
    startSearching();
    stdThread.join();
}

// Wakes up the thread that will start the search
void Thread::startSearching()
{
    assert(worker != nullptr);
    runCustomJob([this]()
                    { worker->startSearching(1, 99); });
}

// Blocks on the condition variable until the thread has finished searching
void Thread::waitToFinishSearch()
{

    std::unique_lock<std::mutex> lk(mutex);
    cv.wait(lk, [&]
            { return !searching; });
}


void ThreadPool::set(int numThreads)
{
    // Destroys existing threads and creates the requested number of threads
    // We dont implement multiple threads yet so this is left empty
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

    // After ownership transfer 'states' becomes empty, so if we stop the search
    // and call 'go' again without setting a new position states.get() == nullptr.
    assert(stateInfos.get() || setupStates.get());

    if (stateInfos.get())
        setupStates = std::move(stateInfos); // Ownership transfer, states is now empty

    main_thread()->startSearching();
}
