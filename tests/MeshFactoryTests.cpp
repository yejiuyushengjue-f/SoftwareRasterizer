#include "math/Math.h"
#include "scenes/MeshFactory.h"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

void assertTangentsAreValid(const std::vector<sr::Vertex>& vertices)
{
    for (const sr::Vertex& vertex : vertices) {
        assert(std::isfinite(vertex.tangent.x));
        assert(std::isfinite(vertex.tangent.y));
        assert(std::isfinite(vertex.tangent.z));
        assert(sr::dot(vertex.tangent, vertex.tangent) > 0.9f);
        assert(std::abs(sr::dot(vertex.normal, vertex.tangent)) < 0.0015f);
        assert(vertex.tangentSign == 1.0f || vertex.tangentSign == -1.0f);
    }
}

} // namespace

void runMeshFactoryTests()
{
    const std::vector<sr::Vertex> sphere = sr::MeshFactory::makeSphere(0.72f, 24, 48);
    const std::vector<sr::Vertex> cube = sr::MeshFactory::makeCube(1.35f);
    const std::vector<sr::Vertex> ground = sr::MeshFactory::makeGround();

    assert(sphere.size() == 24u * 48u * 6u);
    assert(cube.size() == 36u);
    assert(ground.size() == 12u * 10u * 6u);

    assertTangentsAreValid(sphere);
    assertTangentsAreValid(cube);
    assertTangentsAreValid(ground);
}
