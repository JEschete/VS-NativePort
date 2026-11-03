#pragma once
// Hand-maintained (tools/port/owned.csv).
// The skeleton pose, the weapon model table and the bone camera as the copied
// code reads them. SkeletonLive.cpp defines them.

#include "Actors/ModelDraw.h"
#include "Actors/WeaponModel.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: D_800F3C58 (BATTLE/BATTLE.PRG 44F14.c): the bone matrices of the
// skeleton being drawn and its scale.
extern Actors_SkeletonPose Actors_skeletonPose;
// decomp: D_800F3C78 (BATTLE/BATTLE.PRG 49268.c): the placed gimmick model's
// root matrix. The original address is boneMatrices[1] of D_800F3C58; the
// native keeps it apart, as nothing reads it and the actor pose writes its own
// boneMatrices[1] before reading it.
extern MATRIX Actors_skeletonRootMatrix;
// decomp: vs_battle_wepModels (BATTLE/BATTLE.PRG 3A1A0.h): two weapon model
// slots per actor slot, for the first ten actor slots.
enum { kActorsWeaponModelCount = 20 };
extern Actors_WeaponModel* Actors_weaponModels[kActorsWeaponModelCount];
// decomp: D_800F49B8 (BATTLE/BATTLE.PRG 38C1C.c): the camera the bone queries
// view the bones from; the room pass and the pose builder prepare it.
extern MATRIX Actors_boneMatrixScratch;

#ifdef __cplusplus
}
#endif
