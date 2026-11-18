// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Overlays.h"

// decomp: vs_overlay_slots (vs_overlay_slots@SLUS_010.40)
void* const Core_overlaySlots[5] = { Core_OverlayAddress(0x80068800u), Core_OverlayAddress(0x800F9800u),
    Core_OverlayAddress(0x80102800u), Core_OverlayAddress(0x800F9800u), Core_OverlayAddress(0x800F9800u) };

