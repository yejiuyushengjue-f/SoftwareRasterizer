#include "renderer/RenderStats.h"

namespace sr {

double elapsedRenderMilliseconds(std::chrono::steady_clock::time_point begin, std::chrono::steady_clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

} // namespace sr
