#pragma once
// The event script VM's effect state: the scanline overlay's frame and the
// pending effect wait. ScriptEffects.cpp defines them.

#include <cstdint>

namespace VS::Event {
// The frame the event scanline overlay draws next.
// decomp: D_800F4B20 (D_800F4B20@BATTLE/BATTLE.PRG)
extern std::uint16_t scanlineOverlayFrame;
// Nonzero while a script waits for its effect to reach its trigger.
// decomp: D_800F4BB9 (D_800F4BB9@BATTLE/BATTLE.PRG)
extern std::uint8_t isEffectWaitPending;
}  // namespace VS::Event
