#include "scenes/AssetLocator.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

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

void touch(const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::binary);
    file << "test";
}

std::filesystem::path uniqueSandbox()
{
    return std::filesystem::temp_directory_path()
        / ("simple_renderer_asset_locator_tests_" + std::to_string(reinterpret_cast<std::uintptr_t>(&uniqueSandbox)));
}

} // namespace

void runAssetLocatorTests()
{
    const std::filesystem::path sandbox = uniqueSandbox();
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "res" / "Texture");
    std::filesystem::create_directories(sandbox / "res" / "Model");
    std::filesystem::create_directories(sandbox / "build" / "tests");

    const std::filesystem::path texture = sandbox / "res" / "Texture" / "albedo.fake";
    const std::filesystem::path model = sandbox / "res" / "Model" / "mesh.obj";
    touch(texture);
    touch(model);

    {
        ScopedCurrentPath scopedPath(sandbox / "build" / "tests");

        assert(sr::AssetLocator::findTexture("albedo.fake") == texture);
        assert(sr::AssetLocator::findModel("mesh.obj") == model);
        assert(sr::AssetLocator::findFirstModelWithExtension(".obj") == model);

        assert(sr::AssetLocator::findTexture("missing.fake").empty());
        assert(sr::AssetLocator::findModel("missing.obj").empty());
        assert(sr::AssetLocator::findFirstModelWithExtension(".fbx").empty());
    }

    std::filesystem::remove_all(sandbox);
}
