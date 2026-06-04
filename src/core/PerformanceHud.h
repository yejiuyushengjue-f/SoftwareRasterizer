#pragma once

#include "core/Framebuffer.h"
#include "renderer/Renderer.h"

#include <string>

namespace sr {

struct FrameStats {
    double framesPerSecond = 0.0;
    double frameMilliseconds = 0.0;
    double updateMilliseconds = 0.0;
    double renderMilliseconds = 0.0;
    double hudMilliseconds = 0.0;
    double presentMilliseconds = 0.0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    std::string renderModeName = "Final";
    RendererStats renderer;
};

class PerformanceMonitor {
public:
    void submit(const FrameStats& stats);
    bool hasStats() const;
    const FrameStats& displayStats() const;

private:
    bool initialized_ = false;
    FrameStats smoothed_;
};

void drawPerformanceHud(Framebuffer& framebuffer, const FrameStats& stats);

} // namespace sr
