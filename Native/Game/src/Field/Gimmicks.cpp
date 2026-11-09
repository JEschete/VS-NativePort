// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/TriggerMarkers.h"
#include "Field/Zone.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Render/Palette.h"

// decomp: D_800E856C (D_800E856C@BATTLE/BATTLE.PRG)
MATRIX Field_triggerMarkerMatrix; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8594 (D_800E8594@BATTLE/BATTLE.PRG)
Field_TriggerMarkerVertex Field_triggerMarkerVertex; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E85A0 (D_800E85A0@BATTLE/BATTLE.PRG)
char Field_triggerMarkerRecordTail[192]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8FA8 (D_800E8FA8@BATTLE/BATTLE.PRG)
u_char Field_gimmickPaletteParameters[32]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8FB4 (D_800E8FB4@BATTLE/BATTLE.PRG)
void* Field_gimmickModelTemplates[5]; /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800E8FD0 (D_800E8FD0@BATTLE/BATTLE.PRG)
u_char Field_kGimmickRollOrientations[3424]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9030 (D_800E9030@BATTLE/BATTLE.PRG)
u_char Field_kGimmickOrientationRotations[3328]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E90A8 (D_800E90A8@BATTLE/BATTLE.PRG)
int Field_interactingGimmickId; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E919C (D_800E919C@BATTLE/BATTLE.PRG)
u_char Field_gimmickTemplateIndices[2964]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E91AC (D_800E91AC@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSpinHeightSteps[2948]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E91BC (D_800E91BC@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSpinPitchSteps[2932]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E91CC (D_800E91CC@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSpinYawSteps[2916]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E91DC (D_800E91DC@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSpinRollSteps[2900]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E91EC (D_800E91EC@BATTLE/BATTLE.PRG)
u_char Field_kGimmickBounceHeightSteps[2884]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E91F0 (D_800E91F0@BATTLE/BATTLE.PRG)
u_char Field_kGimmickTiltDepthSteps[2880]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E91FC (D_800E91FC@BATTLE/BATTLE.PRG)
u_char Field_kGimmickTiltAngleSteps[2868]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9208 (D_800E9208@BATTLE/BATTLE.PRG)
u_char Field_kGimmickRollHeightSteps[2856]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9218 (D_800E9218@BATTLE/BATTLE.PRG)
u_char Field_kGimmickRollDisplacementSteps[2840]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9228 (D_800E9228@BATTLE/BATTLE.PRG)
u_char Field_kGimmickRollAngleSteps[2824]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9238 (D_800E9238@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSquashHeightSteps[2808]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9248 (D_800E9248@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSquashScaleX[2792]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9258 (D_800E9258@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSquashScaleY[2776]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9268 (D_800E9268@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSquashScaleZ[2760]; /* contents: module image (ModuleData.cpp) */

// decomp: vs_battle_roomData (vs_battle_roomData@BATTLE/BATTLE.PRG)
Field_RoomRecord Field_roomData;

// decomp: D_800F1CDC (D_800F1CDC@BATTLE/BATTLE.PRG)
u_short Field_roomAnimationTicks;

// decomp: D_800F1D28 (D_800F1D28@BATTLE/BATTLE.PRG)
Field_GimmickAnimationJob Field_gimmickAnimationQueue[8];

// decomp: D_800F1D68 (D_800F1D68@BATTLE/BATTLE.PRG)
short Field_platformFreezeFrames;

// decomp: D_800F1D6A (D_800F1D6A@BATTLE/BATTLE.PRG)
short Field_pendingGimmickAnimationCount;

// decomp: D_800F1D6C (D_800F1D6C@BATTLE/BATTLE.PRG)
u_short Field_triggerMarkerFrameTicks;

// decomp: D_800F1D6E (D_800F1D6E@BATTLE/BATTLE.PRG)
u_char Field_areMovingPlatformsPaused;

// decomp: D_800F1D6F (D_800F1D6F@BATTLE/BATTLE.PRG)
char Field_isPlatformFreezeTintActive;

// decomp: D_800F1D70 (D_800F1D70@BATTLE/BATTLE.PRG)
u_char Field_platformBounceFrames[1304];

// decomp: D_800F4448 (D_800F4448@BATTLE/BATTLE.PRG)
char Field_gimmickPaletteColors[240];

// decomp: D_800F45E0 (D_800F45E0@BATTLE/BATTLE.PRG)
Field_GimmickInstance* Field_gimmickInstances[16];

// decomp: D_800F49E8 (D_800F49E8@BATTLE/BATTLE.PRG)
int Field_gimmickInteractionDirection;

// decomp: D_80068BEC (D_80068BEC@src/BATTLE/BATTLE.PRG/146C.c)
short const Field_kPlatformBounceOffsets[] = { 8, 14, 16, 11, 7, 3, 1 };

}  // extern "C"
