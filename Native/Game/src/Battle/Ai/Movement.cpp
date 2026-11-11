// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

// decomp: D_800F58C4 (D_800F58C4@BATTLE/BATTLE.PRG)
int Battle_Ai_savedMovementRandomState;

// decomp: D_800F58D8 (D_800F58D8@BATTLE/BATTLE.PRG)
unsigned short (*Battle_Ai_patrolRoutes)[8];

// decomp: BATTLE.PRG func_800E24EC (src/BATTLE/BATTLE.PRG/771A8.c:1973)
int Battle_Ai_UpdateTargetApproachMovement(Battle_Ai_TargetApproachView* s, int option)
{
    int result = 1, target = s->targetId, mode, value, angle, distance, one, radius;
    unsigned int oldPosition;
    Battle_Ai_ApproachTargetRecordView* model;
    if (target == s->actorId) {
        VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)s), 0);
        goto done;
    }
    model = (Battle_Ai_ApproachTargetRecordView*)Actors_actorRecords[target];
    if (s->model->opaque8 & 0x200000) {
        option = 1;
    }
    if ((!(model->opaque8 & 0x1F0000)
                ? ((model->x >> 6) != s->targetTileX || (model->z >> 6) != s->targetTileZ)
                : (s->destination.bytes[0] != (s->targetTileX >> 1)
                      || s->destination.bytes[2] != (s->targetTileZ >> 1)))
        || s->range != s->previousRange || target != s->previousTargetId
        || option != s->previousOption || s->opaqueF8++ > 180) {
        s->opaqueF8 = 0;
        s->opaqueE8 = 0;
        s->opaqueE7 = 0;
    }
    s->previousOption = option;
    s->previousTargetId = target;
    s->previousRange = s->range;
    if (s->relations[target].active) {
        s->opaqueE4 = 0;
        if (s->relations[target].opaque0Bit2) {
            mode = s->rangeMode;
            switch (mode) {
            case 0:
                if ((unsigned int)((s->range - 96) * (s->range - 96))
                    < s->distancesSquared[target]) {
                    s->rangeMode = 0;
                } else {
                    s->rangeMode = 1;
                }
                break;
            case 1:
                value = s->range;
                if ((unsigned int)(value * value) < s->distancesSquared[target]) {
                out_of_range:
                    s->rangeMode = 0;
                } else {
                    if ((unsigned int)((value - 128) * (value - 128))
                        >= s->distancesSquared[target]) {
                        s->rangeMode = 2;
                    } else {
                        s->rangeMode = mode;
                    }
                }
                break;
            case 2:
                if ((unsigned int)((s->range - 32) * (s->range - 32))
                    < s->distancesSquared[target]) {
                    s->rangeMode = 1;
                } else {
                    s->rangeMode = mode;
                }
                break;
            }
        } else {
            goto out_of_range;
        }
        if ((s->rangeMode != 2 && s->opaqueE8) || (s->rangeMode != 0 && s->opaqueE7)) {
            goto recover;
        }
        if (option == 0 && s->rangeMode == 2) {
            s->rangeMode = 1;
        }
        one = 1;
        switch (s->rangeMode) {
        case 0:
            if (option == one) {
                s->opaque0 = one;
            }
            s->opaqueC = one;
            s->opaque1 = VS::Battle::Ai::IsLiveAiTargetBeyondNearRadius(s, target, s->range);
            if (s->movementFlags & 8) {
                oldPosition = s->destination.word;
                angle = VS::Core::Math::Arctangent2(s->x - (s->destination.bytes[0] << 7) - 64,
                            s->z - (s->destination.bytes[2] << 7) - 64)
                      & 4095;
                distance = VS::Core::Math::SquaredVectorLength(s->x - (s->destination.bytes[0] << 7) - 64, 0,
                    s->z - (s->destination.bytes[2] << 7) - 64);
                s->savedDestination.word = oldPosition;
                distance = VS::Core::Math::SquareRootInteger(distance);
                radius = s->range - 96;
                VS::Battle::Ai::ProjectLiveClearRadialDestination((Battle_Ai_ActorAiState *)((void*)s), s->tile, angle,
                    distance - (radius >= 0 ? radius : -radius));
                if ((s->destination.word & 0xFF00FF) == (s->tile.word & 0xFF00FF)) {
                    s->opaqueD = one;
                    s->destination.word = oldPosition;
                }
            }
            s->movementTarget = s->targetId;
            VS::Battle::Ai::DispatchLiveAiLocomotionMode(s, 0, 0);
            break;
        case 1:
        recover:
            s->movementTarget = target;
            VS::Battle::Ai::RecoverLiveMovement((Battle_Ai_ActorAiState *)((void*)s));
            break;
        case 2:
            s->opaque3 = one;
            *(unsigned char*)((uintptr_t)&Battle_Ai_FrameState()->directionCache) = 0;
            s->opaque4 = one;
            s->moveX = model->x;
            s->moveZ = model->z;
            if (s->settingsByte0Bit7) {
                s->opaque1 = one;
                s->opaque9 = one;
                s->targetX = s->moveX;
                s->targetZ = s->moveZ;
            } else {
                s->movementTarget = target;
            }
            angle = VS::Core::Math::Arctangent2(s->moveX - s->x, s->moveZ - s->z);
            VS::Battle::Ai::DispatchLiveAiLocomotionMode(s, 1, angle);
            if (s->actor->id >= 9) {
                s->opaqueE8 = 1;
            }
            break;
        }
    } else {
        if (!s->opaqueE4 && s->relations[target].opaque0Bit1) {
            if (s->settingsBit13) {
                s->opaqueE4 = 1;
                s->timeout = 255;
                s->cachedDestination.bytes[0] = s->targetTileX >> 1;
                s->cachedDestination.bytes[2] = s->targetTileZ >> 1;
            } else {
                if (s->relations[target].visible) {
                    result = 0;
                    goto done;
                }
                ((void (*)(Battle_Ai_ActionExecutionView* ai_state, int awareness_marker, int duration_frames))Battle_Ai_SetTemporaryAwarenessIndicator)((Battle_Ai_ActionExecutionView *)((void*)s), 7, 40);
                if (!VS::Battle::Ai::UpdateLiveHesitationTurn(s, 128, 1, 1)) {
                    result = 0;
                }
                goto done;
            }
        }
        if (s->opaqueE8) {
            goto recover;
        }
        if (s->opaqueE4) {
            s->destination.word = s->cachedDestination.word;
            if (s->timeout) {
                s->timeout--;
            }
            if ((s->destination.word & 0xFF00FF) == (s->tile.word & 0xFF00FF)) {
                if (s->relations[target].visible) {
                    result = 0;
                    goto done;
                }
                ((void (*)(Battle_Ai_ActionExecutionView* ai_state, int awareness_marker, int duration_frames))Battle_Ai_SetTemporaryAwarenessIndicator)((Battle_Ai_ActionExecutionView *)((void*)s), 7, 40);
                if (!VS::Battle::Ai::UpdateLiveHesitationTurn(s, 128, 1, 1)) {
                cancel:
                    s->opaqueE4 = 0;
                    result = 0;
                }
            } else if (s->timeout) {
                s->opaque1 = 1;
                s->opaqueC = 1;
                VS::Battle::Ai::DispatchLiveAiLocomotionMode(s, 0, 0);
            } else {
                goto cancel;
            }
        } else if (!VS::Battle::Ai::UpdateLiveHesitationTurn(s, 0, 3, 1)) {
            result = 0;
        }
    }
    goto done;

done:
    VS::Battle::Ai::RefreshLiveAiTargetTile(s);
    return result;
}

// decomp: BATTLE.PRG func_800E2B2C (src/BATTLE/BATTLE.PRG/771A8.c:2185)
int Battle_Ai_UpdateTargetFacing(Battle_Ai_MovementDecisionView* context, int targetId)
{
    Actors_ActorRecord* actor = context->model;
    int angle;
    if (targetId == context->actorId) {
        VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)context), 0);
        return 1;
    }
    if (context->facingDelay == 0) {
        return 0;
    }
    if (context->reactionPending != 0) {
        if (VS::Battle::Ai::UpdateLiveHesitationTurn(context, 128, 3, 6)) {
            return 1;
        }
        context->facingPending = 0;
        goto failed;
    }
    angle = VS::Core::Math::Arctangent2(context->x - context->moveX, context->z - context->moveZ);
    if ((context->destination & 0xFF00FF) != (context->previousFacingTile & 0xFF00FF)
        && ((context->targetFlags[targetId] >> 5) & 1)) {
        if (context->facingPending == 0) {
            context->facingPending = 1;
            context->angle = angle;
            context->previousFacingTile = context->destination;
        }
    }
    if (context->facingPending == 0) {
        return 0;
    }
    context->opaque13A = context->facingDelay;
    if (((actor->instance.facing + (short)actor->instance.lookYaw) & 0xFFF)
        == (context->angle & 0xFFF)) {
        if (!((context->targetFlags[targetId] >> 5) & 1)) {
            if (VS::Battle::Ai::UpdateLiveHesitationTurn(context, 128, 1, 6)) {
                return 1;
            }
            context->facingPending = 0;
            goto failed;
        }
        context->facingPending = 1;
        context->angle = angle & 0xFFF;
        context->previousFacingTile = context->destination;
    }
    VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)context), 4);
    return 1;
failed:
    return 0;
}

}  // extern "C"
