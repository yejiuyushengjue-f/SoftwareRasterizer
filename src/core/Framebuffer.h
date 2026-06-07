#pragma once

#include "core/Color.h"

#include <cstdint>
#include <vector>

namespace sr {

class Framebuffer {
public:
    Framebuffer(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    const std::uint32_t* pixels() const { return pixels_.data(); }
    std::uint32_t* pixels() { return pixels_.data(); }

    void resize(int width, int height);
    void clear(Color color);
    void clearDepth(float depth);
    void setPixel(int x, int y, Color color);
    bool depthTest(int x, int y, float depth, float tolerance = 0.0f) const;
    bool setDepthIfCloser(int x, int y, float depth);
    bool setPixelIfCloser(int x, int y, float depth, Color color);

private:
    std::size_t indexOf(int x, int y) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint32_t> pixels_;
    std::vector<float> depth_;
};

} // namespace sr
