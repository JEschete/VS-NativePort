// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Core/Arena.hpp"
#include "Core/Overlays.hpp"

// decomp: vs_overlay_slots (vs_overlay_slots@SLUS_010.40)
constinit void* const VS::Core::kOverlaySlots[VS::Core::kOverlaySlotCount] = { VS::Core::OverlayAddress(0x80068800u), VS::Core::OverlayAddress(0x800F9800u),
    VS::Core::OverlayAddress(0x80102800u), VS::Core::OverlayAddress(0x800F9800u), VS::Core::OverlayAddress(0x800F9800u) };