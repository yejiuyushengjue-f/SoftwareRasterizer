#pragma once

#include "renderer/IRenderPass.h"

namespace sr {

class ShadowPass final : public IRenderPass {
public:
    void execute(RenderContext& context) override;
};

class PrepareGeometryPass final : public IRenderPass {
public:
    void execute(RenderContext& context) override;
};

class DepthPrepass final : public IRenderPass {
public:
    void execute(RenderContext& context) override;
};

class ColorPass final : public IRenderPass {
public:
    void execute(RenderContext& context) override;
};

} // namespace sr
