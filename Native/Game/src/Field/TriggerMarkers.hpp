#pragma once
// The VRAM sources of the animated trigger-marker texture and palette.
// TriggerMarkers.cpp defines them; the module image fills them at start-up and
// the marker animation moves their x to the current frame.

#include <cstddef>

#include "Render/DisplayControl.hpp"

namespace VS::Field {
// A libgpu RECT in the module image.
inline constexpr std::size_t kTriggerMarkerSourceBytes = 8;
static_assert(sizeof(Render::ScreenRect) == kTriggerMarkerSourceBytes);

// The texture frame copied to the marker's texture page.
// decomp: D_800E85BC (src/BATTLE/BATTLE.PRG/146C.c:312,
// D_800E85BC@BATTLE/BATTLE.PRG)
extern Render::ScreenRect triggerMarkerTextureSource;
// The palette strip copied to the marker's palette row.
// decomp: D_800E85C4 (src/BATTLE/BATTLE.PRG/146C.c:313,
// D_800E85C4@BATTLE/BATTLE.PRG)
extern Render::ScreenRect triggerMarkerPaletteSource;
}  // namespace VS::Field
