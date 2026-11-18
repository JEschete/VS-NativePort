// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Arena.hpp"
#include "Core/Overlays.h"

// decomp: vs_overlay_slots (vs_overlay_slots@SLUS_010.40)
constinit void* const Core_overlaySlots[5] = { VS::Core::OverlayAddress(0x80068800u), VS::Core::OverlayAddress(0x800F9800u),
    VS::Core::OverlayAddress(0x80102800u), VS::Core::OverlayAddress(0x800F9800u), VS::Core::OverlayAddress(0x800F9800u) };

