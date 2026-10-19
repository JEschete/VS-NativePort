#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_TARGETINGDRAW_H
#define VS_OWNED_BATTLE_TARGETINGDRAW_H
#include "Battle/BattleTypes.h"
#include "Battle/TargetVolume.h"
#include "Battle/Targeting.h"
#include "Legacy/Psx.h"

void Battle_DrawSphereVolumeWireframe(Battle_TargetVolume* shape, int color);

#endif  // VS_OWNED_BATTLE_TARGETINGDRAW_H
