#pragma once
// The room event trigger state: whether the queued event was requested
// directly and whether the room's entry script still waits to run.
// Triggers.cpp defines them.

#include <cstdint>

namespace VS::Event {
// Nonzero when the queued event was requested directly, not by a room trigger.
// decomp: D_800F4BA8 (D_800F4BA8@BATTLE/BATTLE.PRG)
extern char isDirectEventRequest;
// Nonzero while the room's entry script waits to run.
// decomp: mpdScriptSection (_mpdScriptSection@BATTLE/BATTLE.PRG)
extern std::int16_t hasRoomEntryScript;
}  // namespace VS::Event
