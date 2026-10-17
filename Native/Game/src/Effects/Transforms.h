// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_TRANSFORMS_H_TYPES
#define VS_EFFECTS_TRANSFORMS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Effects_AnimatedLocalTransform Effects_AnimatedLocalTransform;
typedef struct Effects_TransformDefinition Effects_TransformDefinition;
typedef struct Effects_TransformMotionView Effects_TransformMotionView;
typedef struct Effects_TransformSlot Effects_TransformSlot;

// decomp: func_800CE714_t2_2 (src/BATTLE/BATTLE.PRG/5BF94.h:439)
typedef struct Effects_TransformSlot {
    int reserved0;
    int reserved4;
    int reserved8;
    int reservedC;
    int reserved10;
    int reserved14;
    int reserved18;
    int positionX;
    int positionY;
    int positionZ;
    int reserved28;
    int reserved2C;
    int reserved30;
    u_char actor;
    u_char bone;
    u_char targetActor;
    u_char targetBone;
    MATRIX world;
    MATRIX view;
    u_char reserved78[0x46];
    short opaqueBE;
    u_char reservedC0[0x10];
} Effects_TransformSlot;

// layout: ledger (upstream merge) for func_800D0548_arg1
typedef struct Effects_TransformMotionView {
    /* 0x00 */ short rotation[3];
    /* 0x06 */ u_short flags;
    /* 0x08 */ int reserved8;
    /* 0x0C */ short reservedC;
    /* 0x0E */ short frame;
    /* 0x10 */ int reserved10[3];
    /* 0x1C */ VECTOR position;
    /* 0x2C */ int reserved2C[2];
    /* 0x34 */ u_char actor;
    /* 0x35 */ u_char bone;
    /* 0x36 */ u_char targetActor;
    /* 0x37 */ u_char targetBone;
    /* 0x38 */ MATRIX world;
    /* 0x58 */ MATRIX view;
    /* 0x78 */ MATRIX attachment;
    /* 0x98 */ MATRIX facing;
    /* 0xB8 */ SVECTOR saved;
    /* 0xC0 */ SVECTOR control0;
    /* 0xC8 */ SVECTOR control1;
} Effects_TransformMotionView;

// layout: ledger (upstream merge) for func_800D0B30_t
typedef struct Effects_TransformDefinition {
    /* 0x00 */ u_short flags;
    /* 0x02 */ u_char attachActor;
    /* 0x03 */ u_char attachBone;
    /* 0x04 */ u_char translationCurve;
    /* 0x05 */ u_char curve;
    /* 0x06 */ u_char rotationCurve;
    /* 0x07 */ u_char scaleCurve;
    /* 0x08 */ u_short rotationX;
    /* 0x0A */ u_short mode;
    /* 0x0C */ u_short rotationY;
    /* 0x0E */ u_short limit : 8;
    /* 0x0F */ u_short radius : 4;
    /* 0x0F */ u_short radiusJitter : 4;
    /* 0x10 */ u_short rotationZ;
    /* 0x12 */ u_short shape12;
    /* 0x14 */ short rotationRange[6];
    /* 0x20 */ short translationRange[6];
    /* 0x2C */ short scaleRange[6];
} Effects_TransformDefinition;

// decomp: func_800D0B30_t2 (src/BATTLE/BATTLE.PRG/5BF94.c:200)
typedef struct Effects_AnimatedLocalTransform {
    int reserved0;
    int reserved4;
    SVECTOR rotationOffset;
    VECTOR translation;
    VECTOR scale;
    int reserved30[2];
    VECTOR target;
    MATRIX local;
    int reserved68;
    int reserved6C;
    int reserved70;
} Effects_AnimatedLocalTransform;

#endif // VS_EFFECTS_TRANSFORMS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_TRANSFORMS_H_DECLS)
#define VS_EFFECTS_TRANSFORMS_H_DECLS
#include "Effects/Particles.h"
#include "Effects/Runtime.h"

void Effects_InitializeTransformStates(Effects_EffectInstance* arg0);
void Effects_InterpolateTransformPosition(Effects_TransformDefinition* arg0, Effects_TransformMotionView* arg1);
void Effects_InterpolateTransformSpline(Effects_TransformDefinition* arg0, Effects_TransformMotionView* arg1);
void Effects_AdvanceTransformMotion(Effects_TransformDefinition* arg0, Effects_TransformMotionView* arg1);
void Effects_BuildTransformSplineControls(Effects_TransformDefinition* arg0, Effects_TransformMotionView* arg1);
void Effects_BuildCameraAttachment(Effects_EffectInstance* arg0, Effects_TransformMotionView* arg1, int arg2);
void Effects_BuildEffectOriginAttachment(MATRIX* arg0);
void Effects_BuildAnimatedLocalTransform(Effects_TransformDefinition* arg0, SVECTOR* arg1, Effects_AnimatedLocalTransform* arg2);
void Effects_ResolveTransformAttachment(Effects_EffectInstance* arg0, Effects_TransformMotionView* arg1);
void Effects_UpdateEffectTransforms(Effects_EffectInstance* arg0);

#endif // VS_EFFECTS_TRANSFORMS_H_DECLS
