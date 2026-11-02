#pragma once

#include <span>

#include "Legacy/Psx.h"

extern "C" {
#include "Render/Images.h"
}

// The results screens' atlas draws over the current frame's primitive buffer
// and ordering table. The atlas and color arrays arrive as spans, so an
// out-of-range region or a short color pair draws nothing instead of reading
// past its array.
namespace VS::Render {
void DrawLiveTextureTintFade(std::span<const Render_TextureRegion> atlas, int x,
                             int y, int region_id,
                             std::span<const CVECTOR> colors);

void DrawLiveTextureFlashFade(std::span<const Render_TextureRegion> atlas,
                              int x, int y, int region_id,
                              std::span<const CVECTOR> colors);

void DrawLiveTextureWipe(std::span<const Render_TextureRegion> atlas, int x,
                         int y, int region_id, std::span<const CVECTOR> colors,
                         int wipe_edge_x);
}  // namespace VS::Render
