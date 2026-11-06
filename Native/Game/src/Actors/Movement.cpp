// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Render/Palette.h"

// decomp: D_800E90AC (D_800E90AC@BATTLE/BATTLE.PRG)
u_char Actors_kJumpHangFrames[3204]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E90B4 (D_800E90B4@BATTLE/BATTLE.PRG)
u_char Actors_kJumpHeightAdjustments[3196]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E90BC (D_800E90BC@BATTLE/BATTLE.PRG)
int Actors_motionResultResetWord; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E90C3 (D_800E90C3@BATTLE/BATTLE.PRG)
u_char Actors_kMotionResultAnimations[3181]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E90C4 (D_800E90C4@BATTLE/BATTLE.PRG)
u_char Actors_kMotionResultOneAnimationId; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E90C6 (D_800E90C6@BATTLE/BATTLE.PRG)
u_char Actors_kHangMotionAnimationId; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E90CC (D_800E90CC@BATTLE/BATTLE.PRG)
u_char Actors_kSlowQuadrantMoveAnimations[3172]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E90D0 (D_800E90D0@BATTLE/BATTLE.PRG)
u_char Actors_kFastQuadrantMoveAnimations[3168]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E92E8 (D_800E92E8@BATTLE/BATTLE.PRG)
u_char Actors_kAirTimerEndAnimations[2632]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E92F0 (D_800E92F0@BATTLE/BATTLE.PRG)
u_char Actors_kSoftLandingAnimations[2624]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E92F8 (D_800E92F8@BATTLE/BATTLE.PRG)
u_char Actors_kHardLandingAnimations[2616]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9300 (D_800E9300@BATTLE/BATTLE.PRG)
u_char Actors_kLandingRecoveryAnimations[2608]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F4538 (D_800F4538@BATTLE/BATTLE.PRG)
Actors_ActorRecord* Actors_actorRecords[17];

// decomp: D_800F49F4 (D_800F49F4@BATTLE/BATTLE.PRG)
u_short Actors_supportingPlatformLayer;

// decomp: D_800F49F8 (D_800F49F8@BATTLE/BATTLE.PRG)
u_char Actors_supportingGimmickIndex;

// decomp: D_800F4B18 (D_800F4B18@BATTLE/BATTLE.PRG)
char Actors_isActorUpdateComplete;

// decomp: D_800F4B19 (D_800F4B19@BATTLE/BATTLE.PRG)
u_char Actors_pushedGimmickIndex;

}  // extern "C"
