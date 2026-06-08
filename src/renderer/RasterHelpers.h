#pragma once

#include "math/Math.h"

namespace sr {

inline float rasterEdge(Vec2 a, Vec2 b, Vec2 p)
{
    return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
}

struct BarycentricWeights {
    float w0 = 0.0f;
    float w1 = 0.0f;
    float w2 = 0.0f;
};

inline BarycentricWeights barycentricWeights(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 sample, float area)
{
    return {
        rasterEdge(p1, p2, sample) / area,
        rasterEdge(p2, p0, sample) / area,
        rasterEdge(p0, p1, sample) / area,
    };
}

inline bool isInsideTriangle(BarycentricWeights weights)
{
    return weights.w0 >= 0.0f && weights.w1 >= 0.0f && weights.w2 >= 0.0f;
}

} // namespace sr
