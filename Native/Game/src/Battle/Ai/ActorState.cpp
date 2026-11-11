// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"

// decomp: D_80069C18 (D_80069C18@BATTLE/BATTLE.PRG)
unsigned char Battle_Ai_kSettingsBits30Values[1284]; /* contents: module image (ModuleData.cpp) */

// decomp: D_80069C1C (D_80069C1C@BATTLE/BATTLE.PRG)
unsigned int Battle_Ai_kPerceptionRangesSquared[320]; /* contents: module image (ModuleData.cpp) */

// decomp: D_80069C3C (D_80069C3C@BATTLE/BATTLE.PRG)
Battle_Ai_WanderTiming Battle_Ai_kWanderTimingPresets[208]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F1790 (D_800F1790@BATTLE/BATTLE.PRG)
int (*Battle_Ai_decisionCallbacks[9])(Battle_Ai_ActorAiState*); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800F17B8 (D_800F17B8@BATTLE/BATTLE.PRG)
unsigned short Battle_Ai_viewAngles[45][2]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F17C8 (D_800F17C8@BATTLE/BATTLE.PRG)
unsigned short Battle_Ai_settingsBits8Values[41][2]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F17D8 (D_800F17D8@BATTLE/BATTLE.PRG)
unsigned char Battle_Ai_facingDelayScales[148]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F5878 (D_800F5878@BATTLE/BATTLE.PRG)
Battle_Ai_ActorAiState* Battle_Ai_actorAiStates[16];

// decomp: D_800F58BC (D_800F58BC@BATTLE/BATTLE.PRG)
Battle_Ai_RoomAiState* Battle_Ai_roomAiState;

// decomp: D_800F58C0 (D_800F58C0@BATTLE/BATTLE.PRG)
void* Battle_Ai_roomHasWaypoints;

// decomp: D_800F590C (D_800F590C@BATTLE/BATTLE.PRG)
int Battle_Ai_attributeActorMask;

// decomp: D_80069C08 (D_80069C08@BATTLE/BATTLE.PRG)
const unsigned char Battle_Ai_kFlags34Bit14Values[4] = { 9, 7, 0, 0 };

}  // extern "C"
