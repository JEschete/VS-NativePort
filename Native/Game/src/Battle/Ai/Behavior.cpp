// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/CandidateMovementLive.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Core/FrameTiming.hpp"
#include "Core/Math/Distance.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Field/Gimmicks.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

// decomp: BATTLE.PRG func_800DAAD8 (src/BATTLE/BATTLE.PRG/71618.c:958)
int Battle_Ai_UpdateEscortCombatBehavior(Battle_Ai_EscortView* state)
{
    Battle_Ai_EscortEntryView* entry = state->candidateOrder[0];
    int target;
    Battle_Ai_EscortActorView* actor;

    if (!state->actionStatus && VS::Battle::Ai::TryApproachLivePlannedTarget((Battle_Ai_ActorAiState *)((void*)state), (Battle_Ai_ActionCandidate *)((void*)entry))) {
        return 1;
    }
    target = state->linkedActor;
    if (target < 16) {
        if (VS::Battle::Ai::ApproachLiveActor((Battle_Ai_ActorAiState *)((void*)state), target, 384, 0)) {
            if (state->opaqueE9 == 1 && state->planMode != 7) {
                state->movementTarget = ((Battle_Ai_EscortTargetView*)&entry->opaqueC)->target;
                VS::Battle::Ai::ReleaseLiveActorAiReservations((Battle_Ai_ActorAiState *)((void*)state));
                VS::Battle::Ai::RecoverLiveMovement((Battle_Ai_ActorAiState *)((void*)state));
            }
            return 1;
        }
        if (VS::Battle::Ai::TrackLiveActorPosition((Battle_Ai_ActorAiState *)((void*)state), target)) {
            return 1;
        }
    }
    for (actor = (Battle_Ai_EscortActorView*)Battle_actors[0]; actor; actor = actor->next) {
        if (((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].mode != 3
            && ((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].category) {
            ((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].mode = 3;
            if (VS::Battle::Ai::FleeLiveVisibleEnemies((Battle_Ai_CandidateMovementView *)((void*)state), 0)) {
                ((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].mode =
                    ((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].oldMode;
                return 1;
            }
            ((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].mode =
                ((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].oldMode;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_800DAC80 (src/BATTLE/BATTLE.PRG/71618.c:1006)
int Battle_Ai_TryExecuteActionCandidate(Battle_Ai_ActorAiState* ai_state, Battle_Ai_ActionCandidate* candidate)
{
    if (!VS::Battle::Ai::CanExecuteLiveActionCandidate(ai_state, candidate)) {
        return 0;
    }

    VS::Battle::Ai::TryEngageLiveActor(ai_state);

    if (VS::Battle::Ai::IsLiveEngagementReady(ai_state)) {
        Actors_ActorRecord* actor;

        ai_state->currentCandidate = candidate;
        Battle_Ai_candidateEngagedThisUpdate = 1;
        actor = Actors_actorRecords[candidate->targetId];

        if (actor != NULL && actor->instance.locomotionState) {
            candidate->targetLimb = VS::Battle::Ai::NextLiveActorRandomMasked(2);
            candidate->opaqueCBit7 = 217;
        }

        VS::Battle::Ai::IssueLiveActorCommand(ai_state, 15);

        if (VS::Battle::Ai::DoesLiveActionRequireCooldown(ai_state, candidate)) {
            ai_state->minActionDelay = ai_state->usesRoomActionDelay == 0 ? Battle_Ai_roomAiState->defaultActionDelay : 50;
        }

        return 1;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800DAD9C (src/BATTLE/BATTLE.PRG/71618.c:1093)
int Battle_Ai_TryExecutePlannedActions(Battle_Ai_ActorAiState* ai_state)
{
    int i;
    int j;
    Battle_Ai_ActionCandidate* entry;

    for (i = 0; i < ai_state->candidateCount; ++i) {
        entry = ai_state->candidateOrder[i];

        if (Battle_Ai_TryExecuteActionCandidate(ai_state, entry)) {
            return 1;
        }
    }

    entry = &ai_state->currentChoice;

    if (entry->action) {
        for (j = 0; j < 4; ++j) {
            Battle_Actor* actor = Battle_Ai_roomAiState->specialActors[j];

            if (actor != NULL) {
                Field_GimmickInstance* model;

                entry->targetId = actor->id;
                model = Field_gimmickInstances[actor->id];

                if (model->parent && model->mode == 0 && Battle_Ai_TryExecuteActionCandidate(ai_state, entry)) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

// decomp: BATTLE.PRG func_800DAED0 (src/BATTLE/BATTLE.PRG/71618.c:1128)
void Battle_Ai_UpdateActorDecisions(void)
{
    Battle_Ai_DispatchActorView* actor;
    Battle_Ai_DecisionDispatchView* state;
    int value, height, coordinate;
    for (actor = ((Battle_Ai_DispatchActorView*)Battle_actors[0])->next; actor;
        actor = actor->next) {
        state = ((Battle_Ai_DecisionDispatchView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[actor->id];
        if (!state->opaque14 || state->opaque1D) {
            value = VS::Core::ElapsedFrameCounterTicks();
            if (Battle_Ai_frameTickBudget < value) {
                VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)state), 17);
            } else {
                *(int*)((uintptr_t)&Battle_Ai_FrameState()->lastBudgetTick) = value;
                if (state->opaque14 && state->opaque1D) {
                    if (Battle_Ai_actorAiStates[0]) {
                        if ((((Battle_Ai_DispatchModelView*)Actors_actorRecords[0])->opaque8 & 0x180000)
                            && (coordinate =
                                    ((Battle_Ai_DispatchModelView*)Actors_actorRecords[0])->position[1],
                                state->position[1] < coordinate)) {
                            height = -((Battle_Ai_DecisionDispatchView*)Battle_Ai_actorAiStates[0])->bodyHeight;
                        } else {
                            height = ((Battle_Ai_DecisionDispatchView*)Battle_Ai_actorAiStates[0])->pointHeight;
                        }
                    } else {
                        height = -64;
                    }
                    state->distance = VS::Core::Math::SquaredDistanceWithHeightOffsets(VS::Core::Math::PositionOf(state->position),
                        VS::Core::Math::PositionOf(((Battle_Ai_DispatchActorView*)Battle_actors[0])->model->position),
                        state->pointHeight, height);
                    Battle_Ai_TryExecutePlannedActions((Battle_Ai_ActorAiState *)((void*)state));
                } else {
                    coordinate = state->tileX;
                    state->movementTarget = 16;
                    state->savedDestination = -1;
                    /* Clear the packed header halfword through its raw address. */
                    *(unsigned short*)((char*)state + 12) = 0;
                    state->opaque0 = 0;
                    state->opaque4 = 0;
                    state->opaque8 = 0;
                    value =
                        (*(unsigned short (**)[32])((uintptr_t)&Battle_Ai_FrameState()->tileFlags))[state->tileZ][coordinate]
                        & 15;
                    state->opaque154 = value;
                    state->opaque132 = value;
                    *(int*)((uintptr_t)&Battle_Ai_FrameState()->directionCache) = 0;
                    VS::Battle::Ai::DispatchLiveMovementBehavior(state);
                    VS::Battle::Ai::UpdateLiveActorLocomotionMode(state);
                }
            }
        }
    }
}

}  // extern "C"
