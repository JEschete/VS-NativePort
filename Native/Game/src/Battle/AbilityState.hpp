#pragma once
// The player's mutable battle actor. Learning definitions live in Content.

namespace VS::Battle {

struct Actor;

// decomp: vs_battle_characterState (src/BATTLE/BATTLE.PRG/actor.h:173)
extern VS::Battle::Actor* characterState;

}  // namespace VS::Battle
