// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorMovementLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/SkeletonLive.hpp"
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
#include "Battle/ActionAnimation.h"
#include "Field/PlatformPlacementLive.h"
#include "Render/Palette.h"

// decomp: D_800E8E80 (D_800E8E80@BATTLE/BATTLE.PRG)
u_char Battle_chainAbilityAnimationGroups[168]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F49D8 (D_800F49D8@BATTLE/BATTLE.PRG)
int Battle_chainAbilityAnimationSide;

// decomp: D_800F49E4 (D_800F49E4@BATTLE/BATTLE.PRG)
char Battle_specialActionPitch;

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

}  // extern "C"
