// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_STATE_SCORE_H_TYPES
#define VS_STATE_SCORE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_STATE_SCORE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_STATE_SCORE_H_DECLS)
#define VS_STATE_SCORE_H_DECLS
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

void State_RecordEnemyKillScore(Battle_Actor* enemy);
void State_UpdateWeaponKillStreak(int arg0 __attribute__((unused)));

#endif // VS_STATE_SCORE_H_DECLS
