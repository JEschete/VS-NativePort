// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/WeaponModel.h"
#include "Field/Gimmicks.h"
#include "Render/Palette.h"

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

