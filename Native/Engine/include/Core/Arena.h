#pragma once

// The memory the game's own heap (Core_Memory_*Heap, copied from the decomp)
// manages. On the PS1 it was a fixed RAM range (0x8010C000, 0xF2000 bytes);
// natively it is one aligned block owned by Core. It is larger than the
// original because the heap's block headers and the structs allocated from it
// hold 8-byte pointers on x64.

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_GAME_HEAP_SIZE (16u * 1024u * 1024u)

void* Core_GameHeapArena(void);

// The RAM the PS1 loaded overlay modules into (0x80068800 up to the heap at
// 0x8010C000). The modules are compiled in natively, but the game also uses the
// slots as load buffers (effect data, menu data) and some slots overlap, so the
// slots keep their original offsets inside one block.
#define CORE_OVERLAY_BASE 0x80068800u
#define CORE_OVERLAY_SIZE (0x8010C000u - CORE_OVERLAY_BASE)
extern unsigned char Core_overlayArena[CORE_OVERLAY_SIZE];
#define Core_OverlayAddress(psxAddress) \
  ((void*)(Core_overlayArena + ((psxAddress) - CORE_OVERLAY_BASE)))

#ifdef __cplusplus
}
#endif
