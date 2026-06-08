#pragma once

#include "renderer/RenderSettings.h"
#include "renderer/Vertex.h"

#include <span>

namespace sr {

struct RenderSceneView {
    std::span<const DrawCommand> drawCommands;
    const RenderSettings& settings;
};

} // namespace sr
