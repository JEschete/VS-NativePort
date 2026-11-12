// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

// decomp: D_800E82F4 (D_800E82F4@BATTLE/BATTLE.PRG)
u_short Battle_bladePpDamageBonuses[12]; /* contents: module image (ModuleData.cpp) */

// decomp: _statCalculators (_statCalculators@BATTLE/BATTLE.PRG)
short (*Battle_statCalculators[27])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: vs_battle_actors (vs_battle_actors@BATTLE/BATTLE.PRG)
Battle_Actor* Battle_actors[42];

// decomp: _lastValue (_lastValue@BATTLE/BATTLE.PRG)
u_int Battle_lastActionStatValue;

// decomp: D_800F1A0C (D_800F1A0C@BATTLE/BATTLE.PRG)
int Battle_criticalHitOccurred;

// decomp: _armorDpAdjustmentAmounts (_armorDpAdjustmentAmounts@BATTLE/BATTLE.PRG)
short Battle_limbArmorDpLoss[8];

}  // extern "C"
