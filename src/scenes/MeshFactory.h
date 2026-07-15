#pragma once

#include "renderer/Vertex.h"

#include <vector>

namespace sr::MeshFactory {

std::vector<Vertex> makeSphere(float radius, int latitudeSegments, int longitudeSegments);
std::vector<Vertex> makeCube(float size);
std::vector<Vertex> makeBox(Vec3 size);
std::vector<Vertex> makePanel(float width, float height);
std::vector<Vertex> makeShowcaseSculpture(float height, float baseRadius, float bodyRadius, float neckRadius, int segments);
std::vector<Vertex> makeGround();

} // namespace sr::MeshFactory
