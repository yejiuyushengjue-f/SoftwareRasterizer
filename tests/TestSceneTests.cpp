#include "scenes/TestScene.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

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
    sr::TestScene scene;
    assert(scene.drawCommands().size() == 3);
    assert(scene.activeModelName() != nullptr);

    const std::filesystem::path sandbox = uniqueSandbox();
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);

    {
        ScopedCurrentPath scopedPath(sandbox);
        sr::TestScene missingAssetsScene;
        missingAssetsScene.toggleModel();

        const auto commands = missingAssetsScene.drawCommands();
        assert(commands.size() == 3);
        assert(commands[0].mesh.vertexCount == 24 * 48 * 6);
        assert(commands[1].mesh.vertexCount == 36);
        assert(commands[2].mesh.vertexCount == 12 * 10 * 6);
        assert(std::string_view(missingAssetsScene.activeModelName()) == "Sphere");

        bool recordedTextureFailure = false;
        bool recordedObjFailure = false;
        for (const sr::LoadDiagnostics::Entry& entry : missingAssetsScene.loadDiagnostics().entries()) {
            recordedTextureFailure = recordedTextureFailure || entry.kind == sr::LoadDiagnostics::Kind::Texture;
            recordedObjFailure = recordedObjFailure || entry.kind == sr::LoadDiagnostics::Kind::Obj;
        }
        assert(recordedTextureFailure);
        assert(recordedObjFailure);
    }

    std::filesystem::remove_all(sandbox);
}
