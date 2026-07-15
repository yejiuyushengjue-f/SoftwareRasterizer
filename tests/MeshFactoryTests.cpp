#include "math/Math.h"
#include "scenes/MeshFactory.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(std::string("MeshFactoryTests: ") + message);
    }
}

void requireTangentsAreValid(const std::vector<sr::Vertex>& vertices)
{
    for (const sr::Vertex& vertex : vertices) {
        require(std::isfinite(vertex.tangent.x), "tangent.x must be finite");
        require(std::isfinite(vertex.tangent.y), "tangent.y must be finite");
        require(std::isfinite(vertex.tangent.z), "tangent.z must be finite");
        require(sr::dot(vertex.tangent, vertex.tangent) > 0.9f, "tangent must be normalized");
        require(std::abs(sr::dot(vertex.normal, vertex.tangent)) < 0.0015f, "tangent must be orthogonal to normal");
        require(vertex.tangentSign == 1.0f || vertex.tangentSign == -1.0f, "tangent sign must be valid");
    }
}

} // namespace

void runMeshFactoryTests()
{
    const std::vector<sr::Vertex> sphere = sr::MeshFactory::makeSphere(0.72f, 24, 48);
    const std::vector<sr::Vertex> cube = sr::MeshFactory::makeCube(1.35f);
    const std::vector<sr::Vertex> box = sr::MeshFactory::makeBox({ 1.0f, 2.0f, 3.0f });
    const std::vector<sr::Vertex> panel = sr::MeshFactory::makePanel(9.0f, 8.0f);
    const std::vector<sr::Vertex> sculpture = sr::MeshFactory::makeShowcaseSculpture(1.42f, 0.22f, 0.34f, 0.16f, 14);
    const std::vector<sr::Vertex> ground = sr::MeshFactory::makeGround();

    require(sphere.size() == 24u * 48u * 6u, "sphere vertex count changed");
    require(cube.size() == 36u, "cube vertex count changed");
    require(box.size() == 36u, "box must contain six quads");
    require(panel.size() == 6u, "panel must contain one quad");
    require(sculpture.size() == 420u, "showcase sculpture vertex count changed");
    require(ground.size() == 12u * 10u * 6u, "ground vertex count changed");

    requireTangentsAreValid(sphere);
    requireTangentsAreValid(cube);
    requireTangentsAreValid(box);
    requireTangentsAreValid(panel);
    requireTangentsAreValid(sculpture);
    requireTangentsAreValid(ground);
}
