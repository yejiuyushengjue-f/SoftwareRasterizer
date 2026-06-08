#include "core/ThreadPool.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

void runThreadPoolTests()
{
    const auto require = [](bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    };

    {
        sr::ThreadPool pool;
        std::atomic<std::size_t> calls { 0 };

        pool.parallelFor(0, [&](std::size_t, std::size_t) {
            ++calls;
        });

        require(calls.load() == 0, "parallelFor(0) should not execute any task.");
        require(pool.activeWorkerCountFor(0) == 0, "parallelFor(0) should report zero active workers.");
    }

    {
        sr::ThreadPool pool;
        constexpr std::size_t itemCount = 97;
        std::vector<int> hits(itemCount, 0);
        std::mutex hitsMutex;

        pool.parallelFor(itemCount, [&](std::size_t, std::size_t itemIndex) {
            std::lock_guard<std::mutex> lock(hitsMutex);
            ++hits[itemIndex];
        });

        for (std::size_t itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
            if (hits[itemIndex] != 1) {
                std::ostringstream message;
                message << "parallelFor should execute item " << itemIndex << " exactly once.";
                throw std::runtime_error(message.str());
            }
        }
    }

    {
        sr::ThreadPool pool;
        constexpr std::size_t itemCount = 41;
        std::vector<int> firstHits(itemCount, 0);
        std::vector<int> secondHits(itemCount, 0);
        std::mutex hitsMutex;

        pool.parallelFor(itemCount, [&](std::size_t, std::size_t itemIndex) {
            std::lock_guard<std::mutex> lock(hitsMutex);
            ++firstHits[itemIndex];
        });

        pool.parallelFor(itemCount, [&](std::size_t, std::size_t itemIndex) {
            std::lock_guard<std::mutex> lock(hitsMutex);
            ++secondHits[itemIndex];
        });

        for (std::size_t itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
            if (firstHits[itemIndex] != 1 || secondHits[itemIndex] != 1) {
                std::ostringstream message;
                message << "ThreadPool should remain reusable across calls for item " << itemIndex << '.';
                throw std::runtime_error(message.str());
            }
        }
    }

    {
        sr::ThreadPool pool;
        bool sawException = false;

        try {
            pool.parallelFor(64, [&](std::size_t, std::size_t itemIndex) {
                if (itemIndex == 13) {
                    throw std::runtime_error("thread-pool test");
                }
            });
        } catch (const std::runtime_error& error) {
            sawException = std::string(error.what()) == "thread-pool test";
        }

        require(sawException, "ThreadPool should rethrow worker exceptions on the calling thread.");

        std::vector<int> hits(23, 0);
        std::mutex hitsMutex;
        pool.parallelFor(hits.size(), [&](std::size_t, std::size_t itemIndex) {
            std::lock_guard<std::mutex> lock(hitsMutex);
            ++hits[itemIndex];
        });

        require(std::all_of(hits.begin(), hits.end(), [](int hitCount) {
            return hitCount == 1;
        }), "ThreadPool should stay reusable after an exception.");
    }

    {
        sr::ThreadPool pool;
        pool.ensureWorkerCount(64);

        const std::size_t persistentWorkerCount = pool.workerCount();
        if (persistentWorkerCount >= 3) {
            constexpr std::size_t smallItemCount = 2;
            std::atomic<std::size_t> started { 0 };
            std::atomic<std::size_t> finished { 0 };
            std::atomic<bool> releaseWorkers { false };

            auto future = std::async(std::launch::async, [&] {
                pool.parallelFor(smallItemCount, [&](std::size_t, std::size_t) {
                    started.fetch_add(1, std::memory_order_relaxed);
                    while (!releaseWorkers.load(std::memory_order_acquire)) {
                        std::this_thread::yield();
                    }
                    finished.fetch_add(1, std::memory_order_relaxed);
                });
            });

            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (started.load(std::memory_order_relaxed) < smallItemCount
                && future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready
                && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }

            require(started.load(std::memory_order_relaxed) == smallItemCount,
                "Active workers should start all small tasks before parallelFor can finish.");
            require(future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready,
                "parallelFor returned before its active workers completed the small task batch.");

            releaseWorkers.store(true, std::memory_order_release);
            future.get();

            require(finished.load(std::memory_order_relaxed) == smallItemCount,
                "parallelFor should not return until every active worker finishes its assigned work.");
        }
    }
}
