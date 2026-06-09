#pragma once

#include <chrono>
#include <cstdint>

namespace sr {

struct RendererStats {
    std::uint64_t drawCommands = 0;
    std::uint64_t inputTriangles = 0;
    std::uint64_t rasterizedTriangles = 0;
    std::uint64_t shadowTriangles = 0;
    std::uint64_t shadedPixels = 0;
    std::uint64_t colorPixelsWritten = 0;
    std::uint64_t shadowDepthWrites = 0;
    double shadowPassMilliseconds = 0.0;
    double mainPassMilliseconds = 0.0;
};

double elapsedRenderMilliseconds(std::chrono::steady_clock::time_point begin, std::chrono::steady_clock::time_point end);

} // namespace sr
