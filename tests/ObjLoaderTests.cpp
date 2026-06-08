#include "renderer/ObjLoader.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

void runObjLoaderTests()
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "cpurasterizer_obj_loader_valid_test.obj";

    {
        std::ofstream file(path);
        file << "v 0 0 0\n";
        file << "v 1 0 0\n";
        file << "v 0 1 0\n";
        file << "vt 0 0\n";
        file << "vt 1 0\n";
        file << "vt 0 1\n";
        file << "vn 0 0 1\n";
        file << "f 1/1/1 2/2/1 3/3/1\n";
    }

    const std::vector<sr::Vertex> vertices = sr::ObjLoader::load(path, { false, 1.0f, { 7, 8, 9, 255 } });
    require(vertices.size() == 3, "Expected valid OBJ to load one triangle.");
    require(vertices[0].color.r == 7, "Expected OBJ loader to preserve configured vertex color.");
    require(vertices[1].normal.z > 0.99f, "Expected OBJ loader to parse vertex normal.");

    std::filesystem::remove(path);

    const std::filesystem::path invalidPath = std::filesystem::temp_directory_path() / "cpurasterizer_obj_loader_invalid_token_test.obj";

    {
        std::ofstream file(invalidPath);
        file << "v 0 0 0\n";
        file << "v 1 0 0\n";
        file << "v 0 1 0\n";
        file << "f 1 nope 3\n";
    }

    try {
        (void)sr::ObjLoader::load(invalidPath, { false });
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        std::filesystem::remove(invalidPath);
        require(message.find(invalidPath.string()) != std::string::npos, "Expected invalid face token error to include OBJ path.");
        require(message.find(":4") != std::string::npos, "Expected invalid face token error to include line number.");
        return;
    }

    std::filesystem::remove(invalidPath);
    throw std::runtime_error("Expected invalid face token to throw std::runtime_error.");
}
