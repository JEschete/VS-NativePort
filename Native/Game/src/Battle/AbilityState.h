// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ABILITYSTATE_H_TYPES
#define VS_BATTLE_ABILITYSTATE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_ABILITYSTATE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_ABILITYSTATE_H_DECLS)
#define VS_BATTLE_ABILITYSTATE_H_DECLS
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

extern Battle_Actor* Battle_characterState;
extern u_short Battle_breakArtPointRequirements[15][4];
extern u_short Battle_battleAbilityPointRequirements[150];
extern short Battle_abilityInputResult;

#endif // VS_BATTLE_ABILITYSTATE_H_DECLS
