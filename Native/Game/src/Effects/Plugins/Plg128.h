// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_PLUGINS_PLG128_H_TYPES
#define VS_EFFECTS_PLUGINS_PLG128_H_TYPES
#include "Legacy/Psx.h"

typedef struct Effects_Plg128_CameraEffectState Effects_Plg128_CameraEffectState;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_EFFECTS_PLUGINS_PLG128_H_SET_TYPES_ONLY
#endif
#include "Effects/Camera.h"
#ifdef VS_EFFECTS_PLUGINS_PLG128_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_EFFECTS_PLUGINS_PLG128_H_SET_TYPES_ONLY
#endif

// layout: ledger (upstream merge) for func_800FD9A4_t
typedef struct Effects_Plg128_CameraEffectState {
    /* 0x00 */ u_char lifetime;
    /* 0x01 */ u_char effect;
    /* 0x02 */ short mode;
    /* 0x04 */ int age;
    /* 0x08 */ Effects_EffectCamera startCamera;
    /* 0x48 */ Effects_EffectCamera endCamera;
    /* 0x88 */ VECTOR lookAt;
    /* 0x98 */ VECTOR eye;
    /* 0xA8 */ MATRIX view;
    /* 0xC8 */ int nearClip;
    /* 0xCC */ int reservedCC;
} Effects_Plg128_CameraEffectState;

#endif // VS_EFFECTS_PLUGINS_PLG128_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_PLUGINS_PLG128_H_DECLS)
#define VS_EFFECTS_PLUGINS_PLG128_H_DECLS
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

extern SVECTOR Effects_Plg128_surfaceVertices[5][16];
extern CVECTOR Effects_Plg128_closedRingColorFactors[];
extern SVECTOR Effects_Plg128_surfaceVertices2[5][16];
extern int Effects_Plg128_trailEmissionCounter;
void Effects_Plg128_BuildCameraOffsetTransform(Effects_Plg128_CameraEffectState* arg0, VECTOR* arg1);
void Effects_Plg128_ResolveCameraEndpoints(Effects_Plg128_CameraEffectState* arg0, Effects_ParticleEmitterScratch* arg1, Effects_EffectInstance* arg2,
    Effects_ParticleDefinition* arg3);
void Effects_Plg128_ResolveActorFacingCamera(Effects_Plg128_CameraEffectState* arg0, Effects_ParticleEmitterScratch* arg1, Effects_EffectInstance* arg2,
    Effects_ParticleDefinition* arg3);
void Effects_Plg128_StoreCameraEndpoints(Effects_Plg128_CameraEffectState* arg0, Effects_ParticleEmitterScratch* scratch);
int Effects_Plg128_UpdateEffectCamera(Effects_RendererSlot* arg0, u_int arg1, int arg2);

#endif // VS_EFFECTS_PLUGINS_PLG128_H_DECLS
