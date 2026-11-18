#pragma once

#include "Actors/WeaponModel.h"

namespace VS::Actors {

// vs_battle_actor4, BATTLE/BATTLE.PRG/146C.h:440: runtime model storage, not
// disc data.
struct ActorModelSlot {
  Actors_WeaponModel weapon;
};

}  // namespace VS::Actors
