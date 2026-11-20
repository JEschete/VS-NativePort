#pragma once
// The event script VM's audio state: the room ambient sound an event
// suspended and the music slot its music commands address. ScriptAudio.cpp
// defines them.

#include <cstdint>

namespace VS::Event {
// Nonzero while an event has suspended the room's ambient sound.
// decomp: D_800F4C08 (D_800F4C08@BATTLE/BATTLE.PRG)
extern char isAmbientSoundSuspended;
// The ambient sound to restore when the event resumes it.
// decomp: D_800F4C4C (D_800F4C4C@BATTLE/BATTLE.PRG)
extern std::int16_t suspendedAmbientSoundId;
// The music slot the event music commands address.
// decomp: scriptMusicSlot (D_800F4C69@BATTLE/BATTLE.PRG)
extern char musicSlot;
}  // namespace VS::Event
