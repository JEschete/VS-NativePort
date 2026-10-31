// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
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
void Effects_LoadEffectTimeline(Effects_EffectInstance* effect_instance, int timeline_index, int arg2)
{
    Content_PFileTimelineTable* entry = (Content_PFileTimelineTable *)(Content_PFileTimelineSection(Effects_package->block3Data, timeline_index));
    Effects_EffectTimeline* p;
    int count = 0;
    int i;

    if (effect_instance->context.timeline != NULL) {
        Core_Memory_FreeFromHighEnd(effect_instance->context.timeline);
    }

    effect_instance->context.timeline = (Effects_EffectTimeline *)(Core_Memory_AllocateFromHighEnd(sizeof(Effects_EffectTimeline) + entry->trackCount * sizeof(Effects_TimelineTrack)));
    p = effect_instance->context.timeline;
    p->trackCount = entry->trackCount;
    p->frame = 0;
    p->activeTracks = 0;

    for (i = 0; i < p->trackCount; ++i) {
        p->tracks[i].keys = (u_long *)(Content_PFileTimelineTrackData(Effects_package->block3Data, timeline_index, i));
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
    Core_Debugf("effects", "timeline=%d state=%p tracks=%d stride=%u", timeline_index, (void*)p,
        (int)p->trackCount, (unsigned int)sizeof(Effects_TimelineTrack));
}

// decomp: BATTLE.PRG func_800D5700 (src/BATTLE/BATTLE.PRG/5BF94.c:7920)
void Effects_InitializeParticleTimelineKey(Effects_TimelineTrack* timeline_track)
{
    Effects_ParticleTrackKey* base = (Effects_ParticleTrackKey*)timeline_track->keys;
    timeline_track->keyParameter = base[timeline_track->keyIndex].opaque2Bit4;
    timeline_track->keyTimer = base[timeline_track->keyIndex].duration;
}

// decomp: BATTLE.PRG func_800D5738 (src/BATTLE/BATTLE.PRG/5BF94.c:7927)
void Effects_InitializeShakeTimelineKey(Effects_TimelineTrack* timeline_track)
{
    timeline_track->keyTimer =
        Effects_SegmentedEffectCurveDuration((((Effects_ShakeTrackKey*)&timeline_track->keys[timeline_track->keyIndex * 2 + 1])->curve));
}

// decomp: BATTLE.PRG func_800D5780 (src/BATTLE/BATTLE.PRG/5BF94.c:7933)
int Effects_AdvanceEffectTimelineKey(Effects_TimelineTrack* timeline_track)
{
    if (timeline_track->keyIndex + 1 >= timeline_track->header.keyCount) {
        return 0;
    }
    timeline_track->keyIndex++;
    switch (timeline_track->header.kind) {
    case 1:
        Effects_InitializeParticleTimelineKey(timeline_track);
        break;
    case 0xD:
        Effects_InitializeShakeTimelineKey(timeline_track);
        break;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D57FC (src/BATTLE/BATTLE.PRG/5BF94.c:7950)
int Effects_TickParticleEmissionTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track)
{
    Effects_ParticleTrackKey* e = (Effects_ParticleTrackKey*)timeline_track->keys;
    int ret = 1;
    int key = e[timeline_track->keyIndex].frame;
    int cur = effect_instance->context.timeline->frame;

    if (cur < key) {
        return ret;
    }
    Effects_SampleAllEffectCurves(timeline_track->curveFrame);
    Effects_EmitParticleEvent(
        effect_instance, e[timeline_track->keyIndex].opaque2Bit4, e[timeline_track->keyIndex].opaque3Bit2, e[timeline_track->keyIndex].opaque2Bit0, NULL);
    ++timeline_track->curveFrame;
    if (!(effect_instance->context.timeline->flags & 1)) {
        --timeline_track->keyTimer;
    }
    if (timeline_track->keyTimer == 0) {
        ret = Effects_AdvanceEffectTimelineKey(timeline_track);
        timeline_track->curveFrame = 0;
    }
    return ret;
}

// decomp: BATTLE.PRG func_800D5904 (src/BATTLE/BATTLE.PRG/5BF94.c:7974)
int Effects_TickEffectSoundTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track)
{
    SVECTOR position;
    u_long* commands = timeline_track->keys;
    u_long command = commands[timeline_track->keyIndex];

    if ((command & 0x1FF) != effect_instance->context.timeline->frame) {
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
                effect_instance->transforms[(commands[timeline_track->keyIndex] >> 22) & 63].world.t[0],
                effect_instance->transforms[(commands[timeline_track->keyIndex] >> 22) & 63].world.t[1],
                effect_instance->transforms[(commands[timeline_track->keyIndex] >> 22) & 63].world.t[2]);
            Audio_PlayWorldSfx(bank, (commands[timeline_track->keyIndex] >> 9) & 0x1FF, &position);
        }
    }

    return Effects_AdvanceEffectTimelineKey(timeline_track);
}

// decomp: BATTLE.PRG func_800D5A98 (src/BATTLE/BATTLE.PRG/5BF94.c:8005)
int Effects_TickChildEffectTrack(Effects_EffectInstance* effect_instance, Effects_ChildTrackView* child_track, int child_script_offset)
{
    Effects_EffectSpawnRequest spawn;
    Effects_ChildTrackKey* event = child_track->event;
    Effects_EffectInstance* child;
    int result = 1;

    if (event->frame > effect_instance->context.timeline->frame) {
        return result;
    }

    if (child_track->index < Effects_package->request->count) {
        while (child_track->delay == 0) {
            spawn.script = Effects_package->block8Data;
            spawn.scriptOffset = child_script_offset;
            spawn.priority = 0;
            spawn.source = &Effects_package->request->source;
            spawn.emitFrom = &Effects_package->request->source;
            spawn.target = &Effects_package->request->targets[child_track->index];
            spawn.emitTo = &Effects_package->request->targets[child_track->index];
            spawn.parent = effect_instance;
            spawn.token = effect_instance->token;
            child = Effects_CreatePrimaryEffectInstance(&spawn);
            ++effect_instance->childCount;

            if (spawn.target->type == 4) {
                child->registers[1] = event->actors[spawn.target->value.actor.eventActorSlot];
            } else {
                child->registers[1] = event->actors[0];
            }

            if (++child_track->index == Effects_package->request->count) {
                break;
            }

            child_track->delay += event->delay;
        }

        if (child_track->delay != 0) {
            --child_track->delay;
        }
    } else if (child_track->index == Effects_package->request->count) {
        if (effect_instance->childCount == 0) {
            spawn.script = Effects_package->block8Data;
            spawn.scriptOffset = child_script_offset;
            spawn.priority = 0;
            spawn.source = &Effects_package->request->source;
            spawn.emitFrom = &Effects_package->request->source;
            spawn.target = &Effects_package->request->targets[0];
            spawn.emitTo = &Effects_package->request->targets[0];
            spawn.parent = effect_instance;
            spawn.token = effect_instance->token;
            child = Effects_CreatePrimaryEffectInstance(&spawn);
            ++effect_instance->childCount;
            child->registers[1] = event->final;
            ++child_track->index;
        }
    } else {
        result = 0;
    }

    return result;
}

// decomp: BATTLE.PRG func_800D5D74 (src/BATTLE/BATTLE.PRG/5BF94.c:8069)
int Effects_TickEffectRendererTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track)
{
    Effects_RendererTrackKey* renderer_key = (Effects_RendererTrackKey*)(timeline_track->keyIndex * 8 + (uintptr_t)timeline_track->keys);

    if (renderer_key->frame == effect_instance->context.timeline->frame) {
        Effects_AttachEffectRenderer(effect_instance, Effects_effectRenderers[renderer_key->effectRenderer],
            (renderer_key->opaque1Bit1 + renderer_key->opaque2Bit5 * 256) | (renderer_key->opaque4 << 0x10));
        return Effects_AdvanceEffectTimelineKey(timeline_track);
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
            (char *)(sound + *(((data[event->keyIndex] >> 9) & 31) + ((Effects_PackageBlock11*)sound)->offsets)),
            255, 1);
    }

    result = Effects_AdvanceEffectTimelineKey(event);
    return result;
}

// decomp: BATTLE.PRG func_800D5F8C (src/BATTLE/BATTLE.PRG/5BF94.c:8128)
int Effects_TickSourceActorColorTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track)
{
    Effects_ColorTrackKey* color_key = (Effects_ColorTrackKey*)timeline_track->keys + timeline_track->keyIndex;

    if (color_key->frame == effect_instance->context.timeline->frame) {
        if ((effect_instance->context.emitFrom.type == 4)
            && (Actors_ActorStateFlags(effect_instance->context.emitFrom.value.actor.actor) & 2)) {
            Render_TransformActorColors(color_key->mode, color_key->duration,
                effect_instance->context.emitFrom.value.actor.actor, color_key->red - 0x80,
                color_key->green - 0x80, color_key->blue - 0x80);
        }
        return Effects_AdvanceEffectTimelineKey(timeline_track);
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D6048 (src/BATTLE/BATTLE.PRG/5BF94.c:8144)
int Effects_TickTargetActorColorTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track, int apply_to_all_targets)
{
    int i;
    Effects_ColorTrackKey* entry = (Effects_ColorTrackKey*)timeline_track->keys + timeline_track->keyIndex;

    if (entry->frame != effect_instance->context.timeline->frame) {
        return 1;
    }

    if (apply_to_all_targets != 0) {
        Effects_ActionEffectRequest* effect = &Effects_actionRequest;

        for (i = 0; i < Effects_actionRequest.count; ++i) {
            if ((effect->targets[i].type == 4)
                && (Actors_ActorStateFlags(effect->targets[i].value.actor.actor) & 2)) {
                Render_TransformActorColors(entry->mode, entry->duration,
                    effect->targets[i].value.actor.actor, entry->red - 128,
                    entry->green - 128, entry->blue - 128);
            }
        }
    } else if ((effect_instance->context.emitTo.type == 4)
               && (Actors_ActorStateFlags(effect_instance->context.emitTo.value.actor.actor) & 2)) {
        Render_TransformActorColors(entry->mode, entry->duration, effect_instance->context.emitTo.value.actor.actor,
            entry->red - 128, entry->green - 128, entry->blue - 128);
    }
    return Effects_AdvanceEffectTimelineKey(timeline_track);
}

// decomp: BATTLE.PRG func_800D61AC (src/BATTLE/BATTLE.PRG/5BF94.c:8172)
int Effects_TickOtherActorColorTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track)
{
    int i;
    Effects_ColorTrackKey* color_key = (Effects_ColorTrackKey*)(timeline_track->keys + (timeline_track->keyIndex * 2));

    if (color_key->frame == effect_instance->context.timeline->frame) {
        for (i = 0; i < Effects_actionRequest.otherCount; ++i) {
            if (Actors_ActorStateFlags(Effects_actionRequest.others[i]) & 2) {
                Render_TransformActorColors(color_key->mode, color_key->duration, Effects_actionRequest.others[i],
                    color_key->red - 0x80, (color_key->green - 0x80), (color_key->blue - 0x80));
            }
        }
        return Effects_AdvanceEffectTimelineKey(timeline_track);
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D6298 (src/BATTLE/BATTLE.PRG/5BF94.c:8189)
int Effects_TickSceneColorTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track)
{
    Effects_ColorTrackKey* color_key = (Effects_ColorTrackKey*)timeline_track->keys + (timeline_track->keyIndex);

    if (color_key->frame == effect_instance->context.timeline->frame) {
        Render_TransformSceneColors(color_key->mode, color_key->duration, color_key->red - 0x80,
            color_key->green - 0x80, (color_key->blue - 0x80));
        return Effects_AdvanceEffectTimelineKey(timeline_track);
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D6310 (src/BATTLE/BATTLE.PRG/5BF94.c:8201)
int Effects_TickBackgroundColorTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track)
{
    Effects_ColorTrackKey* color_key = (Effects_ColorTrackKey*)(timeline_track->keys + timeline_track->keyIndex * 2);

    if (color_key->frame == effect_instance->context.timeline->frame) {
        Render_TransformBackgroundColors(color_key->mode, color_key->duration, color_key->red - 0x80,
            color_key->green - 0x80, color_key->blue - 0x80);
        return Effects_AdvanceEffectTimelineKey(timeline_track);
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D6388 (src/BATTLE/BATTLE.PRG/5BF94.c:8213)
void Effects_ApplyTargetActionImpact(Effects_EffectInstance* effect_instance)
{
    if (effect_instance->context.target.type == 4) {
        Battle_ApplyActionTargetEffects(effect_instance->context.target.value.actor.actor, NULL);
    } else {
        Battle_ApplyActionTargetEffects(
            effect_instance->context.target.value.position.pad, &effect_instance->context.target.value.position);
    }
}

// decomp: BATTLE.PRG func_800D63D0 (src/BATTLE/BATTLE.PRG/5BF94.c:8223)
void Effects_ApplySourceActionImpact(Effects_EffectInstance* effect_instance)
{
    if (effect_instance->context.source.type == 4) {
        Battle_ApplyActionSourceEffects(effect_instance->context.source.value.actor.actor, NULL);
    } else {
        Battle_ApplyActionSourceEffects(
            effect_instance->context.source.value.position.pad, &effect_instance->context.source.value.position);
    }
}

// decomp: BATTLE.PRG func_800D6418 (src/BATTLE/BATTLE.PRG/5BF94.c:8233)
void Effects_ActivateTargetRoomTile(Effects_EffectInstance* effect_instance)
{
    if (effect_instance->context.target.type == 5) {
        Field_DisableTrapAtPosition(&effect_instance->context.target.value.position);
    }
}

// decomp: BATTLE.PRG func_800D6448 (src/BATTLE/BATTLE.PRG/5BF94.c:8240)
void Effects_SpawnTransformWeaponTrail(Effects_EffectInstance* effect_instance, u_char transform_index, u_char packed_trail_style)
{
    Effects_SaveEffectResourceContext();
    Effects_SpawnBoneTrail(effect_instance->transforms[transform_index].actor, effect_instance->transforms[transform_index].bone, packed_trail_style % 10, packed_trail_style / 10);
    Effects_RestoreEffectResourceContext();
}

// decomp: BATTLE.PRG func_800D64E4 (src/BATTLE/BATTLE.PRG/5BF94.c:8247)
void Effects_FadeActionWeaponTrails(void) { Effects_FadeBoneTrails(0, 0); }

// decomp: BATTLE.PRG func_800D6508 (src/BATTLE/BATTLE.PRG/5BF94.c:8249)
void Effects_ResumeEffectTransformMotion(Effects_TransformControlView* transform_controls, char transform_index)
{
    transform_controls += transform_index;
    transform_controls->motionResumed = 1;
}

// decomp: BATTLE.PRG func_800D6538 (src/BATTLE/BATTLE.PRG/5BF94.c:8255)
void Effects_HoldEffectTimelineFrame(Effects_EffectInstance* effect_instance) { effect_instance->context.timeline->flags |= 1; }

// decomp: BATTLE.PRG func_800D6554 (src/BATTLE/BATTLE.PRG/5BF94.c:8257)
void Effects_PauseMovingRoomObjects(void) { Field_FreezeRoomGimmicksTemporarily(); }

// decomp: BATTLE.PRG func_800D6574 (src/BATTLE/BATTLE.PRG/5BF94.c:8259)
void Effects_SignalSourceActorFromTimeline(Effects_EffectInstance* effect_instance)
{
    if (effect_instance->context.source.type == 4) {
        Effects_SignalActorEffectAnimation(effect_instance->context.source.value.actor.actor);
    }
}

// decomp: BATTLE.PRG func_800D65A8 (src/BATTLE/BATTLE.PRG/5BF94.c:8266)
void Effects_InvalidateEffectTransformAttachments(Effects_TransformControlView* transform_controls, char transform_index)
{
    transform_controls += transform_index;
    transform_controls->attachmentMode = 0;
}

// decomp: BATTLE.PRG func_800D65D8 (src/BATTLE/BATTLE.PRG/5BF94.c:8272)
void Effects_SetSourceWeaponBrightness(Effects_EffectInstance* effect_instance, u_char brightness_override)
{

    if (effect_instance->context.emitFrom.type == 4) {
        if (brightness_override != 0) {
            Actors_SetWeaponBrightnessOverride(effect_instance->context.emitFrom.value.actor.actor, 1, brightness_override);

        } else {
            Actors_SetWeaponBrightnessOverride(effect_instance->context.emitFrom.value.actor.actor, 0, 0);
        }
    }
}

// decomp: BATTLE.PRG func_800D6628 (src/BATTLE/BATTLE.PRG/5BF94.c:8285)
void Effects_PlaySourceActorReactionSound(Effects_EffectInstance* effect_instance, u_char reaction_sound_variant)
{
    int var;

    if (effect_instance->context.emitFrom.type == 4) {
        if (reaction_sound_variant < 4) {
            Audio_PlayActorModelSoundVariant(effect_instance->context.emitFrom.value.actor.actor, reaction_sound_variant);
        } else {
            do {
                var = Core_Random_Rand() % 3;
            } while (var == Effects_lastReactionSoundVariant);
            Audio_PlayActorModelSoundVariant(effect_instance->context.emitFrom.value.actor.actor, Effects_lastReactionSoundVariant = var);
        }
    }
}

// decomp: BATTLE.PRG func_800D66CC (src/BATTLE/BATTLE.PRG/5BF94.c:8301)
void Effects_ActivateTargetGimmick(Effects_EffectInstance* effect_instance)
{
    if (effect_instance->context.target.type == 5) {
        Field_ApplyUnlockEffectToRoomObject(&effect_instance->context.target.value.position);
    }
}

// decomp: BATTLE.PRG func_800D6860 (src/BATTLE/BATTLE.PRG/5BF94.c:8335)
void Effects_PrepareAshleyTargetImpact(Effects_EffectInstance* effect_instance)
{
    if (effect_instance->context.emitTo.type == 4) {
        Battle_ArmDefenseAbilityTimingWindow(effect_instance->context.emitTo.value.actor.actor);
    }
}

// decomp: BATTLE.PRG func_800D6894 (src/BATTLE/BATTLE.PRG/5BF94.c:8342)
int Effects_TickActionControlTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track)
{
    Effects_ActionControlKey* control_key = (Effects_ActionControlKey*)(timeline_track->keys - (-timeline_track->keyIndex));

    if ((control_key->frame & 0x1FF) == effect_instance->context.timeline->frame) {
        u_char control_operand0 = control_key->opaque2;
        u_char control_operand1 = control_key->opaque3;

        switch (control_key->frame >> 9) {
        case 0:
            Effects_ApplyTargetActionImpact(effect_instance);
            break;

        case 1:
            Effects_ApplySourceActionImpact(effect_instance);
            break;

        case 2:
            Effects_ActivateTargetRoomTile(effect_instance);
            break;

        case 3:
            Effects_SpawnTransformWeaponTrail(effect_instance, control_operand0, control_operand1);
            break;

        case 4:
            Effects_FadeActionWeaponTrails();
            break;

        case 5:
            Effects_ResumeEffectTransformMotion((Effects_TransformControlView*)effect_instance, control_operand0);
            break;

        case 6:
            Effects_HoldEffectTimelineFrame(effect_instance);
            break;

        case 7:
            Effects_PauseMovingRoomObjects();
            break;

        case 8:
            Effects_SignalSourceActorFromTimeline(effect_instance);
            break;

        case 9:
            Effects_InvalidateEffectTransformAttachments((Effects_TransformControlView*)effect_instance, control_operand0);
            break;

        case 10:
            Effects_SetSourceWeaponBrightness(effect_instance, control_operand0);
            break;

        case 11:
            Effects_PlaySourceActorReactionSound(effect_instance, control_operand0);
            break;

        case 12:
            Effects_ActivateTargetGimmick(effect_instance);
            break;

        case 13:
            Effects_FocusCameraOnEffectTransform(effect_instance, control_operand0, control_operand1);
            break;

        case 14:
            Effects_RestoreActionEffectCamera(effect_instance, control_operand0);
            break;

        case 15:
            Effects_PrepareAshleyTargetImpact(effect_instance);
            break;
        }

        return Effects_AdvanceEffectTimelineKey(timeline_track);
    }

    return 1;
}

// decomp: BATTLE.PRG func_800D6A18 (src/BATTLE/BATTLE.PRG/5BF94.c:8422)
int Effects_TickCameraShakeTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track)
{
    Effects_ShakeChannelKey* e = (Effects_ShakeChannelKey*)timeline_track->keys;
    int ret = 1;
    int key = e[timeline_track->keyIndex].frame;
    int cur = effect_instance->context.timeline->frame;

    if (cur < key) {
        return ret;
    }
    if (key == cur) {
        Effects_StartCameraShakeChannels(e->opaque4Bit0, e->opaque4Bit4, e->opaque5Bit1, e->opaque6Bit0, e->opaque6Bit1, e->opaque7Bit0,
            e->opaque7Bit1);
    }
    if (--timeline_track->keyTimer == 0) {
        ret = Effects_AdvanceEffectTimelineKey(timeline_track);
    }
    return ret;
}

// decomp: BATTLE.PRG func_800D6AEC (src/BATTLE/BATTLE.PRG/5BF94.c:8442)
void Effects_TickEffectTimeline(Effects_EffectInstance* effect_instance, int child_script_offset)
{
    Effects_EffectTimeline* temp_a0;
    Effects_TimelineTrack* unused_track_declaration;
    int track_index;

    for (track_index = 0; track_index < effect_instance->context.timeline->trackCount; track_index++) {
        if ((effect_instance->context.timeline->activeTracks >> track_index) & 1) {
            int temp;
            Effects_TimelineTrack* timeline_track =
                &effect_instance->context.timeline->tracks[track_index];
            switch (timeline_track->header.kind) {
            case 1:
                temp = Effects_TickParticleEmissionTrack(effect_instance, timeline_track);
                break;
            case 2:
                temp = Effects_TickEffectSoundTrack(effect_instance, timeline_track);
                break;
            case 3:
                temp = Effects_TickChildEffectTrack(effect_instance, (Effects_ChildTrackView*)timeline_track, child_script_offset);
                break;
            case 4:
                temp = Effects_TickEffectRendererTrack(effect_instance, timeline_track);
                break;
            case 5:
                temp = Effects_TickControllerVibrationTrack(effect_instance, timeline_track);
                break;
            case 6:
                temp = Effects_TickSourceActorColorTrack(effect_instance, timeline_track);
                break;
            case 7:
                temp = Effects_TickTargetActorColorTrack(effect_instance, timeline_track, effect_instance->context.timeline->flags & 2);
                break;
            case 8:
                temp = Effects_TickOtherActorColorTrack(effect_instance, timeline_track);
                break;
            case 9:
                temp = Effects_TickSceneColorTrack(effect_instance, timeline_track);
                break;
            case 10:
                temp = Effects_TickBackgroundColorTrack(effect_instance, timeline_track);
                break;
            case 12:
                temp = Effects_TickActionControlTrack(effect_instance, timeline_track);
                break;
            case 13:
                temp = Effects_TickCameraShakeTrack(effect_instance, timeline_track);
                break;
            }
            if (temp == 0) {
                effect_instance->context.timeline->activeTracks &= ~(1 << track_index);
            }
        }
    }
    if (!(effect_instance->context.timeline->flags & 1)) {
        effect_instance->context.timeline->frame++;
    }
}

}  // extern "C"
