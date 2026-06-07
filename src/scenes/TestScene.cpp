#include "scenes/TestScene.h"

#include "math/Math.h"
#include "renderer/ObjLoader.h"

#include <filesystem>
#include <stdexcept>

namespace sr {

namespace {

std::vector<Vertex> makeSphereMesh(float radius, int latitudeSegments, int longitudeSegments);
std::vector<Vertex> makeGroundMesh();

Texture loadTextureOrCheckerboard(const wchar_t* fileName, int checkerCells)
{
    const std::filesystem::path file(fileName);
    const std::filesystem::path candidates[] = {
        std::filesystem::path(L"res") / L"Texture" / file,
        std::filesystem::path(L"..") / L"res" / L"Texture" / file,
        std::filesystem::path(L"..") / L".." / L"res" / L"Texture" / file,
        std::filesystem::path(L"..") / L".." / L".." / L"res" / L"Texture" / file,
    };

    for (const std::filesystem::path& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            try {
                return Texture::loadFromFile(candidate.c_str());
            } catch (const std::runtime_error&) {
            }
        }
    }

    return Texture::makeCheckerboard(128, 128, checkerCells);
}

std::filesystem::path findFirstObjFile()
{
    const std::filesystem::path candidates[] = {
        std::filesystem::path(L"res") / L"Model",
        std::filesystem::path(L"res") / L"Models",
        std::filesystem::path(L"..") / L"res" / L"Model",
        std::filesystem::path(L"..") / L"res" / L"Models",
        std::filesystem::path(L"..") / L".." / L"res" / L"Model",
        std::filesystem::path(L"..") / L".." / L"res" / L"Models",
        std::filesystem::path(L"..") / L".." / L".." / L"res" / L"Model",
        std::filesystem::path(L"..") / L".." / L".." / L"res" / L"Models",
    };

    for (const std::filesystem::path& directory : candidates) {
        if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
            continue;
        }

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
            const std::filesystem::path extension = entry.path().extension();
            if (entry.is_regular_file() && (extension == L".obj" || extension == L".OBJ")) {
                return entry.path();
            }
        }
    }

    return {};
}

std::vector<Vertex> loadObjModel(bool& objModelAvailable)
{
    const std::filesystem::path objPath = findFirstObjFile();
    if (!objPath.empty()) {
        try {
            objModelAvailable = true;
            return ObjLoader::load(objPath, { true, 0.95f, { 255, 255, 255, 255 } });
        } catch (const std::runtime_error&) {
        }
    }

    objModelAvailable = false;
    return {};
}

Vertex makeVertex(Vec3 position, Vec2 uv, Vec3 normal)
{
    return { position, uv, normalize(normal), { 255, 255, 255, 255 } };
}

Vertex makeVertex(Vec3 position, Vec2 uv, Vec3 normal, Color color)
{
    return { position, uv, normalize(normal), color };
}

Material makeMaterial(
    const Texture* diffuseTexture,
    Color ambientColor,
    Color diffuseColor,
    Color specularColor,
    float ambientStrength,
    float diffuseStrength,
    float specularStrength,
    float shininess,
    const Texture* normalTexture = nullptr,
    float normalStrength = 1.0f)
{
    Material material;
    material.ambientColor = ambientColor;
    material.diffuseColor = diffuseColor;
    material.specularColor = specularColor;
    material.diffuseTexture = diffuseTexture;
    material.normalTexture = normalTexture;
    material.ambientStrength = ambientStrength;
    material.diffuseStrength = diffuseStrength;
    material.specularStrength = specularStrength;
    material.shininess = shininess;
    material.normalStrength = normalStrength;
    return material;
}

std::vector<Vertex> makeSphereMesh(float radius, int latitudeSegments, int longitudeSegments)
{
    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<std::size_t>(latitudeSegments) * static_cast<std::size_t>(longitudeSegments) * 6);

    const auto point = [radius](float u, float v) {
        const float phi = u * 2.0f * pi;
        const float theta = v * pi;
        const float sinTheta = std::sin(theta);
        const Vec3 normal {
            sinTheta * std::cos(phi),
            std::cos(theta),
            sinTheta * std::sin(phi),
        };
        return makeVertex(normal * radius, { u, 1.0f - v }, normal);
    };

    for (int y = 0; y < latitudeSegments; ++y) {
        const float v0 = static_cast<float>(y) / static_cast<float>(latitudeSegments);
        const float v1 = static_cast<float>(y + 1) / static_cast<float>(latitudeSegments);

        for (int x = 0; x < longitudeSegments; ++x) {
            const float u0 = static_cast<float>(x) / static_cast<float>(longitudeSegments);
            const float u1 = static_cast<float>(x + 1) / static_cast<float>(longitudeSegments);

            const Vertex p00 = point(u0, v0);
            const Vertex p10 = point(u1, v0);
            const Vertex p01 = point(u0, v1);
            const Vertex p11 = point(u1, v1);

            vertices.push_back(p00);
            vertices.push_back(p01);
            vertices.push_back(p11);

            vertices.push_back(p00);
            vertices.push_back(p11);
            vertices.push_back(p10);
        }
    }

    assignMeshTangents(vertices.data(), static_cast<int>(vertices.size()));
    return vertices;
}

void addQuad(std::vector<Vertex>& vertices, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 normal)
{
    vertices.push_back(makeVertex(a, { 0.0f, 0.0f }, normal));
    vertices.push_back(makeVertex(b, { 1.0f, 0.0f }, normal));
    vertices.push_back(makeVertex(c, { 1.0f, 1.0f }, normal));

    vertices.push_back(makeVertex(a, { 0.0f, 0.0f }, normal));
    vertices.push_back(makeVertex(c, { 1.0f, 1.0f }, normal));
    vertices.push_back(makeVertex(d, { 0.0f, 1.0f }, normal));
}

std::vector<Vertex> makeCubeMesh(float size)
{
    const float h = size * 0.5f;
    std::vector<Vertex> vertices;
    vertices.reserve(36);

    addQuad(vertices, { -h, -h, h }, { h, -h, h }, { h, h, h }, { -h, h, h }, { 0.0f, 0.0f, 1.0f });
    addQuad(vertices, { h, -h, -h }, { -h, -h, -h }, { -h, h, -h }, { h, h, -h }, { 0.0f, 0.0f, -1.0f });
    addQuad(vertices, { -h, -h, -h }, { -h, -h, h }, { -h, h, h }, { -h, h, -h }, { -1.0f, 0.0f, 0.0f });
    addQuad(vertices, { h, -h, h }, { h, -h, -h }, { h, h, -h }, { h, h, h }, { 1.0f, 0.0f, 0.0f });
    addQuad(vertices, { -h, h, h }, { h, h, h }, { h, h, -h }, { -h, h, -h }, { 0.0f, 1.0f, 0.0f });
    addQuad(vertices, { -h, -h, -h }, { h, -h, -h }, { h, -h, h }, { -h, -h, h }, { 0.0f, -1.0f, 0.0f });

    assignMeshTangents(vertices.data(), static_cast<int>(vertices.size()));
    return vertices;
}

std::vector<Vertex> makeGroundMesh()
{
    constexpr int columns = 12;
    constexpr int rows = 10;
    const float halfWidth = 5.4f;
    const float y = -1.04f;
    const float nearZ = -1.65f;
    const float farZ = -8.85f;
    const float cellWidth = (halfWidth * 2.0f) / static_cast<float>(columns);
    const float cellDepth = (nearZ - farZ) / static_cast<float>(rows);
    const Vec3 normal { 0.0f, 1.0f, 0.0f };

    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows) * 6);

    for (int row = 0; row < rows; ++row) {
        const float z0 = nearZ - static_cast<float>(row) * cellDepth;
        const float z1 = nearZ - static_cast<float>(row + 1) * cellDepth;
        for (int column = 0; column < columns; ++column) {
            const float x0 = -halfWidth + static_cast<float>(column) * cellWidth;
            const float x1 = -halfWidth + static_cast<float>(column + 1) * cellWidth;
            const bool dark = ((row + column) % 2) == 0;
            const Color color = dark ? Color { 55, 68, 76, 255 } : Color { 226, 232, 216, 255 };
            const float u0 = static_cast<float>(column);
            const float u1 = static_cast<float>(column + 1);
            const float v0 = static_cast<float>(row);
            const float v1 = static_cast<float>(row + 1);

            const Vertex a = makeVertex({ x0, y, z0 }, { u0, v0 }, normal, color);
            const Vertex b = makeVertex({ x1, y, z0 }, { u1, v0 }, normal, color);
            const Vertex c = makeVertex({ x1, y, z1 }, { u1, v1 }, normal, color);
            const Vertex d = makeVertex({ x0, y, z1 }, { u0, v1 }, normal, color);

            vertices.push_back(a);
            vertices.push_back(b);
            vertices.push_back(c);
            vertices.push_back(a);
            vertices.push_back(c);
            vertices.push_back(d);
        }
    }

    assignMeshTangents(vertices.data(), static_cast<int>(vertices.size()));
    return vertices;
}

} // namespace

TestScene::TestScene()
    : modelTexture_(loadTextureOrCheckerboard(L"Frosted Metal Texture.jpeg", 10))
    , cubeTexture_(loadTextureOrCheckerboard(L"Cobblestone_pavement_texture.jpeg", 8))
    , normalTexture_(loadTextureOrCheckerboard(L"Cobblestone_pavement_normal_texture.png", 8))
    , modelMaterial_(makeMaterial(&modelTexture_, { 210, 214, 220, 255 }, { 245, 245, 248, 255 }, { 245, 245, 255, 255 }, 0.23f, 0.95f, 0.38f, 36.0f))
    , cubeMaterial_(makeMaterial(&cubeTexture_, { 170, 176, 184, 255 }, { 210, 220, 232, 255 }, { 235, 232, 220, 255 }, 0.20f, 0.9f, 0.42f, 42.0f, &normalTexture_, 0.28f))
    , groundMaterial_(makeMaterial(nullptr, { 205, 210, 205, 255 }, { 240, 244, 236, 255 }, { 70, 76, 84, 255 }, 0.34f, 0.96f, 0.02f, 10.0f))
    , sphereMesh_(makeSphereMesh(0.72f, 24, 48))
    , objMesh_(loadObjModel(objModelAvailable_))
    , cubeMesh_(makeCubeMesh(1.35f))
    , groundMesh_(makeGroundMesh())
{
    commands_[0].material = modelMaterial_;
    applyActiveModel();
    commands_[1].mesh = Mesh { cubeMesh_.data(), static_cast<int>(cubeMesh_.size()) };
    commands_[1].material = cubeMaterial_;
    commands_[1].castsShadow = true;
    commands_[2].mesh = Mesh { groundMesh_.data(), static_cast<int>(groundMesh_.size()) };
    commands_[2].material = groundMaterial_;
    commands_[2].castsShadow = false;
}

void TestScene::toggleModel()
{
    if (usingObjModel_) {
        usingObjModel_ = false;
    } else if (objModelAvailable_) {
        usingObjModel_ = true;
    }

    applyActiveModel();
}

const char* TestScene::activeModelName() const
{
    return usingObjModel_ ? "Linnea" : "Sphere";
}

void TestScene::applyActiveModel()
{
    const std::vector<Vertex>& activeMesh = usingObjModel_ && objModelAvailable_ ? objMesh_ : sphereMesh_;
    if (activeMesh.empty()) {
        commands_[0].mesh = {};
        commands_[0].castsShadow = false;
        return;
    }

    commands_[0].mesh = Mesh { activeMesh.data(), static_cast<int>(activeMesh.size()) };
    commands_[0].castsShadow = !usingObjModel_;
}

void TestScene::update(float deltaSeconds)
{
    rotation_ += deltaSeconds;
    commands_[0].transform = Mat4::translation({ -0.52f, 0.02f, -2.25f })
        * Mat4::rotationY(rotation_ * (usingObjModel_ ? 0.45f : 0.65f))
        * Mat4::rotationX(usingObjModel_ ? 0.0f : std::sin(rotation_ * 0.7f) * 0.18f);
    commands_[1].transform = Mat4::translation({ 0.58f, -0.02f, -3.75f })
        * Mat4::rotationY(-rotation_ * 0.45f)
        * Mat4::rotationX(0.45f)
        * Mat4::rotationZ(0.18f);
    commands_[2].transform = Mat4::identity();
}

} // namespace sr
