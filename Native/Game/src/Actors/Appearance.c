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
void Actors_SetActorPaletteFade(int actor_id, int fade_parameter)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];
    if (actor != NULL) {
        if (fade_parameter == 0) {
            actor->paletteFadeActive = 0;
        } else {
            actor->paletteFadeState[0] = 0;
            actor->paletteFadeActive = 1;
            actor->paletteFadeState[1] = fade_parameter * 2;
            actor->paletteFadeReverse = 0;
        }
    }
}

// decomp: BATTLE.PRG func_8009D88C (src/BATTLE/BATTLE.PRG/30DB0.c:2195)
void Actors_ResetActorWeaponVisibilityBlend(int actor_id)
{
    int i;
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor == NULL) {
        return;
    }

    for (i = 0; i < 2; ++i) {
        Actors_WeaponModel* model = Actors_weaponModels[actor->instance.slot * 2 + i];
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
void Actors_SetWeaponBrightnessOverride(int actor_id, int override_enabled, int brightness)
{
    Actors_WeaponModel* model = Actors_weaponModels[actor_id * 2];

    if (model != NULL) {
        model->reserved5C0 = brightness;
        model->reserved08Bit6 = override_enabled;
    }
}

// decomp: BATTLE.PRG func_8009FB64 (src/BATTLE/BATTLE.PRG/30DB0.c:3467)
void Actors_SetRoomActorScale(VECTOR* room_scale)
{
    int i;

    Actors_roomActorScale = *room_scale;

    for (i = 0; i < 17; ++i) {
        if (Actors_actorRecords[i] != NULL) {
            Actors_actorRecords[i]->scale = Actors_modelRenderContext.actorScale;
        }
    }
}

// decomp: BATTLE.PRG func_8009FD5C (src/BATTLE/BATTLE.PRG/30DB0.c:3548)
int Actors_EnableModelPalettePulse(int actor_id, int pulse_flags, int pulse_slot)
{
    int _[2] __attribute__((unused));
    Field_GimmickInstance* gimmick;
    Actors_ModelEffectSlot* pulse;
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor != NULL) {
        pulse = &actor->instance.palettePulse;
    } else {
        gimmick = Field_gimmickInstances[actor_id];
        if (gimmick == NULL) {
            return -1;
        }
        pulse = &gimmick->palettePulse;
    }

    if (pulse->kind == 0) {
        pulse->level = 0;
        pulse->flags = (pulse->flags & 0xFC) | (pulse_flags & 1);
        if (actor != NULL) {
            Render_LoadClutColors(actor->basePalette, actor->instance.slot + 0x16, 0, 0xA0);
        } else {
            Render_LoadClutColors(gimmick->basePalette, 0x1F, gimmick->slot * 16, 16);
        }
    }

    if (pulse_slot == -1) {
        pulse->kind = 0xFF;
    } else {
        if (pulse_slot >= 8u) {
            return -2;
        }
        pulse->kind |= 1 << (7 - pulse_slot);
    }
    return 0;
}

// decomp: BATTLE.PRG func_8009FE74 (src/BATTLE/BATTLE.PRG/30DB0.c:3586)
int Actors_DisableModelPalettePulse(int actor_id, int pulse_slot)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];
    u_char* active_pulse_mask;

    if (actor == NULL) {
        if (Field_gimmickInstances[actor_id] == NULL) {
            return -1;
        }
        active_pulse_mask = (u_char*)Field_gimmickInstances[actor_id] + 0x64;
    } else {
        active_pulse_mask = (u_char*)actor + 0x64;
    }

    if (pulse_slot == -1) {
        *active_pulse_mask = 0;
    } else {
        int keep_pulse_mask;
        if (pulse_slot > 7u) {
            return -2;
        }
        keep_pulse_mask = ((1 << (7 - pulse_slot)) ^ 0xFF);
        *active_pulse_mask &= keep_pulse_mask;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800A06B8 (src/BATTLE/BATTLE.PRG/30DB0.c:3877)
void Actors_BeginActorShadowBlend(Actors_ActorRecord* actor, int blend_frames)
{
    Actors_ShadowBlend* shadow_blend = &actor->shadowBlend;
    shadow_blend->radiusFrames = 0;
    if (blend_frames == 0) {
        shadow_blend->radiusExpandRate = actor->shadowRadiusExpandRate;
        shadow_blend->radiusShrinkRate = actor->shadowRadiusShrinkRate;
        shadow_blend->level = 0;
    } else {
        shadow_blend->radiusExpandDelta = actor->shadowRadiusExpandRate - shadow_blend->radiusExpandRate;
        shadow_blend->radiusShrinkDelta = actor->shadowRadiusShrinkRate - shadow_blend->radiusShrinkRate;
        shadow_blend->levelDelta = -shadow_blend->level;
    }
    shadow_blend->levelFrames = blend_frames;
    shadow_blend->radiusFrames = blend_frames;
}

// decomp: BATTLE.PRG func_800A071C (src/BATTLE/BATTLE.PRG/30DB0.c:3894)
int Actors_ActorFadeParameters(int actor_id, Render_ColorCode* fade_color_out)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor != NULL) {
        fade_color_out->red = actor->fadeRed;
        fade_color_out->green = actor->fadeGreen;
        fade_color_out->blue = actor->fadeBlue;
        return 0;
    }
    return -1;
}

// decomp: BATTLE.PRG func_800A0768 (src/BATTLE/BATTLE.PRG/30DB0.c:3907)
int Actors_SetActorFadeParameters(int actor_id, Render_ColorCode* fade_color)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor != NULL) {
        actor->fadeRed = fade_color->red;
        actor->fadeGreen = fade_color->green;
        actor->fadeBlue = fade_color->blue;
        return 0;
    }
    return -1;
}

// decomp: BATTLE.PRG func_800A07B4 (src/BATTLE/BATTLE.PRG/30DB0.c:3920)
int Actors_ActorTint(int actor_id, Render_ColorCode* tint_out)
{
    if (Actors_actorRecords[actor_id] == NULL) {
        return -1;
    }

    *tint_out = Actors_actorRecords[actor_id]->instance.tint;
    return 0;
}

// decomp: BATTLE.PRG func_800A07FC (src/BATTLE/BATTLE.PRG/30DB0.c:3930)
int Actors_SetActorTint(int actor_id, Render_ColorCode* tint)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    if (actor == NULL) {
        return -1;
    }

    if (tint == NULL) {
        actor->customTint = 0;
    } else {
        actor->customTint = 1;
        actor->instance.tint = *tint;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800AC37C (src/BATTLE/BATTLE.PRG/40564.c:2179)
void Actors_StartActorAppearanceTransition(int actor_id, int transition_kind)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actor_id];

    switch (transition_kind) {
    case 0:
        actor->instance.actionAnimationState = 14;
        break;
    case 1:
        actor->instance.actionAnimationState = 6;
        break;
    case 2:
        actor->instance.actionAnimationState = 9;
        break;
    }
    if (actor->companionSlot > 1) {
        Actors_StartActorAppearanceTransition(actor->companionSlot, transition_kind);
    }
}

