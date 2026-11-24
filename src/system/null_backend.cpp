#include "null_backend.h"

namespace GFX
{

// Empties the passes' lists unseen, as drawing them would
void NullBackend::Rasterize(TRenderQueue &queue, uint32_t passes)
{
    if (passes & RASTER_SKY)
        queue.SkyBox.clear();

    if (passes & RASTER_SOLID)
        queue.Solid.clear();

    if (passes & RASTER_ZEROTR)
        queue.ZeroTracy.clear();

    if (passes & RASTER_LUMATR)
        queue.LumaTracy.clear();
}

}
