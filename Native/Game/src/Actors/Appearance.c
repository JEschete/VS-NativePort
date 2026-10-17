// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
#include "Actors/Movement.h"
#include "Actors/Palette.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/WeaponModel.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Render/Palette.h"
#include "State/Timers.h"

// decomp: D_800F4438 (D_800F4438@BATTLE/BATTLE.PRG)
VECTOR Actors_roomActorScale;

// decomp: BATTLE.PRG func_8009D7E8 (src/BATTLE/BATTLE.PRG/30DB0.c:2173)
void Actors_SetActorPaletteFade(int arg0, int arg1)
{
    Actors_ActorRecord* temp_a2 = Actors_actorRecords[arg0];
    if (temp_a2 != NULL) {
        if (arg1 == 0) {
            temp_a2->paletteFadeActive = 0;
        } else {
            temp_a2->paletteFadeState[0] = 0;
            temp_a2->paletteFadeActive = 1;
            temp_a2->paletteFadeState[1] = arg1 * 2;
            temp_a2->paletteFadeReverse = 0;
        }
    }
}

// decomp: BATTLE.PRG func_8009D88C (src/BATTLE/BATTLE.PRG/30DB0.c:2195)
void Actors_ResetActorWeaponVisibilityBlend(int arg0)
{
    int i;
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];

    if (temp_a0 == NULL) {
        return;
    }

    for (i = 0; i < 2; ++i) {
        Actors_WeaponModel* model = Actors_weaponModels[temp_a0->instance.slot * 2 + i];
        if ((model != NULL) && (model->attachmentBone != 0)) {
            if (model->reserved08Bit4) {
                model->reserved11 = 0x40;
                model->reserved12 = 0x40;
            } else {
                model->reserved11 = 0;
                model->reserved12 = 0;
            }
            model->reserved13 = 0;
        }
    }
}

// decomp: BATTLE.PRG func_8009F898 (src/BATTLE/BATTLE.PRG/30DB0.c:3363)
void Actors_SetWeaponBrightnessOverride(int arg0, int arg1, int arg2)
{
    Actors_WeaponModel* model = Actors_weaponModels[arg0 * 2];

    if (model != NULL) {
        model->reserved5C0 = arg2;
        model->reserved08Bit6 = arg1;
    }
}

// decomp: BATTLE.PRG func_8009FB64 (src/BATTLE/BATTLE.PRG/30DB0.c:3467)
void Actors_SetRoomActorScale(VECTOR* arg0)
{
    int i;

    Actors_roomActorScale = *arg0;

    for (i = 0; i < 17; ++i) {
        if (Actors_actorRecords[i] != NULL) {
            Actors_actorRecords[i]->scale = Actors_modelRenderContext.actorScale;
        }
    }
}

// decomp: BATTLE.PRG func_8009FD5C (src/BATTLE/BATTLE.PRG/30DB0.c:3548)
int Actors_EnableModelPalettePulse(int arg0, int arg1, int arg2)
{
    int _[2] __attribute__((unused));
    Field_GimmickInstance* temp_t0;
    Actors_ModelEffectSlot* var_s0;
    Actors_ActorRecord* temp_a3 = Actors_actorRecords[arg0];

    if (temp_a3 != NULL) {
        var_s0 = &temp_a3->instance.palettePulse;
    } else {
        temp_t0 = Field_gimmickInstances[arg0];
        if (temp_t0 == NULL) {
            return -1;
        }
        var_s0 = &temp_t0->palettePulse;
    }

    if (var_s0->kind == 0) {
        var_s0->level = 0;
        var_s0->flags = (var_s0->flags & 0xFC) | (arg1 & 1);
        if (temp_a3 != NULL) {
            Render_LoadClutColors(temp_a3->basePalette, temp_a3->instance.slot + 0x16, 0, 0xA0);
        } else {
            Render_LoadClutColors(temp_t0->basePalette, 0x1F, temp_t0->slot * 16, 16);
        }
    }

    if (arg2 == -1) {
        var_s0->kind = 0xFF;
    } else {
        if (arg2 >= 8u) {
            return -2;
        }
        var_s0->kind |= 1 << (7 - arg2);
    }
    return 0;
}

// decomp: BATTLE.PRG func_8009FE74 (src/BATTLE/BATTLE.PRG/30DB0.c:3586)
int Actors_DisableModelPalettePulse(int arg0, int arg1)
{
    Actors_ActorRecord* temp_v0 = Actors_actorRecords[arg0];
    u_char* a0;

    if (temp_v0 == NULL) {
        if (Field_gimmickInstances[arg0] == NULL) {
            return -1;
        }
        a0 = (u_char*)Field_gimmickInstances[arg0] + 0x64;
    } else {
        a0 = (u_char*)temp_v0 + 0x64;
    }

    if (arg1 == -1) {
        *a0 = 0;
    } else {
        int v1;
        if (arg1 > 7u) {
            return -2;
        }
        v1 = ((1 << (7 - arg1)) ^ 0xFF);
        *a0 &= v1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800A06B8 (src/BATTLE/BATTLE.PRG/30DB0.c:3877)
void Actors_BeginActorShadowBlend(Actors_ActorRecord* arg0, int arg1)
{
    Actors_ShadowBlend* a2 = &arg0->shadowBlend;
    a2->radiusFrames = 0;
    if (arg1 == 0) {
        a2->radiusExpandRate = arg0->shadowRadiusExpandRate;
        a2->radiusShrinkRate = arg0->shadowRadiusShrinkRate;
        a2->level = 0;
    } else {
        a2->radiusExpandDelta = arg0->shadowRadiusExpandRate - a2->radiusExpandRate;
        a2->radiusShrinkDelta = arg0->shadowRadiusShrinkRate - a2->radiusShrinkRate;
        a2->levelDelta = -a2->level;
    }
    a2->levelFrames = arg1;
    a2->radiusFrames = arg1;
}

// decomp: BATTLE.PRG func_800A071C (src/BATTLE/BATTLE.PRG/30DB0.c:3894)
int Actors_ActorFadeParameters(int arg0, Render_ColorCode* arg1)
{
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];

    if (temp_a0 != NULL) {
        arg1->red = temp_a0->fadeRed;
        arg1->green = temp_a0->fadeGreen;
        arg1->blue = temp_a0->fadeBlue;
        return 0;
    }
    return -1;
}

// decomp: BATTLE.PRG func_800A0768 (src/BATTLE/BATTLE.PRG/30DB0.c:3907)
int Actors_SetActorFadeParameters(int arg0, Render_ColorCode* arg1)
{
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];

    if (temp_a0 != NULL) {
        temp_a0->fadeRed = arg1->red;
        temp_a0->fadeGreen = arg1->green;
        temp_a0->fadeBlue = arg1->blue;
        return 0;
    }
    return -1;
}

// decomp: BATTLE.PRG func_800A07B4 (src/BATTLE/BATTLE.PRG/30DB0.c:3920)
int Actors_ActorTint(int arg0, Render_ColorCode* arg1)
{
    if (Actors_actorRecords[arg0] == NULL) {
        return -1;
    }

    *arg1 = Actors_actorRecords[arg0]->instance.tint;
    return 0;
}

// decomp: BATTLE.PRG func_800A07FC (src/BATTLE/BATTLE.PRG/30DB0.c:3930)
int Actors_SetActorTint(int arg0, Render_ColorCode* arg1)
{
    Actors_ActorRecord* temp_a2 = Actors_actorRecords[arg0];

    if (temp_a2 == NULL) {
        return -1;
    }

    if (arg1 == NULL) {
        temp_a2->customTint = 0;
    } else {
        temp_a2->customTint = 1;
        temp_a2->instance.tint = *arg1;
    }

    return 0;
}

// decomp: src/BATTLE/BATTLE.PRG/40564.c:108
#define getActorFlags(actor) (*((int*)&(actor)->sequenceFlags - 2))
// decomp: BATTLE.PRG func_800AB788 (src/BATTLE/BATTLE.PRG/40564.c:1651)
void Actors_UpdateActorPalettePulse(Actors_ActorRecord* actor, Actors_PalettePulseView* object, int arg2)
{
    Actors_ModelEffectSlot* state;
    int count;
    int value;
    int i;
    int _[2] __attribute__((unused));

    if (actor == NULL) {
        count = 16;
        state = &object->pulse;
    } else {
        count = 160;

        if (!(getActorFlags(actor) & 0x8000000)) {
            count = 16;
        }

        state = &actor->instance.palettePulse;
    }

    value = state->level;

    if (State_tickSpeed == 4) {
        if (!(state->flags & 2)) {
            value += 2;

            if (value >= 16) {
                state->flags |= 2;
            }
        } else {
            value -= 2;

            if (value <= 0) {
                state->flags &= ~2;
            }
        }
    }

    if (!(state->flags & 2)) {
        value += 2;

        if (value >= 16) {
            state->flags |= 2;
        }
    } else {
        value -= 2;

        if (value <= 0) {
            state->flags &= ~2;
        }
    }

    state->level = value;

    for (i = 0; i < count; ++i) {
        int r;
        int g;
        int b;
        int color = (actor == NULL) ? object->basePalette[i] : actor->basePalette[i];

        if (color == 0) {
            continue;
        }

        r = color & 31;
        g = (color >> 5) & 31;
        b = (color >> 10) & 31;
        color &= 0x8000;

        if (value == 16) {
            r += 8;

            if (r > 31) {
                r = 31;
            }

            g += 18;

            if (g > 31) {
                g = 31;
            }

            b += 30;
        } else {
            r -= value / 2;

            if (r < 0) {
                r = 0;
            }

            g += value / 2;

            if (g > 31) {
                g = 31;
            }

            b += value;
        }

        if (b > 31) {
            b = 31;
        }

        if (actor == NULL) {
            object->pulsedPalette[i] = color | (b << 10) | (g << 5) | r;
        } else {
            actor->currentPalette[i] = color | (b << 10) | (g << 5) | r;
        }
    }

    if (actor == NULL) {
        Render_LoadClutColors(object->pulsedPalette, 31, object->slot * 16, count);
    } else {
        Render_LoadClutColors(actor->currentPalette, actor->instance.slot + 22, 0, count);
    }
}
#undef getActorFlags

// decomp: BATTLE.PRG func_800AB9A4 (src/BATTLE/BATTLE.PRG/40564.c:1769)
void Actors_UpdateActorAppearanceTransition(Field_GimmickInstance* arg0)
{
    SVECTOR boneA;
    SVECTOR boneB;
    u_short* header;
    int count;
    u_short* reference;
    u_short* weaponReference;
    int mode;
    int clutOffset;
    Field_GimmickInstance* object;
    int state;
    int distance;
    int angle;
    int i;
    int changed;
    int speed;
    int skip;
    int capped;
    signed char target;
    int value;
    u_short* colors;
    u_int flags;
    Actors_WeaponModel* weapon;

    if (arg0->motionState < 6) {
        if (arg0->transitionMode) {
            distance = Core_Gte_Sin(0x200);
            distance *= arg0->collisionRadius;
            distance /= ONE;
            Field_SampleActorCollisionPerimeter(arg0, distance);
            ((void (*)(void*, void*))Field_ApplyActorWallRepulsion)(arg0, &arg0->slideX);
            arg0->slideX /= 2;
            arg0->slideZ /= 2;
            Actors_UpdateActorJumpAndFall(arg0->slot, (Actors_ActorRecord*)arg0);
            if (arg0->transitionMode >= 4) {
                if (arg0->slot != 0) {
                    Actors_FindActorAnimationHeader(arg0, 0xAB, &header);
                    if (header != NULL) {
                        arg0->animationLocked = 0;
                        ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(arg0->slot, 0x47, 0, 8);
                        arg0->animationLocked = 1;
                    }
                }
            }
        }
    }
    flags = *(u_int*)((char*)arg0 + 8);
    speed = State_tickSpeed / 2;
    skip = 0;
    if (flags & 0x80) {
        skip = 1;
        if (*(int*)((char*)arg0 + 0x5AC) & 0x08000000) {
            count = 160;
        } else {
            skip = 0;
            count = 16;
        }
        colors = arg0->transitionPalette + skip;
        reference = arg0->basePaletteRows + skip;
    } else {
        object = arg0;
        count = 16;
        colors = (void*)((char*)arg0 + 0x14C);
        reference = (void*)((char*)arg0 + 0x12C);
        if (arg0->motionState == 2) {
            speed *= 2;
        }
    }
    state = arg0->motionState;
    switch (state) {
    case 2:
        mode = 0;
    start_fade:
        changed = 0;
    fade:
        changed |= Render_ApplyPaletteTransition(colors, count - skip, speed, mode, reference);
        if (arg0->transitionFlag) {
            for (i = 0; i < 2; ++i) {
                weapon = Actors_weaponModels[arg0->slot * 2 + i];
                if (weapon != NULL) {
                    weaponReference = weapon->workingPalette;
                    changed |= Render_ApplyPaletteTransition(weapon->uploadPalette, weapon->paletteColorCount, speed,
                        mode, weaponReference);
                }
            }
        }
        if (arg0->motionState == 3 && changed == 0) {
            arg0->baseColor = 0x404040;
            Render_ApplyPaletteTransition(colors, count - skip, speed, 2, reference);
            if (arg0->transitionFlag) {
                for (i = 0; i < 2; ++i) {
                    weapon = Actors_weaponModels[arg0->slot * 2 + i];
                    if (weapon != NULL) {
                        weaponReference = weapon->workingPalette;
                        Render_ApplyPaletteTransition(weapon->uploadPalette, weapon->paletteColorCount, speed, 2,
                            weaponReference);
                    }
                }
            }
        }
    load_cluts:
        if (arg0->transitionFlag) {
            Render_LoadClutColors(arg0->transitionPalette, arg0->slot + 4, 0, count);
            for (i = 0, clutOffset = 160; i < 2; ++i, clutOffset += 48) {
                weapon = Actors_weaponModels[arg0->slot * 2 + i];
                if (weapon != NULL) {
                    weapon->ownerMotionState = arg0->motionState;
                    Render_LoadClutColors(
                        weapon->uploadPalette, arg0->slot + 4, clutOffset, weapon->paletteColorCount);
                }
            }
        } else {
            Render_LoadClutColors(
                (u_short*)((char*)object + 0x14C), 31, object->slot * 16, 16);
        }
        if (changed == 0) {
            ++arg0->motionState;
            if (arg0->transitionFlag) {
                for (i = 0; i < 2; ++i) {
                    weapon = Actors_weaponModels[arg0->slot * 2 + i];
                    if (weapon != NULL) {
                        weapon->ownerMotionState = arg0->motionState;
                    }
                }
            }
        }
    case 1:
    case 15:
    default:
        return;
    case 3:
        changed = 0;
        mode = 1;
        goto fade;
    case 4:
        if (*(int*)((char*)arg0 + 0x5AC) & 8) {
            object = Field_gimmickInstances[arg0->linkedGimmick];
            ((void (*)(int, int, SVECTOR*, int))Actors_ActorTaggedBonePosition)(object->parent, 0xF0, &boneA, 1);
            ((void (*)(int, int, SVECTOR*, int))Actors_ActorTaggedBonePosition)(object->parent, 0xF1, &boneB, 1);
            object->positionX = arg0->positionX;
            object->positionZ = arg0->positionZ;
            object->positionY = (boneA.vy + boneB.vy) / 2 + 60;
            angle = (u_short)arg0->rotationY;
            object->mode = 0xFC;
            *(int*)((char*)object + 0x34) = 0;
            object->parent = 0xFF;
            object->rotationY = (object->rotationY + angle) & 0xFFF;
            arg0->actionPoseActive = 0;
            Actors_pushedGimmickIndex = arg0->linkedGimmick;
        }
        value = (u_char)arg0->baseColor - speed * 4;
        changed = 0;
        if (value < 0) {
            value = 0;
        }
        if (value != 0) {
            changed = 1;
        }
        *(u_char*)&arg0->baseColor = value;
        *((signed char*)arg0 + 0x55) = value;
        i = (signed char)arg0->fadeRed;
        i += speed * 2;
        *((signed char*)arg0 + 0x56) = value;
        if (i > 64) {
            i = 64;
        }
        if (i != 64) {
            changed = 1;
        }
        arg0->fadeRed = i;
        goto load_cluts;
    case 6:
        mode = 3;
        value = *((u_char*)arg0 + 0x57);
        value += 1;
        capped = value & 0xFF;
        changed = capped < 15u;
        i = (signed char)arg0->fadeRed;
        i += speed * 2;
        capped = i < 65;
        *((u_char*)arg0 + 0x57) = value;
        if (capped == 0) {
            i = 64;
        }
        arg0->fadeRed = i;
        goto fade;
    case 7:
        changed = 0;
        mode = 1;
        i = (signed char)arg0->fadeRed;
        i += speed * 2;
        capped = i < 65;
        if (capped == 0) {
            i = 64;
        }
        arg0->fadeRed = i;
        goto fade;
    case 9:
        changed = 0;
        mode = 2;
        *(u_char*)&arg0->baseColor = 0xFF;
        *((u_char*)arg0 + 0x55) = 0xFF;
        *((u_char*)arg0 + 0x56) = 0xFF;
        goto fade;
    case 10:
        changed = 0;
        target = arg0->fadeGreen;
        i = (signed char)arg0->fadeRed;
        i -= speed * 2;
        mode = 4;
        if (i < target) {
            i = target;
        }
        if (i != target) {
            changed = 1;
        }
        arg0->fadeRed = i;
        changed |= Render_StepRgbTowardTarget(&arg0->baseColor, &arg0->tint, -speed * 8);
        goto fade;
    case 11:
        arg0->motionState = 0;
        return;
    case 12:
        changed = 0;
        mode = 5;
        goto fade;
    case 14:
        arg0->fadeRed = 64;
        mode = 6;
        goto start_fade;
    }
}

// decomp: BATTLE.PRG func_800AC37C (src/BATTLE/BATTLE.PRG/40564.c:2179)
void Actors_StartActorAppearanceTransition(int arg0, int arg1)
{
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];

    switch (arg1) {
    case 0:
        temp_a0->instance.actionAnimationState = 14;
        break;
    case 1:
        temp_a0->instance.actionAnimationState = 6;
        break;
    case 2:
        temp_a0->instance.actionAnimationState = 9;
        break;
    }
    if (temp_a0->companionSlot > 1) {
        Actors_StartActorAppearanceTransition(temp_a0->companionSlot, arg1);
    }
}

