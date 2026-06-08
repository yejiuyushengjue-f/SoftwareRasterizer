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
    preset.model.objOptions = { true, 0.95f, { 255, 255, 255, 255 } };
    preset.modelMaterial = makeMaterial(
        { 210, 214, 220, 255 },
        { 245, 245, 248, 255 },
        { 245, 245, 255, 255 },
        0.23f,
        0.95f,
        0.38f,
        36.0f);
    preset.cubeMaterial = makeMaterial(
        { 170, 176, 184, 255 },
        { 210, 220, 232, 255 },
        { 235, 232, 220, 255 },
        0.20f,
        0.9f,
        0.42f,
        42.0f,
        0.28f);
    preset.groundMaterial = makeMaterial(
        { 205, 210, 205, 255 },
        { 240, 244, 236, 255 },
        { 70, 76, 84, 255 },
        0.34f,
        0.96f,
        0.02f,
        10.0f);

    preset.renderSettings.directionalLights = {
        DirectionalLight { normalize({ -0.45f, 0.65f, 1.0f }), { 255, 244, 224, 255 }, 0.8f },
        DirectionalLight { normalize({ 0.75f, 0.35f, 0.25f }), { 135, 178, 255, 255 }, 0.28f },
        DirectionalLight { normalize({ -0.2f, 0.85f, -0.45f }), { 255, 145, 112, 255 }, 0.18f },
    };
    preset.renderSettings.pointLights = {
        PointLight { { -1.35f, 0.85f, -1.7f }, { 255, 205, 150, 255 }, 0.75f, 3.0f },
        PointLight { { 1.45f, -0.35f, -2.25f }, { 120, 210, 255, 255 }, 0.55f, 2.4f },
    };
    preset.renderSettings.shadowTarget = { 0.0f, 0.0f, -3.0f };
    preset.renderSettings.shadowLightDistance = 6.5f;
    preset.renderSettings.shadowOrthoExtent = 4.0f;
    preset.renderSettings.shadowNearPlane = 0.1f;
    preset.renderSettings.shadowFarPlane = 12.0f;
    preset.renderSettings.clearColor = { 18, 20, 28, 255 };
    return preset;
}

} // namespace sr
