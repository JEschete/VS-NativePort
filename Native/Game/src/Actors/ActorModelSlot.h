#pragma once

#include "Actors/WeaponModel.h"

// vs_battle_actor4, BATTLE/BATTLE.PRG/146C.h:440: runtime model storage, not
// disc data.
typedef struct Actors_ActorModelSlot {
  Actors_WeaponModel weapon;
} Actors_ActorModelSlot;
