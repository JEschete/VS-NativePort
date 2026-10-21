// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorState.h"
#include "Actors/Appearance.h"
#include "Audio/ActorSound.h"
#include "Audio/Sfx.h"
#include "Audio/Stream.h"
#include "Battle/Abilities.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResults.h"
#include "Core/Memory/Heap.h"
#include "Effects/ActionEffects.h"
#include "Effects/Camera.h"
#include "Effects/Curves.h"
#include "Effects/EffectResources.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Timeline.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Input/Controller.h"
#include "PFile.h"
#include "Render/Palette.h"

// decomp: BATTLE.PRG func_800D55A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7885)
void Effects_LoadEffectTimeline(Effects_EffectInstance* arg0, int arg1, int arg2)
{
    Content_PFileTimelineTable* entry = Content_PFileTimelineSection(Effects_package->block3Data, arg1);
    Effects_EffectTimeline* p;
    int count = 0;
    int i;

    if (arg0->context.timeline != NULL) {
        Core_Memory_FreeFromHighEnd(arg0->context.timeline);
    }

    arg0->context.timeline = Core_Memory_AllocateFromHighEnd(sizeof(Effects_EffectTimeline) + entry->trackCount * sizeof(Effects_TimelineTrack));
    p = arg0->context.timeline;
    p->trackCount = entry->trackCount;
    p->frame = 0;
    p->activeTracks = 0;

    for (i = 0; i < p->trackCount; ++i) {
        p->tracks[i].keys = Content_PFileTimelineTrackData(Effects_package->block3Data, arg1, i);
        p->tracks[i].header = entry->tracks[i].header;

        if (p->tracks[i].header.kind == 3) {
            ++count;
        }

        p->tracks[i].keyIndex = -1;
        *(int*)&p->tracks[i].curveFrame = 0;
        p->tracks[i].keyParameter = 0;
        Effects_AdvanceEffectTimelineKey(&p->tracks[i]);
        p->activeTracks |= 1 << i;
    }

    p->flags = count ? 2 : 0;
    Core_Debugf("effects", "timeline=%d state=%p tracks=%d stride=%u", arg1, (void*)p,
        (int)p->trackCount, (unsigned int)sizeof(Effects_TimelineTrack));
}

// decomp: BATTLE.PRG func_800D5700 (src/BATTLE/BATTLE.PRG/5BF94.c:7920)
void Effects_InitializeParticleTimelineKey(Effects_TimelineTrack* arg0)
{
    Effects_ParticleTrackKey* base = (Effects_ParticleTrackKey*)arg0->keys;
    arg0->keyParameter = base[arg0->keyIndex].opaque2Bit4;
    arg0->keyTimer = base[arg0->keyIndex].duration;
}

// decomp: BATTLE.PRG func_800D5738 (src/BATTLE/BATTLE.PRG/5BF94.c:7927)
void Effects_InitializeShakeTimelineKey(Effects_TimelineTrack* arg0)
{
    arg0->keyTimer =
        Effects_SegmentedEffectCurveDuration((((Effects_ShakeTrackKey*)&arg0->keys[arg0->keyIndex * 2 + 1])->curve));
}

// decomp: BATTLE.PRG func_800D5780 (src/BATTLE/BATTLE.PRG/5BF94.c:7933)
int Effects_AdvanceEffectTimelineKey(Effects_TimelineTrack* arg0)
{
    if (arg0->keyIndex + 1 >= arg0->header.keyCount) {
        return 0;
    }
    arg0->keyIndex++;
    switch (arg0->header.kind) {
    case 1:
        Effects_InitializeParticleTimelineKey(arg0);
        break;
    case 0xD:
        Effects_InitializeShakeTimelineKey(arg0);
        break;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D57FC (src/BATTLE/BATTLE.PRG/5BF94.c:7950)
int Effects_TickParticleEmissionTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1)
{
    Effects_ParticleTrackKey* e = (Effects_ParticleTrackKey*)arg1->keys;
    int ret = 1;
    int key = e[arg1->keyIndex].frame;
    int cur = arg0->context.timeline->frame;

    if (cur < key) {
        return ret;
    }
    Effects_SampleAllEffectCurves(arg1->curveFrame);
    Effects_EmitParticleEvent(
        arg0, e[arg1->keyIndex].opaque2Bit4, e[arg1->keyIndex].opaque3Bit2, e[arg1->keyIndex].opaque2Bit0, NULL);
    ++arg1->curveFrame;
    if (!(arg0->context.timeline->flags & 1)) {
        --arg1->keyTimer;
    }
    if (arg1->keyTimer == 0) {
        ret = Effects_AdvanceEffectTimelineKey(arg1);
        arg1->curveFrame = 0;
    }
    return ret;
}

// decomp: BATTLE.PRG func_800D5904 (src/BATTLE/BATTLE.PRG/5BF94.c:7974)
int Effects_TickEffectSoundTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1)
{
    SVECTOR position;
    u_long* commands = arg1->keys;
    u_long command = commands[arg1->keyIndex];

    if ((command & 0x1FF) != arg0->context.timeline->frame) {
        return 1;
    }

    if (((command >> 18) & 3) == 2) {
        if (Effects_package->auxiliarySoundHandle != 0) {
            Audio_PlayCutsceneStreamSlotCentered(Effects_package->auxiliarySoundHandle);
        }
    } else {
        int bank = ((command >> 18) & 3) == 0 ? 0x7E : 0xF00000;

        if (((command >> 20) & 3) == 0) {
            Audio_PlaySfxCentered(bank, (command >> 9) & 0x1FF);
        } else {
            setVector(&position,
                arg0->transforms[(commands[arg1->keyIndex] >> 22) & 63].world.t[0],
                arg0->transforms[(commands[arg1->keyIndex] >> 22) & 63].world.t[1],
                arg0->transforms[(commands[arg1->keyIndex] >> 22) & 63].world.t[2]);
            Audio_PlayWorldSfx(bank, (commands[arg1->keyIndex] >> 9) & 0x1FF, &position);
        }
    }

    return Effects_AdvanceEffectTimelineKey(arg1);
}

// decomp: BATTLE.PRG func_800D5A98 (src/BATTLE/BATTLE.PRG/5BF94.c:8005)
int Effects_TickChildEffectTrack(Effects_EffectInstance* arg0, Effects_ChildTrackView* arg1, int arg2)
{
    Effects_EffectSpawnRequest spawn;
    Effects_ChildTrackKey* event = arg1->event;
    Effects_EffectInstance* child;
    int result = 1;

    if (event->frame > arg0->context.timeline->frame) {
        return result;
    }

    if (arg1->index < Effects_package->request->count) {
        while (arg1->delay == 0) {
            spawn.script = Effects_package->block8Data;
            spawn.scriptOffset = arg2;
            spawn.priority = 0;
            spawn.source = &Effects_package->request->source;
            spawn.emitFrom = &Effects_package->request->source;
            spawn.target = &Effects_package->request->targets[arg1->index];
            spawn.emitTo = &Effects_package->request->targets[arg1->index];
            spawn.parent = arg0;
            spawn.token = arg0->token;
            child = Effects_CreatePrimaryEffectInstance(&spawn);
            ++arg0->childCount;

            if (spawn.target->type == 4) {
                child->registers[1] = event->actors[spawn.target->value.actor.eventActorSlot];
            } else {
                child->registers[1] = event->actors[0];
            }

            if (++arg1->index == Effects_package->request->count) {
                break;
            }

            arg1->delay += event->delay;
        }

        if (arg1->delay != 0) {
            --arg1->delay;
        }
    } else if (arg1->index == Effects_package->request->count) {
        if (arg0->childCount == 0) {
            spawn.script = Effects_package->block8Data;
            spawn.scriptOffset = arg2;
            spawn.priority = 0;
            spawn.source = &Effects_package->request->source;
            spawn.emitFrom = &Effects_package->request->source;
            spawn.target = &Effects_package->request->targets[0];
            spawn.emitTo = &Effects_package->request->targets[0];
            spawn.parent = arg0;
            spawn.token = arg0->token;
            child = Effects_CreatePrimaryEffectInstance(&spawn);
            ++arg0->childCount;
            child->registers[1] = event->final;
            ++arg1->index;
        }
    } else {
        result = 0;
    }

    return result;
}

// decomp: BATTLE.PRG func_800D5D74 (src/BATTLE/BATTLE.PRG/5BF94.c:8069)
int Effects_TickEffectRendererTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1)
{
    Effects_RendererTrackKey* temp_a3 = (Effects_RendererTrackKey*)(arg1->keyIndex * 8 + (uintptr_t)arg1->keys);

    if (temp_a3->frame == arg0->context.timeline->frame) {
        Effects_AttachEffectRenderer(arg0, Effects_effectRenderers[temp_a3->effectRenderer],
            (temp_a3->opaque1Bit1 + temp_a3->opaque2Bit5 * 256) | (temp_a3->opaque4 << 0x10));
        return Effects_AdvanceEffectTimelineKey(arg1);
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D5E00 (src/BATTLE/BATTLE.PRG/5BF94.c:8081)
int Effects_TickControllerVibrationTrack(Effects_EffectInstance* actor, Effects_TimelineTrack* event)
{
    int result = 1;
    u_short* data = (u_short*)event->keys;
    int play = 0;
    int i;

    if ((data[event->keyIndex] & 0x1FF) != actor->context.timeline->frame) {
        return result;
    }

    switch (data[event->keyIndex] >> 14) {
    case 1:
        if (actor->context.source.type == 4 && actor->context.source.value.actor.actor == 0) {
            play = 1;
        }
        break;
    case 2:
        if (actor->context.target.type == 4 && actor->context.target.value.actor.actor == 0) {
            play = 1;
        }
        break;
    case 0:
        play = 1;
        break;
    case 3:
        for (i = 0; i < Effects_package->request->count; ++i) {
            if (Effects_package->request->targets[i].type == 4
                && Effects_package->request->targets[i].value.actor.actor == 0) {
                play = 1;
                break;
            }
        }
        break;
    }

    if (play) {
        u_char* sound = Effects_package->block11Data;
        Input_StartVibrationPattern(
            sound + *(((data[event->keyIndex] >> 9) & 31) + ((Effects_PackageBlock11*)sound)->offsets),
            255, 1);
    }

    result = Effects_AdvanceEffectTimelineKey(event);
    return result;
}

// decomp: BATTLE.PRG func_800D5F8C (src/BATTLE/BATTLE.PRG/5BF94.c:8128)
int Effects_TickSourceActorColorTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1)
{
    Effects_ColorTrackKey* temp_s0 = (Effects_ColorTrackKey*)arg1->keys + arg1->keyIndex;

    if (temp_s0->frame == arg0->context.timeline->frame) {
        if ((arg0->context.emitFrom.type == 4)
            && (Actors_ActorStateFlags(arg0->context.emitFrom.value.actor.actor) & 2)) {
            Render_TransformActorColors(temp_s0->mode, temp_s0->duration,
                arg0->context.emitFrom.value.actor.actor, temp_s0->red - 0x80,
                temp_s0->green - 0x80, temp_s0->blue - 0x80);
        }
        return Effects_AdvanceEffectTimelineKey(arg1);
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D6048 (src/BATTLE/BATTLE.PRG/5BF94.c:8144)
int Effects_TickTargetActorColorTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1, int arg2)
{
    int i;
    Effects_ColorTrackKey* entry = (Effects_ColorTrackKey*)arg1->keys + arg1->keyIndex;

    if (entry->frame != arg0->context.timeline->frame) {
        return 1;
    }

    if (arg2 != 0) {
        Effects_ActionEffectRequest* effect = &Effects_actionRequest;

        for (i = 0; i < Effects_actionRequest.count; ++i) {
            if ((effect->targets[i].type == 4)
                && (Actors_ActorStateFlags(effect->targets[i].value.actor.actor) & 2)) {
                Render_TransformActorColors(entry->mode, entry->duration,
                    effect->targets[i].value.actor.actor, entry->red - 128,
                    entry->green - 128, entry->blue - 128);
            }
        }
    } else if ((arg0->context.emitTo.type == 4)
               && (Actors_ActorStateFlags(arg0->context.emitTo.value.actor.actor) & 2)) {
        Render_TransformActorColors(entry->mode, entry->duration, arg0->context.emitTo.value.actor.actor,
            entry->red - 128, entry->green - 128, entry->blue - 128);
    }
    return Effects_AdvanceEffectTimelineKey(arg1);
}

// decomp: BATTLE.PRG func_800D61AC (src/BATTLE/BATTLE.PRG/5BF94.c:8172)
int Effects_TickOtherActorColorTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1)
{
    int i;
    Effects_ColorTrackKey* temp_s2 = (Effects_ColorTrackKey*)(arg1->keys + (arg1->keyIndex * 2));

    if (temp_s2->frame == arg0->context.timeline->frame) {
        for (i = 0; i < Effects_actionRequest.otherCount; ++i) {
            if (Actors_ActorStateFlags(Effects_actionRequest.others[i]) & 2) {
                Render_TransformActorColors(temp_s2->mode, temp_s2->duration, Effects_actionRequest.others[i],
                    temp_s2->red - 0x80, (temp_s2->green - 0x80), (temp_s2->blue - 0x80));
            }
        }
        return Effects_AdvanceEffectTimelineKey(arg1);
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D6298 (src/BATTLE/BATTLE.PRG/5BF94.c:8189)
int Effects_TickSceneColorTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1)
{
    Effects_ColorTrackKey* temp_a3 = (Effects_ColorTrackKey*)arg1->keys + (arg1->keyIndex);

    if (temp_a3->frame == arg0->context.timeline->frame) {
        Render_TransformSceneColors(temp_a3->mode, temp_a3->duration, temp_a3->red - 0x80,
            temp_a3->green - 0x80, (temp_a3->blue - 0x80));
        return Effects_AdvanceEffectTimelineKey(arg1);
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D6310 (src/BATTLE/BATTLE.PRG/5BF94.c:8201)
int Effects_TickBackgroundColorTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1)
{
    Effects_ColorTrackKey* temp_a3 = (Effects_ColorTrackKey*)(arg1->keys + arg1->keyIndex * 2);

    if (temp_a3->frame == arg0->context.timeline->frame) {
        Render_TransformBackgroundColors(temp_a3->mode, temp_a3->duration, temp_a3->red - 0x80,
            temp_a3->green - 0x80, temp_a3->blue - 0x80);
        return Effects_AdvanceEffectTimelineKey(arg1);
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D6388 (src/BATTLE/BATTLE.PRG/5BF94.c:8213)
void Effects_ApplyTargetActionImpact(Effects_EffectInstance* arg0)
{
    if (arg0->context.target.type == 4) {
        Battle_ApplyActionTargetEffects(arg0->context.target.value.actor.actor, NULL);
    } else {
        Battle_ApplyActionTargetEffects(
            arg0->context.target.value.position.pad, &arg0->context.target.value.position);
    }
}

// decomp: BATTLE.PRG func_800D63D0 (src/BATTLE/BATTLE.PRG/5BF94.c:8223)
void Effects_ApplySourceActionImpact(Effects_EffectInstance* arg0)
{
    if (arg0->context.source.type == 4) {
        Battle_ApplyActionSourceEffects(arg0->context.source.value.actor.actor, NULL);
    } else {
        Battle_ApplyActionSourceEffects(
            arg0->context.source.value.position.pad, &arg0->context.source.value.position);
    }
}

// decomp: BATTLE.PRG func_800D6418 (src/BATTLE/BATTLE.PRG/5BF94.c:8233)
void Effects_ActivateTargetRoomTile(Effects_EffectInstance* arg0)
{
    if (arg0->context.target.type == 5) {
        Field_DisableTrapAtPosition(&arg0->context.target.value.position);
    }
}

// decomp: BATTLE.PRG func_800D6448 (src/BATTLE/BATTLE.PRG/5BF94.c:8240)
void Effects_SpawnTransformWeaponTrail(Effects_EffectInstance* arg0, u_char arg1, u_char arg2)
{
    Effects_SaveEffectResourceContext();
    Effects_SpawnBoneTrail(arg0->transforms[arg1].actor, arg0->transforms[arg1].bone, arg2 % 10, arg2 / 10);
    Effects_RestoreEffectResourceContext();
}

// decomp: BATTLE.PRG func_800D64E4 (src/BATTLE/BATTLE.PRG/5BF94.c:8247)
void Effects_FadeActionWeaponTrails(void) { Effects_FadeBoneTrails(0, 0); }

// decomp: BATTLE.PRG func_800D6508 (src/BATTLE/BATTLE.PRG/5BF94.c:8249)
void Effects_ResumeEffectTransformMotion(Effects_TransformControlView* arg0, char arg1)
{
    arg0 += arg1;
    arg0->motionResumed = 1;
}

// decomp: BATTLE.PRG func_800D6538 (src/BATTLE/BATTLE.PRG/5BF94.c:8255)
void Effects_HoldEffectTimelineFrame(Effects_EffectInstance* arg0) { arg0->context.timeline->flags |= 1; }

// decomp: BATTLE.PRG func_800D6554 (src/BATTLE/BATTLE.PRG/5BF94.c:8257)
void Effects_PauseMovingRoomObjects(void) { Field_FreezeRoomGimmicksTemporarily(); }

// decomp: BATTLE.PRG func_800D6574 (src/BATTLE/BATTLE.PRG/5BF94.c:8259)
void Effects_SignalSourceActorFromTimeline(Effects_EffectInstance* arg0)
{
    if (arg0->context.source.type == 4) {
        Effects_SignalActorEffectAnimation(arg0->context.source.value.actor.actor);
    }
}

// decomp: BATTLE.PRG func_800D65A8 (src/BATTLE/BATTLE.PRG/5BF94.c:8266)
void Effects_InvalidateEffectTransformAttachments(Effects_TransformControlView* arg0, char arg1)
{
    arg0 += arg1;
    arg0->attachmentMode = 0;
}

// decomp: BATTLE.PRG func_800D65D8 (src/BATTLE/BATTLE.PRG/5BF94.c:8272)
void Effects_SetSourceWeaponBrightness(Effects_EffectInstance* arg0, u_char arg1)
{

    if (arg0->context.emitFrom.type == 4) {
        if (arg1 != 0) {
            Actors_SetWeaponBrightnessOverride(arg0->context.emitFrom.value.actor.actor, 1, arg1);

        } else {
            Actors_SetWeaponBrightnessOverride(arg0->context.emitFrom.value.actor.actor, 0, 0);
        }
    }
}

// decomp: BATTLE.PRG func_800D6628 (src/BATTLE/BATTLE.PRG/5BF94.c:8285)
void Effects_PlaySourceActorReactionSound(Effects_EffectInstance* arg0, u_char arg1)
{
    int var;

    if (arg0->context.emitFrom.type == 4) {
        if (arg1 < 4) {
            Audio_PlayActorModelSoundVariant(arg0->context.emitFrom.value.actor.actor, arg1);
        } else {
            do {
                var = Core_Random_Rand() % 3;
            } while (var == Effects_lastReactionSoundVariant);
            Audio_PlayActorModelSoundVariant(arg0->context.emitFrom.value.actor.actor, Effects_lastReactionSoundVariant = var);
        }
    }
}

// decomp: BATTLE.PRG func_800D66CC (src/BATTLE/BATTLE.PRG/5BF94.c:8301)
void Effects_ActivateTargetGimmick(Effects_EffectInstance* arg0)
{
    if (arg0->context.target.type == 5) {
        Field_ApplyUnlockEffectToRoomObject(&arg0->context.target.value.position);
    }
}

// decomp: BATTLE.PRG func_800D6860 (src/BATTLE/BATTLE.PRG/5BF94.c:8335)
void Effects_PrepareAshleyTargetImpact(Effects_EffectInstance* arg0)
{
    if (arg0->context.emitTo.type == 4) {
        Battle_ArmDefenseAbilityTimingWindow(arg0->context.emitTo.value.actor.actor);
    }
}

// decomp: BATTLE.PRG func_800D6894 (src/BATTLE/BATTLE.PRG/5BF94.c:8342)
int Effects_TickActionControlTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1)
{
    Effects_ActionControlKey* temp_a2 = (Effects_ActionControlKey*)(arg1->keys - (-arg1->keyIndex));

    if ((temp_a2->frame & 0x1FF) == arg0->context.timeline->frame) {
        u_char temp_a1_2 = temp_a2->opaque2;
        u_char temp_a2_2 = temp_a2->opaque3;

        switch (temp_a2->frame >> 9) {
        case 0:
            Effects_ApplyTargetActionImpact(arg0);
            break;

        case 1:
            Effects_ApplySourceActionImpact(arg0);
            break;

        case 2:
            Effects_ActivateTargetRoomTile(arg0);
            break;

        case 3:
            Effects_SpawnTransformWeaponTrail(arg0, temp_a1_2, temp_a2_2);
            break;

        case 4:
            Effects_FadeActionWeaponTrails();
            break;

        case 5:
            Effects_ResumeEffectTransformMotion((Effects_TransformControlView*)arg0, temp_a1_2);
            break;

        case 6:
            Effects_HoldEffectTimelineFrame(arg0);
            break;

        case 7:
            Effects_PauseMovingRoomObjects();
            break;

        case 8:
            Effects_SignalSourceActorFromTimeline(arg0);
            break;

        case 9:
            Effects_InvalidateEffectTransformAttachments((Effects_TransformControlView*)arg0, temp_a1_2);
            break;

        case 10:
            Effects_SetSourceWeaponBrightness(arg0, temp_a1_2);
            break;

        case 11:
            Effects_PlaySourceActorReactionSound(arg0, temp_a1_2);
            break;

        case 12:
            Effects_ActivateTargetGimmick(arg0);
            break;

        case 13:
            Effects_FocusCameraOnEffectTransform(arg0, temp_a1_2, temp_a2_2);
            break;

        case 14:
            Effects_RestoreActionEffectCamera(arg0, temp_a1_2);
            break;

        case 15:
            Effects_PrepareAshleyTargetImpact(arg0);
            break;
        }

        return Effects_AdvanceEffectTimelineKey(arg1);
    }

    return 1;
}

// decomp: BATTLE.PRG func_800D6A18 (src/BATTLE/BATTLE.PRG/5BF94.c:8422)
int Effects_TickCameraShakeTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1)
{
    Effects_ShakeChannelKey* e = (Effects_ShakeChannelKey*)arg1->keys;
    int ret = 1;
    int key = e[arg1->keyIndex].frame;
    int cur = arg0->context.timeline->frame;

    if (cur < key) {
        return ret;
    }
    if (key == cur) {
        Effects_StartCameraShakeChannels(e->opaque4Bit0, e->opaque4Bit4, e->opaque5Bit1, e->opaque6Bit0, e->opaque6Bit1, e->opaque7Bit0,
            e->opaque7Bit1);
    }
    if (--arg1->keyTimer == 0) {
        ret = Effects_AdvanceEffectTimelineKey(arg1);
    }
    return ret;
}

// decomp: BATTLE.PRG func_800D6AEC (src/BATTLE/BATTLE.PRG/5BF94.c:8442)
void Effects_TickEffectTimeline(Effects_EffectInstance* arg0, int arg1)
{
    Effects_EffectTimeline* temp_a0;
    Effects_TimelineTrack* temp_a1;
    int var_s1;

    for (var_s1 = 0; var_s1 < arg0->context.timeline->trackCount; var_s1++) {
        if ((arg0->context.timeline->activeTracks >> var_s1) & 1) {
            int temp;
            Effects_TimelineTrack* temp_a1 =
                &arg0->context.timeline->tracks[var_s1];
            switch (temp_a1->header.kind) {
            case 1:
                temp = Effects_TickParticleEmissionTrack(arg0, temp_a1);
                break;
            case 2:
                temp = Effects_TickEffectSoundTrack(arg0, temp_a1);
                break;
            case 3:
                temp = Effects_TickChildEffectTrack(arg0, (Effects_ChildTrackView*)temp_a1, arg1);
                break;
            case 4:
                temp = Effects_TickEffectRendererTrack(arg0, temp_a1);
                break;
            case 5:
                temp = Effects_TickControllerVibrationTrack(arg0, temp_a1);
                break;
            case 6:
                temp = Effects_TickSourceActorColorTrack(arg0, temp_a1);
                break;
            case 7:
                temp = Effects_TickTargetActorColorTrack(arg0, temp_a1, arg0->context.timeline->flags & 2);
                break;
            case 8:
                temp = Effects_TickOtherActorColorTrack(arg0, temp_a1);
                break;
            case 9:
                temp = Effects_TickSceneColorTrack(arg0, temp_a1);
                break;
            case 10:
                temp = Effects_TickBackgroundColorTrack(arg0, temp_a1);
                break;
            case 12:
                temp = Effects_TickActionControlTrack(arg0, temp_a1);
                break;
            case 13:
                temp = Effects_TickCameraShakeTrack(arg0, temp_a1);
                break;
            }
            if (temp == 0) {
                arg0->context.timeline->activeTracks &= ~(1 << var_s1);
            }
        }
    }
    if (!(arg0->context.timeline->flags & 1)) {
        arg0->context.timeline->frame++;
    }
}

