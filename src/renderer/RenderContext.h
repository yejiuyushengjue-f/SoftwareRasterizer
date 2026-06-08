#pragma once

#include "core/Camera.h"
#include "core/Framebuffer.h"
#include "renderer/RenderSceneView.h"
#include "renderer/Vertex.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace sr {

enum class RenderMode;
struct RendererStats;
struct ShadowMap;
struct ViewLightSet;

struct ScreenVertex {
    Vec2 position;
    float depth = 0.0f;
    float invW = 1.0f;
    Vec3 worldPositionOverW;
    Vec3 viewPositionOverW;
    Vec2 uvOverW;
    Vec3 normalOverW;
    Vec3 tangentOverW;
    float tangentSignOverW = 1.0f;
    Color color;
};

struct PreparedTriangle {
    const DrawCommand* command = nullptr;
    ScreenVertex vertices[3] = {};
    int minX = 0;
    int maxXExclusive = 0;
    int minY = 0;
    int maxYExclusive = 0;
};

struct ScreenTile {
    int minX = 0;
    int minY = 0;
    int maxXExclusive = 0;
    int maxYExclusive = 0;
};

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
