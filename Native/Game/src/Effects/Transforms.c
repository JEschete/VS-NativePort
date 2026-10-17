// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorState.h"
#include "Actors/Location.h"
#include "Actors/Skeleton.h"
#include "Battle/BattleTypes.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Math/Interpolation.h"
#include "Core/Math/Matrix.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// decomp: BATTLE.PRG func_800CFEF0 (src/BATTLE/BATTLE.PRG/5BF94.c:5940)
void Effects_InitializeTransformStates(Effects_EffectInstance* arg0)
{
    int i;
    for (i = 0; i < Effects_package->block9Data->transformCount; ++i) {
        Battle_EffectDefinitionView* definition =
            &((Battle_EffectDefinitionView*)Effects_package->block10Data)[i];
        Battle_EffectTransformState* state = (void*)&arg0->transforms[i];
        state->rotation[0] = definition->rotationXAndTarget;
        state->rotation[1] = definition->rotationY;
        state->rotation[2] = definition->rotationZ;
        state->position.vx = 0;
        state->position.vy = 0;
        state->position.vz = 0;
        Core_Math_SetIdentityRotation((void*)&state->facing);
        state->opaque6 = 0;
        state->frame = 0;
        *(int*)&state->actor = 0;
        if (definition->flags & 2) {
            switch (definition->flags & 0x18) {
            case 0x10:
                if (arg0->context.emitTo.type == 4) {
                    state->actor = ((u_char*)&arg0->context.emitTo.value)[0];
                    state->bone = ((u_char*)&arg0->context.emitTo.value)[2];
                } else {
                    state->actor = 0xFF;
                    Core_Math_SetTranslationMatrix(
                        (SVECTOR*)&arg0->context.emitTo.value, (void*)&state->attachment);
                }
                break;
            case 8:
                if (arg0->context.emitFrom.type == 4) {
                    state->actor = ((u_char*)&arg0->context.emitFrom.value)[0];
                    state->bone = ((u_char*)&arg0->context.emitFrom.value)[2];
                } else {
                    state->actor = 0xFF;
                    Core_Math_SetTranslationMatrix(
                        (SVECTOR*)&arg0->context.emitFrom.value, (void*)&state->attachment);
                }
                break;
            case 0:
                state->actor = (u_char)definition->attachment;
                state->bone = 0;
                break;
            }
            if (!(definition->flags & 4)) {
                state->bone = ((u_char*)&definition->attachment)[1];
            }
        }
        if (definition->flags & 0x80) {
            switch (definition->flags & 0xC00) {
            case 0x800:
                if (arg0->context.emitTo.type == 4) {
                    state->targetActor = ((u_char*)&arg0->context.emitTo.value)[0];
                    state->targetBone = ((u_char*)&arg0->context.emitTo.value)[2];
                } else {
                    state->targetActor = 0xFF;
                    state->saved = arg0->context.emitTo.value.position;
                }
                break;
            case 0x400:
                if (arg0->context.emitFrom.type == 4) {
                    state->targetActor = ((u_char*)&arg0->context.emitFrom.value)[0];
                    state->targetBone = ((u_char*)&arg0->context.emitFrom.value)[2];
                } else {
                    state->targetActor = 0xFF;
                    state->saved = arg0->context.emitFrom.value.position;
                }
                break;
            case 0:
                state->targetActor = ((u_char*)&definition->rotationXAndTarget)[2] & 31;
                state->targetBone = 0;
                break;
            }
            if (!(definition->flags & 0x200)) {
                state->targetBone = ((u_char*)&definition->rotationXAndTarget)[3];
            }
        }
    }
}

// decomp: BATTLE.PRG func_800D01E4 (src/BATTLE/BATTLE.PRG/5BF94.c:6020)
void Effects_InterpolateTransformPosition(Effects_TransformDefinition* arg0, Effects_TransformMotionView* arg1)
{
    SVECTOR start;
    SVECTOR end;
    SVECTOR result;
    if (arg0->mode & 0x80) {
        start = arg1->saved;
        setVector(&end, arg1->world.t[0], arg1->world.t[1], arg1->world.t[2]);
    } else {
        end = arg1->saved;
        setVector(&start, arg1->world.t[0], arg1->world.t[1], arg1->world.t[2]);
    }
    switch ((arg0->mode >> 5) & 3) {
    case 0:
        Core_Math_InterpolateTimedShortVector(&start, &end, arg0->limit + 1, arg1->frame, &result);
        break;
    case 1:
        Core_Math_InterpolateCosineShortVector(&start, &end, arg0->limit + 1, arg1->frame, &result);
        break;
    case 2:
        Core_Math_InterpolateTimedShortVector(&start, &end, 128,
            Effects_SampleEffectCurve(arg0->curve + 1, arg1->frame), &result);
        break;
    }
    setVector(&arg1->position, result.vx, result.vy, result.vz);
}

// decomp: BATTLE.PRG func_800D037C (src/BATTLE/BATTLE.PRG/5BF94.c:6047)
void Effects_InterpolateTransformSpline(Effects_TransformDefinition* arg0, Effects_TransformMotionView* arg1)
{
    int t;
    SVECTOR start;
    SVECTOR end;
    SVECTOR result;
    if (arg0->mode & 0x80) {
        start = arg1->saved;
        setVector(&end, arg1->world.t[0], arg1->world.t[1], arg1->world.t[2]);
    } else {
        end = arg1->saved;
        setVector(&start, arg1->world.t[0], arg1->world.t[1], arg1->world.t[2]);
    }
    switch ((arg0->mode >> 5) & 3) {
    case 0:
        t = (arg1->frame * 3 * ONE) / (arg0->limit + 1) - ONE;
        break;
    case 1:
        t = (-Core_Gte_Cos((arg1->frame * (ONE / 2)) / (arg0->limit + 1)) * 3) / 2 + ONE / 2;
        break;
    case 2:
        t = Effects_SampleEffectCurve(arg0->curve + 1, arg1->frame) * 96 - ONE;
        break;
    }
    Core_Math_InterpolateLagrangeShortVector(
        &start, &arg1->control0, &arg1->control1, &end, t, &result);
    setVector(&arg1->position, result.vx, result.vy, result.vz);
}

// decomp: BATTLE.PRG func_800D0548 (src/BATTLE/BATTLE.PRG/5BF94.c:6076)
void Effects_AdvanceTransformMotion(Effects_TransformDefinition* arg0, Effects_TransformMotionView* arg1)
{
    switch (arg0->flags & 0x3000) {
    case 0:
        break;
    case 0x1000:
        Effects_InterpolateTransformPosition(arg0, arg1);
        break;
    case 0x2000:
        Effects_InterpolateTransformSpline(arg0, arg1);
        break;
    }
    if ((arg1->flags & 4) && arg0->limit >= arg1->frame) {
        ++arg1->frame;
    }
}

// decomp: BATTLE.PRG func_800D05F4 (src/BATTLE/BATTLE.PRG/5BF94.c:6093)
void Effects_BuildTransformSplineControls(Effects_TransformDefinition* arg0, Effects_TransformMotionView* arg1)
{
    SVECTOR control0;
    SVECTOR control1;
    VECTOR delta;
    VECTOR start;
    VECTOR end;
    MATRIX transform;
    long distance;
    int radius0;
    int radius1;
    int angle0;
    int angle1;

    if (arg0->mode & 0x80) {
        setVector(&start, arg1->saved.vx, arg1->saved.vy, arg1->saved.vz);
        setVector(&end, arg1->world.t[0], arg1->world.t[1], arg1->world.t[2]);
    } else {
        setVector(&start, arg1->world.t[0], arg1->world.t[1], arg1->world.t[2]);
        setVector(&end, arg1->saved.vx, arg1->saved.vy, arg1->saved.vz);
    }

    setVector(&delta, end.vx - start.vx, end.vy - start.vy, end.vz - start.vz);
    distance =
        Core_Math_SquareRootInteger(delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz);
    control0.vz = distance * (((arg0->shape12 >> 8) & 15) - 8) / 8;
    control1.vz = distance + distance * ((arg0->shape12 >> 12) - 8) / 8;
    radius1 = arg0->radius * 32;
    radius0 = radius1;

    if (arg0->radiusJitter) {
        radius0 = radius1 + Core_Random_Rand() % (arg0->radiusJitter * 32);
        radius1 += Core_Random_Rand() % (arg0->radiusJitter * 32);
    }

    angle1 = ((u_char)arg0->shape12 & 15) << 8;
    angle0 = angle1;

    if (arg0->shape12 & 0xF0) {
        angle0 = angle1 + Core_Random_Rand() % ((arg0->shape12 * 16) & 0xF00);
        angle1 += Core_Random_Rand() % ((arg0->shape12 * 16) & 0xF00);
    }

    control0.vx = (Core_Gte_Cos(angle0) * radius0) >> 12;
    control0.vy = (Core_Gte_Sin(angle0) * radius0) >> 12;
    control1.vx = (Core_Gte_Cos(angle1) * radius1) >> 12;
    control1.vy = (Core_Gte_Sin(angle1) * radius1) >> 12;
    Core_Math_BuildLookAtOrientation(&start, &end, &transform);
    transform.t[0] = start.vx;
    transform.t[1] = start.vy;
    transform.t[2] = start.vz;
    Core_Gte_SetRotMatrix(&transform);
    Core_Gte_SetTransMatrix(&transform);
    Core_Gte_RotTrans(&control0, &start, &distance);
    Core_Gte_RotTrans(&control1, &end, &distance);
    setVector(&arg1->control0, start.vx, start.vy, start.vz);
    setVector(&arg1->control1, end.vx, end.vy, end.vz);
}

// decomp: BATTLE.PRG func_800D0984 (src/BATTLE/BATTLE.PRG/5BF94.c:6152)
void Effects_BuildCameraAttachment(Effects_EffectInstance* arg0, Effects_TransformMotionView* arg1, int arg2)
{
    VECTOR delta;
    VECTOR start;
    VECTOR end;
    SVECTOR offset;
    long distance;
    setVector(&start, Render_frameScratch.camera.lookAt.vx >> 12,
        Render_frameScratch.camera.lookAt.vy >> 12, Render_frameScratch.camera.lookAt.vz >> 12);
    setVector(&end, Render_frameScratch.camera.position.vx >> 12,
        Render_frameScratch.camera.position.vy >> 12, Render_frameScratch.camera.position.vz >> 12);
    Core_Math_BuildLookAtOrientation(&start, &end, &arg1->attachment);
    arg1->attachment.t[0] = start.vx;
    arg1->attachment.t[1] = start.vy;
    arg1->attachment.t[2] = start.vz;
    if (arg2) {
        setVector(&delta, end.vx - start.vx, end.vy - start.vy, end.vz - start.vz);
        distance =
            Core_Math_SquareRootInteger(delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz);
        offset.vx = offset.vy = 0;
        offset.vz = distance - 0x600;
        Core_Gte_SetRotMatrix(&arg1->attachment);
        Core_Gte_SetTransMatrix(&arg1->attachment);
        Core_Gte_RotTrans(&offset, &start, &distance);
        arg1->attachment.t[0] = start.vx;
        arg1->attachment.t[1] = start.vy;
        arg1->attachment.t[2] = start.vz;
    }
}

// decomp: BATTLE.PRG func_800D0B08 (src/BATTLE/BATTLE.PRG/5BF94.c:6182)
void Effects_BuildEffectOriginAttachment(MATRIX* arg0) { Core_Math_SetTranslationMatrix(Effects_effectOrigin, arg0); }

// decomp: BATTLE.PRG func_800D0B30 (src/BATTLE/BATTLE.PRG/5BF94.c:6184)
void Effects_BuildAnimatedLocalTransform(Effects_TransformDefinition* arg0, SVECTOR* arg1, Effects_AnimatedLocalTransform* arg2)
{
    if (arg0->flags & 1) {
        Core_Math_InterpolatePairsToShortVector(arg0->rotationRange, Effects_curveSamples[arg0->rotationCurve], &arg2->rotationOffset);
        arg1->vx += arg2->rotationOffset.vx;
        arg1->vy = arg1->vy + arg2->rotationOffset.vy;
        arg1->vz += arg2->rotationOffset.vz;
        Core_Gte_RotMatrix(arg1, &arg2->local);
        Core_Math_InterpolatePairsToWideVector(arg0->translationRange, Effects_curveSamples[arg0->translationCurve], &arg2->translation);
        Core_Gte_TransMatrix(&arg2->local, &arg2->translation);
        Core_Math_InterpolatePairsToWideVector(arg0->scaleRange, Effects_curveSamples[arg0->scaleCurve], &arg2->scale);
        Core_Gte_ScaleMatrix(&arg2->local, &arg2->scale);
        return;
    }
    Core_Math_SetIdentityRotation(&arg2->local);
    arg2->local.t[2] = 0;
    arg2->local.t[1] = 0;
    arg2->local.t[0] = 0;
}

// decomp: BATTLE.PRG func_800D0C60 (src/BATTLE/BATTLE.PRG/5BF94.c:6204)
void Effects_ResolveTransformAttachment(Effects_EffectInstance* arg0, Effects_TransformMotionView* arg1)
{
    Actors_ActorLocationQuery sp10;
    int sp20[2];

    if (arg1->actor < 0x10U) {
        ((void (*)(int, int, MATRIX*, void*))Actors_ActorAnchorMatrix)(arg1->actor, arg1->bone, &arg1->attachment, sp20);
        Actors_ActorPlacement(arg1->actor, &sp10);
        arg1->saved.pad = sp10.location.position.pad;
        return;
    }

    switch (arg1->actor) {
    case 0x10:
    case 0x11:
        Effects_BuildCameraAttachment(arg0, arg1, arg1->actor != 0x10);
        break;
    case 0x12:
        Effects_BuildEffectOriginAttachment(&arg1->attachment);
        break;
    }
}

// decomp: BATTLE.PRG func_800D0D08 (src/BATTLE/BATTLE.PRG/5BF94.c:6227)
void Effects_UpdateEffectTransforms(Effects_EffectInstance* arg0)
{
    int i;
    Effects_AnimatedLocalTransform* scratch = (Effects_AnimatedLocalTransform*)((uintptr_t)Core_ScratchBase() + 0x2C8);

    for (i = 0; i < Effects_package->block9Data->transformCount; ++i) {
        Effects_TransformDefinition* definition = &Effects_package->block10Data[i];
        Effects_TransformMotionView* state = (Effects_TransformMotionView*)&arg0->transforms[i];

        Effects_BuildAnimatedLocalTransform(definition, (SVECTOR*)state->rotation, scratch);

        if ((definition->flags & 0x80) && state->targetActor != 0xFF
            && ((definition->flags & 0x100) || !(state->flags & 2))) {
            if (state->targetActor == 0x12) {
                setVector(&state->saved, Effects_actionRequest.position.vx, Effects_actionRequest.position.vy,
                    Effects_actionRequest.position.vz);
            } else {
                Actors_ActorBonePosition(state->targetActor, state->targetBone, &state->saved, 0);
            }

            state->flags |= 2;
        }

        if (state->actor != 0xFF && ((definition->flags & 0x20) || !(state->flags & 1))) {
            Effects_ResolveTransformAttachment(arg0, state);
            state->flags |= 1;
        }

        if (definition->flags & 2) {
            if ((definition->flags & 0xC000) == 0x4000) {
                ((int*)&state->attachment)[0] = ((int*)&state->attachment)[4] =
                    Core_Gte_Cos(state->saved.pad);
                ((int*)&state->attachment)[1] = Core_Gte_Sin(state->saved.pad);
                ((int*)&state->attachment)[3] = -((int*)&state->attachment)[1];
                ((int*)&state->attachment)[2] = ONE;
            }

            Core_Gte_CompMatrix(&state->attachment, &scratch->local, &state->world);
        } else {
            state->world = scratch->local;
        }

        if ((definition->flags & 0x3000) == 0x2000 && !(state->flags & 8)) {
            Effects_BuildTransformSplineControls(definition, state);
            state->flags |= 8;
        }

        if (definition->flags & 0x80) {
            Effects_AdvanceTransformMotion(definition, state);
        }

        if (definition->flags & 0x40) {
            state->world.t[0] = state->position.vx;
            state->world.t[1] = state->position.vy;
            state->world.t[2] = state->position.vz;
        } else {
            state->world.t[0] += state->position.vx;
            state->world.t[1] += state->position.vy;
            state->world.t[2] += state->position.vz;
        }

        if ((short)definition->flags & 0x8000) {
            if (definition->flags & 0x4000) {
                setVector(
                    &scratch->target, state->saved.vx, state->saved.vy, state->saved.vz);
            } else {
                setVector(&scratch->target, state->attachment.t[0],
                    state->attachment.t[1], state->attachment.t[2]);
            }

            if (scratch->target.vx != state->world.t[0]
                || scratch->target.vy != state->world.t[1]
                || scratch->target.vz != state->world.t[2]) {
                Core_Math_BuildLookAtOrientation(
                    (VECTOR*)state->world.t, &scratch->target, &state->facing);
                state->facing.t[0] = state->world.t[0];
                state->facing.t[1] = state->world.t[1];
                state->facing.t[2] = state->world.t[2];
            }

            Core_Gte_CompMatrix(&state->facing, &scratch->local, &state->world);
        }

        Core_Gte_CompMatrix(&Render_frameScratch.viewMatrix, &state->world, &state->view);
    }
}

