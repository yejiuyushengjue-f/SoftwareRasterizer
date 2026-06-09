#include "renderer/ShadowMapper.h"

#include "renderer/RasterHelpers.h"
#include "renderer/TriangleRasterizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace sr {

namespace {

struct ShadowVertex {
    Vec2 position;
    float depth = 0.0f;
};

struct ShadowProjection {
    Vec2 texelPosition;
    float depth = 0.0f;
    float depth01 = 0.0f;
};

std::size_t checkedDepthCount(int width, int height)
{
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Shadow map dimensions must be positive.");
    }

    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / h) {
        throw std::runtime_error("Shadow map dimensions are too large.");
    }

    return w * h;
}

bool projectToShadowMap(Vec3 worldPosition, const Mat4& lightViewProjection, const ShadowMap& shadowMap, ShadowProjection& out)
{
    if (!isFinite(worldPosition)) {
        return false;
    }

    const Vec4 lightClip = lightViewProjection * Vec4 { worldPosition.x, worldPosition.y, worldPosition.z, 1.0f };
    if (!isFinite(lightClip) || std::abs(lightClip.w) <= 0.000001f) {
        return false;
    }

    const float invW = 1.0f / lightClip.w;
    const float ndcX = lightClip.x * invW;
    const float ndcY = lightClip.y * invW;
    const float ndcZ = lightClip.z * invW;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ)) {
        return false;
    }
    if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f || ndcZ < -1.0f || ndcZ > 1.0f) {
        return false;
    }

    out = {
        {
            (ndcX * 0.5f + 0.5f) * static_cast<float>(shadowMap.width - 1),
            (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(shadowMap.height - 1),
        },
        ndcZ,
        ndcZ * 0.5f + 0.5f,
    };
    return true;
}

float shadowBias(Vec3 normal, Vec3 lightDirection, const RenderSettings& settings)
{
    const float normalDotLight = std::max(0.0f, dot(normalize(normal), normalize(lightDirection)));
    const float slopeBias = settings.shadowSlopeScaleBias * (1.0f - normalDotLight);
    return std::clamp(settings.shadowConstantBias + slopeBias, settings.shadowMinimumBias, settings.shadowMaximumBias);
}

float pcfShadowFactor(const ShadowProjection& projection, float bias, const ShadowMap& shadowMap, const RenderSettings& settings)
{
    const int centerX = static_cast<int>(std::round(projection.texelPosition.x));
    const int centerY = static_cast<int>(std::round(projection.texelPosition.y));
    float weightedVisibility = 0.0f;
    float totalWeight = 0.0f;

    for (int offsetY = -settings.shadowPcfRadius; offsetY <= settings.shadowPcfRadius; ++offsetY) {
        for (int offsetX = -settings.shadowPcfRadius; offsetX <= settings.shadowPcfRadius; ++offsetX) {
            const int absOffsetX = offsetX < 0 ? -offsetX : offsetX;
            const int absOffsetY = offsetY < 0 ? -offsetY : offsetY;
            const float weight = static_cast<float>((settings.shadowPcfRadius + 1 - absOffsetX) * (settings.shadowPcfRadius + 1 - absOffsetY));
            const float closestDepth = shadowMap.sample(centerX + offsetX, centerY + offsetY);
            const float lit = (!std::isfinite(closestDepth) || projection.depth <= closestDepth + bias) ? 1.0f : 0.0f;
            weightedVisibility += lit * weight;
            totalWeight += weight;
        }
    }

    if (totalWeight <= 0.0f) {
        return 1.0f;
    }

    const float visibility = weightedVisibility / totalWeight;
    return settings.shadowMinimumVisibility + (1.0f - settings.shadowMinimumVisibility) * visibility;
}

void drawShadowTriangle(const Vertex* vertices, const Mat4& lightMvp, ShadowMap& shadowMap, RendererStats& stats)
{
    ShadowVertex screen[3] = {};

    for (int i = 0; i < 3; ++i) {
        const Vertex& vertex = vertices[i];
        const Vec4 clip = lightMvp * Vec4 { vertex.position.x, vertex.position.y, vertex.position.z, 1.0f };
        if (!isFinite(clip) || std::abs(clip.w) <= 0.000001f) {
            return;
        }

        const float invW = 1.0f / clip.w;
        const float ndcX = clip.x * invW;
        const float ndcY = clip.y * invW;
        const float ndcZ = clip.z * invW;
        if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ)) {
            return;
        }

        screen[i] = {
            {
                (ndcX * 0.5f + 0.5f) * static_cast<float>(shadowMap.width - 1),
                (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(shadowMap.height - 1),
            },
            ndcZ,
        };
    }

    const Vec2 p0 = screen[0].position;
    const Vec2 p1 = screen[1].position;
    const Vec2 p2 = screen[2].position;
    const float area = rasterEdge(p0, p1, p2);
    if (std::abs(area) <= 0.000001f) {
        return;
    }

    ++stats.shadowTriangles;
    const int minX = std::max(0, static_cast<int>(std::floor(std::min({ p0.x, p1.x, p2.x }))));
    const int maxX = std::min(shadowMap.width - 1, static_cast<int>(std::ceil(std::max({ p0.x, p1.x, p2.x }))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({ p0.y, p1.y, p2.y }))));
    const int maxY = std::min(shadowMap.height - 1, static_cast<int>(std::ceil(std::max({ p0.y, p1.y, p2.y }))));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const Vec2 sample { static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f };
            const BarycentricWeights weights = barycentricWeights(p0, p1, p2, sample, area);
            if (!isInsideTriangle(weights)) {
                continue;
            }

            const float depth = screen[0].depth * weights.w0 + screen[1].depth * weights.w1 + screen[2].depth * weights.w2;
            if (shadowMap.setIfCloser(x, y, depth)) {
                ++stats.shadowDepthWrites;
            }
        }
    }
}

} // namespace

ShadowMap::ShadowMap(int mapWidth, int mapHeight)
    : width(mapWidth)
    , height(mapHeight)
    , depth(checkedDepthCount(mapWidth, mapHeight), std::numeric_limits<float>::infinity())
{
}

void ShadowMap::clear()
{
    std::fill(depth.begin(), depth.end(), std::numeric_limits<float>::infinity());
}

bool ShadowMap::setIfCloser(int x, int y, float value)
{
    if (x < 0 || y < 0 || x >= width || y >= height || !std::isfinite(value)) {
        return false;
    }

    const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
    if (value >= depth[index]) {
        return false;
    }

    depth[index] = value;
    return true;
}

float ShadowMap::sample(int x, int y) const
{
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return std::numeric_limits<float>::infinity();
    }

    return depth[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
}

Mat4 sceneLightViewProjection(const RenderSettings& lighting)
{
    const DirectionalLight& light = lighting.directionalLights[0];
    const Vec3 lightPosition = light.direction * lighting.shadowLightDistance;
    const Mat4 lightView = Mat4::lookAt(lightPosition, lighting.shadowTarget, { 0.0f, 1.0f, 0.0f });
    const float extent = lighting.shadowOrthoExtent;
    const Mat4 lightProjection = Mat4::orthographic(-extent, extent, -extent, extent, lighting.shadowNearPlane, lighting.shadowFarPlane);
    return lightProjection * lightView;
}

void renderShadowMap(const RenderSceneView& scene, const Mat4& lightViewProjection, ShadowMap& shadowMap, RendererStats& stats)
{
    shadowMap.clear();
    for (const DrawCommand& command : scene.drawCommands) {
        if (!command.castsShadow || !command.mesh.vertices || command.mesh.vertexCount < 3) {
            continue;
        }

        const Mat4 lightMvp = lightViewProjection * command.transform;
        for (int i = 0; i + 2 < command.mesh.vertexCount; i += 3) {
            drawShadowTriangle(command.mesh.vertices + i, lightMvp, shadowMap, stats);
        }
    }
}

float shadowFactor(Vec3 worldPosition, Vec3 normal, const DirectionalLight& light, const Mat4& lightViewProjection, const ShadowMap& shadowMap, const RenderSettings& settings)
{
    if (!isFinite(normal)) {
        return 1.0f;
    }

    ShadowProjection projection;
    if (!projectToShadowMap(worldPosition, lightViewProjection, shadowMap, projection)) {
        return 1.0f;
    }

    return pcfShadowFactor(projection, shadowBias(normal, light.direction, settings), shadowMap, settings);
}

float lightSpaceDepth01(Vec3 worldPosition, const Mat4& lightViewProjection, const ShadowMap& shadowMap)
{
    ShadowProjection projection;
    if (!projectToShadowMap(worldPosition, lightViewProjection, shadowMap, projection)) {
        return -1.0f;
    }

    return projection.depth01;
}

} // namespace sr
