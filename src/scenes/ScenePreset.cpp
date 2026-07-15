#include "scenes/ScenePreset.h"

#include "math/Math.h"

namespace sr {

namespace {

Material makeMaterial(
    Color ambientColor,
    Color diffuseColor,
    Color specularColor,
    float ambientStrength,
    float diffuseStrength,
    float specularStrength,
    float shininess,
    float normalStrength = 1.0f)
{
    Material material;
    material.ambientColor = ambientColor;
    material.diffuseColor = diffuseColor;
    material.specularColor = specularColor;
    material.ambientStrength = ambientStrength;
    material.diffuseStrength = diffuseStrength;
    material.specularStrength = specularStrength;
    material.shininess = shininess;
    material.normalStrength = normalStrength;
    return material;
}

} // namespace

ScenePreset ScenePreset::defaults()
{
    ScenePreset preset;
    preset.model.objOptions = { true, 0.95f, { 235, 235, 235, 255 } };

    preset.centralMaterial = makeMaterial(
        { 196, 200, 206, 255 },
        { 236, 238, 242, 255 },
        { 248, 248, 255, 255 },
        0.18f,
        0.92f,
        0.55f,
        68.0f);
    preset.pedestalMaterial = makeMaterial(
        { 168, 164, 158, 255 },
        { 210, 206, 198, 255 },
        { 120, 118, 116, 255 },
        0.26f,
        0.88f,
        0.08f,
        14.0f);
    preset.floorMaterial = makeMaterial(
        { 144, 148, 154, 255 },
        { 205, 212, 220, 255 },
        { 230, 235, 240, 255 },
        0.18f,
        0.94f,
        0.24f,
        42.0f,
        0.72f);
    preset.wallMaterial = makeMaterial(
        { 200, 198, 194, 255 },
        { 234, 230, 224, 255 },
        { 108, 110, 118, 255 },
        0.32f,
        0.88f,
        0.04f,
        8.0f);
    preset.accentMaterial = makeMaterial(
        { 92, 112, 132, 255 },
        { 122, 150, 178, 255 },
        { 244, 244, 248, 255 },
        0.22f,
        0.86f,
        0.30f,
        36.0f);
    preset.monolithMaterial = makeMaterial(
        { 118, 120, 126, 255 },
        { 154, 160, 170, 255 },
        { 250, 248, 244, 255 },
        0.18f,
        0.84f,
        0.42f,
        52.0f);

    preset.renderSettings.directionalLights = {
        DirectionalLight { normalize({ -0.48f, 0.82f, 0.30f }), { 255, 244, 228, 255 }, 1.1f },
        DirectionalLight { normalize({ 0.0f, -1.0f, 0.0f }), { 255, 255, 255, 255 }, 0.0f },
        DirectionalLight { normalize({ 0.0f, -1.0f, 0.0f }), { 255, 255, 255, 255 }, 0.0f },
    };
    preset.renderSettings.pointLights = {
        PointLight { { 1.65f, 1.9f, -2.35f }, { 255, 214, 178, 255 }, 0.95f, 4.4f },
        PointLight { { 0.0f, 0.0f, 0.0f }, { 255, 255, 255, 255 }, 0.0f, 1.0f },
    };
    preset.renderSettings.shadowTarget = { 0.0f, 0.2f, -3.9f };
    preset.renderSettings.shadowLightDistance = 8.5f;
    preset.renderSettings.shadowOrthoExtent = 5.4f;
    preset.renderSettings.shadowNearPlane = 0.1f;
    preset.renderSettings.shadowFarPlane = 16.0f;
    preset.renderSettings.shadowConstantBias = 0.0022f;
    preset.renderSettings.shadowSlopeScaleBias = 0.012f;
    preset.renderSettings.shadowMinimumBias = 0.0028f;
    preset.renderSettings.shadowMaximumBias = 0.03f;
    preset.renderSettings.shadowMinimumVisibility = 0.30f;
    preset.renderSettings.clearColor = { 14, 16, 20, 255 };
    preset.renderSettings.toneMapping.exposure = 1.12f;
    return preset;
}

} // namespace sr
