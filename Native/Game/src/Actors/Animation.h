// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_ANIMATION_H_TYPES
#define VS_ACTORS_ANIMATION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_AnimationPose Actors_AnimationPose;
typedef struct Actors_KeyframeRangeScratch Actors_KeyframeRangeScratch;
typedef struct Actors_SeqAnimationBank Actors_SeqAnimationBank;

// decomp: D_1F8003EC_t (src/BATTLE/BATTLE.PRG/44F14.c:18)
typedef struct Actors_KeyframeRangeScratch {
    u_char reserved0[8];
    int firstFrame;
    int lastFrame;
    u_char** trackData;
} Actors_KeyframeRangeScratch;

// layout: ledger (upstream merge) for D_800F4538_unkC54
typedef struct Actors_AnimationPose {
    /* 0x00 */ SVECTOR boneKeys[42];
    /* 0x150 */ SVECTOR scaleKeys[42];
    /* 0x2A0 */ SVECTOR reserved2A0[42];
    /* 0x3F0 */ SVECTOR trackStates[42];
    /* 0x540 */ short frame;
    /* 0x542 */ u_short nextEventFrame;
    /* 0x544 */ u_char *eventScript;
    /* 0x548 */ u_char animationId;
    /* 0x549 */ u_char eventWait;
    /* 0x54A */ int holdingLastFrame : 1;
    /* 0x54A */ int reachedEnd : 1;
    /* 0x54A */ int reservedBits : 14;
    /* 0x54C */ int reserved54C;
} Actors_AnimationPose;

// decomp: func_800AD494_t (src/BATTLE/BATTLE.PRG/40564.c:18)
typedef struct Actors_SeqAnimationBank {
    int animationData;
    u_int animationMap; /* offset from the struct (on-disc pointer) */
    u_short headers[0];
} Actors_SeqAnimationBank;
_Static_assert(sizeof(Actors_SeqAnimationBank) == 0x8, "Actors_SeqAnimationBank must keep its PS1 layout");

#endif // VS_ACTORS_ANIMATION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_ANIMATION_H_DECLS)
#define VS_ACTORS_ANIMATION_H_DECLS
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

extern u_char Actors_sharedSequenceIdsByKind[592];
extern u_char Actors_sharedSequenceBankIndex;
extern char Actors_reservedControlByte;
extern u_char Actors_drawnWeaponAnimationBases[3232];
extern u_char Actors_sheathedWeaponAnimationBases[3220];
extern Actors_TextureAnimationKey Actors_secondarySlotSequences[188][8];
extern u_char Actors_animationEventLengths[2712];
extern u_char Actors_drawnWeaponSoundIds[2644];
extern u_short Actors_animationEventPoints[97][4];
extern u_char Actors_sequenceTexturePagesOneBased[8425];
extern u_char Actors_sharedSequenceBankTexturePages[8424];
extern void* Actors_sharedSequenceData[4];
extern short Actors_sharedSequenceAnimationIds[4][16];
extern void* Actors_sequenceWorkBuffer;
extern char* Actors_sharedSequenceAnimationData[4][16];
extern void* Actors_primarySequenceBuffer;
extern short Actors_sharedSequenceEntryParams[4][16];
extern char Actors_lastResolvedSequenceIndex;
void Actors_QueueActorBaseSequenceLoad(Battle_Actor* arg0 __attribute__((unused)), int arg1);
void Actors_QueueActorWeaponSequenceLoad(char* arg0, int arg1, int arg2, int arg3);
void Actors_InterruptActorAction(int arg0, int arg1);
void Actors_QueueSharedSequenceLoad(int index, int wepId);
void Actors_UnloadSharedSequence(int arg0);
void Actors_QueueSharedSequenceWithTextures(int index, int wepId, int arg2);
void Actors_QueueSequenceEffectTextures(int index, int wepId, int arg2);
void Actors_ReleaseSequenceEffectTextures(int arg0);
void Actors_SetSharedSequenceBank(int arg0);
void Actors_SharedSequenceBank(void);
void Actors_ReturnActorToIdle(int arg0);
void Actors_ReturnAllActorsToIdle(void);
int Actors_SelectSharedSequenceBank(u_int arg0);
int Actors_SharedSequenceBankId(void);
void Actors_RequestActorAnimationAdvance(int arg0);
void Actors_SetActorWeaponDrawn(int arg0, int arg1, int arg2);
void Actors_PlayActorWeaponDrawSound(int arg0, int arg1);
void Actors_PlayActorActionPose(int arg0, SVECTOR* arg1, int arg2);
int Actors_IsAshleyTransitionPose(void);
int Actors_IsActorActionAnimationBusy(int arg0);
void Actors_RestoreActorIdlePose(int arg0);
void Actors_RestoreActorNeutralPose(int arg0);
void Actors_RestoreActorBasePose(int actorId, int animation);
int Actors_AnimationBattleWindowFrame(int arg0);
void Actors_SetActorSoundOverrides(int arg0, Actors_ModelEffectSlot* arg1);
void Actors_ApplyActorSoundOverrides(int arg0, Actors_ModelEffectSlot* arg1);
int Actors_SetActorTravelAnimation(int arg0, int arg1);
int Actors_ActorTravelAnimation(int arg0, int* arg1);
int Actors_QueueActorAnimation(int arg0, int arg1, int arg2, int arg3);
int Actors_PlayActorAnimation(int actorId, int animation, int arg2, int blendFrames);
int Actors_PlayActorAnimationAtFrame(int arg0, int arg1, int arg2, int arg3);
void Actors_SetActorTransitionMode(int arg0);
void Actors_SetActorAnimationLocked(int arg0, int arg1);
void Actors_PlayActorTurnAnimation(int arg0, int arg1);
int Actors_UpdateActorLocomotionAnimation(Actors_ActorRecord* actor, int speed, int direction);
void Actors_SetActorBonePoseLock(Actors_ActorRecord* arg0, int arg1, int arg2);
void Actors_ClearActorBonePoseLocks(Actors_ActorRecord* arg0);
void Actors_DecodeInitialPoseVector(short* arg0, u_char* arg1, int arg2);
void Actors_InitializeActorAnimationPose(Actors_ActorRecord* actor, Actors_AnimationPose* pose);
intptr_t Actors_FindActorAnimationHeader(void* arg0, int arg1, u_short** arg2);
int Actors_RunAnimationEvents(Actors_ActorRecord* actor, Actors_AnimationPose* state, int mode);
void Actors_IgnoreAnimationEvent(void);
void Actors_RestartActorAnimationEvents(Actors_ActorRecord* arg0);
void Actors_BlendActorPoseHalfway(Actors_AnimationPose* dst, Actors_AnimationPose* src, int count);
void Actors_UpdateActorAnimationBlend(Actors_ActorRecord* arg0);
void Actors_AlignPoseRotationBranches(SVECTOR* dst, SVECTOR* src, int count);
void Actors_UpdateMotionAndLookAt(Actors_ActorRecord* actor, int arg1);
void Actors_ApplyLockedBoneTransform(Actors_ActorRecord* actor, MATRIX* parent, int bone, int saved);

#endif // VS_ACTORS_ANIMATION_H_DECLS
