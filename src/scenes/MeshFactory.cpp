#include "scenes/MeshFactory.h"

#include "math/Math.h"

#include <cstddef>

namespace sr::MeshFactory {

namespace {

Vertex makeVertex(Vec3 position, Vec2 uv, Vec3 normal)
{
    return { position, uv, normalize(normal), { 255, 255, 255, 255 } };
}

Vertex makeVertex(Vec3 position, Vec2 uv, Vec3 normal, Color color)
{
    return { position, uv, normalize(normal), color };
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

} // namespace

std::vector<Vertex> makeSphere(float radius, int latitudeSegments, int longitudeSegments)
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

std::vector<Vertex> makeCube(float size)
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

std::vector<Vertex> makeGround()
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

} // namespace sr::MeshFactory
