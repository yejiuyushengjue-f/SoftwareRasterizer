#include "scenes/TestScene.h"

#include "math/Math.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(std::string("TestSceneTests: ") + message);
    }
}

class ScopedCurrentPath {
public:
    explicit ScopedCurrentPath(const std::filesystem::path& path)
        : original_(std::filesystem::current_path())
    {
        std::filesystem::current_path(path);
    }

    ~ScopedCurrentPath()
    {
        std::filesystem::current_path(original_);
    }

private:
    std::filesystem::path original_;
};

std::filesystem::path uniqueSandbox()
{
    return std::filesystem::temp_directory_path()
        / ("simple_renderer_test_scene_tests_" + std::to_string(reinterpret_cast<std::uintptr_t>(&uniqueSandbox)));
}

enum CommandIndex : std::size_t {
    Floor,
    Roof,
    BackWall,
    LeftWall,
    RightWall,
    FrontWallLeft,
    FrontWallRight,
    FrontWallTop,
    Door,
    Pedestal,
    CentralExhibit,
    AccentBench,
    AccentMonolith,
    PointLightMarker,
    DirectionalLightMarker,
    CommandCount,
};

struct Bounds {
    sr::Vec3 min;
    sr::Vec3 max;
};

Bounds computeBounds(const sr::Mesh& mesh)
{
    require(mesh.vertices != nullptr, "mesh must expose vertices");
    require(mesh.vertexCount > 0, "mesh must contain vertices");

    Bounds bounds { mesh.vertices[0].position, mesh.vertices[0].position };
    for (int i = 1; i < mesh.vertexCount; ++i) {
        const sr::Vec3 position = mesh.vertices[i].position;
        bounds.min.x = std::min(bounds.min.x, position.x);
        bounds.min.y = std::min(bounds.min.y, position.y);
        bounds.min.z = std::min(bounds.min.z, position.z);
        bounds.max.x = std::max(bounds.max.x, position.x);
        bounds.max.y = std::max(bounds.max.y, position.y);
        bounds.max.z = std::max(bounds.max.z, position.z);
    }

    return bounds;
}

void requireValidMesh(const sr::DrawCommand& command, const char* message)
{
    require(command.mesh.vertices != nullptr, message);
    require(command.mesh.vertexCount > 0, message);
}

void requireNonEmptyBoxGeometry(const sr::DrawCommand& command, const char* message)
{
    requireValidMesh(command, message);
    const Bounds bounds = computeBounds(command.mesh);
    const sr::Vec3 extent = bounds.max - bounds.min;
    require(extent.x > 0.001f, message);
    require(extent.y > 0.001f, message);
    require(extent.z > 0.001f, message);
}

void requireValidDirectionalLight(const sr::DirectionalLight& light, const char* message)
{
    require(std::isfinite(light.direction.x), message);
    require(std::isfinite(light.direction.y), message);
    require(std::isfinite(light.direction.z), message);
    require(std::isfinite(light.intensity), message);
    require(light.intensity > 0.0f, message);
    require(std::abs(sr::dot(light.direction, light.direction) - 1.0f) < 0.02f, message);
}

void requireValidPointLight(const sr::PointLight& light, const char* message)
{
    require(std::isfinite(light.position.x), message);
    require(std::isfinite(light.position.y), message);
    require(std::isfinite(light.position.z), message);
    require(std::isfinite(light.intensity), message);
    require(std::isfinite(light.range), message);
    require(light.intensity > 0.0f, message);
    require(light.range > 0.0f, message);
}

} // namespace

void runTestSceneTests()
{
    const std::filesystem::path sandbox = uniqueSandbox();
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);

    {
        ScopedCurrentPath scopedPath(sandbox);
        sr::TestScene scene;
        const auto commands = scene.drawCommands();

        require(commands.size() == CommandCount, "indoor gallery must contain fifteen draw commands");
        for (const sr::DrawCommand& command : commands) {
            requireValidMesh(command, "every gallery command must have a valid mesh");
        }

        require(std::string_view(scene.activeModelName()) == "Placeholder Sculpture", "missing Showcase.obj must use the built-in sculpture");
        require(commands[Floor].material.diffuseTexture != nullptr, "floor must use a diffuse texture");
        require(commands[Floor].material.normalTexture != nullptr, "floor must use a normal map");

        requireNonEmptyBoxGeometry(commands[Roof], "roof must use non-empty box geometry");
        requireNonEmptyBoxGeometry(commands[BackWall], "back wall must use non-empty box geometry");
        requireNonEmptyBoxGeometry(commands[LeftWall], "left wall must use non-empty box geometry");
        requireNonEmptyBoxGeometry(commands[RightWall], "right wall must use non-empty box geometry");
        requireNonEmptyBoxGeometry(commands[FrontWallLeft], "front wall left must use non-empty box geometry");
        requireNonEmptyBoxGeometry(commands[FrontWallRight], "front wall right must use non-empty box geometry");
        requireNonEmptyBoxGeometry(commands[FrontWallTop], "front wall top must use non-empty box geometry");

        require(commands[Door].castsShadow, "door must cast a shadow");
        require(commands[Pedestal].castsShadow, "pedestal must cast a shadow");
        require(commands[CentralExhibit].castsShadow, "central exhibit must cast a shadow");
        require(commands[AccentBench].castsShadow, "accent bench must cast a shadow");
        require(commands[AccentMonolith].castsShadow, "accent monolith must cast a shadow");
        require(!commands[PointLightMarker].castsShadow, "point light marker must not cast a shadow");
        require(!commands[DirectionalLightMarker].castsShadow, "directional light marker must not cast a shadow");

        const sr::RenderSceneView view = scene.renderView();
        int activeDirectionalLights = 0;
        int activePointLights = 0;
        for (const sr::DirectionalLight& light : view.settings.directionalLights) {
            if (light.intensity > 0.0f) {
                requireValidDirectionalLight(light, "gallery must expose a valid active directional light");
                ++activeDirectionalLights;
            }
        }
        for (const sr::PointLight& light : view.settings.pointLights) {
            if (light.intensity > 0.0f) {
                requireValidPointLight(light, "gallery must expose a valid active point light");
                ++activePointLights;
            }
        }
        require(activeDirectionalLights == 1, "gallery must use one active directional light");
        require(activePointLights == 1, "gallery must use one active point light");

        bool recordedTextureFailure = false;
        bool recordedObjFailure = false;
        for (const sr::LoadDiagnostics::Entry& entry : scene.loadDiagnostics().entries()) {
            recordedTextureFailure = recordedTextureFailure || entry.kind == sr::LoadDiagnostics::Kind::Texture;
            recordedObjFailure = recordedObjFailure || entry.kind == sr::LoadDiagnostics::Kind::Obj;
        }
        require(recordedTextureFailure, "missing textures must be diagnosed");
        require(recordedObjFailure, "missing Showcase.obj must be diagnosed");

        const float initialAngle = scene.exhibitRotationAngle();
        scene.update(1.0f);
        require(scene.exhibitRotationAngle() > initialAngle, "central exhibit must rotate by default");

        scene.toggleExhibitRotation();
        require(scene.exhibitRotationPaused(), "F3 action must pause exhibit rotation");
        const float pausedAngle = scene.exhibitRotationAngle();
        scene.update(1.0f);
        require(std::abs(scene.exhibitRotationAngle() - pausedAngle) < 0.000001f, "paused exhibit angle must remain unchanged");

        scene.toggleExhibitRotation();
        scene.update(1.0f);
        require(scene.exhibitRotationAngle() > pausedAngle, "resumed exhibit must continue rotating");
    }

    std::filesystem::remove_all(sandbox);
}
