// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Interpolation.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Vector.h"
#include "Core/Memory/Heap.h"
#include "Effects/Camera.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg128.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

// decomp: D_801004F0 (D_801004F0@EFFECT/PLG128.BIN)
SVECTOR Effects_Plg128_surfaceVertices[5][16]; /* contents: module image (ModuleData.c) */

// decomp: D_80100770 (D_80100770@EFFECT/PLG128.BIN)
CVECTOR Effects_Plg128_closedRingColorFactors[]; /* contents: module image (ModuleData.c) */

// decomp: D_80100790 (D_80100790@EFFECT/PLG128.BIN)
SVECTOR Effects_Plg128_surfaceVertices2[5][16]; /* contents: module image (ModuleData.c) */

// decomp: D_80100A10 (D_80100A10@EFFECT/PLG128.BIN)
int Effects_Plg128_trailEmissionCounter; /* contents: module image (ModuleData.c) */

// decomp: PLG128.BIN func_800FD9A4 (src/EFFECT/PLG128.BIN/F8.c:66)
void Effects_Plg128_BuildCameraOffsetTransform(Effects_Plg128_CameraEffectState* arg0, VECTOR* arg1)
{
    Core_Math_BuildLookAtOrientation(&arg0->eye, &arg0->lookAt, &arg0->view);
    arg0->view.t[0] = arg1->vx;
    arg0->view.t[1] = arg1->vy;
    arg0->view.t[2] = arg1->vz;
}

// decomp: PLG128.BIN func_800FDA04 (src/EFFECT/PLG128.BIN/F8.c:74)
void Effects_Plg128_ResolveCameraEndpoints(Effects_Plg128_CameraEffectState* arg0, Effects_ParticleEmitterScratch* arg1, Effects_EffectInstance* arg2,
    Effects_ParticleDefinition* arg3)
{
    switch (arg1->flags & 7) {
    case 0:
        break;
    case 1:
        Core_Math_AddWideAndShortVectors(&arg1->position, Effects_effectOrigin, &arg1->position);
        break;
    case 2:
        arg1->position.vx += arg2->transforms[arg3->transformIndex].world.t[0];
        arg1->position.vy += arg2->transforms[arg3->transformIndex].world.t[1];
        arg1->position.vz += arg2->transforms[arg3->transformIndex].world.t[2];
        break;
    }
    arg0->lookAt.vx = arg1->position.vx;
    arg0->lookAt.vy = arg1->position.vy;
    arg0->lookAt.vz = arg1->position.vz;
    switch (arg1->flags & 0x1C0) {
    case 0:
        break;
    case 0xC0:
        arg1->attractionOffset.vx += arg2->transforms[arg3->attractionTransform].world.t[0];
        arg1->attractionOffset.vy += arg2->transforms[arg3->attractionTransform].world.t[1];
        arg1->attractionOffset.vz += arg2->transforms[arg3->attractionTransform].world.t[2];
        break;
    case 0x80:
        arg1->attractionOffset.vx += Effects_actionRequest.position.vx;
        arg1->attractionOffset.vy += Effects_actionRequest.position.vy;
        arg1->attractionOffset.vz += Effects_actionRequest.position.vz;
        break;
    }
    arg0->eye.vx = arg1->attractionOffset.vx;
    arg0->eye.vy = arg1->attractionOffset.vy;
    arg0->eye.vz = arg1->attractionOffset.vz;
}

// decomp: PLG128.BIN func_800FDC80 (src/EFFECT/PLG128.BIN/F8.c:111)
void Effects_Plg128_ResolveActorFacingCamera(Effects_Plg128_CameraEffectState* arg0, Effects_ParticleEmitterScratch* arg1, Effects_EffectInstance* arg2,
    Effects_ParticleDefinition* arg3)
{
    switch (arg1->flags & 7) {
    case 0:
        break;
    case 1:
        Core_Math_AddWideAndShortVectors(&arg1->position, Effects_effectOrigin, &arg1->position);
        break;
    case 2:
        arg1->position.vx += arg2->transforms[arg3->transformIndex].world.t[0];
        arg1->position.vy += arg2->transforms[arg3->transformIndex].world.t[1];
        arg1->position.vz += arg2->transforms[arg3->transformIndex].world.t[2];
        break;
    }
    arg0->eye.vx = arg1->position.vx;
    arg0->eye.vy = arg1->position.vy;
    arg0->eye.vz = arg1->position.vz;
    arg0->lookAt.vx = arg1->position.vx
                   + ((-arg1->attractionJitter.vx * Core_Gte_Sin(arg2->transforms[arg3->transformIndex].opaqueBE)) >> 12);
    arg0->lookAt.vz = arg1->position.vz
                   + ((-arg1->attractionJitter.vx * Core_Gte_Cos(arg2->transforms[arg3->transformIndex].opaqueBE)) >> 12);
    arg0->lookAt.vy = arg1->position.vy;
}

// decomp: PLG128.BIN func_800FDE78 (src/EFFECT/PLG128.BIN/F8.c:136)
void Effects_Plg128_StoreCameraEndpoints(Effects_Plg128_CameraEffectState* arg0, Effects_ParticleEmitterScratch* scratch)
{
    arg0->endCamera.position.vx = arg0->lookAt.vx << 12;
    arg0->endCamera.position.vy = arg0->lookAt.vy << 12;
    arg0->endCamera.position.vz = arg0->lookAt.vz << 12;
    arg0->endCamera.lookAt.vx = arg0->eye.vx << 12;
    arg0->endCamera.lookAt.vy = arg0->eye.vy << 12;
    arg0->endCamera.lookAt.vz = arg0->eye.vz << 12;
}

// decomp: PLG128.BIN func_800FDEC4 (src/EFFECT/PLG128.BIN/F8.c:146)
int Effects_Plg128_UpdateEffectCamera(Effects_RendererSlot* arg0, u_int arg1, int arg2)
{
    Effects_Plg128_CameraEffectState* state = arg0->rendererState;
    Effects_EffectInstance* battle = Effects_currentInstance;
    Effects_ParticleEmitterScratch* scratch = (void*)((uintptr_t)Core_ScratchBase() + 0x1D0);
    Effects_ParticleDefinition* effect;
    int alive = 1;
    int curve;
    switch (arg1) {
    case 1:
        state = arg0->rendererState = Core_Memory_AllocateFromHighEnd(sizeof(*state));
        state->effect = arg2 >> 8;
        state->lifetime = arg2;
        state->mode = arg2 >> 16;
        state->age = 0;
        Effects_CaptureEffectCamera(&state->startCamera);
        Effects_CaptureEffectCamera(&state->endCamera);
        Effects_cameraMotionFrame = 0;
        Effects_cameraMotionDuration = arg2 & 0xFF;
        switch (state->mode) {
        case 0:
        case 1:
            Effects_SetEffectCameraTransitionMode(2);
            break;
        }
        break;
    case 2:
        effect = &Effects_package->block5Data->definitions[state->effect];
        scratch->flags = effect->flags;
        curve = effect->positionCurve;
        Core_Math_InterpolatePairsToWideVector(
            effect->positionRange, Effects_SampleEffectCurve(curve, state->age), &scratch->position);
        Core_Math_InterpolatePairsToWideVector(
            effect->extentRange, Effects_SampleEffectCurve(curve, state->age), &scratch->extent);
        Core_Math_InterpolatePairsToWideVector(effect->vectorRanges[0],
            Effects_SampleEffectCurve(effect->directionCurve, state->age), &scratch->baseDirection);
        Core_Math_InterpolatePairsToPlaneVector(effect->pairRanges[2],
            Effects_SampleEffectCurve(effect->speedCurve, state->age), scratch->speedRange);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[6],
            Effects_SampleEffectCurve(effect->attractionOffsetCurve, state->age), &scratch->attractionOffset);
        Core_Math_InterpolatePairsToShortVector(effect->vectorRanges[7],
            Effects_SampleEffectCurve(effect->attractionJitterCurve, state->age), &scratch->attractionJitter);
        state->nearClip = Core_Math_InterpolateShortPair(
            effect->innerExtentRange, Effects_SampleEffectCurve(effect->innerExtentCurve, state->age));
        switch (state->mode) {
        case 0:
            Effects_Plg128_ResolveCameraEndpoints(state, scratch, battle, effect);
            break;
        case 1:
            Effects_Plg128_ResolveActorFacingCamera(state, scratch, battle, effect);
            break;
        }
        switch (scratch->flags & 0x38) {
        case 0:
            Effects_Plg128_BuildCameraOffsetTransform(state, &state->lookAt);
            scratch->sinAngle0 = Core_Gte_Sin(scratch->baseDirection.vx);
            if (scratch->sinAngle0 > 0) {
                scratch->sinAngle0 = -scratch->sinAngle0;
            }
            scratch->cosAngle0 = Core_Gte_Cos(scratch->baseDirection.vx);
            scratch->sinAngle1 = Core_Gte_Sin(scratch->baseDirection.vy);
            scratch->cosAngle1 = Core_Gte_Cos(scratch->baseDirection.vy);
            scratch->vector.vx =
                (((scratch->extent.vx * scratch->cosAngle0) >> 12) * scratch->cosAngle1) >> 12;
            scratch->vector.vy =
                (((scratch->extent.vy * scratch->cosAngle0) >> 12) * scratch->sinAngle1) >> 12;
            scratch->vector.vz = (scratch->extent.vz * scratch->sinAngle0) >> 12;
            break;
        case 0x20:
            Effects_Plg128_BuildCameraOffsetTransform(state, &state->lookAt);
            scratch->sinAngle0 = Core_Gte_Sin(scratch->baseDirection.vx);
            scratch->cosAngle0 = Core_Gte_Cos(scratch->baseDirection.vx);
            scratch->vector.vx = (scratch->extent.vx * scratch->cosAngle0) >> 12;
            scratch->vector.vz = scratch->extent.vz;
            scratch->vector.vy = (scratch->extent.vy * scratch->sinAngle0) >> 12;
            break;
        }
        Core_Gte_SetRotMatrix(&state->view);
        Core_Gte_SetTransMatrix(&state->view);
        Core_Gte_LoadV0(&scratch->vector);
        Core_Gte_Rtv0Tr();
        Core_Gte_StoreLvnl(&state->lookAt);
        switch (state->mode) {
        case 0:
        case 1:
            Effects_Plg128_StoreCameraEndpoints(state, scratch);
            break;
        }
        state->endCamera.nearClip = state->nearClip;
        state->endCamera.farClip = scratch->speedRange[0];
        Effects_cameraMotionEnds[0] = state->startCamera;
        Effects_cameraMotionEnds[1] = state->endCamera;
        ++state->age;
        if (!--state->lifetime) {
            alive = 0;
        }
        break;
    case 3:
        state->lifetime = 1;
        break;
    case 4:
        alive = 0;
        break;
    }
    return alive;
}

