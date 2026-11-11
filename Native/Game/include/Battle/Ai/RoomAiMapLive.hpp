#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/RoomAiMap.hpp"

// Binding from the battle runtime's room AI storage to the typed room AI
// lifecycle (RoomAiMap.cpp).
namespace VS::Battle::Ai {
// Initializes the current room's AI navigation (func_800E6158). The battle
// module's pointer table stores this binding (0x800E6158). Aborts on an
// invalid room header.
void InitializeLiveRoomNavigation();
}  // namespace VS::Battle::Ai
