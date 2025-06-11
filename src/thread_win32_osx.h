#ifndef THREAD_WIN32_OSX_H
#define THREAD_WIN32_OSX_H

#include <thread>

/*
 * On macOS the system creates new threads with a reduced 512 KB stack,
 * which is too small for deep searches.  We explicitly request an
 * 8 MB stack (the Linux default) whenever we create a POSIX thread.
 */

#if defined(__APPLE__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(USE_PTHREADS)

#include <pthread.h>
#include <functional>

class NativeThread
{
    // Zero-initialise so “no thread yet” is a well-defined state (== 0)
    pthread_t thread{};

    static constexpr size_t TH_STACK_SIZE = 8 * 1024 * 1024;

public:
    // ❶ Default-constructible so containers and
    //    owning classes (Thread, ThreadPool…) can value-initialise us.
    NativeThread() = default;

    // ❷ Construct a real POSIX thread
    template <class Function, class... Args>
    explicit NativeThread(Function &&fun, Args &&...args)
    {
        auto func = new std::function<void()>(
            std::bind(std::forward<Function>(fun), std::forward<Args>(args)...));

        pthread_attr_t attr_storage;
        pthread_attr_init(&attr_storage);
        pthread_attr_setstacksize(&attr_storage, TH_STACK_SIZE);

        auto start_routine = [](void *ptr) -> void *
        {
            auto f = reinterpret_cast<std::function<void()> *>(ptr);
            (*f)(); // run user code
            delete f;
            return nullptr;
        };

        pthread_create(&thread, &attr_storage, start_routine, func);
        // attr_storage is automatically destroyed when it goes out of scope
    }

    // ❸ Safe join – only if we actually spawned a thread
    void join()
    {
        if (thread)
            pthread_join(thread, nullptr);
    }
};

#else // Fallback: use the C++ standard library’s std::thread directly

using NativeThread = std::thread;

#endif // platform switch

#endif // THREAD_WIN32_OSX_H
