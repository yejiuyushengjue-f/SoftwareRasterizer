#include "renderer/Renderer.h"

#include "renderer/RasterHelpers.h"
#include "renderer/RenderContext.h"
#include "renderer/RenderPasses.h"
#include "renderer/ShadingHelpers.h"
#include "renderer/Texture.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace sr {

namespace {

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

struct ShadowVertex {
    Vec2 position;
    float depth = 0.0f;
};

struct ShadowProjection {
    Vec2 texelPosition;
    float depth = 0.0f;
    float depth01 = 0.0f;
};

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

bool isValidRenderMode(RenderMode mode)
{
    switch (mode) {
    case RenderMode::Final:
    case RenderMode::Albedo:
    case RenderMode::Normal:
    case RenderMode::Depth:
    case RenderMode::UV:
    case RenderMode::Shadow:
    case RenderMode::Light:
    case RenderMode::LightDepth:
        return true;
    default:
        return false;
    }
}

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

float edge(Vec2 a, Vec2 b, Vec2 p)
{
    return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
}

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

Color mixColor(Color a, Color b, Color c, float wa, float wb, float wc)
{
    const auto blend = [wa, wb, wc](std::uint8_t av, std::uint8_t bv, std::uint8_t cv) {
        const float value = static_cast<float>(av) * wa + static_cast<float>(bv) * wb + static_cast<float>(cv) * wc;
        return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f));
    };

    return {
        blend(a.r, b.r, c.r),
        blend(a.g, b.g, c.g),
        blend(a.b, b.b, c.b),
        blend(a.a, b.a, c.a),
    };
}

Color modulate(Color a, Color b)
{
    return {
        static_cast<std::uint8_t>((static_cast<int>(a.r) * static_cast<int>(b.r)) / 255),
        static_cast<std::uint8_t>((static_cast<int>(a.g) * static_cast<int>(b.g)) / 255),
        static_cast<std::uint8_t>((static_cast<int>(a.b) * static_cast<int>(b.b)) / 255),
        static_cast<std::uint8_t>((static_cast<int>(a.a) * static_cast<int>(b.a)) / 255),
    };
}

Color scaleColor(Color color, float scale)
{
    const auto scaleChannel = [scale](std::uint8_t value) {
        return static_cast<std::uint8_t>(std::clamp(static_cast<float>(value) * scale, 0.0f, 255.0f));
    };

    return {
        scaleChannel(color.r),
        scaleChannel(color.g),
        scaleChannel(color.b),
        color.a,
    };
}

Color grayscale(float value)
{
    if (!std::isfinite(value)) {
        value = 0.0f;
    }
    const std::uint8_t channel = static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
    return { channel, channel, channel, 255 };
}

Color normalToColor(Vec3 normal)
{
    const Vec3 n = normalize(normal);
    return {
        static_cast<std::uint8_t>(std::clamp(n.x * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f),
        static_cast<std::uint8_t>(std::clamp(n.y * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f),
        static_cast<std::uint8_t>(std::clamp(n.z * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f),
        255,
    };
}

Color uvToColor(Vec2 uv)
{
    if (!isFinite(uv)) {
        return { 255, 0, 255, 255 };
    }

    const auto repeat = [](float value) {
        const float wrapped = value - std::floor(value);
        return static_cast<std::uint8_t>(std::clamp(wrapped, 0.0f, 1.0f) * 255.0f);
    };

    return { repeat(uv.x), repeat(uv.y), 32, 255 };
}

Vec3 applyNormalMap(Vec3 normal, Vec3 tangent, float tangentSign, Vec2 uv, const Material& material)
{
    const Vec3 n = normalize(normal);
    if (!material.normalTexture || material.normalStrength <= 0.0f) {
        return n;
    }

    const Color sample = material.normalTexture->sample(uv);
    Vec3 tangentSpaceNormal {
        ((static_cast<float>(sample.r) / 255.0f) * 2.0f - 1.0f) * material.normalStrength,
        ((static_cast<float>(sample.g) / 255.0f) * 2.0f - 1.0f) * material.normalStrength,
        (static_cast<float>(sample.b) / 255.0f) * 2.0f - 1.0f,
    };
    tangentSpaceNormal = normalize(tangentSpaceNormal);

    const Vec3 t = orthogonalizeTangent(tangent, n);
    const Vec3 b = cross(n, t) * (tangentSign < 0.0f ? -1.0f : 1.0f);
    return normalize(t * tangentSpaceNormal.x + b * tangentSpaceNormal.y + n * tangentSpaceNormal.z);
}

Color addLight(Color current, Color lightColor, float diffuse, float specular, Color diffuseColor, Color specularColor)
{
    return {
        static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(current.r)
                + static_cast<float>(diffuseColor.r) * (static_cast<float>(lightColor.r) / 255.0f) * diffuse
                + static_cast<float>(specularColor.r) * (static_cast<float>(lightColor.r) / 255.0f) * specular,
            0.0f,
            255.0f)),
        static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(current.g)
                + static_cast<float>(diffuseColor.g) * (static_cast<float>(lightColor.g) / 255.0f) * diffuse
                + static_cast<float>(specularColor.g) * (static_cast<float>(lightColor.g) / 255.0f) * specular,
            0.0f,
            255.0f)),
        static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(current.b)
                + static_cast<float>(diffuseColor.b) * (static_cast<float>(lightColor.b) / 255.0f) * diffuse
                + static_cast<float>(specularColor.b) * (static_cast<float>(lightColor.b) / 255.0f) * specular,
            0.0f,
            255.0f)),
        diffuseColor.a,
    };
}

Color applyLighting(
    Color surfaceColor,
    Vec3 normal,
    Vec3 viewPosition,
    const ViewLightSet& lights,
    const Material& material,
    float primaryShadow)
{
    const Vec3 n = normalize(normal);
    const Vec3 viewDirection = normalize({ -viewPosition.x, -viewPosition.y, -viewPosition.z });
    const Color ambientColor = modulate(surfaceColor, material.ambientColor);
    const Color diffuseColor = modulate(surfaceColor, material.diffuseColor);

    Color result = scaleColor(ambientColor, material.ambientStrength);

    for (std::size_t i = 0; i < lights.directionalLights.size(); ++i) {
        const DirectionalLight& light = lights.directionalLights[i];
        const float shadow = i == 0 ? primaryShadow : 1.0f;
        const Vec3 lightToSurface = normalize(light.direction);
        const Vec3 halfVector = normalize(lightToSurface + viewDirection);
        const float diffuse = std::max(0.0f, dot(n, lightToSurface)) * material.diffuseStrength * light.intensity * shadow;
        const float specular = std::pow(std::max(0.0f, dot(n, halfVector)), material.shininess) * material.specularStrength * light.intensity * shadow;
        result = addLight(result, light.color, diffuse, specular, diffuseColor, material.specularColor);
    }

    for (const PointLight& light : lights.pointLights) {
        const Vec3 toLight = light.position - viewPosition;
        const float distanceSquared = std::max(0.0001f, dot(toLight, toLight));
        const float distance = std::sqrt(distanceSquared);
        const float attenuation = std::clamp(1.0f - distance / light.range, 0.0f, 1.0f);
        if (attenuation <= 0.0f) {
            continue;
        }

        const Vec3 lightToSurface = toLight / distance;
        const Vec3 halfVector = normalize(lightToSurface + viewDirection);
        const float diffuse = std::max(0.0f, dot(n, lightToSurface)) * material.diffuseStrength * light.intensity * attenuation * attenuation;
        const float specular = std::pow(std::max(0.0f, dot(n, halfVector)), material.shininess) * material.specularStrength * light.intensity * attenuation * attenuation;
        result = addLight(result, light.color, diffuse, specular, diffuseColor, material.specularColor);
    }

    return result;
}

Vec3 transformDirection(const Mat4& matrix, Vec3 direction)
{
    const Vec4 transformed = matrix * Vec4 { direction.x, direction.y, direction.z, 0.0f };
    return normalize({ transformed.x, transformed.y, transformed.z });
}

Vec3 transformPoint(const Mat4& matrix, Vec3 point)
{
    const Vec4 transformed = matrix * Vec4 { point.x, point.y, point.z, 1.0f };
    return { transformed.x, transformed.y, transformed.z };
}

ViewLightSet sceneLightsInView(const RenderSettings& lighting, const Mat4& view)
{
    return {
        {
            DirectionalLight { transformDirection(view, lighting.directionalLights[0].direction), lighting.directionalLights[0].color, lighting.directionalLights[0].intensity },
            DirectionalLight { transformDirection(view, lighting.directionalLights[1].direction), lighting.directionalLights[1].color, lighting.directionalLights[1].intensity },
            DirectionalLight { transformDirection(view, lighting.directionalLights[2].direction), lighting.directionalLights[2].color, lighting.directionalLights[2].intensity },
        },
        {
            PointLight {
                transformPoint(view, lighting.pointLights[0].position),
                lighting.pointLights[0].color,
                lighting.pointLights[0].intensity,
                lighting.pointLights[0].range,
            },
            PointLight {
                transformPoint(view, lighting.pointLights[1].position),
                lighting.pointLights[1].color,
                lighting.pointLights[1].intensity,
                lighting.pointLights[1].range,
            },
        },
    };
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

double elapsedMilliseconds(std::chrono::steady_clock::time_point begin, std::chrono::steady_clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - begin).count();
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

void Renderer::setRenderMode(RenderMode mode)
{
    renderMode_ = isValidRenderMode(mode) ? mode : RenderMode::Final;
}

RenderMode Renderer::renderMode() const
{
    return renderMode_;
}

const char* Renderer::renderModeName() const
{
    switch (renderMode_) {
    case RenderMode::Final:
        return "Final";
    case RenderMode::Albedo:
        return "Albedo";
    case RenderMode::Normal:
        return "Normal";
    case RenderMode::Depth:
        return "Depth";
    case RenderMode::UV:
        return "UV";
    case RenderMode::Shadow:
        return "Shadow Factor";
    case RenderMode::Light:
        return "Light";
    case RenderMode::LightDepth:
        return "Light-space Depth";
    default:
        return "Unknown";
    }
}

const RendererStats& Renderer::stats() const
{
    return stats_;
}

void Renderer::render(const RenderSceneView& scene, const Camera& camera, Framebuffer& framebuffer)
{
    struct ThreadRenderStats {
        std::uint64_t shadedPixels = 0;
        std::uint64_t colorPixelsWritten = 0;
    };

    stats_ = {};
    framebuffer.clear(scene.settings.clearColor);

    RenderContext context {
        framebuffer,
        camera,
        scene,
        renderMode_,
        stats_,
        shadowMap_,
    };

    ShadowPass shadowPass;
    PrepareGeometryPass prepareGeometryPass;
    DepthPrepass depthPrepass;
    ColorPass colorPass;

    context.primaryLight = &scene.settings.directionalLights[0];
    context.lightViewProjection = sceneLightViewProjection(scene.settings);
    context.shadowPass = [this](RenderContext& passContext) {
        const auto shadowBegin = std::chrono::steady_clock::now();
        renderShadowMap(passContext.scene, passContext.lightViewProjection, passContext.shadowMap);
        const auto shadowEnd = std::chrono::steady_clock::now();
        passContext.stats.shadowPassMilliseconds = elapsedMilliseconds(shadowBegin, shadowEnd);
    };
    shadowPass.execute(context);

    const auto mainBegin = std::chrono::steady_clock::now();
    const DirectionalLight& light = *context.primaryLight;
    const Mat4& lightViewProjection = context.lightViewProjection;
    context.view = camera.viewMatrix();
    const ViewLightSet lights = sceneLightsInView(scene.settings, context.view);
    context.lights = &lights;
    context.projection = camera.projectionMatrix(framebuffer.width(), framebuffer.height());

    context.prepareGeometryPass = [&](RenderContext& passContext) {
    std::vector<PreparedTriangle>& preparedTriangles = passContext.preparedTriangles;
    const Mat4& view = passContext.view;
    const Mat4& projection = passContext.projection;
    for (const DrawCommand& command : scene.drawCommands) {
        if (!command.mesh.vertices || command.mesh.vertexCount < 3) {
            continue;
        }

        ++stats_.drawCommands;
        stats_.inputTriangles += static_cast<std::uint64_t>(command.mesh.vertexCount / 3);

        const Mat4 modelView = view * command.transform;
        const CommandMatrices matrices {
            modelView,
            projection * modelView,
            lightViewProjection * command.transform,
        };

        for (int triangleIndex = 0; triangleIndex + 2 < command.mesh.vertexCount; triangleIndex += 3) {
            const Vertex* vertices = command.mesh.vertices + triangleIndex;
            ClipVertex clipTriangle[3] = {};
            bool triangleValid = true;

            for (int i = 0; i < 3; ++i) {
                const Vertex& vertex = vertices[i];
                const Vec4 worldPosition = command.transform * Vec4 { vertex.position.x, vertex.position.y, vertex.position.z, 1.0f };
                const Vec4 viewPosition = matrices.modelView * Vec4 { vertex.position.x, vertex.position.y, vertex.position.z, 1.0f };
                const Vec4 clip = matrices.mvp * Vec4 { vertex.position.x, vertex.position.y, vertex.position.z, 1.0f };
                const Vec3 normal = transformDirection(matrices.modelView, vertex.normal);
                const Vec3 tangent = transformDirection(matrices.modelView, vertex.tangent);
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
                const float area = edge(p0, p1, p2);
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
                ++stats_.rasterizedTriangles;
            }
        }
    }
    };
    prepareGeometryPass.execute(context);

    const int tileSize = std::max(1, scene.settings.tileSize);
    std::vector<ScreenTile>& tiles = context.tiles;
    for (int y = 0; y < framebuffer.height(); y += tileSize) {
        for (int x = 0; x < framebuffer.width(); x += tileSize) {
            tiles.push_back({
                x,
                y,
                std::min(framebuffer.width(), x + tileSize),
                std::min(framebuffer.height(), y + tileSize),
            });
        }
    }

    tileWorkers_.ensureWorkerCount(tiles.size());
    context.renderWorkerCount = tileWorkers_.activeWorkerCountFor(tiles.size());

    context.depthPrepass = [&](RenderContext& passContext) {
    const std::vector<PreparedTriangle>& preparedTriangles = passContext.preparedTriangles;
    const std::vector<ScreenTile>& tiles = passContext.tiles;
    const auto rasterizeDepthTile = [&](std::size_t tileIndex) {
        const ScreenTile& tile = tiles[tileIndex];
        for (const PreparedTriangle& triangle : preparedTriangles) {
            const int minX = std::max(tile.minX, triangle.minX);
            const int maxXExclusive = std::min(tile.maxXExclusive, triangle.maxXExclusive);
            const int minY = std::max(tile.minY, triangle.minY);
            const int maxYExclusive = std::min(tile.maxYExclusive, triangle.maxYExclusive);
            if (minX >= maxXExclusive || minY >= maxYExclusive) {
                continue;
            }

            const ScreenVertex* screen = triangle.vertices;
            const Vec2 p0 = screen[0].position;
            const Vec2 p1 = screen[1].position;
            const Vec2 p2 = screen[2].position;
            const float area = edge(p0, p1, p2);

            for (int y = minY; y < maxYExclusive; ++y) {
                for (int x = minX; x < maxXExclusive; ++x) {
                    const Vec2 sample { static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f };
                    const float w0 = edge(p1, p2, sample) / area;
                    const float w1 = edge(p2, p0, sample) / area;
                    const float w2 = edge(p0, p1, sample) / area;
                    if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                        continue;
                    }

                    const float depth = screen[0].depth * w0 + screen[1].depth * w1 + screen[2].depth * w2;
                    framebuffer.setDepthIfCloser(x, y, depth);
                }
            }
        }
    };

    tileWorkers_.parallelFor(tiles.size(), [&](std::size_t, std::size_t tileIndex) {
        rasterizeDepthTile(tileIndex);
    });
    };
    depthPrepass.execute(context);

    const RenderMode mode = renderMode_;
    const bool needsSurfaceColor = mode == RenderMode::Final || mode == RenderMode::Albedo;
    const bool needsNormal = mode == RenderMode::Final || mode == RenderMode::Normal || mode == RenderMode::Shadow || mode == RenderMode::Light;
    const bool needsViewPosition = mode == RenderMode::Final || mode == RenderMode::Light;
    const bool needsShadow = mode == RenderMode::Final || mode == RenderMode::Shadow || mode == RenderMode::Light;
    const bool needsLightSpaceDepth = mode == RenderMode::LightDepth;
    const bool needsLightSpacePosition = needsShadow || needsLightSpaceDepth;
    std::vector<ThreadRenderStats> threadStats(context.renderWorkerCount);

    context.colorPass = [&](RenderContext& passContext) {
    const std::vector<PreparedTriangle>& preparedTriangles = passContext.preparedTriangles;
    const std::vector<ScreenTile>& tiles = passContext.tiles;
    const auto rasterizeColorTile = [&](std::size_t tileIndex, ThreadRenderStats& localStats) {
        const ScreenTile& tile = tiles[tileIndex];
        for (const PreparedTriangle& triangle : preparedTriangles) {
            const int minX = std::max(tile.minX, triangle.minX);
            const int maxXExclusive = std::min(tile.maxXExclusive, triangle.maxXExclusive);
            const int minY = std::max(tile.minY, triangle.minY);
            const int maxYExclusive = std::min(tile.maxYExclusive, triangle.maxYExclusive);
            if (minX >= maxXExclusive || minY >= maxYExclusive) {
                continue;
            }

            const DrawCommand& command = *triangle.command;
            const ScreenVertex* screen = triangle.vertices;
            const Vec2 p0 = screen[0].position;
            const Vec2 p1 = screen[1].position;
            const Vec2 p2 = screen[2].position;
            const float area = edge(p0, p1, p2);

            for (int y = minY; y < maxYExclusive; ++y) {
                for (int x = minX; x < maxXExclusive; ++x) {
                    const Vec2 sample { static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f };
                    const float w0 = edge(p1, p2, sample) / area;
                    const float w1 = edge(p2, p0, sample) / area;
                    const float w2 = edge(p0, p1, sample) / area;
                    if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                        continue;
                    }

                    const float depth = screen[0].depth * w0 + screen[1].depth * w1 + screen[2].depth * w2;
                    const float interpolatedInvW = screen[0].invW * w0 + screen[1].invW * w1 + screen[2].invW * w2;
                    if (!std::isfinite(depth) || !std::isfinite(interpolatedInvW) || interpolatedInvW <= 0.000001f) {
                        continue;
                    }
                    if (!framebuffer.depthTest(x, y, depth, scene.settings.depthPrepassTolerance)) {
                        continue;
                    }

                    const Vec2 uv {
                        (screen[0].uvOverW.x * w0 + screen[1].uvOverW.x * w1 + screen[2].uvOverW.x * w2) / interpolatedInvW,
                        (screen[0].uvOverW.y * w0 + screen[1].uvOverW.y * w1 + screen[2].uvOverW.y * w2) / interpolatedInvW,
                    };

                    Vec3 normal {};
                    Vec3 shadingNormal {};
                    if (needsNormal) {
                        normal = {
                            (screen[0].normalOverW.x * w0 + screen[1].normalOverW.x * w1 + screen[2].normalOverW.x * w2) / interpolatedInvW,
                            (screen[0].normalOverW.y * w0 + screen[1].normalOverW.y * w1 + screen[2].normalOverW.y * w2) / interpolatedInvW,
                            (screen[0].normalOverW.z * w0 + screen[1].normalOverW.z * w1 + screen[2].normalOverW.z * w2) / interpolatedInvW,
                        };
                        const Vec3 tangent = {
                            (screen[0].tangentOverW.x * w0 + screen[1].tangentOverW.x * w1 + screen[2].tangentOverW.x * w2) / interpolatedInvW,
                            (screen[0].tangentOverW.y * w0 + screen[1].tangentOverW.y * w1 + screen[2].tangentOverW.y * w2) / interpolatedInvW,
                            (screen[0].tangentOverW.z * w0 + screen[1].tangentOverW.z * w1 + screen[2].tangentOverW.z * w2) / interpolatedInvW,
                        };
                        const float tangentSign = (screen[0].tangentSignOverW * w0 + screen[1].tangentSignOverW * w1 + screen[2].tangentSignOverW * w2) / interpolatedInvW;
                        if (!isFinite(normal) || !isFinite(tangent) || !std::isfinite(tangentSign)) {
                            continue;
                        }
                        shadingNormal = applyNormalMap(normal, tangent, tangentSign, uv, command.material);
                    }

                    Vec3 viewPosition {};
                    if (needsViewPosition) {
                        viewPosition = {
                            (screen[0].viewPositionOverW.x * w0 + screen[1].viewPositionOverW.x * w1 + screen[2].viewPositionOverW.x * w2) / interpolatedInvW,
                            (screen[0].viewPositionOverW.y * w0 + screen[1].viewPositionOverW.y * w1 + screen[2].viewPositionOverW.y * w2) / interpolatedInvW,
                            (screen[0].viewPositionOverW.z * w0 + screen[1].viewPositionOverW.z * w1 + screen[2].viewPositionOverW.z * w2) / interpolatedInvW,
                        };
                    }

                    float shadow = 1.0f;
                    float lightDepth = -1.0f;
                    if (needsLightSpacePosition) {
                        const Vec3 worldPosition = {
                            (screen[0].worldPositionOverW.x * w0 + screen[1].worldPositionOverW.x * w1 + screen[2].worldPositionOverW.x * w2) / interpolatedInvW,
                            (screen[0].worldPositionOverW.y * w0 + screen[1].worldPositionOverW.y * w1 + screen[2].worldPositionOverW.y * w2) / interpolatedInvW,
                            (screen[0].worldPositionOverW.z * w0 + screen[1].worldPositionOverW.z * w1 + screen[2].worldPositionOverW.z * w2) / interpolatedInvW,
                        };
                        if (!isFinite(worldPosition)) {
                            continue;
                        }
                        if (needsShadow) {
                            shadow = shadowFactor(worldPosition, normal, light, lightViewProjection, shadowMap_, scene.settings);
                        }
                        if (needsLightSpaceDepth) {
                            lightDepth = lightSpaceDepth01(worldPosition, lightViewProjection, shadowMap_);
                        }
                    }

                    Color surfaceColor { 255, 255, 255, 255 };
                    Color albedo { 255, 255, 255, 255 };
                    if (needsSurfaceColor) {
                        surfaceColor = mixColor(screen[0].color, screen[1].color, screen[2].color, w0, w1, w2);
                        if (command.material.diffuseTexture) {
                            surfaceColor = modulate(command.material.diffuseTexture->sample(uv), surfaceColor);
                        }
                        albedo = modulate(surfaceColor, command.material.diffuseColor);
                    }

                    Color color = albedo;
                    ++localStats.shadedPixels;
                    switch (mode) {
                    case RenderMode::Final:
                        color = applyLighting(surfaceColor, shadingNormal, viewPosition, lights, command.material, shadow);
                        break;
                    case RenderMode::Albedo:
                        color = albedo;
                        break;
                    case RenderMode::Normal:
                        color = ::sr::normalToColor(shadingNormal);
                        break;
                    case RenderMode::Depth:
                        color = ::sr::grayscale(depth * 0.5f + 0.5f);
                        break;
                    case RenderMode::UV:
                        color = ::sr::uvToColor(uv);
                        break;
                    case RenderMode::Shadow:
                        color = ::sr::grayscale(shadow);
                        break;
                    case RenderMode::Light:
                        color = applyLighting({ 255, 255, 255, 255 }, shadingNormal, viewPosition, lights, command.material, shadow);
                        break;
                    case RenderMode::LightDepth:
                        color = lightDepth >= 0.0f ? ::sr::grayscale(lightDepth) : Color { 64, 0, 96, 255 };
                        break;
                    }

                    framebuffer.setPixel(x, y, color);
                    ++localStats.colorPixelsWritten;
                }
            }
        }
    };

    tileWorkers_.parallelFor(tiles.size(), [&](std::size_t workerIndex, std::size_t tileIndex) {
        ThreadRenderStats& localStats = threadStats[workerIndex];
        rasterizeColorTile(tileIndex, localStats);
    });
    };
    colorPass.execute(context);

    for (const ThreadRenderStats& localStats : threadStats) {
        stats_.shadedPixels += localStats.shadedPixels;
        stats_.colorPixelsWritten += localStats.colorPixelsWritten;
    }
    const auto mainEnd = std::chrono::steady_clock::now();
    stats_.mainPassMilliseconds = elapsedMilliseconds(mainBegin, mainEnd);
}

void Renderer::renderShadowMap(const RenderSceneView& scene, const Mat4& lightViewProjection, ShadowMap& shadowMap)
{
    shadowMap.clear();
    for (const DrawCommand& command : scene.drawCommands) {
        if (!command.castsShadow || !command.mesh.vertices || command.mesh.vertexCount < 3) {
            continue;
        }

        const Mat4 lightMvp = lightViewProjection * command.transform;
        for (int i = 0; i + 2 < command.mesh.vertexCount; i += 3) {
            drawShadowTriangle(command.mesh.vertices + i, lightMvp, shadowMap);
        }
    }
}

void Renderer::drawShadowTriangle(const Vertex* vertices, const Mat4& lightMvp, ShadowMap& shadowMap)
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
    const float area = edge(p0, p1, p2);
    if (std::abs(area) <= 0.000001f) {
        return;
    }

    ++stats_.shadowTriangles;
    const int minX = std::max(0, static_cast<int>(std::floor(std::min({ p0.x, p1.x, p2.x }))));
    const int maxX = std::min(shadowMap.width - 1, static_cast<int>(std::ceil(std::max({ p0.x, p1.x, p2.x }))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({ p0.y, p1.y, p2.y }))));
    const int maxY = std::min(shadowMap.height - 1, static_cast<int>(std::ceil(std::max({ p0.y, p1.y, p2.y }))));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const Vec2 sample { static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f };
            const float w0 = edge(p1, p2, sample) / area;
            const float w1 = edge(p2, p0, sample) / area;
            const float w2 = edge(p0, p1, sample) / area;

            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                continue;
            }

            const float depth = screen[0].depth * w0 + screen[1].depth * w1 + screen[2].depth * w2;
            if (shadowMap.setIfCloser(x, y, depth)) {
                ++stats_.shadowDepthWrites;
            }
        }
    }
}

} // namespace sr
