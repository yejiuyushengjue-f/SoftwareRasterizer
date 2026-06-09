#pragma once

#include "core/Framebuffer.h"
#include "renderer/RenderSceneView.h"
#include "renderer/RenderStats.h"
#include "renderer/TileScheduler.h"
#include "renderer/Vertex.h"

#include <array>
#include <vector>

namespace sr {

struct ClipVertex {
    Vec4 clip;
    Vec3 worldPosition;
    Vec3 viewPosition;
    Vec2 uv;
    Vec3 normal;
    Vec3 tangent;
    float tangentSign = 1.0f;
    Color color;
};

struct ClipPolygon {
    std::array<ClipVertex, 12> vertices;
    int count = 0;
};

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

struct FragmentRequirements {
    bool surfaceColor = false;
    bool normal = false;
    bool viewPosition = false;
    bool worldPosition = false;
};

struct InterpolatedFragment {
    float depth = 0.0f;
    Vec2 uv;
    Vec3 normal;
    Vec3 tangent;
    float tangentSign = 1.0f;
    Vec3 worldPosition;
    Vec3 viewPosition;
    Color vertexColor { 255, 255, 255, 255 };
};

bool isFinite(Vec2 value);
bool isFinite(Vec3 value);
bool isFinite(Vec4 value);

ClipPolygon clipTriangleToFrustum(const ClipVertex* triangle);
bool toScreenVertex(const ClipVertex& vertex, int width, int height, ScreenVertex& out);

void buildPreparedTriangles(
    const RenderSceneView& scene,
    const Mat4& view,
    const Mat4& projection,
    Framebuffer& framebuffer,
    RendererStats& stats,
    std::vector<PreparedTriangle>& preparedTriangles);

bool intersectTile(const ScreenTile& tile, const PreparedTriangle& triangle, int& minX, int& maxXExclusive, int& minY, int& maxYExclusive);
void rasterizeDepthTile(Framebuffer& framebuffer, const ScreenTile& tile, const std::vector<PreparedTriangle>& preparedTriangles);
bool interpolateFragment(
    const PreparedTriangle& triangle,
    int x,
    int y,
    Framebuffer& framebuffer,
    float depthTolerance,
    const FragmentRequirements& requirements,
    InterpolatedFragment& out);

} // namespace sr
