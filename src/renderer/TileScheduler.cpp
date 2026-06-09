#include "renderer/TileScheduler.h"

#include <algorithm>

namespace sr {

std::vector<ScreenTile> TileScheduler::buildTiles(int width, int height, int tileSize)
{
    std::vector<ScreenTile> tiles;
    if (width <= 0 || height <= 0) {
        activeWorkerCount_ = 0;
        return tiles;
    }

    const int clampedTileSize = std::max(1, tileSize);
    for (int y = 0; y < height; y += clampedTileSize) {
        for (int x = 0; x < width; x += clampedTileSize) {
            tiles.push_back({
                x,
                y,
                std::min(width, x + clampedTileSize),
                std::min(height, y + clampedTileSize),
            });
        }
    }

    workers_.ensureWorkerCount(tiles.size());
    activeWorkerCount_ = workers_.activeWorkerCountFor(tiles.size());
    return tiles;
}

std::size_t TileScheduler::activeWorkerCount() const
{
    return activeWorkerCount_;
}

void TileScheduler::parallelFor(std::size_t itemCount, const std::function<void(std::size_t workerIndex, std::size_t itemIndex)>& task)
{
    workers_.parallelFor(itemCount, task);
}

} // namespace sr
