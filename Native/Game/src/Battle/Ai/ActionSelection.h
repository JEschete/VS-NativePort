// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_AI_ACTIONSELECTION_H_TYPES
#define VS_BATTLE_AI_ACTIONSELECTION_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_AI_ACTIONSELECTION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_AI_ACTIONSELECTION_H_DECLS)
#define VS_BATTLE_AI_ACTIONSELECTION_H_DECLS
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"

void Battle_Ai_ForgetAiActionTarget(Battle_Ai_ActorAiState* ai_state);
void Battle_Ai_BuildAiActionMetadata(Battle_Ai_ActionMetadataView* state);

#endif // VS_BATTLE_AI_ACTIONSELECTION_H_DECLS
