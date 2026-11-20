#pragma once
// The event script VM's actor state: the actors a script command has taken
// over and the cutscene skip state. ScriptActors.cpp defines them.

#include "Event/ScriptVm.hpp"

namespace VS::Event {
// Nonzero for each actor slot a script command controls.
// decomp: D_800F4B70 (D_800F4B70@BATTLE/BATTLE.PRG)
extern char scriptControlledActors[kScriptControlledActorCount];
// The CutsceneSkip state (ScriptVm.hpp) of the running event.
// decomp: scriptSkipMode (D_800F4C2C@BATTLE/BATTLE.PRG)
extern char skipState;
}  // namespace VS::Event
