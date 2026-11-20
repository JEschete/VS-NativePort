// The event script VM's audio state (ScriptAudio.hpp).
#include "Event/ScriptAudio.hpp"

namespace VS::Event {
// decomp: D_800F4C08 (D_800F4C08@BATTLE/BATTLE.PRG)
char isAmbientSoundSuspended;

// decomp: D_800F4C4C (D_800F4C4C@BATTLE/BATTLE.PRG)
std::int16_t suspendedAmbientSoundId;

// decomp: scriptMusicSlot (D_800F4C69@BATTLE/BATTLE.PRG)
char musicSlot;
}  // namespace VS::Event
