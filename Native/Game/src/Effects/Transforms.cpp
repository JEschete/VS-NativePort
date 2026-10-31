// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorState.h"
#include "Actors/Location.h"
#include "Actors/Skeleton.h"
#include "Battle/BattleTypes.h"
#include "Core/Math/Interpolation.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/ScalarMath.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// decomp: BATTLE.PRG func_800CFEF0 (src/BATTLE/BATTLE.PRG/5BF94.c:5940)
void Effects_InitializeTransformStates(Effects_EffectInstance* effect_instance)
{
    int i;
    for (i = 0; i < Effects_package->block9Data->transformCount; ++i) {
        Battle_EffectDefinitionView* definition =
            &((Battle_EffectDefinitionView*)Effects_package->block10Data)[i];
        Battle_EffectTransformState* state = (Battle_EffectTransformState *)((void*)&effect_instance->transforms[i]);
        state->rotation[0] = definition->rotationXAndTarget;
        state->rotation[1] = definition->rotationY;
        state->rotation[2] = definition->rotationZ;
        state->position.vx = 0;
        state->position.vy = 0;
        state->position.vz = 0;
        Core_Math_SetIdentityRotation((MATRIX *)((void*)&state->facing));
        state->opaque6 = 0;
        state->frame = 0;
        *(int*)&state->actor = 0;
        if (definition->flags & 2) {
            switch (definition->flags & 0x18) {
            case 0x10:
                if (effect_instance->context.emitTo.type == 4) {
                    state->actor = ((u_char*)&effect_instance->context.emitTo.value)[0];
                    state->bone = ((u_char*)&effect_instance->context.emitTo.value)[2];
                } else {
                    state->actor = 0xFF;
                    Core_Math_SetTranslationMatrix(
                        (SVECTOR*)&effect_instance->context.emitTo.value, (MATRIX *)((void*)&state->attachment));
                }
                break;
            case 8:
                if (effect_instance->context.emitFrom.type == 4) {
                    state->actor = ((u_char*)&effect_instance->context.emitFrom.value)[0];
                    state->bone = ((u_char*)&effect_instance->context.emitFrom.value)[2];
                } else {
                    state->actor = 0xFF;
                    Core_Math_SetTranslationMatrix(
                        (SVECTOR*)&effect_instance->context.emitFrom.value, (MATRIX *)((void*)&state->attachment));
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
                if (effect_instance->context.emitTo.type == 4) {
                    state->targetActor = ((u_char*)&effect_instance->context.emitTo.value)[0];
                    state->targetBone = ((u_char*)&effect_instance->context.emitTo.value)[2];
                } else {
                    state->targetActor = 0xFF;
                    state->saved = effect_instance->context.emitTo.value.position;
                }
                break;
            case 0x400:
                if (effect_instance->context.emitFrom.type == 4) {
                    state->targetActor = ((u_char*)&effect_instance->context.emitFrom.value)[0];
                    state->targetBone = ((u_char*)&effect_instance->context.emitFrom.value)[2];
                } else {
                    state->targetActor = 0xFF;
                    state->saved = effect_instance->context.emitFrom.value.position;
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
void Effects_InterpolateTransformPosition(Effects_TransformDefinition* transform_definition, Effects_TransformMotionView* transform_motion)
{
    SVECTOR start;
    SVECTOR end;
    SVECTOR result;
    if (transform_definition->mode & 0x80) {
        start = transform_motion->saved;
        setVector(&end, transform_motion->world.t[0], transform_motion->world.t[1], transform_motion->world.t[2]);
    } else {
        end = transform_motion->saved;
        setVector(&start, transform_motion->world.t[0], transform_motion->world.t[1], transform_motion->world.t[2]);
    }
    switch ((transform_definition->mode >> 5) & 3) {
    case 0:
        Core_Math_InterpolateTimedShortVector(&start, &end, transform_definition->limit + 1, transform_motion->frame, &result);
        break;
    case 1:
        Core_Math_InterpolateCosineShortVector(&start, &end, transform_definition->limit + 1, transform_motion->frame, &result);
        break;
    case 2:
        Core_Math_InterpolateTimedShortVector(&start, &end, 128,
            Effects_SampleEffectCurve(transform_definition->curve + 1, transform_motion->frame), &result);
        break;
    }
    setVector(&transform_motion->position, result.vx, result.vy, result.vz);
}

// decomp: BATTLE.PRG func_800D037C (src/BATTLE/BATTLE.PRG/5BF94.c:6047)
void Effects_InterpolateTransformSpline(Effects_TransformDefinition* transform_definition, Effects_TransformMotionView* transform_motion)
{
    int t;
    SVECTOR start;
    SVECTOR end;
    SVECTOR result;
    if (transform_definition->mode & 0x80) {
        start = transform_motion->saved;
        setVector(&end, transform_motion->world.t[0], transform_motion->world.t[1], transform_motion->world.t[2]);
    } else {
        end = transform_motion->saved;
        setVector(&start, transform_motion->world.t[0], transform_motion->world.t[1], transform_motion->world.t[2]);
    }
    switch ((transform_definition->mode >> 5) & 3) {
    case 0:
        t = (transform_motion->frame * 3 * ONE) / (transform_definition->limit + 1) - ONE;
        break;
    case 1:
        t = (-Core_Gte_Cos((transform_motion->frame * (ONE / 2)) / (transform_definition->limit + 1)) * 3) / 2 + ONE / 2;
        break;
    case 2:
        t = Effects_SampleEffectCurve(transform_definition->curve + 1, transform_motion->frame) * 96 - ONE;
        break;
    }
    Core_Math_InterpolateLagrangeShortVector(
        &start, &transform_motion->control0, &transform_motion->control1, &end, t, &result);
    setVector(&transform_motion->position, result.vx, result.vy, result.vz);
}

// decomp: BATTLE.PRG func_800D0548 (src/BATTLE/BATTLE.PRG/5BF94.c:6076)
void Effects_AdvanceTransformMotion(Effects_TransformDefinition* transform_definition, Effects_TransformMotionView* transform_motion)
{
    switch (transform_definition->flags & 0x3000) {
    case 0:
        break;
    case 0x1000:
        Effects_InterpolateTransformPosition(transform_definition, transform_motion);
        break;
    case 0x2000:
        Effects_InterpolateTransformSpline(transform_definition, transform_motion);
        break;
    }
    if ((transform_motion->flags & 4) && transform_definition->limit >= transform_motion->frame) {
        ++transform_motion->frame;
    }
}

// decomp: BATTLE.PRG func_800D05F4 (src/BATTLE/BATTLE.PRG/5BF94.c:6093)
void Effects_BuildTransformSplineControls(Effects_TransformDefinition* transform_definition, Effects_TransformMotionView* transform_motion)
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

    if (transform_definition->mode & 0x80) {
        setVector(&start, transform_motion->saved.vx, transform_motion->saved.vy, transform_motion->saved.vz);
        setVector(&end, transform_motion->world.t[0], transform_motion->world.t[1], transform_motion->world.t[2]);
    } else {
        setVector(&start, transform_motion->world.t[0], transform_motion->world.t[1], transform_motion->world.t[2]);
        setVector(&end, transform_motion->saved.vx, transform_motion->saved.vy, transform_motion->saved.vz);
    }

    setVector(&delta, end.vx - start.vx, end.vy - start.vy, end.vz - start.vz);
    distance =
        Core_Math_SquareRootInteger(delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz);
    control0.vz = distance * (((transform_definition->shape12 >> 8) & 15) - 8) / 8;
    control1.vz = distance + distance * ((transform_definition->shape12 >> 12) - 8) / 8;
    radius1 = transform_definition->radius * 32;
    radius0 = radius1;

    if (transform_definition->radiusJitter) {
        radius0 = radius1 + Core_Random_Rand() % (transform_definition->radiusJitter * 32);
        radius1 += Core_Random_Rand() % (transform_definition->radiusJitter * 32);
    }

    angle1 = ((u_char)transform_definition->shape12 & 15) << 8;
    angle0 = angle1;

    if (transform_definition->shape12 & 0xF0) {
        angle0 = angle1 + Core_Random_Rand() % ((transform_definition->shape12 * 16) & 0xF00);
        angle1 += Core_Random_Rand() % ((transform_definition->shape12 * 16) & 0xF00);
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
    setVector(&transform_motion->control0, start.vx, start.vy, start.vz);
    setVector(&transform_motion->control1, end.vx, end.vy, end.vz);
}

// decomp: BATTLE.PRG func_800D0984 (src/BATTLE/BATTLE.PRG/5BF94.c:6152)
void Effects_BuildCameraAttachment(Effects_EffectInstance* effect_instance, Effects_TransformMotionView* transform_motion, int offset_toward_camera)
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
    Core_Math_BuildLookAtOrientation(&start, &end, &transform_motion->attachment);
    transform_motion->attachment.t[0] = start.vx;
    transform_motion->attachment.t[1] = start.vy;
    transform_motion->attachment.t[2] = start.vz;
    if (offset_toward_camera) {
        setVector(&delta, end.vx - start.vx, end.vy - start.vy, end.vz - start.vz);
        distance =
            Core_Math_SquareRootInteger(delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz);
        offset.vx = offset.vy = 0;
        offset.vz = distance - 0x600;
        Core_Gte_SetRotMatrix(&transform_motion->attachment);
        Core_Gte_SetTransMatrix(&transform_motion->attachment);
        Core_Gte_RotTrans(&offset, &start, &distance);
        transform_motion->attachment.t[0] = start.vx;
        transform_motion->attachment.t[1] = start.vy;
        transform_motion->attachment.t[2] = start.vz;
    }
}

// decomp: BATTLE.PRG func_800D0B08 (src/BATTLE/BATTLE.PRG/5BF94.c:6182)
void Effects_BuildEffectOriginAttachment(MATRIX* attachment_matrix) { Core_Math_SetTranslationMatrix(Effects_effectOrigin, attachment_matrix); }

// decomp: BATTLE.PRG func_800D0B30 (src/BATTLE/BATTLE.PRG/5BF94.c:6184)
void Effects_BuildAnimatedLocalTransform(Effects_TransformDefinition* transform_definition, SVECTOR* rotation_angles, Effects_AnimatedLocalTransform* local_transform)
{
    if (transform_definition->flags & 1) {
        Core_Math_InterpolatePairsToShortVector(transform_definition->rotationRange, Effects_curveSamples[transform_definition->rotationCurve], &local_transform->rotationOffset);
        rotation_angles->vx += local_transform->rotationOffset.vx;
        rotation_angles->vy = rotation_angles->vy + local_transform->rotationOffset.vy;
        rotation_angles->vz += local_transform->rotationOffset.vz;
        Core_Gte_RotMatrix(rotation_angles, &local_transform->local);
        Core_Math_InterpolatePairsToWideVector(transform_definition->translationRange, Effects_curveSamples[transform_definition->translationCurve], &local_transform->translation);
        Core_Gte_TransMatrix(&local_transform->local, &local_transform->translation);
        Core_Math_InterpolatePairsToWideVector(transform_definition->scaleRange, Effects_curveSamples[transform_definition->scaleCurve], &local_transform->scale);
        Core_Gte_ScaleMatrix(&local_transform->local, &local_transform->scale);
        return;
    }
    Core_Math_SetIdentityRotation(&local_transform->local);
    local_transform->local.t[2] = 0;
    local_transform->local.t[1] = 0;
    local_transform->local.t[0] = 0;
}

// decomp: BATTLE.PRG func_800D0C60 (src/BATTLE/BATTLE.PRG/5BF94.c:6204)
void Effects_ResolveTransformAttachment(Effects_EffectInstance* effect_instance, Effects_TransformMotionView* transform_motion)
{
    Actors_ActorLocationQuery actor_placement;
    int bone_length_output[2];

    if (transform_motion->actor < 0x10U) {
        ((void (*)(int, int, MATRIX*, void*))Actors_ActorAnchorMatrix)(transform_motion->actor, transform_motion->bone, &transform_motion->attachment, bone_length_output);
        Actors_ActorPlacement(transform_motion->actor, &actor_placement);
        transform_motion->saved.pad = actor_placement.location.position.pad;
        return;
    }

    switch (transform_motion->actor) {
    case 0x10:
    case 0x11:
        Effects_BuildCameraAttachment(effect_instance, transform_motion, transform_motion->actor != 0x10);
        break;
    case 0x12:
        Effects_BuildEffectOriginAttachment(&transform_motion->attachment);
        break;
    }
}

// decomp: BATTLE.PRG func_800D0D08 (src/BATTLE/BATTLE.PRG/5BF94.c:6227)
void Effects_UpdateEffectTransforms(Effects_EffectInstance* effect_instance)
{
    int i;
    Effects_AnimatedLocalTransform* scratch = (Effects_AnimatedLocalTransform*)((uintptr_t)Core_ScratchBase() + 0x2C8);

    for (i = 0; i < Effects_package->block9Data->transformCount; ++i) {
        Effects_TransformDefinition* definition = &Effects_package->block10Data[i];
        Effects_TransformMotionView* state = (Effects_TransformMotionView*)&effect_instance->transforms[i];

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
            Effects_ResolveTransformAttachment(effect_instance, state);
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

}  // extern "C"
