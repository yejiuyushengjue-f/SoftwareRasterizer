#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace sr {

class ThreadPool {
public:
    ThreadPool() = default;
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    void ensureWorkerCount(std::size_t itemCount);
    std::size_t activeWorkerCountFor(std::size_t itemCount) const;
    std::size_t workerCount() const;
    void parallelFor(std::size_t itemCount, const std::function<void(std::size_t workerIndex, std::size_t itemIndex)>& task);

private:
    void workerLoop(std::size_t workerIndex);
    std::size_t targetWorkerCountFor(std::size_t itemCount) const;

    std::vector<std::thread> workers_;
    mutable std::mutex mutex_;
    std::condition_variable workCv_;
    std::condition_variable doneCv_;
    std::function<void(std::size_t, std::size_t)> task_;
    std::exception_ptr taskException_;
    std::atomic<std::size_t> nextItem_ { 0 };
    std::size_t itemCount_ = 0;
    std::size_t activeWorkerCount_ = 0;
    std::size_t completedWorkerCount_ = 0;
    std::uint64_t generation_ = 0;
    bool taskReady_ = false;
    std::atomic<bool> cancel_ { false };
    bool stop_ = false;
};

} // namespace sr
