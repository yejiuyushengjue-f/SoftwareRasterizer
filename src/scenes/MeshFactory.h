#pragma once

#include "renderer/Vertex.h"

#include <vector>

namespace sr::MeshFactory {

std::vector<Vertex> makeSphere(float radius, int latitudeSegments, int longitudeSegments);
std::vector<Vertex> makeCube(float size);
std::vector<Vertex> makeGround();

} // namespace sr::MeshFactory
