#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live binding for the empty handlers (Runtime.hpp) whose original callers
// and function tables pass an actor's stats.
struct Battle_ActorStats;

namespace VS::Core {
// BATTLE.PRG vs_battle_nop0 with its original void(Battle_ActorStats*)
// shape, for the copied callers and the module function table.
void IgnoreLiveActorCallback(Battle_ActorStats* actor) noexcept;
}  // namespace VS::Core
