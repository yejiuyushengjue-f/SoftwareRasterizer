#pragma once

namespace sr {

struct RenderContext;

class IRenderPass {
public:
    virtual ~IRenderPass() = default;
    virtual void execute(RenderContext& context) = 0;
};

} // namespace sr
