#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live bindings of the queued actor commands and Ashley's movement input
// (ActorCommands.hpp) to the battle actor table, the actor records, the
// camera and the controller state.
namespace VS::Actors {
// decomp: BATTLE.PRG func_8006E7F0 (src/BATTLE/BATTLE.PRG/146C.c:2837)
void UpdateLiveAshleyMovementInput();
// decomp: BATTLE.PRG func_80075554 (src/BATTLE/BATTLE.PRG/146C.c:5629)
// Returns the first nonzero trap or fall result.
int ExecuteLiveQueuedActorCommands();
}  // namespace VS::Actors
