#include "core/ThreadPool.h"

#include <exception>

#include "core/Logger.h"

namespace rl {

ThreadPool::ThreadPool(unsigned workers) {
    if (workers == 0) workers = 1;
    threads_.reserve(workers);
    for (unsigned i = 0; i < workers; ++i) threads_.emplace_back([this] { workerLoop(); });
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();
    for (auto& t : threads_) t.join();
}

void ThreadPool::post(std::function<void()> task) {
    {
        std::lock_guard lock(mutex_);
        queue_.push_back(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::workerLoop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_) return;  // queued work is dropped on shutdown; in-flight tasks finish
            task = std::move(queue_.front());
            queue_.pop_front();
        }
        try {
            task();
        } catch (const std::exception& e) {
            log::error("pool", std::string("Unhandled task exception: ") + e.what());
        } catch (...) {
            log::error("pool", "Unhandled task exception");
        }
    }
}

void MainThreadQueue::post(std::function<void()> fn) {
    std::function<void()> waker;
    {
        std::lock_guard lock(mutex_);
        pending_.push_back(std::move(fn));
        waker = waker_;
    }
    if (waker) waker();
}

void MainThreadQueue::setWaker(std::function<void()> waker) {
    std::lock_guard lock(mutex_);
    waker_ = std::move(waker);
}

void MainThreadQueue::drain() {
    std::vector<std::function<void()>> batch;
    {
        std::lock_guard lock(mutex_);
        batch.swap(pending_);
    }
    for (auto& fn : batch) fn();
}

}  // namespace rl
