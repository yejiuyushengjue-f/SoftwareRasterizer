#pragma once

#include "core/Camera.h"
#include "core/Framebuffer.h"
#include "renderer/RenderStats.h"
#include "renderer/RenderSceneView.h"
#include "renderer/ShadowMapper.h"
#include "renderer/TileScheduler.h"

namespace sr {

enum class RenderMode {
    Final = 0,
    Albedo,
    Normal,
    Depth,
    UV,
    Shadow,
    Light,
    LightDepth,
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void render(const RenderSceneView& scene, const Camera& camera, Framebuffer& framebuffer);
    void setRenderMode(RenderMode mode);
    RenderMode renderMode() const;
    const char* renderModeName() const;
    const RendererStats& stats() const;

private:
    ShadowMap shadowMap_;
    RenderMode renderMode_ = RenderMode::Final;
    RendererStats stats_;
    TileScheduler tileScheduler_;
};

} // namespace sr
