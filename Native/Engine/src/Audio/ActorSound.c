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
void Audio_PlayRandomActorImpactSound(u_int* arg0, int arg1, int arg2)
{
    Audio_PlayBankSfx(0x7E, arg0, Core_Random_NextBounded(3) + 8, arg1, arg2);
}

// decomp: BATTLE.PRG func_800AD62C (src/BATTLE/BATTLE.PRG/40564.c:2789)
void Audio_ActorSoundPanAndVolume(int arg0, int* arg1, int* arg2)
{
    int temp_v0;
    Actors_ActorRecord* temp_s0 = Actors_actorRecords[arg0];

    if (temp_s0->soundEffect.kind == 2) {
        *arg1 = 0x80;
        if (arg2 != NULL) {
            *arg2 = 0;
        }
    } else {
        if (temp_s0->instance.actionAnimationState || temp_s0->instance.isPreviewModel || !temp_s0->instance.soundEnabled
            || temp_s0->instance.movementMode || temp_s0->instance.locomotionState || temp_s0->instance.motionLocked) {
            ((void (*)(int, int, SVECTOR*, int))Actors_ProjectActorTaggedBone)(arg0, 0xFF, &temp_s0->soundPosition, 0);
        }

        temp_v0 = Audio_SfxPanAndVolume(*(int*)&temp_s0->soundPosition, temp_s0->soundPosition.vz);
        *arg1 = temp_v0 >> 0x10;

        if (temp_s0->soundEffect.level != 0) {
            *arg1 = temp_s0->soundEffect.level;
        }

        if (arg2 != NULL) {
            *arg2 = temp_v0 & 0xFFFF;
            if (temp_s0->soundEffect.flags != 0) {
                *arg2 = temp_s0->soundEffect.flags;
            }
        }
    }
}

// decomp: BATTLE.PRG func_800AEEC4 (src/BATTLE/BATTLE.PRG/44F14.c:110) [C: not yet ported]
int Audio_UpdateActorSoundTransition()
{
    VS_UNPORTED("Audio::UpdateActorSoundTransition");
    return 0;
}

// decomp: BATTLE.PRG func_800AE4FC (src/BATTLE/BATTLE.PRG/44F14.c:637)
void Audio_PlayActorModelSound(Actors_ModelInstance* arg0, int arg1)
{
    Actors_ActorRecord* actor;
    int temp;
    int sp[2];
    int type = arg1;

    if (arg0->position.vy >= 0xBB8) {
        return;
    }

    actor = Actors_actorRecords[arg0->slot];

    if (actor->soundEffect.kind == 2) {
        sp[0] = 0x80;
        sp[1] = 0;
    } else {
        if ((*((int*)&actor->instance.quadCount + 1) & 0x3F9F00) != 0x8000) {
            Actors_ProjectActorTaggedBone(arg0->slot, 0xFF, &actor->soundPosition, 0);
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
        Audio_PlayRandomActorImpactSound(arg0->model->soundSection, sp[0], sp[1]);
    } else if (type == 4) {
        Audio_PlayBankSfx(0x7E, arg0->model->soundSection, Core_Random_Rand() % 4 + 4, sp[0], sp[1]);
    } else {
        Audio_PlayBankSfx(0x180, arg0->model->soundSection, type, sp[0], sp[1]);
    }
}

// decomp: BATTLE.PRG func_800AE68C (src/BATTLE/BATTLE.PRG/44F14.c:685)
void Audio_PlayActorModelSoundVariant(int arg0, int arg1)
{
    Audio_PlayActorModelSound(&Actors_actorRecords[arg0]->instance, arg1 + 4);
}

// decomp: BATTLE.PRG func_800AE6C0 (src/BATTLE/BATTLE.PRG/44F14.c:690)
void Audio_PlayActorSoundPair(Actors_ActorRecord* arg0, int arg1, int arg2)
{
    int sp10[2];
    Actors_ActorRecord* temp_s0 = Actors_actorRecords[arg0->instance.slot];

    if (temp_s0->soundEffect.kind == 2) {
        sp10[0] = 0x80;
        sp10[1] = 0;
    } else {
        int temp_v0;

        if (temp_s0->instance.actionAnimationState || temp_s0->instance.isPreviewModel || !temp_s0->instance.soundEnabled
            || temp_s0->instance.movementMode || temp_s0->instance.locomotionState || temp_s0->instance.motionLocked) {
            Actors_ProjectActorTaggedBone(arg0->instance.slot, 0xFF, &temp_s0->soundPosition, 0);
        }

        temp_v0 = Audio_SfxPanAndVolume(*(int*)&temp_s0->soundPosition, temp_s0->soundPosition.vz);
        sp10[0] = temp_v0 >> 0x10;

        if (temp_s0->soundEffect.level != 0) {
            sp10[0] = temp_s0->soundEffect.level;
        }

        sp10[1] = temp_v0 & 0xFFFF;

        if (temp_s0->soundEffect.flags != 0) {
            sp10[1] = temp_s0->soundEffect.flags;
        }
    }

    if (sp10[1] != 0) {
        Audio_PlaySfx(0x7E, arg1, sp10[0], sp10[1]);
        if (arg2 != 0) {
            Audio_PlaySfx(0x7E, arg2, sp10[0], sp10[1]);
        }
    }
}

// decomp: BATTLE.PRG func_800AE7D8 (src/BATTLE/BATTLE.PRG/44F14.c:728)
void Audio_PlayGimmickSoundPair(void* arg0, int arg1, int arg2)
{
    SVECTOR* temp_s0;

    temp_s0 = arg0 + 0x1C;
    Audio_PlayWorldSfx(0x7E, arg1, temp_s0);
    if (arg2 != 0) {
        Audio_PlayWorldSfx(0x7E, arg2, temp_s0);
    }
}

// decomp: BATTLE.PRG func_800AE828 (src/BATTLE/BATTLE.PRG/44F14.c:739)
void Audio_PlayActorSurfaceContactSound(int arg0, Actors_ActorRecord* arg1, int arg2)
{
    int sp10;
    int sp14;
    int id;
    int floor;
    Field_MpdTileCell* room;

    if ((u_char)(arg1->animationId - 0xCA) < 18) {
        return;
    }
    if (arg0 == 1) {
        return;
    }
    if (*((int*)&arg1->sequenceFlags - 2) & 0xF0000000) {
        id = Audio_objectSurfaceSoundIds[Field_gimmickInstances[Field_FindTopGimmickAtTile(arg1->instance.currentTileX,
                                       arg1->instance.currentTileZ, 1)]
                ->objectInfo.objectId];
    } else {
        floor = arg1->instance.currentTileLayer;
        id = 0xD;
        if (arg1->contactKind != 0) {
            floor = arg1->contactKind;
        }
        room = Field_CollisionCellAt(arg1->instance.currentTileX, arg1->instance.currentTileZ, floor);
        if (room != NULL) {
            id = Field_kTileSurfaceIds[room->surfaceKind];
        }
    }
    if (arg2 == 0x41) {
        id += 0x10;
    } else if (arg2 == 0x42) {
        id += 0x18;
    } else {
        Effects_RoomWeatherState(&sp10, &sp14);
        if (sp14 >= 6 && sp10 > 0 && sp10 < 3) {
            id += 8;
        }
    }
    Audio_PlayActorModelSound(&arg1->instance, id);
}

