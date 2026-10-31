// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
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
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/WordCopy.h"
#include "Effects/ActionEffects.h"
#include "Field/Gimmicks.h"
#include "Render/Palette.h"

// decomp: D_800E8E80 (D_800E8E80@BATTLE/BATTLE.PRG)
u_char Battle_chainAbilityAnimationGroups[168]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F49D8 (D_800F49D8@BATTLE/BATTLE.PRG)
int Battle_chainAbilityAnimationSide;

// decomp: D_800F49E4 (D_800F49E4@BATTLE/BATTLE.PRG)
char Battle_specialActionPitch;

// decomp: BATTLE.PRG func_8009DD00 (src/BATTLE/BATTLE.PRG/30DB0.c:2331)
void Battle_PlayMeleeAttackAnimation(int actor_slot, SVECTOR* target_position, int animation_parameter)
{
    int _[2];
    Actors_ActorRecord* actor;
    int target_pitch;
    int animation_id;

    actor = Actors_actorRecords[actor_slot];
    if (actor != NULL) {
        actor->instance.facingDelta = Actors_ActorFacingDeltaToPoint(actor, target_position);
        actor->instance.turnFrames = 0xA;
        if (actor->collisionRadius >= 0x81U) {
            actor->instance.turnFrames = 0x14;
        }

        target_pitch = Actors_ActorPitchToPoint(actor, target_position);

        if (target_pitch >= -0xE4) {
            animation_id = 0x49;
            if (target_pitch >= 0xAC) {
                animation_id = 0x4A;
                if ((target_pitch >= 0x301) && (animation_parameter == 0) && (actor->modelId == 0x28)) {
                    animation_id = 0x4E;
                }
            }
        } else {
            animation_id = 0x48;
        }

        Actors_PlayActorAnimation(actor_slot, animation_id, 0, 6);
        Actors_ResetActorHeadTrackingSpeed(actor);
    }
}

// decomp: BATTLE.PRG func_8009DDF4 (src/BATTLE/BATTLE.PRG/30DB0.c:2365)
void Battle_PlayDefenseAnimation(int actor_slot, SVECTOR* target_position)
{
    int _[3] __attribute__((unused));
    Actors_ActorRecord* actor = Actors_actorRecords[actor_slot];

    if (actor->instance.weaponDrawn
        && ((actor->instance.slot == 0)
            || (!actor->instance.motionLocked && !actor->instance.isJumping
                && ((actor->instance.movementMode != 2)) && (actor->instance.movementMode != 3)))) {
        int blend_frames;
        int animation_id;
        int target_pitch;

        if (actor->collisionRadius < 0x80) {
            actor->instance.facingDelta = Actors_ActorFacingDeltaToPoint(actor, target_position);
            actor->instance.turnFrames = 4;
            blend_frames = 8;
        } else {
            blend_frames = 4;
        }

        target_pitch = Actors_ActorPitchToPoint(actor, target_position);
        animation_id = 0x4F;

        if (target_pitch >= -0xE4) {
            animation_id = 0x50;
            if (target_pitch >= 0xAC) {
                animation_id = 0x51;
            }
        }

        Actors_PlayActorAnimation(actor_slot, animation_id, 0, blend_frames);
        Actors_ResetActorHeadTrackingSpeed(actor);
    }

    if (actor->instance.slot == 0) {
        if (actor->instance.movementMode != 3) {
            actor->instance.movementMode = 0;
        }
        actor->instance.jumpCountdown = 0;
    }
}

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
        Actors_FindActorAnimationHeader(actor, animation_id, &animation_header);
        if (animation_header == NULL) {
            animation_id = 6;
            blend_frames = 0x10;
        }
        break;
    }

    Actors_PlayActorAnimation(actor_slot, animation_id, 0, blend_frames);
    if (pose_mode < 3) {
        Core_Memory_CopyAlignedWords(&actor->blendPose, &actor->pose, sizeof actor->blendPose);
    }
}

// decomp: BATTLE.PRG func_8009E2E0 (src/BATTLE/BATTLE.PRG/30DB0.c:2554)
void Battle_PlaySpellCastAnimation(int actor_slot, SVECTOR* target_position, int cast_phase)
{
    u_char cast_animations[2] = { 0x55, 0x58 };
    u_char fallback_cast_animations[2] = { 0x57, 0x59 };
    u_short* animation_header;
    int angle;
    int diff;
    int anim;
    Actors_ActorRecord* actor = Actors_actorRecords[actor_slot];

    if (actor == NULL) {
        return;
    }

    if (cast_phase == 0) {
        if (target_position != NULL) {
            int dx = target_position->vx - actor->instance.position.vx;
            int dz = target_position->vz - actor->instance.position.vz;

            if (dx == 0 && dz == 0) {
                angle = actor->instance.facing;
            } else {
                angle = (ONE * 3 / 4 - Core_Gte_Atan2(dz, dx)) & 0xFFF;
            }

            angle -= actor->instance.facing;

            if (angle > ONE / 2) {
                angle -= ONE;
            } else if (angle < -ONE / 2) {
                angle += ONE;
            }

            diff = angle;
        } else {
            diff = 0;
        }

        actor->instance.facingDelta = diff;
        actor->instance.turnFrames = 10;

        if (actor->collisionRadius > 0x80) {
            actor->instance.turnFrames = 20;
        }
    }

    anim = cast_animations[cast_phase];

    if (actor->instance.weaponDrawn) {
        anim += 100;
    }

    Actors_FindActorAnimationHeader(actor, anim, &animation_header);

    if (animation_header == 0) {
        anim = fallback_cast_animations[cast_phase];
        if (actor->instance.weaponDrawn) {
            anim += 100;
        }
    }

    Actors_PlayActorAnimation(actor_slot, anim, 0, 6);
    Actors_ResetActorHeadTrackingSpeed(actor);
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

                    var_s0 = Core_Math_SquareRootInteger(var_s0 + distance_z_squared);
                    Actors_ActorBonePosition(actor->instance.slot, 0, &bone_position, 0);
                    source_pitch = Core_Gte_Atan2(source_position->vy - bone_position.vy, var_s0);
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
                        source_facing_delta = (0xC00 - Core_Gte_Atan2(source_delta_z, source_delta_x)) & 0xFFF;
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
                ((void (*)(int, int))Field_SetMovingPlatformActor)(actor->instance.supportLayer - 2, -1);
                actor->instance.supportLayer = 0;
            }
            var_s0 = 0x3B;
        } else {
            if (actor->instance.reactionState == 3) {
                Actors_CancelActorTeleportTransition(actor);
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
        Actors_ClearActorMovementRequest(actor);
        actor->animationSet = 0xFF;
        Actors_PlayActorAnimation(actor_slot, var_s0, 0, blend_frames);
        Actors_ResetActorHeadTrackingSpeed(actor);
    }
}

// decomp: BATTLE.PRG func_8009EC9C (src/BATTLE/BATTLE.PRG/30DB0.c:2956)
void Battle_PlayChainAbilityAnimation(int actor_slot, SVECTOR* target_position, int chain_ability_index, int chain_index)
{
    SVECTOR bone_position;
    int dx;
    int dz;
    int z;
    int zz;
    int dist;
    int pitch;
    int angle;
    int turn;
    int anim;
    Actors_ActorRecord* actor = Actors_actorRecords[actor_slot];

    if (actor == NULL) {
        return;
    }

    anim = Battle_chainAbilityAnimationGroups[chain_ability_index] * 6 + 0xCA;

    if (target_position == NULL) {
        pitch = 0;
    } else {
        dist = target_position->vx - actor->instance.position.vx;
        dist = dist * dist;
        z = target_position->vz - actor->instance.position.vz;

        if (dist < 0) {
            dist = -dist;
        }

        zz = z * z;

        if (zz < 0) {
            zz = -zz;
        }

        dist = Core_Math_SquareRootInteger(dist + zz);
        Actors_ActorBonePosition(actor->instance.slot, 0, &bone_position, 0);
        pitch = Core_Gte_Atan2(target_position->vy - bone_position.vy, dist);
    }

    if (pitch >= -0xE4) {
        if (pitch >= 0xAC) {
            anim += 4;
        } else {
            anim += 2;
        }
    }

    if (chain_index == 1) {
        Battle_chainAbilityAnimationSide = chain_index;
    }

    Battle_chainAbilityAnimationSide ^= 1;
    anim += Battle_chainAbilityAnimationSide;

    if (target_position == NULL) {
        turn = 0;
    } else {
        dx = target_position->vx - actor->instance.position.vx;
        dz = target_position->vz - actor->instance.position.vz;

        if ((dx == 0) && (dz == 0)) {
            angle = actor->instance.facing;
        } else {
            angle = (ONE * 3 / 4 - Core_Gte_Atan2(dz, dx)) & 0xFFF;
        }

        angle -= actor->instance.facing;

        if (angle > ONE / 2) {
            angle -= ONE;
        } else if (angle < -ONE / 2) {
            angle += ONE;
        }
        turn = angle;
    }

    actor->instance.facingDelta = turn;
    actor->instance.turnFrames = 0xA;
    Actors_PlayActorAnimation(actor_slot, anim, 0, 8);
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
            actor->instance.facingDelta = Actors_ActorFacingDeltaToPoint(actor, target_position);
            actor->instance.turnFrames = 4;
            blend_frames = 8;
        } else {
            blend_frames = 4;
        }

        target_pitch = Actors_ActorPitchToPoint(actor, target_position);
        animation_id = 0x4F;

        if (target_pitch >= -0xE4) {
            animation_id = 0x50;
            if (target_pitch >= 0xAC) {
                animation_id = 0x51;
            }
        }

        Actors_PlayActorAnimation(actor_slot, animation_id, 0, blend_frames);
        Actors_ResetActorHeadTrackingSpeed(actor);
    }

    if (actor->instance.slot == 0) {
        if (actor->instance.movementMode != 3) {
            actor->instance.movementMode = 0;
        }
        actor->instance.jumpCountdown = 0;
    }

    Effects_QueueActionImpactEffects(actor_slot);
}

// decomp: BATTLE.PRG func_8009EFEC (src/BATTLE/BATTLE.PRG/30DB0.c:3087)
void Battle_PlaySpecialActionAnimation(int actorId, SVECTOR* target, int mode)
{
    u_short* animation;
    Actors_ActorRecord* fallbackActor;
    Actors_ActorRecord* actor;
    int facingDelta;
    int angle;
    int dz;
    int dx;
    int pitch;
    int reactionAnimation;
    int value;
    int verticalAngle;

    actor = Actors_actorRecords[actorId];
    if (mode == 0) {
        if (target != NULL) {
            dx = target->vx - actor->instance.position.vx;
            dz = target->vz - actor->instance.position.vz;
            if ((dx == 0) && (dz == 0)) {
                angle = actor->instance.facing;
            } else {
                angle = (ONE * 3 / 4 - Core_Gte_Atan2(dz, dx)) & 0xFFF;
            }
            angle = angle - actor->instance.facing;
            if (angle > ONE / 2) {
                angle -= ONE;
            } else if (angle < -ONE / 2) {
                angle += ONE;
            }
            facingDelta = angle;
        } else {
            facingDelta = 0;
        }
        actor->instance.facingDelta = facingDelta;
        actor->instance.turnFrames = 0xA;
        if ((u_short)actor->collisionRadius >= 0x80U) {
            actor->instance.turnFrames = 0x14;
        }
        Actors_FindActorAnimationHeader(actor, 0xBFU, &animation);
        value = 0xBF;
        if (animation == NULL) {
            value = 0xC1;
        }
        Actors_PlayActorAnimation(actorId, value, 0, 8);
        Actors_ResetActorHeadTrackingSpeed(actor);
        return;
    } else if (mode == 1) {
        Actors_FindActorAnimationHeader(actor, 0xFFU, &animation);
        if (animation == NULL) {
            fallbackActor = Actors_actorRecords[actorId];
            if (fallbackActor != NULL) {
                fallbackActor->instance.facingDelta = Actors_ActorFacingDeltaToPoint(fallbackActor, target);
                fallbackActor->instance.turnFrames = 0xA;
                if ((u_short)fallbackActor->collisionRadius >= 0x81U) {
                    fallbackActor->instance.turnFrames = 0x14;
                }
                pitch = Actors_ActorPitchToPoint(fallbackActor, target);
                reactionAnimation = 0x48;
                if (pitch >= -0xE4) {
                    reactionAnimation = 0x49;
                    if (pitch >= 0xAC) {
                        reactionAnimation = 0x4A;
                    }
                }
                Actors_PlayActorAnimation(actorId, reactionAnimation, 0, 6);
                Actors_ResetActorHeadTrackingSpeed(fallbackActor);
                return;
            }
        } else {
            SVECTOR origin;
            verticalAngle = 0;
            if (target != NULL) {
                value = target->vx - actor->instance.position.vx;
                value = value * value;
                verticalAngle = target->vz - actor->instance.position.vz;
                if (value < 0) {
                    value = -value;
                }
                verticalAngle = verticalAngle * verticalAngle;
                if (verticalAngle < 0) {
                    verticalAngle = -verticalAngle;
                }
                value = Core_Math_SquareRootInteger(value + verticalAngle);
                Actors_ActorBonePosition(actor->instance.slot, 0, &origin, 0);
                verticalAngle = Core_Gte_Atan2(target->vy - origin.vy, value);
            }
            if (verticalAngle < -0xE4) {
                Battle_specialActionPitch = 0;
            } else {
                if (verticalAngle >= 0xAC) {
                    Battle_specialActionPitch = 2;
                } else {
                    Battle_specialActionPitch = 1;
                }
            }
            Actors_PlayActorAnimation(actorId, 0xFF, 0, 8);
            return;
        }
    }
}

// decomp: BATTLE.PRG func_8009F298 (src/BATTLE/BATTLE.PRG/30DB0.c:3189)
void Battle_PlayBreakArtAnimation(int actor_slot, SVECTOR* target_position, int action_phase)
{
    u_short* animation_header;

    Battle_PlaySpecialActionAnimation(actor_slot, target_position, action_phase);
    if (action_phase == 1) {
        Actors_FindActorAnimationHeader(Actors_actorRecords[actor_slot], 0xFE, &animation_header);
        if (animation_header != 0) {
            Actors_QueueActorAnimation((int)actor_slot, 0xFE, 8, 0);
        }
    }
}

// decomp: BATTLE.PRG func_800A2574 (src/BATTLE/BATTLE.PRG/38C1C.c:625)
void Battle_FinishEnemyCombatPose(int actor_slot, short facing)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_slot];
    ((void (*)(int, int, int))Actors_TurnActorToAtRate)(actor_slot, facing, 8);
    Battle_SetEnemyCombatPose(actor_slot, 3);
    actor->instance.poseState = 6;
}

}  // extern "C"
