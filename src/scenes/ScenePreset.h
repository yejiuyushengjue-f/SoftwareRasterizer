#pragma once

#include "renderer/Material.h"
#include "renderer/ObjLoader.h"
#include "renderer/RenderSettings.h"

namespace sr {

struct ScenePreset {
    struct TextureFiles {
        const wchar_t* sculptureDiffuse = L"Frosted Metal Texture.jpeg";
        const wchar_t* floorDiffuse = L"Cobblestone_pavement_texture.jpeg";
        const wchar_t* floorNormal = L"Cobblestone_pavement_normal_texture.png";
    };

    struct ModelFiles {
        const wchar_t* preferredObj = L"Showcase.obj";
        const char* displayName = "Showcase OBJ";
        const char* builtinDisplayName = "Placeholder Sculpture";
        ObjLoadOptions objOptions;
    };

    struct MeshParameters {
        float floorWidth = 9.0f;
        float floorDepth = 8.0f;
        float wallWidth = 9.0f;
        float wallHeight = 4.6f;
        float wallThickness = 0.18f;
        float roofThickness = 0.16f;
        float frontWallOffset = 2.1f;
        float doorWidth = 1.7f;
        float doorHeight = 2.35f;
        float doorThickness = 0.10f;
        Vec3 pedestalSize = { 1.45f, 0.72f, 1.45f };
        Vec3 benchSize = { 1.25f, 0.52f, 0.92f };
        Vec3 monolithSize = { 0.72f, 1.78f, 0.72f };
        float pointLightMarkerRadius = 0.19f;
        float directionalLightMarkerRadius = 0.38f;
        int lightMarkerLatitudeSegments = 10;
        int lightMarkerLongitudeSegments = 20;

        float sculptureHeight = 1.42f;
        float sculptureBaseRadius = 0.22f;
        float sculptureBodyRadius = 0.34f;
        float sculptureNeckRadius = 0.16f;
        int sculptureSegments = 14;
    };

    struct AnimationParameters {
        float centralRotationSpeed = 0.34f;
    };

    TextureFiles textures;
    ModelFiles model;
    MeshParameters meshes;
    AnimationParameters animation;
    Material centralMaterial;
    Material pedestalMaterial;
    Material floorMaterial;
    Material wallMaterial;
    Material doorMaterial;
    Material accentMaterial;
    Material monolithMaterial;
    Material pointLightMarkerMaterial;
    Material directionalLightMarkerMaterial;
    RenderSettings renderSettings;

    static ScenePreset defaults();
};

} // namespace sr
