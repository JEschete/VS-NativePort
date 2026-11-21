#pragma once
// Hand-maintained (tools/port/owned.csv).
// The skeleton pose, the weapon model table and the bone camera as the copied
// code reads them. SkeletonLive.cpp defines them.

#include "Actors/ModelDraw.hpp"
#include "Actors/WeaponModel.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Actors {

// decomp: D_800F3C58 (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/49268Private.h:8): the bone matrices of the skeleton
// being drawn and its scale.
extern SkeletonPose skeletonPose;
// decomp: D_800F3C78 (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/49268.c:13): the
// placed gimmick model's root matrix. The original address is bone_matrices[1]
// of D_800F3C58; the native keeps it apart, as nothing reads it and the actor
// pose writes its own bone_matrices[1] before reading it.
extern VS::Core::Gte::MATRIX skeletonRootMatrix;
// decomp: vs_battle_wepModels (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/entity.h:525): two weapon model slots per actor slot,
// for the first ten actor slots.
inline constexpr int kWeaponModelCount = 20;
extern VS::Actors::WeaponModel* weaponModels[kWeaponModelCount];
// decomp: D_800F49B8 (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/38C1C.h:17): the
// camera the bone queries view the bones from; the room pass and the pose
// builder prepare it.
extern VS::Core::Gte::MATRIX boneMatrixScratch;

}  // namespace VS::Actors
