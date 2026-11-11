#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/Relations.hpp"

struct Battle_ActionRequest;
struct Battle_Ai_AiCreationView;
struct Battle_Ai_ReactionView;

// Bindings from the battle actor list and AI state table to the typed actor
// relation and Ashley alliance policies (Relations.cpp). Each aborts with a
// diagnostic when a state, actor ID or the actor list is invalid.
namespace VS::Battle::Ai {
// Classifies the room from the actor `state`'s point of view; `state` must
// already be registered in the AI state table.
void InitializeLiveActorAiRelations(Battle_Ai_AiCreationView* state);
// Restores every room actor's saved Ashley relation.
void ApplyLiveSavedActorAiRelations();
void EndLiveAshleyAlliance(Battle_Ai_ReactionView* state);
void TryBeginLiveAshleyAlliance(Battle_Ai_ReactionView* state, int forced);
// Links each listed actor's unresolved AI linked actor to its group leader
// (func_800E5998); every actor after Ashley must have an AI state (#337).
void ResolveLiveAiLinkedActors();
// The AI state `state` reacts to the completed action `request`, which the
// original reads as actionStatApplication; nothing happens without a request
// (func_800DD000).
void ApplyLiveActorActionReaction(Battle_Ai_ReactionView* state,
                                  const Battle_ActionRequest* request);
// Reclassifies the room or shares allies' hostility from the AI state
// `state`'s point of view (func_800DD344).
void PropagateLiveActorRelationships(Battle_Ai_ReactionView* state);
// Lets the completed action's acting actor engage again and has every AI
// actor react to it; nothing happens without a request (func_800DEEA4).
void NotifyLiveActionRelationshipChanges(Battle_ActionRequest* request);
}  // namespace VS::Battle::Ai
