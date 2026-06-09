#include "renderer/TriangleRasterizer.h"

#include "renderer/RasterHelpers.h"

#include <algorithm>
#include <cmath>

namespace sr {

namespace {

float clipDistance(const ClipVertex& vertex, int plane)
{
    switch (plane) {
    case 0:
        return vertex.clip.x + vertex.clip.w;
    case 1:
        return vertex.clip.w - vertex.clip.x;
    case 2:
        return vertex.clip.y + vertex.clip.w;
    case 3:
        return vertex.clip.w - vertex.clip.y;
    case 4:
        return vertex.clip.z + vertex.clip.w;
    default:
        return vertex.clip.w - vertex.clip.z;
    }
}

Color lerpColor(Color a, Color b, float t)
{
    const auto lerpChannel = [t](std::uint8_t av, std::uint8_t bv) {
        return static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(av) + (static_cast<float>(bv) - static_cast<float>(av)) * t,
            0.0f,
            255.0f));
    };

    return {
        lerpChannel(a.r, b.r),
        lerpChannel(a.g, b.g),
        lerpChannel(a.b, b.b),
        lerpChannel(a.a, b.a),
    };
}

ClipVertex lerpClipVertex(const ClipVertex& a, const ClipVertex& b, float t)
{
    const auto lerpFloat = [t](float av, float bv) {
        return av + (bv - av) * t;
    };

    return {
        {
            lerpFloat(a.clip.x, b.clip.x),
            lerpFloat(a.clip.y, b.clip.y),
            lerpFloat(a.clip.z, b.clip.z),
            lerpFloat(a.clip.w, b.clip.w),
        },
        {
            lerpFloat(a.worldPosition.x, b.worldPosition.x),
            lerpFloat(a.worldPosition.y, b.worldPosition.y),
            lerpFloat(a.worldPosition.z, b.worldPosition.z),
        },
        {
            lerpFloat(a.viewPosition.x, b.viewPosition.x),
            lerpFloat(a.viewPosition.y, b.viewPosition.y),
            lerpFloat(a.viewPosition.z, b.viewPosition.z),
        },
        {
            lerpFloat(a.uv.x, b.uv.x),
            lerpFloat(a.uv.y, b.uv.y),
        },
        normalize({
            lerpFloat(a.normal.x, b.normal.x),
            lerpFloat(a.normal.y, b.normal.y),
            lerpFloat(a.normal.z, b.normal.z),
        }),
        normalize({
            lerpFloat(a.tangent.x, b.tangent.x),
            lerpFloat(a.tangent.y, b.tangent.y),
            lerpFloat(a.tangent.z, b.tangent.z),
        }),
        lerpFloat(a.tangentSign, b.tangentSign),
        lerpColor(a.color, b.color, t),
    };
}

ClipPolygon clipPolygonAgainstPlane(const ClipPolygon& input, int plane)
{
    ClipPolygon output;
    if (input.count == 0) {
        return output;
    }

    ClipVertex previous = input.vertices[static_cast<std::size_t>(input.count - 1)];
    float previousDistance = clipDistance(previous, plane);
    bool previousInside = previousDistance >= 0.0f;

    for (int i = 0; i < input.count; ++i) {
        const ClipVertex& current = input.vertices[static_cast<std::size_t>(i)];
        const float currentDistance = clipDistance(current, plane);
        const bool currentInside = currentDistance >= 0.0f;

        if (currentInside != previousInside) {
            const float denominator = previousDistance - currentDistance;
            const float t = std::clamp(std::abs(denominator) <= 0.000001f ? 0.0f : previousDistance / denominator, 0.0f, 1.0f);
            if (output.count < static_cast<int>(output.vertices.size())) {
                output.vertices[static_cast<std::size_t>(output.count++)] = lerpClipVertex(previous, current, t);
            }
        }

        if (currentInside && output.count < static_cast<int>(output.vertices.size())) {
            output.vertices[static_cast<std::size_t>(output.count++)] = current;
        }

        previous = current;
        previousDistance = currentDistance;
        previousInside = currentInside;
    }

    return output;
}

Vec3 transformDirection(const Mat4& matrix, Vec3 direction)
{
    const Vec4 transformed = matrix * Vec4 { direction.x, direction.y, direction.z, 0.0f };
    return normalize({ transformed.x, transformed.y, transformed.z });
}

} // namespace

bool isFinite(Vec2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool isFinite(Vec3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool isFinite(Vec4 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w);
}

ClipPolygon clipTriangleToFrustum(const ClipVertex* triangle)
{
    ClipPolygon polygon;
    polygon.vertices[0] = triangle[0];
    polygon.vertices[1] = triangle[1];
    polygon.vertices[2] = triangle[2];
    polygon.count = 3;

    for (int plane = 0; plane < 6 && polygon.count > 0; ++plane) {
        polygon = clipPolygonAgainstPlane(polygon, plane);
    }
    return polygon;
}

bool toScreenVertex(const ClipVertex& vertex, int width, int height, ScreenVertex& out)
{
    if (width <= 0 || height <= 0 || !isFinite(vertex.clip) || std::abs(vertex.clip.w) <= 0.000001f) {
        return false;
    }

    const float invW = 1.0f / vertex.clip.w;
    const float ndcX = vertex.clip.x * invW;
    const float ndcY = vertex.clip.y * invW;
    const float ndcZ = vertex.clip.z * invW;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ)) {
        return false;
    }

    out = {
        {
            (ndcX * 0.5f + 0.5f) * static_cast<float>(width - 1),
            (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height - 1),
        },
        ndcZ,
        invW,
        vertex.worldPosition * invW,
        vertex.viewPosition * invW,
        { vertex.uv.x * invW, vertex.uv.y * invW },
        vertex.normal * invW,
        vertex.tangent * invW,
        vertex.tangentSign * invW,
        vertex.color,
    };
    return true;
}

void buildPreparedTriangles(
    const RenderSceneView& scene,
    const Mat4& view,
    const Mat4& projection,
    Framebuffer& framebuffer,
    RendererStats& stats,
    std::vector<PreparedTriangle>& preparedTriangles)
{
    for (const DrawCommand& command : scene.drawCommands) {
        if (!command.mesh.vertices || command.mesh.vertexCount < 3) {
            continue;
        }

        ++stats.drawCommands;
        stats.inputTriangles += static_cast<std::uint64_t>(command.mesh.vertexCount / 3);

        const Mat4 modelView = view * command.transform;
        const Mat4 mvp = projection * modelView;

        for (int triangleIndex = 0; triangleIndex + 2 < command.mesh.vertexCount; triangleIndex += 3) {
            const Vertex* vertices = command.mesh.vertices + triangleIndex;
            ClipVertex clipTriangle[3] = {};
            bool triangleValid = true;

            for (int i = 0; i < 3; ++i) {
                const Vertex& vertex = vertices[i];
                const Vec4 worldPosition = command.transform * Vec4 { vertex.position.x, vertex.position.y, vertex.position.z, 1.0f };
                const Vec4 viewPosition = modelView * Vec4 { vertex.position.x, vertex.position.y, vertex.position.z, 1.0f };
                const Vec4 clip = mvp * Vec4 { vertex.position.x, vertex.position.y, vertex.position.z, 1.0f };
                const Vec3 normal = transformDirection(modelView, vertex.normal);
                const Vec3 tangent = transformDirection(modelView, vertex.tangent);
                if (!isFinite(worldPosition) || !isFinite(viewPosition) || !isFinite(clip) || !isFinite(normal) || !isFinite(tangent)) {
                    triangleValid = false;
                    break;
                }

                clipTriangle[i] = {
                    clip,
                    { worldPosition.x, worldPosition.y, worldPosition.z },
                    { viewPosition.x, viewPosition.y, viewPosition.z },
                    vertex.uv,
                    normal,
                    tangent,
                    vertex.tangentSign,
                    vertex.color,
                };
            }

            if (!triangleValid) {
                continue;
            }

            const ClipPolygon clippedPolygon = clipTriangleToFrustum(clipTriangle);
            if (clippedPolygon.count < 3) {
                continue;
            }

            for (int i = 1; i + 1 < clippedPolygon.count; ++i) {
                ScreenVertex screen[3] = {};
                if (!toScreenVertex(clippedPolygon.vertices[0], framebuffer.width(), framebuffer.height(), screen[0])
                    || !toScreenVertex(clippedPolygon.vertices[static_cast<std::size_t>(i)], framebuffer.width(), framebuffer.height(), screen[1])
                    || !toScreenVertex(clippedPolygon.vertices[static_cast<std::size_t>(i + 1)], framebuffer.width(), framebuffer.height(), screen[2])) {
                    continue;
                }

                const Vec2 p0 = screen[0].position;
                const Vec2 p1 = screen[1].position;
                const Vec2 p2 = screen[2].position;
                const float area = rasterEdge(p0, p1, p2);
                if (area <= 0.000001f) {
                    continue;
                }

                PreparedTriangle prepared;
                prepared.command = &command;
                prepared.vertices[0] = screen[0];
                prepared.vertices[1] = screen[1];
                prepared.vertices[2] = screen[2];
                prepared.minX = std::max(0, static_cast<int>(std::floor(std::min({ p0.x, p1.x, p2.x }))));
                prepared.maxXExclusive = std::min(framebuffer.width(), static_cast<int>(std::ceil(std::max({ p0.x, p1.x, p2.x }))) + 1);
                prepared.minY = std::max(0, static_cast<int>(std::floor(std::min({ p0.y, p1.y, p2.y }))));
                prepared.maxYExclusive = std::min(framebuffer.height(), static_cast<int>(std::ceil(std::max({ p0.y, p1.y, p2.y }))) + 1);
                preparedTriangles.push_back(prepared);
                ++stats.rasterizedTriangles;
            }
        }
    }
}

bool intersectTile(const ScreenTile& tile, const PreparedTriangle& triangle, int& minX, int& maxXExclusive, int& minY, int& maxYExclusive)
{
    minX = std::max(tile.minX, triangle.minX);
    maxXExclusive = std::min(tile.maxXExclusive, triangle.maxXExclusive);
    minY = std::max(tile.minY, triangle.minY);
    maxYExclusive = std::min(tile.maxYExclusive, triangle.maxYExclusive);
    return minX < maxXExclusive && minY < maxYExclusive;
}

void rasterizeDepthTile(Framebuffer& framebuffer, const ScreenTile& tile, const std::vector<PreparedTriangle>& preparedTriangles)
{
    for (const PreparedTriangle& triangle : preparedTriangles) {
        int minX = 0;
        int maxXExclusive = 0;
        int minY = 0;
        int maxYExclusive = 0;
        if (!intersectTile(tile, triangle, minX, maxXExclusive, minY, maxYExclusive)) {
            continue;
        }

        const ScreenVertex* screen = triangle.vertices;
        const Vec2 p0 = screen[0].position;
        const Vec2 p1 = screen[1].position;
        const Vec2 p2 = screen[2].position;
        const float area = rasterEdge(p0, p1, p2);

        for (int y = minY; y < maxYExclusive; ++y) {
            for (int x = minX; x < maxXExclusive; ++x) {
                const Vec2 sample { static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f };
                const BarycentricWeights weights = barycentricWeights(p0, p1, p2, sample, area);
                if (!isInsideTriangle(weights)) {
                    continue;
                }

                const float depth = screen[0].depth * weights.w0 + screen[1].depth * weights.w1 + screen[2].depth * weights.w2;
                framebuffer.setDepthIfCloser(x, y, depth);
            }
        }
    }
}

bool interpolateFragment(
    const PreparedTriangle& triangle,
    int x,
    int y,
    Framebuffer& framebuffer,
    float depthTolerance,
    const FragmentRequirements& requirements,
    InterpolatedFragment& out)
{
    const ScreenVertex* screen = triangle.vertices;
    const Vec2 p0 = screen[0].position;
    const Vec2 p1 = screen[1].position;
    const Vec2 p2 = screen[2].position;
    const float area = rasterEdge(p0, p1, p2);
    const Vec2 sample { static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f };
    const BarycentricWeights weights = barycentricWeights(p0, p1, p2, sample, area);
    if (!isInsideTriangle(weights)) {
        return false;
    }

    const float depth = screen[0].depth * weights.w0 + screen[1].depth * weights.w1 + screen[2].depth * weights.w2;
    const float interpolatedInvW = screen[0].invW * weights.w0 + screen[1].invW * weights.w1 + screen[2].invW * weights.w2;
    if (!std::isfinite(depth) || !std::isfinite(interpolatedInvW) || interpolatedInvW <= 0.000001f) {
        return false;
    }
    if (!framebuffer.depthTest(x, y, depth, depthTolerance)) {
        return false;
    }

    out.depth = depth;
    out.uv = {
        (screen[0].uvOverW.x * weights.w0 + screen[1].uvOverW.x * weights.w1 + screen[2].uvOverW.x * weights.w2) / interpolatedInvW,
        (screen[0].uvOverW.y * weights.w0 + screen[1].uvOverW.y * weights.w1 + screen[2].uvOverW.y * weights.w2) / interpolatedInvW,
    };
    out.vertexColor = {
        static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(screen[0].color.r) * weights.w0 + static_cast<float>(screen[1].color.r) * weights.w1 + static_cast<float>(screen[2].color.r) * weights.w2,
            0.0f,
            255.0f)),
        static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(screen[0].color.g) * weights.w0 + static_cast<float>(screen[1].color.g) * weights.w1 + static_cast<float>(screen[2].color.g) * weights.w2,
            0.0f,
            255.0f)),
        static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(screen[0].color.b) * weights.w0 + static_cast<float>(screen[1].color.b) * weights.w1 + static_cast<float>(screen[2].color.b) * weights.w2,
            0.0f,
            255.0f)),
        static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(screen[0].color.a) * weights.w0 + static_cast<float>(screen[1].color.a) * weights.w1 + static_cast<float>(screen[2].color.a) * weights.w2,
            0.0f,
            255.0f)),
    };

    if (requirements.normal) {
        out.normal = {
            (screen[0].normalOverW.x * weights.w0 + screen[1].normalOverW.x * weights.w1 + screen[2].normalOverW.x * weights.w2) / interpolatedInvW,
            (screen[0].normalOverW.y * weights.w0 + screen[1].normalOverW.y * weights.w1 + screen[2].normalOverW.y * weights.w2) / interpolatedInvW,
            (screen[0].normalOverW.z * weights.w0 + screen[1].normalOverW.z * weights.w1 + screen[2].normalOverW.z * weights.w2) / interpolatedInvW,
        };
        out.tangent = {
            (screen[0].tangentOverW.x * weights.w0 + screen[1].tangentOverW.x * weights.w1 + screen[2].tangentOverW.x * weights.w2) / interpolatedInvW,
            (screen[0].tangentOverW.y * weights.w0 + screen[1].tangentOverW.y * weights.w1 + screen[2].tangentOverW.y * weights.w2) / interpolatedInvW,
            (screen[0].tangentOverW.z * weights.w0 + screen[1].tangentOverW.z * weights.w1 + screen[2].tangentOverW.z * weights.w2) / interpolatedInvW,
        };
        out.tangentSign = (screen[0].tangentSignOverW * weights.w0 + screen[1].tangentSignOverW * weights.w1 + screen[2].tangentSignOverW * weights.w2) / interpolatedInvW;
        if (!isFinite(out.normal) || !isFinite(out.tangent) || !std::isfinite(out.tangentSign)) {
            return false;
        }
    }

    if (requirements.viewPosition) {
        out.viewPosition = {
            (screen[0].viewPositionOverW.x * weights.w0 + screen[1].viewPositionOverW.x * weights.w1 + screen[2].viewPositionOverW.x * weights.w2) / interpolatedInvW,
            (screen[0].viewPositionOverW.y * weights.w0 + screen[1].viewPositionOverW.y * weights.w1 + screen[2].viewPositionOverW.y * weights.w2) / interpolatedInvW,
            (screen[0].viewPositionOverW.z * weights.w0 + screen[1].viewPositionOverW.z * weights.w1 + screen[2].viewPositionOverW.z * weights.w2) / interpolatedInvW,
        };
    }

    if (requirements.worldPosition) {
        out.worldPosition = {
            (screen[0].worldPositionOverW.x * weights.w0 + screen[1].worldPositionOverW.x * weights.w1 + screen[2].worldPositionOverW.x * weights.w2) / interpolatedInvW,
            (screen[0].worldPositionOverW.y * weights.w0 + screen[1].worldPositionOverW.y * weights.w1 + screen[2].worldPositionOverW.y * weights.w2) / interpolatedInvW,
            (screen[0].worldPositionOverW.z * weights.w0 + screen[1].worldPositionOverW.z * weights.w1 + screen[2].worldPositionOverW.z * weights.w2) / interpolatedInvW,
        };
        if (!isFinite(out.worldPosition)) {
            return false;
        }
    }

    return true;
}

} // namespace sr
