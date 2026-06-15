#pragma once

#include "renderer/Material.h"
#include "renderer/RenderSettings.h"
#include "renderer/ShadowMapper.h"
#include "renderer/TriangleRasterizer.h"

#include <array>

namespace sr {

enum class RenderMode;
struct LinearColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct ViewLightSet {
    std::array<DirectionalLight, 3> directionalLights;
    std::array<PointLight, 2> pointLights;
};

struct ShadingContext {
    RenderMode mode;
    FragmentRequirements requirements;
    const ViewLightSet& lights;
    const DirectionalLight& primaryLight;
    const Mat4& lightViewProjection;
    const ShadowMap& shadowMap;
    const RenderSettings& settings;
};

FragmentRequirements fragmentRequirements(RenderMode mode);
ViewLightSet sceneLightsInView(const RenderSettings& lighting, const Mat4& view);
Vec3 applyNormalMap(Vec3 normal, Vec3 tangent, float tangentSign, Vec2 uv, const Material& material);
LinearColor applyLighting(
    LinearColor surfaceColor,
    Vec3 normal,
    Vec3 viewPosition,
    const ViewLightSet& lights,
    const Material& material,
    float primaryShadow,
    float gamma);
Color shadeFragment(const InterpolatedFragment& fragment, const DrawCommand& command, const ShadingContext& context);

Color grayscale(float value);
Color normalToColor(Vec3 normal);
Color uvToColor(Vec2 uv);
Color modulate(Color a, Color b);
LinearColor toLinearColor(Color color, float gamma);
Color toDisplayColor(LinearColor color, const ToneMappingSettings& settings, bool applyToneMapping);

} // namespace sr
