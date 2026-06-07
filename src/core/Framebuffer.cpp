#include "core/Framebuffer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace sr {

namespace {

std::size_t checkedPixelCount(int width, int height)
{
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Framebuffer dimensions must be positive.");
    }

    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / h) {
        throw std::runtime_error("Framebuffer dimensions are too large.");
    }

    return w * h;
}

} // namespace

Framebuffer::Framebuffer(int width, int height)
    : width_(width)
    , height_(height)
    , pixels_(checkedPixelCount(width, height))
    , depth_(pixels_.size(), std::numeric_limits<float>::infinity())
{
}

void Framebuffer::resize(int width, int height)
{
    if (width == width_ && height == height_) {
        return;
    }

    width_ = width;
    height_ = height;
    pixels_.assign(checkedPixelCount(width_, height_), 0);
    depth_.assign(pixels_.size(), std::numeric_limits<float>::infinity());
}

void Framebuffer::clear(Color color)
{
    std::fill(pixels_.begin(), pixels_.end(), color.toBGRA());
    clearDepth(std::numeric_limits<float>::infinity());
}

void Framebuffer::clearDepth(float depth)
{
    std::fill(depth_.begin(), depth_.end(), depth);
}

void Framebuffer::setPixel(int x, int y, Color color)
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }

    pixels_[indexOf(x, y)] = color.toBGRA();
}

bool Framebuffer::depthTest(int x, int y, float depth, float tolerance) const
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_ || !std::isfinite(depth)) {
        return false;
    }

    return depth <= depth_[indexOf(x, y)] + tolerance;
}

bool Framebuffer::setDepthIfCloser(int x, int y, float depth)
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_ || !std::isfinite(depth)) {
        return false;
    }

    const std::size_t index = indexOf(x, y);
    if (depth >= depth_[index]) {
        return false;
    }

    depth_[index] = depth;
    return true;
}

bool Framebuffer::setPixelIfCloser(int x, int y, float depth, Color color)
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_ || !std::isfinite(depth)) {
        return false;
    }

    const std::size_t index = indexOf(x, y);
    if (depth >= depth_[index]) {
        return false;
    }

    depth_[index] = depth;
    pixels_[index] = color.toBGRA();
    return true;
}

std::size_t Framebuffer::indexOf(int x, int y) const
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
}

} // namespace sr
