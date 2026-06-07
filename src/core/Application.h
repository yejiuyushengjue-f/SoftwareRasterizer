#pragma once

#include "core/Camera.h"
#include "core/Framebuffer.h"
#include "core/PerformanceHud.h"
#include "platform/Win32Window.h"
#include "renderer/Renderer.h"
#include "scenes/TestScene.h"

#include <chrono>

namespace sr {

class Application {
public:
    Application(void* nativeInstance, int showCommand);

    int run();

private:
    void updateWindowTitle();
    void resizeFramebufferToWindow();
    void update(float deltaSeconds);
    void render();

    Framebuffer framebuffer_;
    Win32Window window_;
    Camera camera_;
    Renderer renderer_;
    TestScene scene_;
    PerformanceMonitor performanceMonitor_;
    std::chrono::steady_clock::time_point lastFrameTime_;
    RenderMode lastWindowTitleMode_ = RenderMode::Final;
    const char* lastWindowTitleModel_ = "";
    bool performanceHudVisible_ = true;
};

} // namespace sr
