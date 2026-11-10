// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Field/Gimmicks.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: D_80069BBC (D_80069BBC@BATTLE/BATTLE.PRG)
u_int Battle_Ai_kTimedStatusFlags[344]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F58E8 (D_800F58E8@BATTLE/BATTLE.PRG)
Battle_ActorStats* Battle_Ai_planningStatsSnapshot;

// decomp: D_800F58EC (D_800F58EC@BATTLE/BATTLE.PRG)
Battle_Ai_CandidateEntryView (*Battle_Ai_planningPassCandidates)[16];

// decomp: D_800F58F0 (D_800F58F0@BATTLE/BATTLE.PRG)
int Battle_Ai_fallbackCandidateCount;

// decomp: D_800F58F4 (D_800F58F4@BATTLE/BATTLE.PRG)
int Battle_Ai_unusedPlanCounter;

// decomp: D_800F58F8 (D_800F58F8@BATTLE/BATTLE.PRG)
Battle_Ai_CandidateEntryView* Battle_Ai_fallbackCandidates;

// decomp: D_800F5900 (D_800F5900@BATTLE/BATTLE.PRG)
Battle_Ai_ReactionRecord* Battle_Ai_planningWorkspace;

// decomp: D_800F5904 (D_800F5904@BATTLE/BATTLE.PRG)
u_char (*Battle_Ai_candidateExclusions)[256];

// decomp: BATTLE.PRG func_800DEB10 (src/BATTLE/BATTLE.PRG/74088.c:1458)
void Battle_Ai_ReconcileCompletedActionPlan(Battle_Ai_ReactionRecord* completed_action)
{
    int i;
    int j;
    Battle_Ai_ActorAiState* entry;
    Battle_Ai_EngagementData* command_actor;

    if (completed_action == NULL) {
        return;
    }

    if (completed_action->opaque4.opaque40 != 0) {
        return;
    }

    entry = Battle_Ai_actorAiStates[completed_action->opaque4.opaque0];
    command_actor = entry->actor;

    if (entry == NULL) {
        return;
    }

    if (command_actor->opaque9 != 24) {
        return;
    }

    for (i = 0; i < entry->candidateCount; ++i) {
        Battle_Ai_ActionCandidate** list = entry->candidateOrder;
        Battle_Ai_ActionCandidate* candidate = list[i];

        if ((completed_action->opaque0 == candidate->action) && (command_actor->opaqueC == candidate->row)
            && (command_actor->opaqueE == candidate->column)) {
            entry->opaqueFB = candidate->targetId;
            for (j = 0; j < completed_action->opaque4A; ++j) {
                Battle_Ai_EngagementActor* elem = &completed_action->opaque4C[j];

                if (elem->opaque40 == 0) {
                    Field_GimmickInstance* actor = Field_gimmickInstances[elem->opaque0];

                    if ((actor == NULL) || ((0x14 >> actor->objectInfo.objectId) & 1)) {
                        Battle_Ai_ForgetAiActionTarget(entry);
                        return;
                    }
                }
            }
        }
    }

    VS::Battle::Ai::RememberLiveAiActionTarget(entry);
}

}  // extern "C"
