// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
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
u_char Battle_chainAbilityAnimationGroups[168]; /* contents: module image (ModuleData.c) */

// decomp: D_800F49D8 (D_800F49D8@BATTLE/BATTLE.PRG)
int Battle_chainAbilityAnimationSide;

// decomp: D_800F49E4 (D_800F49E4@BATTLE/BATTLE.PRG)
char Battle_specialActionPitch;

// decomp: BATTLE.PRG func_8009DD00 (src/BATTLE/BATTLE.PRG/30DB0.c:2331)
void Battle_PlayMeleeAttackAnimation(int arg0, SVECTOR* arg1, int arg2)
{
    int _[2];
    Actors_ActorRecord* temp_s0;
    int temp_v0;
    int var_a1;

    temp_s0 = Actors_actorRecords[arg0];
    if (temp_s0 != NULL) {
        temp_s0->instance.facingDelta = Actors_ActorFacingDeltaToPoint(temp_s0, arg1);
        temp_s0->instance.turnFrames = 0xA;
        if (temp_s0->collisionRadius >= 0x81U) {
            temp_s0->instance.turnFrames = 0x14;
        }

        temp_v0 = Actors_ActorPitchToPoint(temp_s0, arg1);

        if (temp_v0 >= -0xE4) {
            var_a1 = 0x49;
            if (temp_v0 >= 0xAC) {
                var_a1 = 0x4A;
                if ((temp_v0 >= 0x301) && (arg2 == 0) && (temp_s0->modelId == 0x28)) {
                    var_a1 = 0x4E;
                }
            }
        } else {
            var_a1 = 0x48;
        }

        Actors_PlayActorAnimation(arg0, var_a1, 0, 6);
        Actors_ResetActorHeadTrackingSpeed(temp_s0);
    }
}

// decomp: BATTLE.PRG func_8009DDF4 (src/BATTLE/BATTLE.PRG/30DB0.c:2365)
void Battle_PlayDefenseAnimation(int arg0, SVECTOR* arg1)
{
    int _[3] __attribute__((unused));
    Actors_ActorRecord* temp_s0 = Actors_actorRecords[arg0];

    if (temp_s0->instance.weaponDrawn
        && ((temp_s0->instance.slot == 0)
            || (!temp_s0->instance.motionLocked && !temp_s0->instance.isJumping
                && ((temp_s0->instance.movementMode != 2)) && (temp_s0->instance.movementMode != 3)))) {
        int var_s1;
        int var_a1;
        int temp_v0;

        if (temp_s0->collisionRadius < 0x80) {
            temp_s0->instance.facingDelta = Actors_ActorFacingDeltaToPoint(temp_s0, arg1);
            temp_s0->instance.turnFrames = 4;
            var_s1 = 8;
        } else {
            var_s1 = 4;
        }

        temp_v0 = Actors_ActorPitchToPoint(temp_s0, arg1);
        var_a1 = 0x4F;

        if (temp_v0 >= -0xE4) {
            var_a1 = 0x50;
            if (temp_v0 >= 0xAC) {
                var_a1 = 0x51;
            }
        }

        Actors_PlayActorAnimation(arg0, var_a1, 0, var_s1);
        Actors_ResetActorHeadTrackingSpeed(temp_s0);
    }

    if (temp_s0->instance.slot == 0) {
        if (temp_s0->instance.movementMode != 3) {
            temp_s0->instance.movementMode = 0;
        }
        temp_s0->instance.jumpCountdown = 0;
    }
}

// decomp: BATTLE.PRG func_8009DF3C (src/BATTLE/BATTLE.PRG/30DB0.c:2408)
void Battle_SetEnemyCombatPose(int arg0, int arg1)
{
    u_short* sp10;
    Actors_ActorRecord* temp_s2 = Actors_actorRecords[arg0];

    int var_s0 = 0;
    int var_s1 = 0;

    if (temp_s2->instance.slot == 0) {
        return;
    }

    switch (arg1) {
    case 0:
        var_s0 = 6;
        break;
    case 1:
        var_s0 = 0xC6;
        break;
    case 2:
        var_s0 = 0xC8;
        break;
    case 3:
        var_s0 = temp_s2->animationId;
        if ((var_s0 != 0xC6) && (var_s0 != 0xC8)) {
            return;
        }
        ++var_s0;
        var_s1 = 8;
        Actors_FindActorAnimationHeader(temp_s2, var_s0, &sp10);
        if (sp10 == NULL) {
            var_s0 = 6;
            var_s1 = 0x10;
        }
        break;
    }

    Actors_PlayActorAnimation(arg0, var_s0, 0, var_s1);
    if (arg1 < 3) {
        Core_Memory_CopyAlignedWords(&temp_s2->blendPose, &temp_s2->pose, sizeof temp_s2->blendPose);
    }
}

// decomp: BATTLE.PRG func_8009E2E0 (src/BATTLE/BATTLE.PRG/30DB0.c:2554)
void Battle_PlaySpellCastAnimation(int arg0, SVECTOR* arg1, int arg2)
{
    u_char sp10[2] = { 0x55, 0x58 };
    u_char sp18[2] = { 0x57, 0x59 };
    u_short* sp20;
    int angle;
    int diff;
    int anim;
    Actors_ActorRecord* actor = Actors_actorRecords[arg0];

    if (actor == NULL) {
        return;
    }

    if (arg2 == 0) {
        if (arg1 != NULL) {
            int dx = arg1->vx - actor->instance.position.vx;
            int dz = arg1->vz - actor->instance.position.vz;

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

    anim = sp10[arg2];

    if (actor->instance.weaponDrawn) {
        anim += 100;
    }

    Actors_FindActorAnimationHeader(actor, anim, &sp20);

    if (sp20 == 0) {
        anim = sp18[arg2];
        if (actor->instance.weaponDrawn) {
            anim += 100;
        }
    }

    Actors_PlayActorAnimation(arg0, anim, 0, 6);
    Actors_ResetActorHeadTrackingSpeed(actor);
}

// decomp: BATTLE.PRG func_8009EA14 (src/BATTLE/BATTLE.PRG/30DB0.c:2837)
void Battle_PlayActorDamageReaction(int arg0, SVECTOR* arg1)
{
    SVECTOR sp10;
    Actors_ActorRecord* temp_s1;
    int temp_a0;
    int temp_a1;
    int var_s0;
    int temp_v1;
    int temp_v1_2;
    int var_s4;
    int var_v1;
    int var_v1_2;
    u_int var_v0;

    // var_s4 = saved_reg_s4;

    temp_s1 = Actors_actorRecords[arg0];

    if (temp_s1 != NULL) {

        var_s0 = 0x5B;

        if (!(temp_s1->actionPoseActive << 3)) {
            if (arg0 == 0) {

                var_v1 = 0;

                if (arg1 != NULL) {

                    var_s0 = arg1->vx - temp_s1->instance.position.vx;
                    var_s0 *= var_s0;
                    temp_v1 = arg1->vz - temp_s1->instance.position.vz;

                    if (var_s0 < 0) {
                        var_s0 = -var_s0;
                    }

                    var_v1_2 = temp_v1 * temp_v1;

                    if (var_v1_2 < 0) {
                        var_v1_2 = -var_v1_2;
                    }

                    var_s0 = Core_Math_SquareRootInteger(var_s0 + var_v1_2);
                    Actors_ActorBonePosition(temp_s1->instance.slot, 0, &sp10, 0);
                    var_v1 = Core_Gte_Atan2(arg1->vy - sp10.vy, var_s0);
                }

                var_s0 = 0x45;

                if (var_v1 >= -0xE4) {
                    var_s0 = 0x44;
                    if (var_v1 < 0xAC) {
                        goto block_11;
                    }
                }
            } else {
            block_11:
                temp_v1_2 = 0;

                if (arg1 != NULL) {

                    temp_a1 = arg1->vx - temp_s1->instance.position.vx;
                    temp_a0 = arg1->vz - temp_s1->instance.position.vz;

                    if ((temp_a1 == 0) && (temp_a0 == 0)) {
                        temp_v1_2 = temp_s1->instance.facing;
                    } else {
                        temp_v1_2 = (0xC00 - Core_Gte_Atan2(temp_a0, temp_a1)) & 0xFFF;
                    }

                    temp_v1_2 = temp_v1_2 - temp_s1->instance.facing;

                    if (temp_v1_2 > 0x800) {
                        temp_v1_2 = temp_v1_2 - 0x1000;
                    } else {
                        if (temp_v1_2 < -0x800) {
                            temp_v1_2 = temp_v1_2 + 0x1000;
                        }
                    }
                }

                var_v0 = temp_v1_2 + 0x200;
                var_s0 = 0x40;

                if (var_v0 >= 0x801) {
                    var_s0 = 0x42;
                }
            }
        }

        if (temp_s1->instance.locomotionState == 2) {
            if (temp_s1->instance.supportLayer) {
                ((void (*)(int, int))Field_SetMovingPlatformActor)(temp_s1->instance.supportLayer - 2, -1);
                temp_s1->instance.supportLayer = 0;
            }
            var_s0 = 0x3B;
        } else {
            if (temp_s1->instance.reactionState == 3) {
                Actors_CancelActorTeleportTransition(temp_s1);
            }
            temp_s1->instance.turnFrames = 0;
            if (temp_s1->instance.slot == 0) {
                temp_s1->instance.jumpCountdown = 0;
                temp_s1->instance.locomotionState = 0;
                temp_s1->instance.movementPending = 0;
                temp_s1->instance.movementMode = 0;
                var_s4 = 0;
            } else {
                var_s4 = 8;
            }
        }
        Actors_ClearActorMovementRequest(temp_s1);
        temp_s1->animationSet = 0xFF;
        Actors_PlayActorAnimation(arg0, var_s0, 0, var_s4);
        Actors_ResetActorHeadTrackingSpeed(temp_s1);
    }
}

// decomp: BATTLE.PRG func_8009EC9C (src/BATTLE/BATTLE.PRG/30DB0.c:2956)
void Battle_PlayChainAbilityAnimation(int arg0, SVECTOR* arg1, int arg2, int arg3)
{
    SVECTOR sp10;
    int dx;
    int dz;
    int z;
    int zz;
    int dist;
    int pitch;
    int angle;
    int turn;
    int anim;
    Actors_ActorRecord* actor = Actors_actorRecords[arg0];

    if (actor == NULL) {
        return;
    }

    anim = Battle_chainAbilityAnimationGroups[arg2] * 6 + 0xCA;

    if (arg1 == NULL) {
        pitch = 0;
    } else {
        dist = arg1->vx - actor->instance.position.vx;
        dist = dist * dist;
        z = arg1->vz - actor->instance.position.vz;

        if (dist < 0) {
            dist = -dist;
        }

        zz = z * z;

        if (zz < 0) {
            zz = -zz;
        }

        dist = Core_Math_SquareRootInteger(dist + zz);
        Actors_ActorBonePosition(actor->instance.slot, 0, &sp10, 0);
        pitch = Core_Gte_Atan2(arg1->vy - sp10.vy, dist);
    }

    if (pitch >= -0xE4) {
        if (pitch >= 0xAC) {
            anim += 4;
        } else {
            anim += 2;
        }
    }

    if (arg3 == 1) {
        Battle_chainAbilityAnimationSide = arg3;
    }

    Battle_chainAbilityAnimationSide ^= 1;
    anim += Battle_chainAbilityAnimationSide;

    if (arg1 == NULL) {
        turn = 0;
    } else {
        dx = arg1->vx - actor->instance.position.vx;
        dz = arg1->vz - actor->instance.position.vz;

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
    Actors_PlayActorAnimation(arg0, anim, 0, 8);
}

// decomp: BATTLE.PRG func_8009EE9C (src/BATTLE/BATTLE.PRG/30DB0.c:3040)
void Battle_PlayDefenseAbilityAnimation(int arg0, SVECTOR* arg1, int arg2)
{
    int _[3] __attribute__((unused));
    Actors_ActorRecord* temp_s0 = Actors_actorRecords[arg0];

    if (temp_s0->instance.weaponDrawn
        && ((temp_s0->instance.slot == 0)
            || (!temp_s0->instance.motionLocked && !temp_s0->instance.isJumping
                && (temp_s0->instance.movementMode != 2) && (temp_s0->instance.movementMode != 3)))) {
        int temp_v0;
        int var_a1;
        int var_s1;

        if (temp_s0->collisionRadius < 0x80) {
            temp_s0->instance.facingDelta = Actors_ActorFacingDeltaToPoint(temp_s0, arg1);
            temp_s0->instance.turnFrames = 4;
            var_s1 = 8;
        } else {
            var_s1 = 4;
        }

        temp_v0 = Actors_ActorPitchToPoint(temp_s0, arg1);
        var_a1 = 0x4F;

        if (temp_v0 >= -0xE4) {
            var_a1 = 0x50;
            if (temp_v0 >= 0xAC) {
                var_a1 = 0x51;
            }
        }

        Actors_PlayActorAnimation(arg0, var_a1, 0, var_s1);
        Actors_ResetActorHeadTrackingSpeed(temp_s0);
    }

    if (temp_s0->instance.slot == 0) {
        if (temp_s0->instance.movementMode != 3) {
            temp_s0->instance.movementMode = 0;
        }
        temp_s0->instance.jumpCountdown = 0;
    }

    Effects_QueueActionImpactEffects(arg0);
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
void Battle_PlayBreakArtAnimation(int arg0, SVECTOR* arg1, int arg2)
{
    u_short* sp10;

    Battle_PlaySpecialActionAnimation(arg0, arg1, arg2);
    if (arg2 == 1) {
        Actors_FindActorAnimationHeader(Actors_actorRecords[arg0], 0xFE, &sp10);
        if (sp10 != 0) {
            Actors_QueueActorAnimation((int)arg0, 0xFE, 8, 0);
        }
    }
}

// decomp: BATTLE.PRG func_800A2574 (src/BATTLE/BATTLE.PRG/38C1C.c:625)
void Battle_FinishEnemyCombatPose(int arg0, short arg1)
{
    Actors_ActorRecord* temp_s1 = Actors_actorRecords[arg0];
    ((void (*)(int, int, int))Actors_TurnActorToAtRate)(arg0, arg1, 8);
    Battle_SetEnemyCombatPose(arg0, 3);
    temp_s1->instance.poseState = 6;
}

