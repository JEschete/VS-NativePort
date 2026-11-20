#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live binding for the empty handlers (Runtime.hpp) whose original callers
// and function tables pass an actor's stats.
namespace VS::Battle {
struct ActorStats;
}  // namespace VS::Battle

namespace VS::Core {
// BATTLE.PRG vs_battle_nop0 with its original void(VS::Battle::ActorStats*)
// shape, for the copied callers and the module function table.
void IgnoreLiveActorCallback(VS::Battle::ActorStats* actor) noexcept;
}  // namespace VS::Core
