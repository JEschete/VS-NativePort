#pragma once

#include <cstdint>

struct Content_DiscQueueSlot;

namespace VS::Effects {

// decomp: D_800EB9B4_t (src/GIM/SCREFF2.PRG/0.h:5). The screen-effect
// controller BATTLE.PRG and SCREFF2.PRG share; ScreenOverlaysLive.cpp maps it
// to the typed ScreenOverlayLifetime. The queue slot is a native pointer, so
// the record is larger than the original 16 bytes.
struct LiveScreenEffectOverlay {
  // Set while SCREFF2.PRG loads; scripts wait for it to clear.
  std::uint8_t loading;
  // 1 while the feedback tiles hold their reserved texture pages.
  std::uint8_t feedback_pages_reserved;
  // Ordering-table layer of the feedback tiles; 0 turns them off.
  std::uint16_t feedback_layer;
  // Radial warp start scale (0x1000 is 1.0, read unsigned); with zero growth,
  // 0x1000 leaves the frame unwarped.
  std::int16_t radial_warp_scale;
  // Radial warp growth added to 1.0 for each step outward.
  std::int16_t radial_warp_growth;
  // Draws the vignette while nonzero.
  std::int32_t vignette_enabled;
  // Disc request that loads SCREFF2.PRG.
  Content_DiscQueueSlot* slot;
};

// decomp: D_800EB9B4 (src/GIM/SCREFF2.PRG/0.h:22). The attached controller,
// or null while no screen effect is active.
extern LiveScreenEffectOverlay* screenEffectOverlay;

// decomp: D_800F4ED8 (src/BATTLE/BATTLE.PRG/5BF94.c:425). The controller the
// overlay pointer attaches to.
extern LiveScreenEffectOverlay screenEffectDefaults;

}  // namespace VS::Effects
