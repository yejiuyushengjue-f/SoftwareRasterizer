#pragma once

#include "renderer/Material.h"
#include "renderer/ObjLoader.h"
#include "renderer/RenderSettings.h"

namespace sr {

struct ScenePreset {
    struct TextureFiles {
        const wchar_t* modelDiffuse = L"Frosted Metal Texture.jpeg";
        const wchar_t* cubeDiffuse = L"Cobblestone_pavement_texture.jpeg";
        const wchar_t* cubeNormal = L"Cobblestone_pavement_normal_texture.png";
    };

    struct ModelFiles {
        const wchar_t* preferredObj = L"Linnea.obj";
        const char* displayName = "Linnea";
        const char* builtinDisplayName = "Sphere";
        ObjLoadOptions objOptions;
    };

    struct MeshParameters {
        float sphereRadius = 0.72f;
        int sphereLatitudeSegments = 24;
        int sphereLongitudeSegments = 48;
        float cubeSize = 1.35f;
    };

    struct AnimationParameters {
        float objRotationSpeed = 0.45f;
        float sphereRotationSpeed = 0.65f;
        float cubeRotationSpeed = -0.45f;
        float sphereWobbleSpeed = 0.7f;
        float sphereWobbleAmount = 0.18f;
    };

    TextureFiles textures;
    ModelFiles model;
    MeshParameters meshes;
    AnimationParameters animation;
    Material modelMaterial;
    Material cubeMaterial;
    Material groundMaterial;
    RenderSettings renderSettings;

    static ScenePreset defaults();
};

} // namespace sr
