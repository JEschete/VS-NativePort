// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_SKELETON_H_TYPES
#define VS_ACTORS_SKELETON_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_ShpBone Actors_ShpBone;
typedef struct Actors_ShpModel Actors_ShpModel;

// decomp: _armature_t (src/BATTLE/BATTLE.PRG/3A1A0.h:83)
typedef struct Actors_ShpBone {
    int length;
    u_char parentIndex;
    u_char reserved5;
    u_char tag;
    u_char limbIds;
    u_char auxiliaryTag;
    u_char reserved9;
    u_short jointInitValue;
    u_char poseLockSlot;
    u_char reservedD;
    u_char reservedE;
    u_char reservedF;
} Actors_ShpBone;
_Static_assert(sizeof(Actors_ShpBone) == 0x10, "Actors_ShpBone must keep its PS1 layout");

// decomp: D_800F4538_unk68 (src/BATTLE/BATTLE.PRG/3A1A0.h:92)
typedef struct Actors_ShpModel {
    u_char header[48];
    u_int soundSection; /* offset from the struct (on-disc pointer) */
    u_int groupSection; /* offset from the struct (on-disc pointer) */
    u_int vertexSection; /* offset from the struct (on-disc pointer) */
    u_int polygonSection; /* offset from the struct (on-disc pointer) */
    Actors_ShpBone bones[0];
} Actors_ShpModel;
_Static_assert(sizeof(Actors_ShpModel) == 0x40, "Actors_ShpModel must keep its PS1 layout");

#endif // VS_ACTORS_SKELETON_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_SKELETON_H_DECLS)
#define VS_ACTORS_SKELETON_H_DECLS
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/TextureAnimation.h"
#include "Actors/WeaponModel.h"
#include "Render/Palette.h"

extern Actors_SkeletonPose Actors_skeletonPose;
extern MATRIX Actors_skeletonRootMatrix;
extern Actors_WeaponModel* Actors_weaponModels[20];
extern MATRIX Actors_boneMatrixScratch;
int Actors_ActorPitchToPoint(Actors_ActorRecord* actor, SVECTOR* target_point);
int Actors_SetActorHeadTracking(int actor_id, int tracking_target, SVECTOR* tracking_vector, int tracking_step);
void Actors_ResetActorHeadTrackingSpeed(Actors_ActorRecord* actor);
int Actors_ActorTaggedBoneMatrix(int actor_id, int bone_tag, MATRIX* matrix_out, u_long* bone_length_out);
void Actors_ActorBoneMatrix(int actor_id, int bone_index, MATRIX* matrix_out, u_long* bone_length_out);
int Actors_ProjectActorTaggedBone(int actor_id, int bone_tag, SVECTOR* screen_position_out, int endpoint);
int Actors_ProjectActorBone(int actor_id, int bone_index, int* screen_position_out, int endpoint);
int Actors_FindActorBoneTag(int actor_id, int tag_value, int tag_kind);
int Actors_ActorBoneTag(int actor_id, int bone_index, int tag_kind);
int Actors_ActorAnchorMatrix(int actor_id, int bone_selector, int* matrix_out, int* bone_length_out);
int Actors_ActorBoneWorldMatrix(int actor_id, int bone_selector, void* matrix_out, int* bone_length_out);
int Actors_ActorTaggedBonePosition(int actor_id, int bone_tag, SVECTOR* position_out, int endpoint);
int Actors_ActorBonePosition(int actor_id, int bone_index, SVECTOR* position_out, int endpoint);
int Actors_ActorHighGroupBonePosition(int actor_id, int limb_index, SVECTOR* position_out, int endpoint);
int Actors_ActorLowGroupBonePosition(int actor_id, int limb_index, SVECTOR* position_out, int endpoint);
int Actors_ActorBoneEndpointPosition(int actorId, int bone, SVECTOR* result, int endpoint);
MATRIX* Actors_ActorBoneWorldTransform(int actorId, int bone, MATRIX* unused);
void Actors_BuildActorPoseAtMatrix(int actor_id, MATRIX* camera_matrix);

#endif // VS_ACTORS_SKELETON_H_DECLS
