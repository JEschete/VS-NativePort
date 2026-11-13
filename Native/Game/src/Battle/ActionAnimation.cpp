// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorMovementLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/WordCopyLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/ActionAnimation.h"
#include "Effects/ActionEffects.h"
#include "Field/PlatformPlacementLive.h"
#include "Render/Palette.h"

// decomp: D_800E8E80 (D_800E8E80@BATTLE/BATTLE.PRG)
u_char Battle_chainAbilityAnimationGroups[168]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F49D8 (D_800F49D8@BATTLE/BATTLE.PRG)
int Battle_chainAbilityAnimationSide;

// decomp: D_800F49E4 (D_800F49E4@BATTLE/BATTLE.PRG)
char Battle_specialActionPitch;

// decomp: BATTLE.PRG func_8009DF3C (src/BATTLE/BATTLE.PRG/30DB0.c:2408)
void Battle_SetEnemyCombatPose(int actor_slot, int pose_mode)
{
    u_short* animation_header;
    Actors_ActorRecord* actor = Actors_actorRecords[actor_slot];

    int animation_id = 0;
    int blend_frames = 0;

    if (actor->instance.slot == 0) {
        return;
    }

    switch (pose_mode) {
    case 0:
        animation_id = 6;
        break;
    case 1:
        animation_id = 0xC6;
        break;
    case 2:
        animation_id = 0xC8;
        break;
    case 3:
        animation_id = actor->animationId;
        if ((animation_id != 0xC6) && (animation_id != 0xC8)) {
            return;
        }
        ++animation_id;
        blend_frames = 8;
        VS::Actors::FindLiveActorAnimationHeader(actor, animation_id, &animation_header);
        if (animation_header == NULL) {
            animation_id = 6;
            blend_frames = 0x10;
        }
        break;
    }

    VS::Actors::PlayLiveActorAnimation(actor_slot, animation_id, 0, blend_frames);
    if (pose_mode < 3) {
        VS::Core::Memory::CopyLiveAlignedWords(&actor->blendPose, &actor->pose, sizeof actor->blendPose);
    }
}

// decomp: BATTLE.PRG func_8009EA14 (src/BATTLE/BATTLE.PRG/30DB0.c:2837)
void Battle_PlayActorDamageReaction(int actor_slot, SVECTOR* source_position)
{
    SVECTOR bone_position;
    Actors_ActorRecord* actor;
    int source_delta_z;
    int source_delta_x;
    int var_s0;
    int distance_delta_z;
    int source_facing_delta;
    int blend_frames;
    int source_pitch;
    int distance_z_squared;
    u_int biased_facing_delta;

    // var_s4 = saved_reg_s4;

    actor = Actors_actorRecords[actor_slot];

    if (actor != NULL) {

        var_s0 = 0x5B;

        if (!(actor->actionPoseActive << 3)) {
            if (actor_slot == 0) {

                source_pitch = 0;

                if (source_position != NULL) {

                    var_s0 = source_position->vx - actor->instance.position.vx;
                    var_s0 *= var_s0;
                    distance_delta_z = source_position->vz - actor->instance.position.vz;

                    if (var_s0 < 0) {
                        var_s0 = -var_s0;
                    }

                    distance_z_squared = distance_delta_z * distance_delta_z;

                    if (distance_z_squared < 0) {
                        distance_z_squared = -distance_z_squared;
                    }

                    var_s0 = VS::Core::Math::SquareRootInteger(var_s0 + distance_z_squared);
                    VS::Actors::LiveActorBonePosition(actor->instance.slot, 0, &bone_position, 0);
                    source_pitch = VS::Core::Math::Arctangent2(source_position->vy - bone_position.vy, var_s0);
                }

                var_s0 = 0x45;

                if (source_pitch >= -0xE4) {
                    var_s0 = 0x44;
                    if (source_pitch < 0xAC) {
                        goto block_11;
                    }
                }
            } else {
            block_11:
                source_facing_delta = 0;

                if (source_position != NULL) {

                    source_delta_x = source_position->vx - actor->instance.position.vx;
                    source_delta_z = source_position->vz - actor->instance.position.vz;

                    if ((source_delta_x == 0) && (source_delta_z == 0)) {
                        source_facing_delta = actor->instance.facing;
                    } else {
                        source_facing_delta = (0xC00 - VS::Core::Math::Arctangent2(source_delta_z, source_delta_x)) & 0xFFF;
                    }

                    source_facing_delta = source_facing_delta - actor->instance.facing;

                    if (source_facing_delta > 0x800) {
                        source_facing_delta = source_facing_delta - 0x1000;
                    } else {
                        if (source_facing_delta < -0x800) {
                            source_facing_delta = source_facing_delta + 0x1000;
                        }
                    }
                }

                biased_facing_delta = source_facing_delta + 0x200;
                var_s0 = 0x40;

                if (biased_facing_delta >= 0x801) {
                    var_s0 = 0x42;
                }
            }
        }

        if (actor->instance.locomotionState == 2) {
            if (actor->instance.supportLayer) {
                ((void (*)(int, int))Field_SetMovingPlatformActorEntry)(actor->instance.supportLayer - 2, -1);
                actor->instance.supportLayer = 0;
            }
            var_s0 = 0x3B;
        } else {
            if (actor->instance.reactionState == 3) {
                VS::Actors::CancelLiveActorTeleportTransition(actor);
            }
            actor->instance.turnFrames = 0;
            if (actor->instance.slot == 0) {
                actor->instance.jumpCountdown = 0;
                actor->instance.locomotionState = 0;
                actor->instance.movementPending = 0;
                actor->instance.movementMode = 0;
                blend_frames = 0;
            } else {
                blend_frames = 8;
            }
        }
        VS::Actors::ClearLiveActorMovementRequest(actor);
        actor->animationSet = 0xFF;
        VS::Actors::PlayLiveActorAnimation(actor_slot, var_s0, 0, blend_frames);
        VS::Actors::ResetLiveActorHeadTrackingSpeed(actor);
    }
}

// decomp: BATTLE.PRG func_8009EE9C (src/BATTLE/BATTLE.PRG/30DB0.c:3040)
void Battle_PlayDefenseAbilityAnimation(int actor_slot, SVECTOR* target_position, int arg2)
{
    int _[3] __attribute__((unused));
    Actors_ActorRecord* actor = Actors_actorRecords[actor_slot];

    if (actor->instance.weaponDrawn
        && ((actor->instance.slot == 0)
            || (!actor->instance.motionLocked && !actor->instance.isJumping
                && (actor->instance.movementMode != 2) && (actor->instance.movementMode != 3)))) {
        int target_pitch;
        int animation_id;
        int blend_frames;

        if (actor->collisionRadius < 0x80) {
            actor->instance.facingDelta = VS::Actors::LiveActorFacingDeltaToPoint(actor, target_position);
            actor->instance.turnFrames = 4;
            blend_frames = 8;
        } else {
            blend_frames = 4;
        }

        target_pitch = VS::Actors::LiveActorPitchToPoint(actor, target_position);
        animation_id = 0x4F;

        if (target_pitch >= -0xE4) {
            animation_id = 0x50;
            if (target_pitch >= 0xAC) {
                animation_id = 0x51;
            }
        }

        VS::Actors::PlayLiveActorAnimation(actor_slot, animation_id, 0, blend_frames);
        VS::Actors::ResetLiveActorHeadTrackingSpeed(actor);
    }

    if (actor->instance.slot == 0) {
        if (actor->instance.movementMode != 3) {
            actor->instance.movementMode = 0;
        }
        actor->instance.jumpCountdown = 0;
    }

    Effects_QueueActionImpactEffects(actor_slot);
}

// decomp: BATTLE.PRG func_800A2574 (src/BATTLE/BATTLE.PRG/38C1C.c:625)
void Battle_FinishEnemyCombatPose(int actor_slot, short facing)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_slot];
    VS::Actors::TurnLiveActorToAtRate(actor_slot, facing, 8);
    Battle_SetEnemyCombatPose(actor_slot, 3);
    actor->instance.poseState = 6;
}

}  // extern "C"
