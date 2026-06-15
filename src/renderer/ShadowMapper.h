#pragma once

#include "core/ThreadPool.h"
#include "renderer/RenderSceneView.h"
#include "renderer/RenderStats.h"
#include "renderer/TileScheduler.h"

#include <vector>

namespace sr {

struct ShadowMap {
    int width = 512;
    int height = 512;
    std::vector<float> depth;

    ShadowMap(int width = 512, int height = 512);
    void clear();
    bool setIfCloser(int x, int y, float value);
    float sample(int x, int y) const;
};

Mat4 sceneLightViewProjection(const RenderSettings& lighting);
void renderShadowMap(
    const RenderSceneView& scene,
    const Mat4& lightViewProjection,
    ShadowMap& shadowMap,
    RendererStats& stats,
    ThreadPool& workers,
    int tileSize);
float shadowFactor(Vec3 worldPosition, Vec3 normal, const DirectionalLight& light, const Mat4& lightViewProjection, const ShadowMap& shadowMap, const RenderSettings& settings);
float lightSpaceDepth01(Vec3 worldPosition, const Mat4& lightViewProjection, const ShadowMap& shadowMap);

} // namespace sr
