#pragma once

#include "core/ThreadPool.h"

#include <cstddef>
#include <functional>
#include <vector>

namespace sr {

struct ScreenTile {
    int minX = 0;
    int minY = 0;
    int maxXExclusive = 0;
    int maxYExclusive = 0;
};

class TileScheduler {
public:
    std::vector<ScreenTile> buildTiles(int width, int height, int tileSize);
    std::size_t activeWorkerCount() const;
    void parallelFor(std::size_t itemCount, const std::function<void(std::size_t workerIndex, std::size_t itemIndex)>& task);

private:
    ThreadPool workers_;
    std::size_t activeWorkerCount_ = 0;
};

} // namespace sr
