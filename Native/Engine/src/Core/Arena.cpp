#include "Core/Arena.h"

namespace {
constexpr unsigned kArenaAlignment = 64;
alignas(kArenaAlignment) unsigned char g_game_heap[CORE_GAME_HEAP_SIZE];
}  // namespace

alignas(kArenaAlignment) unsigned char Core_overlayArena[CORE_OVERLAY_SIZE];

void* Core_GameHeapArena(void) { return g_game_heap; }
