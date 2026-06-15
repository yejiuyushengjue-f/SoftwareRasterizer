#include "renderer/Renderer.h"
#include "core/Camera.h"
#include "core/Framebuffer.h"
#include "renderer/RenderSceneView.h"
#include "scenes/MeshFactory.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

void runRendererTests()
{
    sr::ShadowMap shadowMap(4, 4);
    shadowMap.clear();

    require(shadowMap.width == 4, "Expected shadow map width to match constructor argument.");
    require(shadowMap.height == 4, "Expected shadow map height to match constructor argument.");
    require(shadowMap.depth.size() == 16, "Expected shadow map storage to contain width * height entries.");

    const std::vector<sr::Vertex> cube = sr::MeshFactory::makeCube(1.0f);
    sr::DrawCommand command;
    command.mesh = { cube.data(), static_cast<int>(cube.size()) };
    command.transform = sr::Mat4::translation({ 0.0f, 0.0f, -3.0f });
    command.castsShadow = true;

    const sr::DrawCommand commands[] = { command };
    sr::RenderSettings settings;
    settings.directionalLights[0] = { sr::normalize({ -0.45f, 0.65f, 1.0f }), { 255, 244, 224, 255 }, 0.8f };
    settings.directionalLights[1] = { sr::normalize({ 0.75f, 0.35f, 0.25f }), { 135, 178, 255, 255 }, 0.28f };
    settings.directionalLights[2] = { sr::normalize({ -0.2f, 0.85f, -0.45f }), { 255, 145, 112, 255 }, 0.18f };
    settings.pointLights[0] = { { -1.35f, 0.85f, -1.7f }, { 255, 205, 150, 255 }, 0.75f, 3.0f };
    settings.pointLights[1] = { { 1.45f, -0.35f, -2.25f }, { 120, 210, 255, 255 }, 0.55f, 2.4f };
    const sr::RenderSceneView scene { commands, settings };

    sr::Camera camera;
    sr::Framebuffer framebuffer(32, 32);
    sr::Renderer renderer;

    renderer.render(scene, camera, framebuffer);
    const sr::RendererStats& stats = renderer.stats();

    require(stats.drawCommands == 1, "Expected renderer to process one draw command.");
    require(stats.inputTriangles == static_cast<std::uint64_t>(cube.size() / 3), "Expected renderer input triangle count to match cube mesh.");
    require(stats.rasterizedTriangles > 0, "Expected renderer to rasterize visible triangles.");
    require(stats.shadedPixels > 0, "Expected renderer to shade at least one pixel.");
    require(stats.colorPixelsWritten > 0, "Expected renderer to write color pixels.");
    require(stats.shadowTriangles > 0, "Expected shadow pass to project at least one triangle.");
    require(stats.shadowDepthWrites > 0, "Expected shadow pass to write depth texels.");
    require(stats.shadowPassMilliseconds >= 0.0, "Expected shadow pass timing to be non-negative.");
    require(stats.mainPassMilliseconds >= 0.0, "Expected main pass timing to be non-negative.");

    const std::uint32_t clearPixel = scene.settings.clearColor.toBGRA();
    const std::uint32_t* pixels = framebuffer.pixels();
    const int pixelCount = framebuffer.width() * framebuffer.height();
    const bool hasWrittenPixel = std::any_of(pixels, pixels + pixelCount, [clearPixel](std::uint32_t pixel) {
        return pixel != clearPixel;
    });
    require(hasWrittenPixel, "Expected framebuffer to contain at least one non-clear pixel.");

    std::vector<std::uint32_t> firstFrame(pixels, pixels + pixelCount);
    renderer.render(scene, camera, framebuffer);
    const bool repeatedFrameMatches = std::equal(firstFrame.begin(), firstFrame.end(), framebuffer.pixels());
    require(repeatedFrameMatches, "Expected repeated renders of the same scene to produce identical pixels.");

    sr::RenderSettings brightSettings = settings;
    brightSettings.toneMapping.enabled = true;
    brightSettings.toneMapping.exposure = 1.3f;
    brightSettings.directionalLights[0].intensity = 7.5f;
    brightSettings.directionalLights[1].intensity = 3.5f;
    brightSettings.directionalLights[2].intensity = 2.0f;
    brightSettings.pointLights[0].intensity = 4.0f;
    brightSettings.pointLights[1].intensity = 3.5f;
    command.material.ambientStrength = 0.35f;
    command.material.specularStrength = 0.9f;

    const sr::DrawCommand brightCommands[] = { command };
    const sr::RenderSceneView brightScene { brightCommands, brightSettings };

    sr::Framebuffer toneMappedFramebuffer(32, 32);
    renderer.setRenderMode(sr::RenderMode::Final);
    renderer.render(brightScene, camera, toneMappedFramebuffer);
    const std::vector<std::uint32_t> toneMappedPixels(
        toneMappedFramebuffer.pixels(),
        toneMappedFramebuffer.pixels() + pixelCount);

    brightSettings.toneMapping.enabled = false;
    const sr::RenderSceneView nonToneMappedScene { brightCommands, brightSettings };
    sr::Framebuffer nonToneMappedFramebuffer(32, 32);
    renderer.render(nonToneMappedScene, camera, nonToneMappedFramebuffer);
    const std::uint32_t* nonToneMappedPixels = nonToneMappedFramebuffer.pixels();

    bool foundToneMappedDifference = false;
    for (int i = 0; i < pixelCount; ++i) {
        if (toneMappedPixels[static_cast<std::size_t>(i)] != nonToneMappedPixels[i]
            && toneMappedPixels[static_cast<std::size_t>(i)] != clearPixel
            && nonToneMappedPixels[i] != clearPixel) {
            foundToneMappedDifference = true;
            break;
        }
    }
    require(foundToneMappedDifference, "Expected tone mapping toggle to change at least one shaded pixel.");
}
