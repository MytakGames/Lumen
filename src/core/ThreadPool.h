#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace rl {

// Shared "owner still alive" flag checked by callbacks that may outlive their owner.
using AliveFlag = std::shared_ptr<std::atomic<bool>>;
inline AliveFlag makeAliveFlag() { return std::make_shared<std::atomic<bool>>(true); }

// Fixed-size worker pool for blocking work (HTTP, disk, process waits).
class ThreadPool {
public:
    explicit ThreadPool(unsigned workers = 4);
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void post(std::function<void()> task);

    template <class F>
    auto submit(F&& f) -> std::future<decltype(f())> {
        using R = decltype(f());
        auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
        auto fut = task->get_future();
        post([task] { (*task)(); });
        return fut;
    }

private:
    void workerLoop();

    std::vector<std::thread> threads_;
    std::deque<std::function<void()>> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

// Results produced on worker threads are posted here and executed on the UI
// thread once per frame, so UI state is only ever touched from one thread.
class MainThreadQueue {
public:
    void post(std::function<void()> fn);
    void drain();
    // Called (from any thread) after each post, e.g. to wake an idle message loop.
    void setWaker(std::function<void()> waker);

private:
    std::mutex mutex_;
    std::vector<std::function<void()>> pending_;
    std::function<void()> waker_;
};

}  // namespace rl
