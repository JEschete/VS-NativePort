#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_ACTIONRESULTS_H
#define VS_OWNED_BATTLE_ACTIONRESULTS_H
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

void Battle_ApplyActionTargetEffects(int actor_id, SVECTOR* position);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_BATTLE_ACTIONRESULTS_H
