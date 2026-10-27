// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Audio/ActorSound.h"
#include "Audio/Sfx.h"
#include "Core/Random.h"
#include "Effects/Weather.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Render/Palette.h"

// decomp: D_800E91A4 (D_800E91A4@BATTLE/BATTLE.PRG)
u_char Audio_objectSurfaceSoundIds[2956]; /* contents: module image (ModuleData.c) */

// decomp: BATTLE.PRG func_8006CD60 (src/BATTLE/BATTLE.PRG/146C.c:2144)
void Audio_PlayRandomActorImpactSound(u_int* sound_bank, int pan, int volume)
{
    Audio_PlayBankSfx(0x7E, sound_bank, Core_Random_NextBounded(3) + 8, pan, volume);
}

// decomp: BATTLE.PRG func_800AD62C (src/BATTLE/BATTLE.PRG/40564.c:2789)
void Audio_ActorSoundPanAndVolume(int actor_index, int* output_pan, int* output_volume)
{
    int packed_pan_volume;
    Actors_ActorRecord* actor = Actors_actorRecords[actor_index];

    if (actor->soundEffect.kind == 2) {
        *output_pan = 0x80;
        if (output_volume != NULL) {
            *output_volume = 0;
        }
    } else {
        if (actor->instance.actionAnimationState || actor->instance.isPreviewModel || !actor->instance.soundEnabled
            || actor->instance.movementMode || actor->instance.locomotionState || actor->instance.motionLocked) {
            ((void (*)(int, int, SVECTOR*, int))Actors_ProjectActorTaggedBone)(actor_index, 0xFF, &actor->soundPosition, 0);
        }

        packed_pan_volume = Audio_SfxPanAndVolume(*(int*)&actor->soundPosition, actor->soundPosition.vz);
        *output_pan = packed_pan_volume >> 0x10;

        if (actor->soundEffect.level != 0) {
            *output_pan = actor->soundEffect.level;
        }

        if (output_volume != NULL) {
            *output_volume = packed_pan_volume & 0xFFFF;
            if (actor->soundEffect.flags != 0) {
                *output_volume = actor->soundEffect.flags;
            }
        }
    }
}

// decomp: BATTLE.PRG func_800AE4FC (src/BATTLE/BATTLE.PRG/44F14.c:637)
void Audio_PlayActorModelSound(Actors_ModelInstance* model_instance, int sound_index)
{
    Actors_ActorRecord* actor;
    int temp;
    int sp[2];
    int type = sound_index;

    if (model_instance->position.vy >= 0xBB8) {
        return;
    }

    actor = Actors_actorRecords[model_instance->slot];

    if (actor->soundEffect.kind == 2) {
        sp[0] = 0x80;
        sp[1] = 0;
    } else {
        if ((*((int*)&actor->instance.quadCount + 1) & 0x3F9F00) != 0x8000) {
            Actors_ProjectActorTaggedBone(model_instance->slot, 0xFF, &actor->soundPosition, 0);
        }

        temp = Audio_SfxPanAndVolume(*(int*)&actor->soundPosition, actor->soundPosition.vz);
        sp[0] = temp >> 0x10;

        if (actor->soundEffect.level != 0) {
            sp[0] = actor->soundEffect.level;
        }

        sp[1] = temp & 0xFFFF;

        if (actor->soundEffect.flags != 0) {
            sp[1] = actor->soundEffect.flags;
        }
    }

    if (sp[1] == 0) {
        return;
    }

    if (type == 8) {
        Audio_PlayRandomActorImpactSound(VS_ONDISC(model_instance->model, soundSection, void *), sp[0], sp[1]);
    } else if (type == 4) {
        Audio_PlayBankSfx(0x7E, VS_ONDISC(model_instance->model, soundSection, void *), Core_Random_Rand() % 4 + 4, sp[0], sp[1]);
    } else {
        Audio_PlayBankSfx(0x180, VS_ONDISC(model_instance->model, soundSection, void *), type, sp[0], sp[1]);
    }
}

// decomp: BATTLE.PRG func_800AE68C (src/BATTLE/BATTLE.PRG/44F14.c:685)
void Audio_PlayActorModelSoundVariant(int actor_index, int variant_index)
{
    Audio_PlayActorModelSound(&Actors_actorRecords[actor_index]->instance, variant_index + 4);
}

// decomp: BATTLE.PRG func_800AE6C0 (src/BATTLE/BATTLE.PRG/44F14.c:690)
void Audio_PlayActorSoundPair(Actors_ActorRecord* source_actor, int first_effect_id, int second_effect_id)
{
    int pan_volume[2];
    Actors_ActorRecord* actor = Actors_actorRecords[source_actor->instance.slot];

    if (actor->soundEffect.kind == 2) {
        pan_volume[0] = 0x80;
        pan_volume[1] = 0;
    } else {
        int packed_pan_volume;

        if (actor->instance.actionAnimationState || actor->instance.isPreviewModel || !actor->instance.soundEnabled
            || actor->instance.movementMode || actor->instance.locomotionState || actor->instance.motionLocked) {
            Actors_ProjectActorTaggedBone(source_actor->instance.slot, 0xFF, &actor->soundPosition, 0);
        }

        packed_pan_volume = Audio_SfxPanAndVolume(*(int*)&actor->soundPosition, actor->soundPosition.vz);
        pan_volume[0] = packed_pan_volume >> 0x10;

        if (actor->soundEffect.level != 0) {
            pan_volume[0] = actor->soundEffect.level;
        }

        pan_volume[1] = packed_pan_volume & 0xFFFF;

        if (actor->soundEffect.flags != 0) {
            pan_volume[1] = actor->soundEffect.flags;
        }
    }

    if (pan_volume[1] != 0) {
        Audio_PlaySfx(0x7E, first_effect_id, pan_volume[0], pan_volume[1]);
        if (second_effect_id != 0) {
            Audio_PlaySfx(0x7E, second_effect_id, pan_volume[0], pan_volume[1]);
        }
    }
}

// decomp: BATTLE.PRG func_800AE7D8 (src/BATTLE/BATTLE.PRG/44F14.c:728)
void Audio_PlayGimmickSoundPair(void* gimmick, int first_effect_id, int second_effect_id)
{
    SVECTOR* sound_position;

    sound_position = gimmick + 0x1C;
    Audio_PlayWorldSfx(0x7E, first_effect_id, sound_position);
    if (second_effect_id != 0) {
        Audio_PlayWorldSfx(0x7E, second_effect_id, sound_position);
    }
}

// decomp: BATTLE.PRG func_800AE828 (src/BATTLE/BATTLE.PRG/44F14.c:739)
void Audio_PlayActorSurfaceContactSound(int actor_index, Actors_ActorRecord* actor, int contact_sound_code)
{
    int weather_kind;
    int weather_intensity;
    int id;
    int floor;
    Field_MpdTileCell* room;

    if ((u_char)(actor->animationId - 0xCA) < 18) {
        return;
    }
    if (actor_index == 1) {
        return;
    }
    if (*((int*)&actor->sequenceFlags - 2) & 0xF0000000) {
        id = Audio_objectSurfaceSoundIds[Field_gimmickInstances[Field_FindTopGimmickAtTile(actor->instance.currentTileX,
                                       actor->instance.currentTileZ, 1)]
                ->objectInfo.objectId];
    } else {
        floor = actor->instance.currentTileLayer;
        id = 0xD;
        if (actor->contactKind != 0) {
            floor = actor->contactKind;
        }
        room = Field_CollisionCellAt(actor->instance.currentTileX, actor->instance.currentTileZ, floor);
        if (room != NULL) {
            id = Field_kTileSurfaceIds[room->surfaceKind];
        }
    }
    if (contact_sound_code == 0x41) {
        id += 0x10;
    } else if (contact_sound_code == 0x42) {
        id += 0x18;
    } else {
        Effects_RoomWeatherState(&weather_kind, &weather_intensity);
        if (weather_intensity >= 6 && weather_kind > 0 && weather_kind < 3) {
            id += 8;
        }
    }
    Audio_PlayActorModelSound(&actor->instance, id);
}

