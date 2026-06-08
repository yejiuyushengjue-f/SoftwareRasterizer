#include "renderer/Renderer.h"
#include "core/Camera.h"
#include "core/Framebuffer.h"
#include "renderer/RenderSceneView.h"
#include "scenes/MeshFactory.h"

#include <cassert>
#include <cstdint>
#include <vector>

void runRendererTests()
{
    sr::ShadowMap shadowMap(4, 4);
    shadowMap.clear();

    assert(shadowMap.width == 4);
    assert(shadowMap.height == 4);
    assert(shadowMap.depth.size() == 16);

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

    assert(stats.drawCommands == 1);
    assert(stats.inputTriangles == static_cast<std::uint64_t>(cube.size() / 3));
    assert(stats.rasterizedTriangles > 0);
    assert(stats.shadedPixels > 0);
    assert(stats.colorPixelsWritten > 0);
    assert(stats.shadowPassMilliseconds >= 0.0);
    assert(stats.mainPassMilliseconds >= 0.0);
}
