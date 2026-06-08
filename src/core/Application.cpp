#include "core/Application.h"

#include <algorithm>
#include <chrono>
#include <string>

namespace sr {

namespace {

double elapsedMilliseconds(std::chrono::steady_clock::time_point begin, std::chrono::steady_clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

} // namespace

Application::Application(void* nativeInstance, int showCommand)
    : framebuffer_(960, 540)
    , window_(nativeInstance, showCommand, framebuffer_.width(), framebuffer_.height(), "CPU Rasterizer")
    , lastFrameTime_(std::chrono::steady_clock::now())
{
    updateWindowTitle();
    resizeFramebufferToWindow();
}

int Application::run()
{
    while (window_.processMessages()) {
        const auto frameBegin = std::chrono::steady_clock::now();
        const std::chrono::duration<float> elapsed = frameBegin - lastFrameTime_;
        lastFrameTime_ = frameBegin;

        const float deltaSeconds = std::min(elapsed.count(), 0.1f);
        const auto updateBegin = std::chrono::steady_clock::now();
        update(deltaSeconds);
        const auto updateEnd = std::chrono::steady_clock::now();

        const auto renderBegin = std::chrono::steady_clock::now();
        render();
        const auto renderEnd = std::chrono::steady_clock::now();

        double hudMilliseconds = 0.0;
        if (performanceHudVisible_ && performanceMonitor_.hasStats()) {
            const auto hudBegin = std::chrono::steady_clock::now();
            drawPerformanceHud(framebuffer_, performanceMonitor_.displayStats());
            const auto hudEnd = std::chrono::steady_clock::now();
            hudMilliseconds = elapsedMilliseconds(hudBegin, hudEnd);
        }

        const auto presentBegin = std::chrono::steady_clock::now();
        window_.present(framebuffer_);
        const auto presentEnd = std::chrono::steady_clock::now();

        const auto frameEnd = std::chrono::steady_clock::now();
        const double frameMilliseconds = elapsedMilliseconds(frameBegin, frameEnd);
        FrameStats stats;
        stats.framesPerSecond = frameMilliseconds > 0.0 ? 1000.0 / frameMilliseconds : 0.0;
        stats.frameMilliseconds = frameMilliseconds;
        stats.updateMilliseconds = elapsedMilliseconds(updateBegin, updateEnd);
        stats.renderMilliseconds = elapsedMilliseconds(renderBegin, renderEnd);
        stats.hudMilliseconds = hudMilliseconds;
        stats.presentMilliseconds = elapsedMilliseconds(presentBegin, presentEnd);
        stats.framebufferWidth = framebuffer_.width();
        stats.framebufferHeight = framebuffer_.height();
        stats.renderModeName = renderer_.renderModeName();
        stats.renderer = renderer_.stats();
        performanceMonitor_.submit(stats);
    }

    return 0;
}

void Application::update(float deltaSeconds)
{
    const InputState input = window_.inputState();
    if (input.toggleFullscreen) {
        window_.toggleFullscreen();
    }
    if (input.togglePerformanceHud) {
        performanceHudVisible_ = !performanceHudVisible_;
    }
    if (input.toggleModel) {
        scene_.toggleModel();
    }
    resizeFramebufferToWindow();

    camera_.update(input, deltaSeconds);
    if (input.renderModeSelection > 0) {
        renderer_.setRenderMode(static_cast<RenderMode>(input.renderModeSelection - 1));
    }
    if (renderer_.renderMode() != lastWindowTitleMode_ || scene_.activeModelName() != lastWindowTitleModel_) {
        updateWindowTitle();
    }
    scene_.update(deltaSeconds);
}

void Application::updateWindowTitle()
{
    std::string title = "CPU Rasterizer - Render Mode: ";
    title += renderer_.renderModeName();
    title += " - Model: ";
    title += scene_.activeModelName();
    window_.setTitle(title.c_str());
    lastWindowTitleMode_ = renderer_.renderMode();
    lastWindowTitleModel_ = scene_.activeModelName();
}

void Application::resizeFramebufferToWindow()
{
    int width = 0;
    int height = 0;
    if (window_.clientSize(width, height)) {
        framebuffer_.resize(width, height);
    }
}

void Application::render()
{
    renderer_.render(scene_.renderView(), camera_, framebuffer_);
}

} // namespace sr
