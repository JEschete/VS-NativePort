#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_CONDITIONSNAPSHOT_H
#define VS_OWNED_BATTLE_AI_CONDITIONSNAPSHOT_H
#include "Actors/ActorAllocation.h"
#include "Actors/ModelInstance.h"
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Legacy/Psx.h"

void Battle_Ai_CaptureActorConditionTable(Battle_Ai_ActionTiming*);

#endif  // VS_OWNED_BATTLE_AI_CONDITIONSNAPSHOT_H
