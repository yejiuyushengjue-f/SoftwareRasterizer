#include "renderer/RenderPasses.h"

#include "renderer/RenderContext.h"

namespace sr {

void ShadowPass::execute(RenderContext& context)
{
    if (context.shadowPass) {
        context.shadowPass(context);
    }
}

void PrepareGeometryPass::execute(RenderContext& context)
{
    if (context.prepareGeometryPass) {
        context.prepareGeometryPass(context);
    }
}

void DepthPrepass::execute(RenderContext& context)
{
    if (context.depthPrepass) {
        context.depthPrepass(context);
    }
}

void ColorPass::execute(RenderContext& context)
{
    if (context.colorPass) {
        context.colorPass(context);
    }
}

} // namespace sr
