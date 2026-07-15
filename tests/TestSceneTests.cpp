#include "scenes/TestScene.h"

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

        require(commands.size() == 8, "indoor gallery must contain eight draw commands");
        for (const sr::DrawCommand& command : commands) {
            require(command.mesh.vertices != nullptr, "every gallery command must have a mesh");
            require(command.mesh.vertexCount > 0, "every gallery mesh must contain vertices");
        }

        require(std::string_view(scene.activeModelName()) == "Placeholder Sculpture", "missing Showcase.obj must use the built-in sculpture");
        require(commands[0].material.diffuseTexture != nullptr, "floor must use a diffuse texture");
        require(commands[0].material.normalTexture != nullptr, "floor must use a normal map");
        require(commands[4].castsShadow, "pedestal must cast a shadow");
        require(commands[5].castsShadow, "central exhibit must cast a shadow");
        require(commands[7].castsShadow, "monolith must cast a shadow");

        const sr::RenderSceneView view = scene.renderView();
        int activeDirectionalLights = 0;
        int activePointLights = 0;
        for (const sr::DirectionalLight& light : view.settings.directionalLights) {
            activeDirectionalLights += light.intensity > 0.0f ? 1 : 0;
        }
        for (const sr::PointLight& light : view.settings.pointLights) {
            activePointLights += light.intensity > 0.0f ? 1 : 0;
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
