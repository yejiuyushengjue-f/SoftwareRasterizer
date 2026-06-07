#pragma once

#include "core/Camera.h"
#include "core/Framebuffer.h"
#include "renderer/Vertex.h"

#include <array>
#include <cstdint>
#include <vector>

namespace sr {

class TestScene;

enum class RenderMode {
    Final = 0,
    Albedo,
    Normal,
    Depth,
    UV,
    Shadow,
    Light,
    LightDepth,
};

struct DirectionalLight {
    Vec3 direction;
    Color color;
    float intensity = 1.0f;
};

struct PointLight {
    Vec3 position;
    Color color;
    float intensity = 1.0f;
    float range = 1.0f;
};

struct ViewLightSet {
    std::array<DirectionalLight, 3> directionalLights;
    std::array<PointLight, 2> pointLights;
};

struct ShadowMap {
    int width = 512;
    int height = 512;
    std::vector<float> depth;

    ShadowMap(int width = 512, int height = 512);
    void clear();
    bool setIfCloser(int x, int y, float value);
    float sample(int x, int y) const;
};

struct RendererStats {
    std::uint64_t drawCommands = 0;
    std::uint64_t inputTriangles = 0;
    std::uint64_t rasterizedTriangles = 0;
    std::uint64_t shadowTriangles = 0;
    std::uint64_t shadedPixels = 0;
    std::uint64_t colorPixelsWritten = 0;
    std::uint64_t shadowDepthWrites = 0;
    double shadowPassMilliseconds = 0.0;
    double mainPassMilliseconds = 0.0;
};

class Renderer {
public:
    void render(const TestScene& scene, const Camera& camera, Framebuffer& framebuffer);
    void setRenderMode(RenderMode mode);
    RenderMode renderMode() const;
    const char* renderModeName() const;
    const RendererStats& stats() const;

private:
    struct CommandMatrices {
        Mat4 modelView;
        Mat4 mvp;
        Mat4 lightMvp;
    };

    ShadowMap shadowMap_;
    RenderMode renderMode_ = RenderMode::Final;
    RendererStats stats_;

    void draw(
        const DrawCommand& command,
        const Mat4& view,
        const Mat4& projection,
        const Mat4& lightViewProjection,
        const DirectionalLight& light,
        const ViewLightSet& lights,
        const ShadowMap& shadowMap,
        Framebuffer& framebuffer);
    void drawTriangle(
        const DrawCommand& command,
        const Vertex* vertices,
        const CommandMatrices& matrices,
        const Mat4& lightViewProjection,
        const DirectionalLight& light,
        const ViewLightSet& lights,
        const ShadowMap& shadowMap,
        Framebuffer& framebuffer);
    void renderDepthPrepass(const TestScene& scene, const Mat4& view, const Mat4& projection, const Mat4& lightViewProjection, Framebuffer& framebuffer);
    void drawDepthPrepass(const DrawCommand& command, const CommandMatrices& matrices, Framebuffer& framebuffer);
    void drawDepthTriangle(const Vertex* vertices, const CommandMatrices& matrices, Framebuffer& framebuffer);
    void renderShadowMap(const TestScene& scene, const Mat4& lightViewProjection, ShadowMap& shadowMap);
    void drawShadowTriangle(const Vertex* vertices, const Mat4& lightMvp, ShadowMap& shadowMap);
};

} // namespace sr
