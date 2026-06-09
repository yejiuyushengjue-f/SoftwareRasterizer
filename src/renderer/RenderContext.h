#pragma once

#include "core/Camera.h"
#include "core/Framebuffer.h"
#include "renderer/RenderSceneView.h"
#include "renderer/RenderStats.h"
#include "renderer/ShadowMapper.h"
#include "renderer/TileScheduler.h"
#include "renderer/TriangleRasterizer.h"

#include <functional>
#include <vector>

namespace sr {

enum class RenderMode;
struct ViewLightSet;

struct RenderContext {
    Framebuffer& framebuffer;
    const Camera& camera;
    RenderSceneView scene;
    RenderMode renderMode;
    RendererStats& stats;
    ShadowMap& shadowMap;

    Mat4 lightViewProjection = Mat4::identity();
    Mat4 view = Mat4::identity();
    Mat4 projection = Mat4::identity();
    const ViewLightSet* lights = nullptr;
    const DirectionalLight* primaryLight = nullptr;

    std::vector<PreparedTriangle> preparedTriangles;
    std::vector<ScreenTile> tiles;
    std::size_t renderWorkerCount = 0;

    std::function<void(RenderContext&)> shadowPass;
    std::function<void(RenderContext&)> prepareGeometryPass;
    std::function<void(RenderContext&)> depthPrepass;
    std::function<void(RenderContext&)> colorPass;
};

} // namespace sr
