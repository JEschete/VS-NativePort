// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_AI_ORBITINGBOSS_H_TYPES
#define VS_BATTLE_AI_ORBITINGBOSS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_AI_ORBITINGBOSS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_AI_ORBITINGBOSS_H_DECLS)
#define VS_BATTLE_AI_ORBITINGBOSS_H_DECLS
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"

extern short Battle_Ai_radialRetreatPath[36][2];
extern int Battle_Ai_orbitAngularVelocity;
extern int Battle_Ai_orbitHeightOffset;
extern Battle_Ai_ArenaOrbitState* Battle_Ai_arenaOrbit;
int Battle_Ai_UpdateBossRadialRetreat(Battle_Ai_RadialRetreatView* state);
int Battle_Ai_IsBossActionReady(Battle_Ai_ActorAiState* arg0);
void Battle_Ai_UpdateBossOrbitTracking(Battle_Ai_OrbitTrackingView* state);
int Battle_Ai_UpdateOrbitingBossBehavior(Battle_Ai_ActorAiState* arg0);

#endif // VS_BATTLE_AI_ORBITINGBOSS_H_DECLS
