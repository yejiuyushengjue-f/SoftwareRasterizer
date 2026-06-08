#pragma once

#include "core/Color.h"
#include "math/Math.h"

#include <array>

namespace sr {

struct DirectionalLight {
    Vec3 direction = { 0.0f, 1.0f, 0.0f };
    Color color = { 255, 255, 255, 255 };
    float intensity = 1.0f;
};

struct PointLight {
    Vec3 position = {};
    Color color = { 255, 255, 255, 255 };
    float intensity = 1.0f;
    float range = 1.0f;
};

struct RenderSettings {
    Color clearColor = { 18, 20, 28, 255 };
    int tileSize = 32;

    std::array<DirectionalLight, 3> directionalLights = {};
    std::array<PointLight, 2> pointLights = {};

    Vec3 shadowTarget = { 0.0f, 0.0f, -3.0f };
    float shadowLightDistance = 6.5f;
    float shadowOrthoExtent = 4.0f;
    float shadowNearPlane = 0.1f;
    float shadowFarPlane = 12.0f;
    float shadowConstantBias = 0.0025f;
    float shadowSlopeScaleBias = 0.014f;
    float shadowMinimumBias = 0.0035f;
    float shadowMaximumBias = 0.035f;
    int shadowPcfRadius = 1;
    float shadowMinimumVisibility = 0.35f;

    float depthPrepassTolerance = 0.000001f;
};

} // namespace sr
