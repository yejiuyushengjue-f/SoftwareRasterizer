#pragma once

#include "core/Color.h"
#include "math/Math.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sr {

inline Color grayscale(float value)
{
    if (!std::isfinite(value)) {
        value = 0.0f;
    }
    const std::uint8_t channel = static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
    return { channel, channel, channel, 255 };
}

inline Color normalToColor(Vec3 normal)
{
    const Vec3 n = normalize(normal);
    return {
        static_cast<std::uint8_t>(std::clamp(n.x * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f),
        static_cast<std::uint8_t>(std::clamp(n.y * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f),
        static_cast<std::uint8_t>(std::clamp(n.z * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f),
        255,
    };
}

inline Color uvToColor(Vec2 uv)
{
    if (!std::isfinite(uv.x) || !std::isfinite(uv.y)) {
        return { 255, 0, 255, 255 };
    }

    const auto repeat = [](float value) {
        const float wrapped = value - std::floor(value);
        return static_cast<std::uint8_t>(std::clamp(wrapped, 0.0f, 1.0f) * 255.0f);
    };

    return { repeat(uv.x), repeat(uv.y), 32, 255 };
}

} // namespace sr
