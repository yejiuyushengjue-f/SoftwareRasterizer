#include "renderer/Renderer.h"

#include "renderer/RenderContext.h"
#include "renderer/RenderPasses.h"
#include "renderer/Shading.h"
#include "renderer/TriangleRasterizer.h"

#include <chrono>
#include <vector>

namespace sr {

namespace {

bool isValidRenderMode(RenderMode mode)
{
    switch (mode) {
    case RenderMode::Final:
    case RenderMode::Albedo:
    case RenderMode::Normal:
    case RenderMode::Depth:
    case RenderMode::UV:
    case RenderMode::Shadow:
    case RenderMode::Light:
    case RenderMode::LightDepth:
        return true;
    default:
        return false;
    }
}

} // namespace

void Renderer::setRenderMode(RenderMode mode)
{
    renderMode_ = isValidRenderMode(mode) ? mode : RenderMode::Final;
}

RenderMode Renderer::renderMode() const
{
    return renderMode_;
}

const char* Renderer::renderModeName() const
{
    switch (renderMode_) {
    case RenderMode::Final:
        return "Final";
    case RenderMode::Albedo:
        return "Albedo";
    case RenderMode::Normal:
        return "Normal";
    case RenderMode::Depth:
        return "Depth";
    case RenderMode::UV:
        return "UV";
    case RenderMode::Shadow:
        return "Shadow Factor";
    case RenderMode::Light:
        return "Light";
    case RenderMode::LightDepth:
        return "Light-space Depth";
    default:
        return "Unknown";
    }
}

const RendererStats& Renderer::stats() const
{
    return stats_;
}

void Renderer::render(const RenderSceneView& scene, const Camera& camera, Framebuffer& framebuffer)
{
    struct ThreadRenderStats {
        std::uint64_t shadedPixels = 0;
        std::uint64_t colorPixelsWritten = 0;
    };

    stats_ = {};
    framebuffer.clear(scene.settings.clearColor);

    RenderContext context {
        framebuffer,
        camera,
        scene,
        renderMode_,
        stats_,
        shadowMap_,
    };

    ShadowPass shadowPass;
    PrepareGeometryPass prepareGeometryPass;
    DepthPrepass depthPrepass;
    ColorPass colorPass;

    context.primaryLight = &scene.settings.directionalLights[0];
    context.lightViewProjection = sceneLightViewProjection(scene.settings);
    context.shadowPass = [](RenderContext& passContext) {
        const auto shadowBegin = std::chrono::steady_clock::now();
        renderShadowMap(passContext.scene, passContext.lightViewProjection, passContext.shadowMap, passContext.stats);
        const auto shadowEnd = std::chrono::steady_clock::now();
        passContext.stats.shadowPassMilliseconds = elapsedRenderMilliseconds(shadowBegin, shadowEnd);
    };
    shadowPass.execute(context);

    const auto mainBegin = std::chrono::steady_clock::now();
    context.view = camera.viewMatrix();
    const ViewLightSet lights = sceneLightsInView(scene.settings, context.view);
    context.lights = &lights;
    context.projection = camera.projectionMatrix(framebuffer.width(), framebuffer.height());

    context.prepareGeometryPass = [](RenderContext& passContext) {
        buildPreparedTriangles(
            passContext.scene,
            passContext.view,
            passContext.projection,
            passContext.framebuffer,
            passContext.stats,
            passContext.preparedTriangles);
    };
    prepareGeometryPass.execute(context);

    context.tiles = tileScheduler_.buildTiles(framebuffer.width(), framebuffer.height(), scene.settings.tileSize);
    context.renderWorkerCount = tileScheduler_.activeWorkerCount();

    context.depthPrepass = [this](RenderContext& passContext) {
        tileScheduler_.parallelFor(passContext.tiles.size(), [&](std::size_t, std::size_t tileIndex) {
            rasterizeDepthTile(passContext.framebuffer, passContext.tiles[tileIndex], passContext.preparedTriangles);
        });
    };
    depthPrepass.execute(context);

    const FragmentRequirements requirements = fragmentRequirements(renderMode_);
    const ShadingContext shadingContext {
        renderMode_,
        requirements,
        *context.lights,
        *context.primaryLight,
        context.lightViewProjection,
        shadowMap_,
        scene.settings,
    };
    std::vector<ThreadRenderStats> threadStats(context.renderWorkerCount);

    context.colorPass = [this, &shadingContext, &threadStats](RenderContext& passContext) {
        tileScheduler_.parallelFor(passContext.tiles.size(), [&](std::size_t workerIndex, std::size_t tileIndex) {
            ThreadRenderStats& localStats = threadStats[workerIndex];
            const ScreenTile& tile = passContext.tiles[tileIndex];

            for (const PreparedTriangle& triangle : passContext.preparedTriangles) {
                int minX = 0;
                int maxXExclusive = 0;
                int minY = 0;
                int maxYExclusive = 0;
                if (!intersectTile(tile, triangle, minX, maxXExclusive, minY, maxYExclusive)) {
                    continue;
                }

                for (int y = minY; y < maxYExclusive; ++y) {
                    for (int x = minX; x < maxXExclusive; ++x) {
                        InterpolatedFragment fragment;
                        if (!interpolateFragment(
                                triangle,
                                x,
                                y,
                                passContext.framebuffer,
                                passContext.scene.settings.depthPrepassTolerance,
                                shadingContext.requirements,
                                fragment)) {
                            continue;
                        }

                        const Color color = shadeFragment(fragment, *triangle.command, shadingContext);
                        ++localStats.shadedPixels;
                        passContext.framebuffer.setPixel(x, y, color);
                        ++localStats.colorPixelsWritten;
                    }
                }
            }
        });
    };
    colorPass.execute(context);

    for (const ThreadRenderStats& localStats : threadStats) {
        stats_.shadedPixels += localStats.shadedPixels;
        stats_.colorPixelsWritten += localStats.colorPixelsWritten;
    }

    const auto mainEnd = std::chrono::steady_clock::now();
    stats_.mainPassMilliseconds = elapsedRenderMilliseconds(mainBegin, mainEnd);
}

} // namespace sr
