// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_AI_ACTORSTATE_H_TYPES
#define VS_BATTLE_AI_ACTORSTATE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_AI_ACTORSTATE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_AI_ACTORSTATE_H_DECLS)
#define VS_BATTLE_AI_ACTORSTATE_H_DECLS
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"

extern unsigned char Battle_Ai_kSettingsBits30Values[1284];
extern unsigned int Battle_Ai_kPerceptionRangesSquared[320];
extern Battle_Ai_WanderTiming Battle_Ai_kWanderTimingPresets[208];
extern int (*Battle_Ai_decisionCallbacks[9])(Battle_Ai_ActorAiState*);
extern unsigned short Battle_Ai_viewAngles[45][2];
extern unsigned short Battle_Ai_settingsBits8Values[41][2];
extern unsigned char Battle_Ai_facingDelayScales[148];
extern Battle_Ai_ActorAiState* Battle_Ai_actorAiStates[16];
extern Battle_Ai_RoomAiState* Battle_Ai_roomAiState;
extern void* Battle_Ai_roomHasWaypoints;
extern int Battle_Ai_attributeActorMask;
extern const unsigned char Battle_Ai_kFlags34Bit14Values[4];
void Battle_Ai_ResolveAiLinkedActors(void);
void Battle_Ai_ApplySavedAiSettingsFlags(int* settings_word, Battle_Ai_SavedSettingsPair* saved_settings);
void Battle_Ai_ApplyActorAiSettings(Battle_Ai_SettingsView* state, Battle_Ai_SettingsWord* settings);
int Battle_Ai_CreateActorAiState(Battle_Ai_AiActorView* actor, int options);

#endif // VS_BATTLE_AI_ACTORSTATE_H_DECLS
