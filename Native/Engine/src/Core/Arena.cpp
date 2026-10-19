#include "Core/Arena.h"

namespace {
alignas(64) unsigned char g_gameHeap[CORE_GAME_HEAP_SIZE];
}

extern "C" {
alignas(64) unsigned char Core_overlayArena[CORE_OVERLAY_SIZE];
}

extern "C" void* Core_GameHeapArena(void) { return g_gameHeap; }
