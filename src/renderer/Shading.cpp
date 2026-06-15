#include "renderer/Shading.h"

#include "renderer/Renderer.h"
#include "renderer/Texture.h"

#include <algorithm>
#include <cmath>

namespace sr {

namespace {

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

LinearColor addLinearColor(LinearColor a, LinearColor b)
{
    return {
        a.r + b.r,
        a.g + b.g,
        a.b + b.b,
        a.a + b.a,
    };
}

LinearColor scaleLinearColor(LinearColor color, float scale)
{
    return {
        color.r * scale,
        color.g * scale,
        color.b * scale,
        color.a,
    };
}

LinearColor multiplyLinearColor(LinearColor a, LinearColor b)
{
    return {
        a.r * b.r,
        a.g * b.g,
        a.b * b.b,
        a.a * b.a,
    };
}

LinearColor lightContribution(
    LinearColor lightColor,
    float diffuse,
    float specular,
    LinearColor diffuseColor,
    LinearColor specularColor)
{
    return {
        diffuseColor.r * lightColor.r * diffuse + specularColor.r * lightColor.r * specular,
        diffuseColor.g * lightColor.g * diffuse + specularColor.g * lightColor.g * specular,
        diffuseColor.b * lightColor.b * diffuse + specularColor.b * lightColor.b * specular,
        diffuseColor.a,
    };
}

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
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

} // namespace

FragmentRequirements fragmentRequirements(RenderMode mode)
{
    FragmentRequirements requirements;
    requirements.surfaceColor = mode == RenderMode::Final || mode == RenderMode::Albedo;
    requirements.normal = mode == RenderMode::Final || mode == RenderMode::Normal || mode == RenderMode::Shadow || mode == RenderMode::Light;
    requirements.viewPosition = mode == RenderMode::Final || mode == RenderMode::Light;
    requirements.worldPosition = mode == RenderMode::Final || mode == RenderMode::Shadow || mode == RenderMode::Light || mode == RenderMode::LightDepth;
    return requirements;
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

Color modulate(Color a, Color b)
{
    return {
        static_cast<std::uint8_t>((static_cast<int>(a.r) * static_cast<int>(b.r)) / 255),
        static_cast<std::uint8_t>((static_cast<int>(a.g) * static_cast<int>(b.g)) / 255),
        static_cast<std::uint8_t>((static_cast<int>(a.b) * static_cast<int>(b.b)) / 255),
        static_cast<std::uint8_t>((static_cast<int>(a.a) * static_cast<int>(b.a)) / 255),
    };
}

LinearColor toLinearColor(Color color, float gamma)
{
    const float safeGamma = gamma > 0.0001f ? gamma : 2.2f;
    const auto convertChannel = [safeGamma](std::uint8_t value) {
        return std::pow(clamp01(static_cast<float>(value) / 255.0f), safeGamma);
    };

    return {
        convertChannel(color.r),
        convertChannel(color.g),
        convertChannel(color.b),
        clamp01(static_cast<float>(color.a) / 255.0f),
    };
}

Color toDisplayColor(LinearColor color, const ToneMappingSettings& settings, bool applyToneMapping)
{
    LinearColor mapped = {
        std::max(0.0f, color.r),
        std::max(0.0f, color.g),
        std::max(0.0f, color.b),
        clamp01(color.a),
    };

    if (applyToneMapping && settings.enabled) {
        const float exposure = std::max(0.0f, settings.exposure);
        mapped.r *= exposure;
        mapped.g *= exposure;
        mapped.b *= exposure;
        mapped.r = mapped.r / (1.0f + mapped.r);
        mapped.g = mapped.g / (1.0f + mapped.g);
        mapped.b = mapped.b / (1.0f + mapped.b);
    }

    const float gamma = settings.gamma > 0.0001f ? settings.gamma : 2.2f;
    const float invGamma = 1.0f / gamma;
    const auto encodeChannel = [invGamma](float value) {
        const float encoded = std::pow(clamp01(value), invGamma);
        return static_cast<std::uint8_t>(std::clamp(encoded * 255.0f, 0.0f, 255.0f));
    };

    return {
        encodeChannel(mapped.r),
        encodeChannel(mapped.g),
        encodeChannel(mapped.b),
        static_cast<std::uint8_t>(std::clamp(mapped.a * 255.0f, 0.0f, 255.0f)),
    };
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

LinearColor applyLighting(
    LinearColor surfaceColor,
    Vec3 normal,
    Vec3 viewPosition,
    const ViewLightSet& lights,
    const Material& material,
    float primaryShadow,
    float gamma)
{
    const Vec3 n = normalize(normal);
    const Vec3 viewDirection = normalize({ -viewPosition.x, -viewPosition.y, -viewPosition.z });
    const LinearColor ambientColor = multiplyLinearColor(surfaceColor, toLinearColor(material.ambientColor, gamma));
    const LinearColor diffuseColor = multiplyLinearColor(surfaceColor, toLinearColor(material.diffuseColor, gamma));
    const LinearColor specularColor = toLinearColor(material.specularColor, gamma);

    LinearColor result = scaleLinearColor(ambientColor, material.ambientStrength);

    for (std::size_t i = 0; i < lights.directionalLights.size(); ++i) {
        const DirectionalLight& light = lights.directionalLights[i];
        const float shadow = i == 0 ? primaryShadow : 1.0f;
        const Vec3 lightToSurface = normalize(light.direction);
        const Vec3 halfVector = normalize(lightToSurface + viewDirection);
        const float diffuse = std::max(0.0f, dot(n, lightToSurface)) * material.diffuseStrength * light.intensity * shadow;
        const float specular = std::pow(std::max(0.0f, dot(n, halfVector)), material.shininess) * material.specularStrength * light.intensity * shadow;
        result = addLinearColor(
            result,
            lightContribution(toLinearColor(light.color, gamma), diffuse, specular, diffuseColor, specularColor));
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
        result = addLinearColor(
            result,
            lightContribution(toLinearColor(light.color, gamma), diffuse, specular, diffuseColor, specularColor));
    }

    return result;
}

Color shadeFragment(const InterpolatedFragment& fragment, const DrawCommand& command, const ShadingContext& context)
{
    const bool needsShadow = context.requirements.worldPosition && context.mode != RenderMode::LightDepth;

    Color surfaceColor { 255, 255, 255, 255 };
    Color albedo { 255, 255, 255, 255 };
    if (context.requirements.surfaceColor) {
        surfaceColor = fragment.vertexColor;
        if (command.material.diffuseTexture) {
            surfaceColor = modulate(command.material.diffuseTexture->sample(fragment.uv), surfaceColor);
        }
        albedo = modulate(surfaceColor, command.material.diffuseColor);
    }

    Vec3 shadingNormal = fragment.normal;
    if (context.requirements.normal) {
        shadingNormal = applyNormalMap(fragment.normal, fragment.tangent, fragment.tangentSign, fragment.uv, command.material);
    }

    float shadow = 1.0f;
    if (needsShadow) {
        shadow = ::sr::shadowFactor(
            fragment.worldPosition,
            fragment.normal,
            context.primaryLight,
            context.lightViewProjection,
            context.shadowMap,
            context.settings);
    }

    switch (context.mode) {
    case RenderMode::Final:
        return toDisplayColor(
            applyLighting(
                toLinearColor(surfaceColor, context.settings.toneMapping.gamma),
                shadingNormal,
                fragment.viewPosition,
                context.lights,
                command.material,
                shadow,
                context.settings.toneMapping.gamma),
            context.settings.toneMapping,
            true);
    case RenderMode::Albedo:
        return albedo;
    case RenderMode::Normal:
        return normalToColor(shadingNormal);
    case RenderMode::Depth:
        return grayscale(fragment.depth * 0.5f + 0.5f);
    case RenderMode::UV:
        return uvToColor(fragment.uv);
    case RenderMode::Shadow:
        return grayscale(shadow);
    case RenderMode::Light:
        return toDisplayColor(
            applyLighting(
                toLinearColor({ 255, 255, 255, 255 }, context.settings.toneMapping.gamma),
                shadingNormal,
                fragment.viewPosition,
                context.lights,
                command.material,
                shadow,
                context.settings.toneMapping.gamma),
            context.settings.toneMapping,
            true);
    case RenderMode::LightDepth: {
        const float lightDepth = lightSpaceDepth01(fragment.worldPosition, context.lightViewProjection, context.shadowMap);
        return lightDepth >= 0.0f ? grayscale(lightDepth) : Color { 64, 0, 96, 255 };
    }
    default:
        return mixColor({ 255, 0, 255, 255 }, { 255, 0, 255, 255 }, { 255, 0, 255, 255 }, 1.0f, 0.0f, 0.0f);
    }
}

} // namespace sr
