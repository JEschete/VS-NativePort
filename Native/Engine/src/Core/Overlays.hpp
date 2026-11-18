#pragma once
// The load address of each overlay slot (decomp: vs_overlay_slots), as
// pointers into the native workspace (Core::OverlayAddress).
#include <cstddef>

namespace VS::Core {
inline constexpr std::size_t kOverlaySlotCount = 5;

// decomp: vs_overlay_slots (vs_overlay_slots@SLUS_010.40)
extern void* const kOverlaySlots[kOverlaySlotCount];
}  // namespace VS::Core
