// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

// decomp: vs_battle_characterState (vs_battle_characterState@BATTLE/BATTLE.PRG)
Battle_Actor* Battle_characterState;

// decomp: vs_main_artsPointsRequirements (vs_main_artsPointsRequirements@SLUS_010.40)
u_short Battle_breakArtPointRequirements[15][4]; /* contents: module image (ModuleData.cpp) */

// decomp: vs_main_battleAbilitiesPointsRequirements (vs_main_battleAbilitiesPointsRequirements@SLUS_010.40)
u_short Battle_battleAbilityPointRequirements[150]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8005FFB0 (D_8005FFB0@SLUS_010.40)
short Battle_abilityInputResult;

}  // extern "C"
