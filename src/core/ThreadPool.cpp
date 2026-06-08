#include "core/ThreadPool.h"

#include <algorithm>

namespace sr {

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
        ++generation_;
    }

    workCv_.notify_all();

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::ensureWorkerCount(std::size_t itemCount)
{
    const std::size_t targetWorkerCount = targetWorkerCountFor(itemCount);
    while (workers_.size() < targetWorkerCount) {
        const std::size_t workerIndex = workers_.size();
        workers_.emplace_back([this, workerIndex] {
            workerLoop(workerIndex);
        });
    }
}

std::size_t ThreadPool::activeWorkerCountFor(std::size_t itemCount) const
{
    if (itemCount == 0) {
        return 0;
    }

    const std::size_t availableWorkers = workers_.empty() ? 1 : workers_.size();
    return std::min(itemCount, availableWorkers);
}

std::size_t ThreadPool::workerCount() const
{
    return workers_.size();
}

void ThreadPool::parallelFor(std::size_t itemCount, const std::function<void(std::size_t workerIndex, std::size_t itemIndex)>& task)
{
    if (itemCount == 0) {
        return;
    }

    ensureWorkerCount(itemCount);
    const std::size_t activeWorkerCount = activeWorkerCountFor(itemCount);
    if (itemCount == 1 || workers_.empty()) {
        for (std::size_t itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
            task(0, itemIndex);
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        task_ = task;
        taskException_ = nullptr;
        nextItem_.store(0, std::memory_order_relaxed);
        itemCount_ = itemCount;
        activeWorkerCount_ = activeWorkerCount;
        completedWorkerCount_ = 0;
        cancel_.store(false, std::memory_order_relaxed);
        taskReady_ = true;
        ++generation_;
    }

    workCv_.notify_all();

    std::unique_lock<std::mutex> lock(mutex_);
    doneCv_.wait(lock, [this] {
        return completedWorkerCount_ >= activeWorkerCount_;
    });

    std::exception_ptr taskException = taskException_;
    task_ = {};
    taskReady_ = false;
    lock.unlock();

    if (taskException) {
        std::rethrow_exception(taskException);
    }
}

void ThreadPool::workerLoop(std::size_t workerIndex)
{
    std::uint64_t observedGeneration = 0;

    for (;;) {
        std::function<void(std::size_t, std::size_t)> task;
        std::size_t itemCount = 0;
        std::size_t activeWorkerCount = 0;
        bool participatesInGeneration = false;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            workCv_.wait(lock, [this, &observedGeneration] {
                return stop_ || (taskReady_ && generation_ != observedGeneration);
            });

            if (stop_) {
                return;
            }

            observedGeneration = generation_;
            task = task_;
            itemCount = itemCount_;
            activeWorkerCount = activeWorkerCount_;
            participatesInGeneration = workerIndex < activeWorkerCount && static_cast<bool>(task);
        }

        if (participatesInGeneration) {
            for (;;) {
                if (cancel_.load(std::memory_order_relaxed)) {
                    break;
                }

                const std::size_t itemIndex = nextItem_.fetch_add(1, std::memory_order_relaxed);
                if (itemIndex >= itemCount) {
                    break;
                }

                try {
                    task(workerIndex, itemIndex);
                } catch (...) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (!taskException_) {
                        taskException_ = std::current_exception();
                    }
                    cancel_.store(true, std::memory_order_relaxed);
                    break;
                }
            }
        }

        if (participatesInGeneration) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++completedWorkerCount_;
            if (completedWorkerCount_ >= activeWorkerCount_) {
                doneCv_.notify_one();
            }
        }
    }
}

std::size_t ThreadPool::targetWorkerCountFor(std::size_t itemCount) const
{
    if (itemCount <= 1) {
        return 0;
    }

    unsigned int hardware = std::thread::hardware_concurrency();
    if (hardware == 0) {
        hardware = 1;
    }

    return std::min(itemCount, static_cast<std::size_t>(hardware));
}

} // namespace sr
