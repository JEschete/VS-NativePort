#pragma once

// The memory the game's own heap (Core_Memory_*Heap, copied from the decomp) manages. On the PS1 it was a fixed
// RAM range (0x8010C000, 0xF2000 bytes); natively it is one aligned block owned by Core. It is larger than the
// original because the heap's block headers and the structs allocated from it hold 8-byte pointers on x64.

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_GAME_HEAP_SIZE (16u * 1024u * 1024u)

void* Core_GameHeapArena(void);

#ifdef __cplusplus
}
#endif
