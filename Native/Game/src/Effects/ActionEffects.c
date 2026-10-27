// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/Animation.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/Hud.h"
#include "Battle/Targeting.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Effects/Trails.h"
#include "Field/GameMode.h"
#include "Field/Interactions.h"
#include "Field/Map.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"
#include "State/Timers.h"

// decomp: BATTLE.PRG func_8006C480 (src/BATTLE/BATTLE.PRG/146C.c:1923)
void Effects_StartWeaponTrail(int actor_index, int bone_index) { Effects_SpawnBoneTrail(actor_index, bone_index, 0, 0); }

// decomp: BATTLE.PRG func_8006C4A4 (src/BATTLE/BATTLE.PRG/146C.c:1925)
void Effects_FadeWeaponTrails(int arg0, int arg1) { Effects_FadeBoneTrails(arg0, arg1); }

// decomp: BATTLE.PRG func_8006C4C4 (src/BATTLE/BATTLE.PRG/146C.c:1927)
void Effects_QueueActionHitEffect(int effect_id, Battle_HitResult* hit_result, int targetLimb)
{
    Effects_HitEffectRequest hit_effect_request;
    Battle_ActionRequest* queued_action = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    hit_effect_request.opaque0 = effect_id;
    hit_effect_request.opaque2 = 1;
    hit_effect_request.opaque3 = 0;
    hit_effect_request.opaque4 = 4;
    hit_effect_request.targetActor = queued_action->primaryTarget.target.targetActor;
    hit_effect_request.opaqueA = 0;

    if (hit_result->targetsPosition == 0) {
        hit_effect_request.opaque10 = 4;
        hit_effect_request.opaque14.targetActor = hit_result->target.targetActor;
        hit_effect_request.opaque14.targetLimb = targetLimb;
        hit_effect_request.opaque14.hitThreshold =
            Actors_FindActorBoneTag(hit_result->target.targetActor, hit_result->target.targetLimb, 2);
    } else {
        hit_effect_request.opaque10 = 5;
        hit_effect_request.opaque14 = hit_result->target;
    }

    hit_effect_request.opaque12 = queued_action->effectVariant;

    Effects_BeginSecondaryEffects((Effects_ActionEffectRequest*)&hit_effect_request, 0);
}

// decomp: BATTLE.PRG func_8006C84C (src/BATTLE/BATTLE.PRG/146C.c:2044)
int Effects_QueueActionImpactEffects(int attacker_index)
{
    Battle_ActionRequest* queued_action;
    int i;

    queued_action = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    for (i = 0; i < queued_action->targetCount; ++i) {
        if (queued_action->targets[i].targetsPosition == 0) {
            if (queued_action->actionId < 0x28) {

                if (((Battle_actionDefinitions[queued_action->actionId]
                            .hits[0]
                            .prerequisiteFunctionId))
                    == 3) {

                    Effects_QueueActionHitEffect(queued_action->actionId, &queued_action->targets[i],
                        queued_action->targets[i].target.impactKind);

                } else {
                    switch (queued_action->targets[i].target.impactKind) {
                    case 0:
                    case 2:
                    case 5:
                        if (queued_action->actionId < 0x16) {
                            Effects_QueueActionHitEffect(
                                1, &queued_action->targets[i], queued_action->targets[i].target.impactKind);
                        } else {
                            Effects_QueueActionHitEffect(queued_action->actionId, &queued_action->targets[i],
                                queued_action->targets[i].target.impactKind);
                        }
                        break;

                    case 1:
                        Effects_QueueActionHitEffect(
                            0x27, &queued_action->targets[i], queued_action->targets[i].target.impactKind);
                        break;

                    case 3:
                    case 4:
                    case 6:
                        Effects_QueueActionHitEffect(
                            0x26, &queued_action->targets[i], queued_action->targets[i].target.impactKind);
                        break;
                    }
                }

            } else {
                Effects_QueueActionHitEffect(queued_action->actionId, &queued_action->targets[i],
                    queued_action->targets[i].target.impactKind);
            }

        } else {
            Effects_QueueActionHitEffect(0x26, &queued_action->targets[i], 0);
        }

        Battle_PlayWeaponHitSounds(attacker_index);
    }

    return 1;
}

// decomp: BATTLE.PRG func_8006CDB8 (src/BATTLE/BATTLE.PRG/146C.c:2149)
void Effects_SignalActorEffectAnimation(int actor_index) { Actors_RequestActorAnimationAdvance(actor_index); }

// decomp: BATTLE.PRG func_8006D028 (src/BATTLE/BATTLE.PRG/146C.c:2211)
void Effects_TintActionActorRed(int actor_index, int unused __attribute__((unused)))
{
    Effects_tintedActor = actor_index;
    Render_TransformActorColors(0xE, 8, actor_index, 0x10, 0, 0);
}

// decomp: BATTLE.PRG func_8006D064 (src/BATTLE/BATTLE.PRG/146C.c:2217)
void Effects_RestoreActionActorTint(void)
{
    Render_TransformActorColors(8, 4, Effects_tintedActor, 0, 0, 0);
    Effects_tintedActor = 0;
}

// decomp: BATTLE.PRG func_8006F450 (src/BATTLE/BATTLE.PRG/146C.c:3171)
void Effects_SpawnGroundPlaneImpactEffect(u_short* world_position_components)
{
    Effects_ActionEffectRequest request;

    request.effectId = 36;
    request.count = 1;
    request.source.type = 4;
    request.otherCount = 0;
    request.source.value.actor.actor = 0;
    request.targets[0].type = 5;
    request.targets[0].value.position.vx = world_position_components[0];
    request.targets[0].value.position.vz = world_position_components[2];
    request.targets[0].value.position.vy = 0;
    Effects_BeginSecondaryEffects(&request, 0);
}

// decomp: BATTLE.PRG func_8006F4B0 (src/BATTLE/BATTLE.PRG/146C.c:3187)
void Effects_SpawnTrapTileEffect(Actors_ActorLocation* actor_location)
{
    Effects_ActionEffectRequest request;

    request.effectId = 33;
    request.count = 1;
    request.source.type = 4;
    request.otherCount = 0;
    request.source.value.actor.actor = 0;
    request.targets[0].type = 5;
    request.targets[0].value.position.vx = actor_location->packedTile.target.tileX * 128 + 64;
    request.targets[0].value.position.vz = actor_location->packedTile.target.tileZ * 128 + 64;
    request.targets[0].value.position.vy = actor_location->position.vy;
    if (!(Effects_effectBusyFlags & 2)) {
        Effects_BeginSecondaryEffects(&request, 0);
    }
}

// decomp: BATTLE.PRG func_8006F848 (src/BATTLE/BATTLE.PRG/146C.c:3298)
void Effects_SpawnAbilityLearnedEffect(void)
{
    Effects_ActionEffectRequest request;

    request.effectId = 20;
    request.count = 1;
    request.source.type = 5;
    request.targets[0].type = 4;
    request.otherCount = 0;
    request.targets[0].value.actor.actor = 0;
    request.targets[0].value.actor.part = 0xFD;
    Effects_BeginSecondaryEffects(&request, 0);
}

// decomp: BATTLE.PRG func_800733F8 (src/BATTLE/BATTLE.PRG/146C.c:4613)
void Effects_BeginAshleyStandaloneEffect(int effect_id)
{
    Effects_ActionEffectRequest request;

    request.effectId = effect_id;
    Field_controlMode = 4;
    request.count = 1;
    request.otherCount = 0;
    request.source.type = 4;
    request.source.value.actor.actor = 0;
    request.source.value.actor.eventActorSlot = 0;
    request.source.value.actor.part = 0;
    request.targets[0].type = 4;
    request.targets[0].value.actor.actor = 0;
    request.targets[0].value.actor.eventActorSlot = 0;
    request.targets[0].value.actor.part = 0;
    request.position.vz = 0;
    request.position.vy = 0;
    request.position.vx = 0;
    Effects_BeginActionEffect(&request, 0, 0);
    State_tickSpeed = 4;
    Battle_SetHudHidden(1);
    Field_SetMinimapVisible(0);
}

// decomp: BATTLE.PRG func_80073484 (src/BATTLE/BATTLE.PRG/146C.c:4638)
void Effects_EndAshleyStandaloneEffect(void)
{
    Field_EnterFieldExplorationMode();
    State_tickSpeed = 2;
    Battle_SetHudHidden(0);
    Field_SetMinimapVisible(1);
}

// decomp: BATTLE.PRG func_80074294 (src/BATTLE/BATTLE.PRG/146C.c:5064)
int Effects_ActionEffectBusyFlags(void) { return Effects_effectBusyFlags & 3; }

// decomp: BATTLE.PRG func_8007BE20 (src/BATTLE/BATTLE.PRG/146C.c:8581)
void Effects_StartScriptedActorEffect(int effect_id, int target_actor_index)
{
    Effects_ActionEffectRequest request;
    int i;

    int count = 0;
    Battle_Actor** actors = Battle_actors;

    request.count = 1;
    request.source.type = 5;
    request.effectId = effect_id;
    request.otherCount = 0;
    request.source.value.position.vz = 0;
    request.source.value.position.vy = 0;
    request.source.value.position.vx = 0;
    request.targets[0].type = 4;
    request.targets[0].value.actor.actor = target_actor_index;
    request.targets[0].value.actor.part = 0;

    for (i = 0; i < 16; ++i) {
        if ((actors[i] != NULL) && (i != target_actor_index)) {
            request.others[count] = i;
            ++count;
        }
    }

    request.otherCount = count;
    Effects_BeginActionEffect(&request, 0, 0);
}

// decomp: BATTLE.PRG func_8007BEC4 (src/BATTLE/BATTLE.PRG/146C.c:8611)
void Effects_ReleaseScriptedEffectTrigger(int track_index) { Effects_StartEffectTrack(track_index); }

// decomp: BATTLE.PRG func_8007BEE4 (src/BATTLE/BATTLE.PRG/146C.c:8613)
int Effects_ScriptedEffectPlaybackState(void) { return Effects_PrimaryEffectPhase(); }

// decomp: BATTLE.PRG func_8007BF0C (src/BATTLE/BATTLE.PRG/146C.c:8617)
int Effects_IsScriptedEffectTriggerReady(void) { return Effects_CanStartEffectTrack() != 0; }

