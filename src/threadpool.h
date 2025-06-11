#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "bitposition.h"
#include "worker.h"
#include "thread_win32_osx.h"


// Abstraction of a thread. It contains a pointer to the worker and a native thread.
// After construction, the native thread is started with idle_loop()
// waiting for a signal to start searching.
// When the signal is received, the thread starts searching and when
// the search is finished, it goes back to idle_loop() waiting for a new signal.
class Thread
{
public:
    Thread();
    virtual ~Thread();

    void startSearching();
    void run_custom_job(std::function<void()> f);

    void waitToFinishSearch();
    size_t id() const { return idx; }

    std::unique_ptr<Worker> worker;
    std::function<void()> jobFunc;

private:
    std::mutex mutex;
    std::condition_variable cv;
    size_t idx, nthreads;
    bool exit = false, searching = true; // Set before starting std::thread
    NativeThread stdThread;
};

// ThreadPool struct handles all the threads-related stuff like init, starting,
// parking and, most importantly, launching a thread. All the access to threads
// is done through this class.
class ThreadPool
{
public:
    ThreadPool() {}

    ~ThreadPool()
    {
        // destroy any existing thread(s)
        if (threads.size() > 0)
        {
            main_thread()->waitToFinishSearch();

            threads.clear();
        }
    }

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;

    ThreadPool &operator=(const ThreadPool &) = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;

    bool stop;

    void startThinking(BitPosition &pos, std::unique_ptr<std::deque<StateInfo>> & stateInfos, int timeLimit, bool pondering);
    void waitToFinishSearch();
    void clear();
    void set(int numThreads);

    Thread *main_thread() const { return threads.front().get(); }

    auto cbegin() const noexcept { return threads.cbegin(); }
    auto begin() noexcept { return threads.begin(); }
    auto end() noexcept { return threads.end(); }
    auto cend() const noexcept { return threads.cend(); }
    auto size() const noexcept { return threads.size(); }
    auto empty() const noexcept { return threads.empty(); }

private:
    std::unique_ptr<std::deque<StateInfo>> setupStates;
    std::vector<std::unique_ptr<Thread>> threads;
};

#endif // #ifndef THREADPOOL_H