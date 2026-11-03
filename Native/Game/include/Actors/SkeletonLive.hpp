#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <optional>

#include "Actors/Skeleton.hpp"
#include "Core/Math/Matrix.hpp"

extern "C" {
#include "Legacy/Psx.h"
}

struct Actors_ActorRecord;

// Bindings from the actor records, the field gimmicks that share their ids
// and the weapon models to the typed skeleton queries (Skeleton.cpp), in the
// original argument order for the generated callers (tools/port/copy_fixups.py
// OWNED_CALL_BINDINGS). An id names the actor in its slot, else the gimmick
// in that slot. Selectors past the bone matrices, bones past the loaded bone
// table, weapon slots past the weapon table and null outputs are reported on
// the "actors" channel and refused with kSkeletonNoOwner; the originals read
// past them. The bone queries compute without the GTE and leave the model
// workspace alone; only the projections use the GTE.
namespace VS::Actors {
// func_8009E228: the pitch from the actor towards `target`; 0 for no target.
int LiveActorPitchToPoint(const Actors_ActorRecord* actor,
                          const SVECTOR* target);
// func_8009FC60: starts head tracking towards `target`; a null `aim` looks
// along the actor's look angles and kHeadTrackingUsesRadiusStep picks the
// actor's default turn step. kSkeletonNoOwner for no actor and
// kSkeletonNoHeadBone for a model without the head's auxiliary tag.
int SetLiveActorHeadTracking(int actor_id, int target, const SVECTOR* aim,
                             int step);
// func_8009FD38: the actor's default head turn step.
void ResetLiveActorHeadTrackingSpeed(Actors_ActorRecord* actor);

// func_800A1178: the stored matrix and length of the bone with `tag`.
int LiveActorTaggedBoneMatrix(int actor_id, int tag, MATRIX* matrix_out,
                              u_long* length_out);
// func_800A11D8: the stored matrix and length of `bone`; nothing for an empty
// slot.
void LiveActorBoneMatrix(int actor_id, int bone, MATRIX* matrix_out,
                         u_long* length_out);

// func_800A1280: the screen position of the bone with `tag`. The target-point
// selector projects the target point to vx, vy (screen) and vz (depth);
// another tag packs the screen word and depth word of the bone endpoint into
// the record's two words. An actor taking a reaction has no target point.
int ProjectLiveActorTaggedBone(int actor_id, int tag, SVECTOR* screen_out,
                               int endpoint);
// func_800A13EC: the packed screen word and depth of a bone endpoint.
int ProjectLiveActorBone(int actor_id, int bone, int* screen_out, int endpoint);

// func_800A152C: the first bone whose label of `kind` matches `tag`, or
// kSkeletonTagNotFound.
int FindLiveActorBoneTag(int actor_id, int tag, int kind);
// func_800A1648: the bone's label of `kind`, or kSkeletonNoLimbGroup for a
// bone outside every limb group.
int LiveActorBoneTag(int actor_id, int bone, int kind);

// func_800A1720: LiveActorBoneWorldMatrix; the ground-anchor selector reads
// bone 0 lowered to the actor's ground height.
int LiveActorAnchorMatrix(int actor_id, int selector, MATRIX* matrix_out,
                          int* length_out);
// func_800A17BC: the camera-relative transform of the selected bone and its
// length (0 for the root selector).
int LiveActorBoneWorldMatrix(int actor_id, int selector, MATRIX* matrix_out,
                             int* length_out);

// func_800A190C: the camera-relative endpoint of the bone with `tag`, or the
// target, ground or anchor point for those selectors.
int LiveActorTaggedBonePosition(int actor_id, int tag, SVECTOR* position_out,
                                int endpoint);
// func_800A1AF8: the camera-relative endpoint of `bone`.
int LiveActorBonePosition(int actor_id, int bone, SVECTOR* position_out,
                          int endpoint);
// func_800A1B28 and func_800A1B9C: the endpoint of the first bone of a limb
// group, by the high or low limb nibble.
int LiveActorHighGroupBonePosition(int actor_id, int limb,
                                   SVECTOR* position_out, int endpoint);
int LiveActorLowGroupBonePosition(int actor_id, int limb, SVECTOR* position_out,
                                  int endpoint);
// func_800A1C10: the camera-relative endpoint of the selected bone.
int LiveActorBoneEndpointPosition(int actor_id, int selector,
                                  SVECTOR* position_out, int endpoint);
// func_800A1DE8: the selected bone's transform seen from the bone camera; the
// root selector gives the actor's root transform. None for an empty slot or a
// missing weapon.
[[nodiscard]] std::optional<Core::Math::Transform3> LiveActorBoneViewTransform(
    int actor_id, int selector);

// func_800A4828: builds the actor's pose from `camera` without drawing it.
void BuildLiveActorPoseAtMatrix(int actor_id, MATRIX* camera);
}  // namespace VS::Actors
