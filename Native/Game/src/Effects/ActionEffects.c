// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
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
void Effects_StartWeaponTrail(int arg0, int arg1) { Effects_SpawnBoneTrail(arg0, arg1, 0, 0); }

// decomp: BATTLE.PRG func_8006C4A4 (src/BATTLE/BATTLE.PRG/146C.c:1925)
void Effects_FadeWeaponTrails(int arg0, int arg1) { Effects_FadeBoneTrails(arg0, arg1); }

// decomp: BATTLE.PRG func_8006C4C4 (src/BATTLE/BATTLE.PRG/146C.c:1927)
void Effects_QueueActionHitEffect(int arg0, Battle_HitResult* arg1, int targetLimb)
{
    Effects_HitEffectRequest sp10;
    Battle_ActionRequest* temp_s0 = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    sp10.opaque0 = arg0;
    sp10.opaque2 = 1;
    sp10.opaque3 = 0;
    sp10.opaque4 = 4;
    sp10.targetActor = temp_s0->primaryTarget.target.targetActor;
    sp10.opaqueA = 0;

    if (arg1->targetsPosition == 0) {
        sp10.opaque10 = 4;
        sp10.opaque14.targetActor = arg1->target.targetActor;
        sp10.opaque14.targetLimb = targetLimb;
        sp10.opaque14.hitThreshold =
            Actors_FindActorBoneTag(arg1->target.targetActor, arg1->target.targetLimb, 2);
    } else {
        sp10.opaque10 = 5;
        sp10.opaque14 = arg1->target;
    }

    sp10.opaque12 = temp_s0->effectVariant;

    Effects_BeginSecondaryEffects((Effects_ActionEffectRequest*)&sp10, 0);
}

// decomp: BATTLE.PRG func_8006C84C (src/BATTLE/BATTLE.PRG/146C.c:2044)
int Effects_QueueActionImpactEffects(int arg0)
{
    Battle_ActionRequest* temp_s0;
    int i;

    temp_s0 = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    for (i = 0; i < temp_s0->targetCount; ++i) {
        if (temp_s0->targets[i].targetsPosition == 0) {
            if (temp_s0->actionId < 0x28) {

                if (((Battle_actionDefinitions[temp_s0->actionId]
                            .hits[0]
                            .prerequisiteFunctionId))
                    == 3) {

                    Effects_QueueActionHitEffect(temp_s0->actionId, &temp_s0->targets[i],
                        temp_s0->targets[i].target.impactKind);

                } else {
                    switch (temp_s0->targets[i].target.impactKind) {
                    case 0:
                    case 2:
                    case 5:
                        if (temp_s0->actionId < 0x16) {
                            Effects_QueueActionHitEffect(
                                1, &temp_s0->targets[i], temp_s0->targets[i].target.impactKind);
                        } else {
                            Effects_QueueActionHitEffect(temp_s0->actionId, &temp_s0->targets[i],
                                temp_s0->targets[i].target.impactKind);
                        }
                        break;

                    case 1:
                        Effects_QueueActionHitEffect(
                            0x27, &temp_s0->targets[i], temp_s0->targets[i].target.impactKind);
                        break;

                    case 3:
                    case 4:
                    case 6:
                        Effects_QueueActionHitEffect(
                            0x26, &temp_s0->targets[i], temp_s0->targets[i].target.impactKind);
                        break;
                    }
                }

            } else {
                Effects_QueueActionHitEffect(temp_s0->actionId, &temp_s0->targets[i],
                    temp_s0->targets[i].target.impactKind);
            }

        } else {
            Effects_QueueActionHitEffect(0x26, &temp_s0->targets[i], 0);
        }

        Battle_PlayWeaponHitSounds(arg0);
    }

    return 1;
}

// decomp: BATTLE.PRG func_8006CDB8 (src/BATTLE/BATTLE.PRG/146C.c:2149)
void Effects_SignalActorEffectAnimation(int arg0) { Actors_RequestActorAnimationAdvance(arg0); }

// decomp: BATTLE.PRG func_8006D028 (src/BATTLE/BATTLE.PRG/146C.c:2211)
void Effects_TintActionActorRed(int arg0, int unused __attribute__((unused)))
{
    Effects_tintedActor = arg0;
    Render_TransformActorColors(0xE, 8, arg0, 0x10, 0, 0);
}

// decomp: BATTLE.PRG func_8006D064 (src/BATTLE/BATTLE.PRG/146C.c:2217)
void Effects_RestoreActionActorTint(void)
{
    Render_TransformActorColors(8, 4, Effects_tintedActor, 0, 0, 0);
    Effects_tintedActor = 0;
}

// decomp: BATTLE.PRG func_8006F450 (src/BATTLE/BATTLE.PRG/146C.c:3171)
void Effects_SpawnGroundPlaneImpactEffect(u_short* arg0)
{
    Effects_ActionEffectRequest request;

    request.effectId = 36;
    request.count = 1;
    request.source.type = 4;
    request.otherCount = 0;
    request.source.value.actor.actor = 0;
    request.targets[0].type = 5;
    request.targets[0].value.position.vx = arg0[0];
    request.targets[0].value.position.vz = arg0[2];
    request.targets[0].value.position.vy = 0;
    Effects_BeginSecondaryEffects(&request, 0);
}

// decomp: BATTLE.PRG func_8006F4B0 (src/BATTLE/BATTLE.PRG/146C.c:3187)
void Effects_SpawnTrapTileEffect(Actors_ActorLocation* arg0)
{
    Effects_ActionEffectRequest request;

    request.effectId = 33;
    request.count = 1;
    request.source.type = 4;
    request.otherCount = 0;
    request.source.value.actor.actor = 0;
    request.targets[0].type = 5;
    request.targets[0].value.position.vx = arg0->packedTile.target.tileX * 128 + 64;
    request.targets[0].value.position.vz = arg0->packedTile.target.tileZ * 128 + 64;
    request.targets[0].value.position.vy = arg0->position.vy;
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
void Effects_BeginAshleyStandaloneEffect(int arg0)
{
    Effects_ActionEffectRequest request;

    request.effectId = arg0;
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
void Effects_StartScriptedActorEffect(int arg0, int arg1)
{
    Effects_ActionEffectRequest request;
    int i;

    int count = 0;
    Battle_Actor** actors = Battle_actors;

    request.count = 1;
    request.source.type = 5;
    request.effectId = arg0;
    request.otherCount = 0;
    request.source.value.position.vz = 0;
    request.source.value.position.vy = 0;
    request.source.value.position.vx = 0;
    request.targets[0].type = 4;
    request.targets[0].value.actor.actor = arg1;
    request.targets[0].value.actor.part = 0;

    for (i = 0; i < 16; ++i) {
        if ((actors[i] != NULL) && (i != arg1)) {
            request.others[count] = i;
            ++count;
        }
    }

    request.otherCount = count;
    Effects_BeginActionEffect(&request, 0, 0);
}

// decomp: BATTLE.PRG func_8007BEC4 (src/BATTLE/BATTLE.PRG/146C.c:8611)
void Effects_ReleaseScriptedEffectTrigger(int arg0) { Effects_StartEffectTrack(arg0); }

// decomp: BATTLE.PRG func_8007BEE4 (src/BATTLE/BATTLE.PRG/146C.c:8613)
int Effects_ScriptedEffectPlaybackState(void) { return Effects_PrimaryEffectPhase(); }

// decomp: BATTLE.PRG func_8007BF0C (src/BATTLE/BATTLE.PRG/146C.c:8617)
int Effects_IsScriptedEffectTriggerReady(void) { return Effects_CanStartEffectTrack() != 0; }

