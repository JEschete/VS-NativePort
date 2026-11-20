// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Actors/ActorModelSlot.hpp"
#include "Actors/ModelInstance.h"
#include "Actors/ShpModel.hpp"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.h"
#include "Render/Palette.hpp"

extern u_short Battle_bladePpDamageBonuses[12];
extern short (*Battle_statCalculators[27])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
extern Battle_Actor* Battle_actors[42];
extern u_int Battle_lastActionStatValue;
extern int Battle_criticalHitOccurred;
extern short Battle_limbArmorDpLoss[8];
