#include "scenes/MeshFactory.h"

#include "math/Math.h"

#include <algorithm>
#include <array>
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

void addQuad(std::vector<Vertex>& vertices, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 normal, Vec2 uvMin, Vec2 uvMax)
{
    vertices.push_back(makeVertex(a, { uvMin.x, uvMin.y }, normal));
    vertices.push_back(makeVertex(b, { uvMax.x, uvMin.y }, normal));
    vertices.push_back(makeVertex(c, { uvMax.x, uvMax.y }, normal));

    vertices.push_back(makeVertex(a, { uvMin.x, uvMin.y }, normal));
    vertices.push_back(makeVertex(c, { uvMax.x, uvMax.y }, normal));
    vertices.push_back(makeVertex(d, { uvMin.x, uvMax.y }, normal));
}

void addTriangleFan(std::vector<Vertex>& vertices, Vec3 center, float y, float radius, int segments, bool topCap)
{
    const Vec3 normal = topCap ? Vec3 { 0.0f, 1.0f, 0.0f } : Vec3 { 0.0f, -1.0f, 0.0f };
    const float direction = topCap ? 1.0f : -1.0f;
    const Vertex centerVertex = makeVertex(center, { 0.5f, 0.5f }, normal);

    for (int i = 0; i < segments; ++i) {
        const float u0 = static_cast<float>(i) / static_cast<float>(segments);
        const float u1 = static_cast<float>(i + 1) / static_cast<float>(segments);
        const float angle0 = u0 * 2.0f * pi;
        const float angle1 = u1 * 2.0f * pi;
        const Vec3 p0 { std::cos(angle0) * radius, y, std::sin(angle0) * radius };
        const Vec3 p1 { std::cos(angle1) * radius, y, std::sin(angle1) * radius };
        const Vertex edge0 = makeVertex(
            p0,
            { 0.5f + std::cos(angle0) * 0.5f, 0.5f + std::sin(angle0) * 0.5f * direction },
            normal);
        const Vertex edge1 = makeVertex(
            p1,
            { 0.5f + std::cos(angle1) * 0.5f, 0.5f + std::sin(angle1) * 0.5f * direction },
            normal);

        vertices.push_back(centerVertex);
        vertices.push_back(topCap ? edge1 : edge0);
        vertices.push_back(topCap ? edge0 : edge1);
    }
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
    return makeBox({ size, size, size });
}

std::vector<Vertex> makeBox(Vec3 size)
{
    const float hx = size.x * 0.5f;
    const float hy = size.y * 0.5f;
    const float hz = size.z * 0.5f;
    std::vector<Vertex> vertices;
    vertices.reserve(36);

    addQuad(vertices, { -hx, -hy, hz }, { hx, -hy, hz }, { hx, hy, hz }, { -hx, hy, hz }, { 0.0f, 0.0f, 1.0f });
    addQuad(vertices, { hx, -hy, -hz }, { -hx, -hy, -hz }, { -hx, hy, -hz }, { hx, hy, -hz }, { 0.0f, 0.0f, -1.0f });
    addQuad(vertices, { -hx, -hy, -hz }, { -hx, -hy, hz }, { -hx, hy, hz }, { -hx, hy, -hz }, { -1.0f, 0.0f, 0.0f });
    addQuad(vertices, { hx, -hy, hz }, { hx, -hy, -hz }, { hx, hy, -hz }, { hx, hy, hz }, { 1.0f, 0.0f, 0.0f });
    addQuad(vertices, { -hx, hy, hz }, { hx, hy, hz }, { hx, hy, -hz }, { -hx, hy, -hz }, { 0.0f, 1.0f, 0.0f });
    addQuad(vertices, { -hx, -hy, -hz }, { hx, -hy, -hz }, { hx, -hy, hz }, { -hx, -hy, hz }, { 0.0f, -1.0f, 0.0f });

    assignMeshTangents(vertices.data(), static_cast<int>(vertices.size()));
    return vertices;
}

std::vector<Vertex> makePanel(float width, float height)
{
    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    std::vector<Vertex> vertices;
    vertices.reserve(6);

    addQuad(
        vertices,
        { -halfWidth, -halfHeight, 0.0f },
        { halfWidth, -halfHeight, 0.0f },
        { halfWidth, halfHeight, 0.0f },
        { -halfWidth, halfHeight, 0.0f },
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f },
        { width * 0.25f, height * 0.25f });

    assignMeshTangents(vertices.data(), static_cast<int>(vertices.size()));
    return vertices;
}

std::vector<Vertex> makeShowcaseSculpture(float height, float baseRadius, float bodyRadius, float neckRadius, int segments)
{
    const int safeSegments = std::max(3, segments);
    const float shoulderRadius = bodyRadius * 0.72f;
    const float shoulderHeight = height * 0.64f;
    const float bodyHeight = height * 0.28f;
    const float neckHeight = height * 0.82f;
    const std::array<float, 5> ringHeights { 0.0f, bodyHeight, shoulderHeight, neckHeight, height };
    const std::array<float, 5> ringRadii { baseRadius, bodyRadius, shoulderRadius, neckRadius, neckRadius * 0.84f };

    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<std::size_t>(safeSegments) * (ringHeights.size() - 1u) * 6u + static_cast<std::size_t>(safeSegments) * 6u);

    for (std::size_t ring = 0; ring + 1 < ringHeights.size(); ++ring) {
        for (int segment = 0; segment < safeSegments; ++segment) {
            const float u0 = static_cast<float>(segment) / static_cast<float>(safeSegments);
            const float u1 = static_cast<float>(segment + 1) / static_cast<float>(safeSegments);
            const float angle0 = u0 * 2.0f * pi;
            const float angle1 = u1 * 2.0f * pi;

            const float y0 = ringHeights[ring];
            const float y1 = ringHeights[ring + 1];
            const float r0 = ringRadii[ring];
            const float r1 = ringRadii[ring + 1];
            const float slope = (r0 - r1) / std::max(0.0001f, y1 - y0);

            const Vec3 normal0 = normalize({ std::cos(angle0), slope, std::sin(angle0) });
            const Vec3 normal1 = normalize({ std::cos(angle1), slope, std::sin(angle1) });

            const Vec3 a { std::cos(angle0) * r0, y0, std::sin(angle0) * r0 };
            const Vec3 b { std::cos(angle1) * r0, y0, std::sin(angle1) * r0 };
            const Vec3 c { std::cos(angle1) * r1, y1, std::sin(angle1) * r1 };
            const Vec3 d { std::cos(angle0) * r1, y1, std::sin(angle0) * r1 };

            vertices.push_back(makeVertex(a, { u0, y0 / height }, normal0));
            vertices.push_back(makeVertex(c, { u1, y1 / height }, normal1));
            vertices.push_back(makeVertex(b, { u1, y0 / height }, normal1));

            vertices.push_back(makeVertex(a, { u0, y0 / height }, normal0));
            vertices.push_back(makeVertex(d, { u0, y1 / height }, normal0));
            vertices.push_back(makeVertex(c, { u1, y1 / height }, normal1));
        }
    }

    addTriangleFan(vertices, { 0.0f, 0.0f, 0.0f }, 0.0f, baseRadius, safeSegments, false);
    addTriangleFan(vertices, { 0.0f, height, 0.0f }, height, ringRadii.back(), safeSegments, true);

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
