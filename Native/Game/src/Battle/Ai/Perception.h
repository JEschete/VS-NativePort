// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_AI_PERCEPTION_H_TYPES
#define VS_BATTLE_AI_PERCEPTION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_Ai_RelationWord Battle_Ai_RelationWord;

// layout: ledger (upstream merge) for func_800DA1D4_t
typedef struct Battle_Ai_RelationWord {
    /* 0x00 */ u_int active : 1;
    /* 0x00 */ u_int reserved0Bit1 : 4;
    /* 0x00 */ u_int visible : 1;
    /* 0x00 */ u_int classification : 2;
    /* 0x01 */ u_int counter : 7;
    /* 0x01 */ u_int timer : 5;
    /* 0x02 */ u_int classificationTimer : 7;
    /* 0x03 */ u_int blocked : 1;
    /* 0x03 */ u_int previousClassification : 2;
    /* 0x03 */ u_int reserved3Bit6 : 2;
} Battle_Ai_RelationWord;
_Static_assert(sizeof(Battle_Ai_RelationWord) == 0x4, "Battle_Ai_RelationWord must keep its PS1 layout");

#endif // VS_BATTLE_AI_PERCEPTION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_AI_PERCEPTION_H_DECLS)
#define VS_BATTLE_AI_PERCEPTION_H_DECLS
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"

extern int Battle_Ai_frameTickBudget;
extern int Battle_Ai_secondPerceptionTarget;
extern int Battle_Ai_candidateEngagedThisUpdate;
void Battle_Ai_SetTemporaryAwarenessIndicator(Battle_Ai_ActorAiState* ai_state, int awareness_marker, int duration_frames);
int Battle_Ai_IsTargetOccludedAtExtent(
    SVECTOR* from, SVECTOR* to, Battle_Ai_PerceptionView* state, int id);
int Battle_Ai_IsTargetOccludedAtHeight(
    SVECTOR* from, SVECTOR* to, Battle_Ai_PerceptionView* state, int id);
void Battle_Ai_UpdateTargetPerception(Battle_Ai_PerceptionView* state, int id);
void Battle_Ai_UpdateActorPerception(void);
void Battle_Ai_UnlinkEngagedActor(Battle_Ai_ActorAiState* ai_state);
void Battle_Ai_UpdateEngagementBuildup(Battle_Ai_ActorAiState* ai_state);
void Battle_Ai_AnimateEngagementPhase(void);
void Battle_Ai_ApplyActorActionReaction(Battle_Ai_ReactionView* state, Battle_Ai_StatApplicationView* app);
void Battle_Ai_PropagateActorRelationships(Battle_Ai_ReactionView* state);
void Battle_Ai_NotifyActionRelationshipChanges(Battle_Ai_ReactionRecord* completed_action);

#endif // VS_BATTLE_AI_PERCEPTION_H_DECLS
