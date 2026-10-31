// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Core/Memory/Heap.h"
#include "Field/Collision.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomEffects.h"
#include "Field/Zone.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "State/StateFlags.h"

// decomp: D_800E85CC (D_800E85CC@BATTLE/BATTLE.PRG)
int (*Field_kTextureEffectHandlers[7])(Field_SwingEffectRecord*); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800E85E8 (D_800E85E8@BATTLE/BATTLE.PRG)
int (*Field_kRoomEffectOpcodeHandlers[13])(Field_RoomEffectObjectView*, Field_RoomEffectCommandHeader*); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800F1D00 (D_800F1D00@BATTLE/BATTLE.PRG)
short Field_isRoomEffectSoundRestored;

// decomp: D_800F1DD8 (D_800F1DD8@BATTLE/BATTLE.PRG)
Field_RoomEffectInstance Field_roomEffectInstances[100];

// decomp: D_800F2258 (D_800F2258@BATTLE/BATTLE.PRG)
short Field_roomEffectSoundId;

// decomp: D_800F225A (D_800F225A@BATTLE/BATTLE.PRG)
char Field_textureEffectCount;

// decomp: D_800F225B (D_800F225B@BATTLE/BATTLE.PRG)
char Field_roomEffectScriptCount;

// decomp: BATTLE.PRG func_8009195C (src/BATTLE/BATTLE.PRG/2842C.c:550)
void* Field_FindRoomGeometryGroup(int group_index)
{
    if (Field_roomData.geometry != NULL) {
        int group_count = *Field_roomData.geometry;
        int* groups = Field_roomData.geometry + 1;
        if ((group_index >= 0) && (group_index < group_count)) {
            return groups + (group_index * 16);
        }
    }
    return NULL;
}

// decomp: BATTLE.PRG func_80091998 (src/BATTLE/BATTLE.PRG/2842C.c:562)
int Field_ShowRoomGeometryGroup(int group_index)
{
    int* group = (int *)(Field_FindRoomGeometryGroup(group_index));
    if (group != NULL) {
        *group &= ~0x100;
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800919D8 (src/BATTLE/BATTLE.PRG/2842C.c:572)
int Field_HideRoomGeometryGroup(int group_index)
{
    int* group = (int *)(Field_FindRoomGeometryGroup(group_index));
    if (group != NULL) {
        *group |= 0x100;
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_80091A1C (src/BATTLE/BATTLE.PRG/2842C.c:582)
int Field_SwapRoomGeometryGroupFlags(int first_group_index, int second_group_index)
{
    int* first_group = (int *)(Field_FindRoomGeometryGroup(first_group_index));
    int* second_group = (int *)(Field_FindRoomGeometryGroup(second_group_index));

    if ((first_group != NULL) && (second_group != NULL)) {
        int saved_flags = *first_group;
        *first_group = *second_group;
        *second_group = saved_flags;
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_80091A78 (src/BATTLE/BATTLE.PRG/2842C.c:596)
int Field_AddRoomGeometryDepthBias(int group_index, int depth_bias)
{
    u_short* group_halfwords = (u_short *)(Field_FindRoomGeometryGroup(group_index));
    if (group_halfwords != NULL) {
        group_halfwords[1] = (u_short)(group_halfwords[1] + depth_bias);
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_80091AC0 (src/BATTLE/BATTLE.PRG/2842C.c:606)
int Field_DisableRoomGeometryGroup(int group_index)
{
    int* group = (int *)(Field_FindRoomGeometryGroup(group_index));
    if (group != NULL) {
        *group |= 0x1000;
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_80091B04 (src/BATTLE/BATTLE.PRG/2842C.c:616)
void Field_UpdateRoomEffects(void)
{
    int i;

    for (i = 0; i < Field_textureEffectCount; ++i) {
        if ((Field_roomEffectInstances[i].isActive != 0) && (Field_roomEffectInstances[i].kindOrObjectCount < 7)
            && (Field_roomEffectInstances[i].isUpdateEnabled != 0)) {
            Field_roomEffectInstances[i].isActive = Field_kTextureEffectHandlers[Field_roomEffectInstances[i].kindOrObjectCount](Field_roomEffectInstances[i].currentData);
        }
    }

    for (i = 0; i < Field_roomEffectScriptCount; ++i) {
        if ((Field_roomEffectInstances[i + 64].isActive != 0) && (Field_roomEffectInstances[i + 64].isUpdateEnabled != 0)) {
            Field_roomEffectInstances[i + 64].isActive =
                Field_ExecuteRoomEffectCommands((Field_RoomEffectObjectView*)Field_roomEffectInstances[i + 64].currentData,
                    Field_roomEffectInstances[i + 64].kindOrObjectCount, &Field_roomEffectInstances[i + 64]);
            if (Field_roomEffectInstances[i + 64].isActive == 0) {
                Core_Memory_FreeFromLowEnd(Field_roomEffectInstances[i + 64].currentData);
                Field_roomEffectInstances[i + 64].currentData = Field_roomEffectInstances[i + 64].originalData;
            }
        }
    }
}

// decomp: BATTLE.PRG func_80091C6C (src/BATTLE/BATTLE.PRG/2842C.c:640)
int Field_AdvanceRoomTextureFrames(Field_TextureFrameEffect* texture_frame_effect)
{
    Field_TextureFrameEffect* quad = texture_frame_effect;
    void* base = Field_roomData.geometry;

    if (++texture_frame_effect->tick > texture_frame_effect->delay) {
        int u;
        int v;
        Field_MpdPolygonUvView* polygon;

        texture_frame_effect->tick = 0;

        if (++texture_frame_effect->frame >= texture_frame_effect->frameCount) {
            texture_frame_effect->frame = 0;
        }

        u = texture_frame_effect->baseU + texture_frame_effect->frameWidth * (texture_frame_effect->frame % texture_frame_effect->columns);
        v = texture_frame_effect->baseV + texture_frame_effect->frameHeight * (texture_frame_effect->frame / texture_frame_effect->columns);
        polygon = (Field_MpdPolygonUvView *)(((u_char*)(base)) + texture_frame_effect->geometryOffset);

        if ((polygon->type & 0x3C) == 0x34) {
            polygon->u0 = texture_frame_effect->u[0] + u;
            polygon->u1 = texture_frame_effect->u[1] + u;
            polygon->u2 = texture_frame_effect->u[2] + u;
            polygon->v0 = texture_frame_effect->v[0] + v;
            polygon->v1 = texture_frame_effect->v[1] + v;
            polygon->v2 = texture_frame_effect->v[2] + v;
        } else {
            polygon->u0 = quad->u[0] + u;
            polygon->u1 = quad->u[1] + u;
            polygon->u2 = quad->u[2] + u;
            polygon->u3 = quad->u[3] + u;
            polygon->v0 = quad->v[0] + v;
            polygon->v1 = quad->v[1] + v;
            polygon->v2 = quad->v[2] + v;
            polygon->v3 = quad->v[3] + v;
        }
    }

    return 1;
}

// decomp: BATTLE.PRG func_80091E10 (src/BATTLE/BATTLE.PRG/2842C.c:682)
int Field_ScrollRoomTextureCoordinates(Field_TextureScrollEffect* texture_scroll_effect)
{
    Field_TextureScrollEffect* quad = texture_scroll_effect;
    void* base = Field_roomData.geometry;

    if (++texture_scroll_effect->tick > texture_scroll_effect->delay) {
        int u = texture_scroll_effect->currentU;
        int v = texture_scroll_effect->currentV;
        Field_MpdPolygonUvView* polygon;

        u += texture_scroll_effect->stepU;
        v += texture_scroll_effect->stepV;
        texture_scroll_effect->tick = 0;

        if (u < texture_scroll_effect->baseU) {
            u = texture_scroll_effect->baseU + texture_scroll_effect->width - texture_scroll_effect->windowWidth;
        }

        if (v < texture_scroll_effect->baseV) {
            v = texture_scroll_effect->baseV + texture_scroll_effect->height - texture_scroll_effect->windowHeight;
        }

        if (texture_scroll_effect->baseU + texture_scroll_effect->width < u + texture_scroll_effect->windowWidth) {
            u = texture_scroll_effect->baseU;
        }

        if (texture_scroll_effect->baseV + texture_scroll_effect->height < v + texture_scroll_effect->windowHeight) {
            v = texture_scroll_effect->baseV;
        }

        texture_scroll_effect->currentU = u;
        texture_scroll_effect->currentV = v;
        polygon = (Field_MpdPolygonUvView *)(((u_char*)(base)) + texture_scroll_effect->geometryOffset);

        if ((polygon->type & 0x3C) == 0x34) {
            polygon->u0 = texture_scroll_effect->u[0] + u;
            polygon->u1 = texture_scroll_effect->u[1] + u;
            polygon->u2 = texture_scroll_effect->u[2] + u;
            polygon->v0 = texture_scroll_effect->v[0] + v;
            polygon->v1 = texture_scroll_effect->v[1] + v;
            polygon->v2 = texture_scroll_effect->v[2] + v;
        } else {
            polygon->u0 = quad->u[0] + u;
            polygon->u1 = quad->u[1] + u;
            polygon->u2 = quad->u[2] + u;
            polygon->u3 = quad->u[3] + u;
            polygon->v0 = quad->v[0] + v;
            polygon->v1 = quad->v[1] + v;
            polygon->v2 = quad->v[2] + v;
            polygon->v3 = quad->v[3] + v;
        }
    }

    return 1;
}

// decomp: BATTLE.PRG func_80091FE8 (src/BATTLE/BATTLE.PRG/2842C.c:738)
int Field_AnimateRoomTextureImages(Field_TextureRegionScrollEffect* texture_region_effect)
{
    RECT source;
    RECT halfSource;

    if (++texture_region_effect->tick > texture_region_effect->delay) {
        int destX;
        int destY;
        int halfDestX;
        int halfSourceX;

        texture_region_effect->rect.x -= 320;
        texture_region_effect->rect.y += 256;
        texture_region_effect->tick = 0;

        if (texture_region_effect->scrollStep == 0) {
            if (++texture_region_effect->frame > texture_region_effect->frameCount) {
                texture_region_effect->frame = 1;
            }

            setRECT(&source, texture_region_effect->rect.x + texture_region_effect->rect.w * (texture_region_effect->frame % texture_region_effect->columns),
                texture_region_effect->rect.y + texture_region_effect->rect.h * (texture_region_effect->frame / texture_region_effect->columns), texture_region_effect->rect.w,
                texture_region_effect->rect.h);
        } else {
            texture_region_effect->frame += texture_region_effect->scrollStep;

            if (texture_region_effect->frame >= texture_region_effect->rect.h) {
                texture_region_effect->frame = 0;
            }

            setRECT(&source, texture_region_effect->rect.x + texture_region_effect->rect.w / 2, texture_region_effect->rect.y + texture_region_effect->frame,
                texture_region_effect->rect.w / 2, texture_region_effect->rect.h - texture_region_effect->frame);
        }

        halfDestX = (texture_region_effect->rect.x % 64) / 2 - 64;
        destX = halfDestX + (texture_region_effect->rect.x & 0x3C0);
        destY = (texture_region_effect->rect.y % 256) / 2 + (texture_region_effect->rect.y & 0x100);
        halfSourceX = (source.x % 64) / 2 - 64;
        setRECT(&halfSource, halfSourceX + (source.x & 0x3C0),
            (source.y % 256) / 2 + (source.y & 0x100), source.w / 2, source.h / 2);
        MoveImage(&source, texture_region_effect->rect.x, texture_region_effect->rect.y);
        MoveImage(&halfSource, destX, destY);

        if (texture_region_effect->scrollStep != 0 && texture_region_effect->frame != 0) {
            int wrapY;

            source.y = texture_region_effect->rect.y;
            source.h = texture_region_effect->frame;
            wrapY = texture_region_effect->rect.y + texture_region_effect->rect.h - texture_region_effect->frame;
            destY = (wrapY % 256) / 2 + (wrapY & 0x100);
            halfSource.y = (source.y % 256) / 2 + (source.y & 0x100);
            halfSource.h = source.h / 2;
            MoveImage(&source, texture_region_effect->rect.x, wrapY);
            MoveImage(&halfSource, destX, destY);
        }

        texture_region_effect->rect.x += 320;
        texture_region_effect->rect.y -= 256;
    }

    return 1;
}

// decomp: BATTLE.PRG func_800923F8 (src/BATTLE/BATTLE.PRG/2842C.c:801)
int Field_UpdateRoomRotationEffect(Field_SwingEffectRecord* swing_effect)
{
    int* base = Field_roomData.geometry;
    int* p;
    int delta;
    int ret = 1;

    if (swing_effect->needsReset != 0) {
        swing_effect->needsReset = 0;
        swing_effect->angle = swing_effect->startAngle;
        swing_effect->angularVelocity = swing_effect->initialVelocity;
    }

    swing_effect->angle += swing_effect->angularVelocity;
    swing_effect->angularVelocity += swing_effect->acceleration * 8;
    delta = (swing_effect->angle - swing_effect->restAngle) & 0xFFF;

    if (delta > ONE / 2) {
        delta = ONE - delta;
    }

    if (delta < ABS(swing_effect->angularVelocity)) {
        swing_effect->angularVelocity = -(swing_effect->angularVelocity >> swing_effect->dampingShift);

        if (ABS(swing_effect->angularVelocity) < 0x21) {
            swing_effect->angle = swing_effect->restAngle;
            ret = 0;
        }
    }

    p = (int *)(((u_char*)((void*)base)) + swing_effect->geometryOffset);

    if (swing_effect->axis == 1) {
        p[5] = swing_effect->angle;
        p[0] |= 0x6000;
    } else if (swing_effect->axis == 2) {
        p[4] = swing_effect->angle;
    } else {
        p[6] = swing_effect->angle;
    }

    if (ret != 0) {
        return 1;
    }

    delta = swing_effect->startAngle;
    swing_effect->initialVelocity = -swing_effect->initialVelocity;
    swing_effect->angularVelocity = swing_effect->initialVelocity;
    swing_effect->startAngle = swing_effect->restAngle;
    swing_effect->restAngle = delta;
    swing_effect->acceleration = -swing_effect->acceleration;
    return 0;
}

// decomp: BATTLE.PRG func_80092540 (src/BATTLE/BATTLE.PRG/2842C.c:855)
int Field_SkipRoomEffectCommand(void) { return 0; }

// decomp: BATTLE.PRG func_80092548 (src/BATTLE/BATTLE.PRG/2842C.c:857)
void Field_InitializeRoomEffects(void)
{
    int sizes[] = { 24, 20, 32, 0, 0, 0, 36 };
    void* data;
    int count;
    int length;
    int index;
    int consumed;

    Field_roomEffectSoundId = -1;
    Field_isRoomEffectSoundRestored = 0;
    Field_textureEffectCount = 0;

    if ((int)Field_roomData.header.textureEffectsLength > 0) {
        data = Field_roomData.textureEffects;
        length = Field_roomData.header.textureEffectsLength;
        consumed = 0;
        count = 0;

        while (consumed < length) {
            Field_SwingEffectRecord* effect = (Field_SwingEffectRecord *)(data);

            if (effect->kind < 7 && sizes[effect->kind] != 0) {
                Field_roomEffectInstances[count].currentData = effect;
                Field_roomEffectInstances[count].isActive = effect->startsInactive == 0;
                Field_roomEffectInstances[count].isUpdateEnabled = 1;
                Field_roomEffectInstances[count].reserved03 = 0;
                Field_roomEffectInstances[count].kindOrObjectCount = effect->kind;
                effect->needsReset = 1;
                consumed += sizes[effect->kind];
                data = (u_char*)(data) + (sizes[effect->kind]);
                ++count;
            } else {
                consumed = length;
            }
        }

        Field_textureEffectCount = count;
    }

    Field_roomEffectScriptCount = 0;

    if ((int)Field_roomData.header.effectScriptsLength <= 0) {
        return;
    }

    data = Field_roomData.effectScripts;
    count = Field_roomData.header.effectScriptsLength;
    length = 0;

    for (index = 64; length < count; ++index) {
        Field_RoomEffectScriptHeader* script = (Field_RoomEffectScriptHeader *)(data);
        Field_RoomEffectInstance* entry = &Field_roomEffectInstances[index];
        Field_RoomEffectChunkHeader* chunk;
        int size;

        entry->isActive = script->isActive;
        entry->kindOrObjectCount = script->objectCount;
        entry->isUpdateEnabled = 1;
        entry->reserved03 = 0;
        entry->currentData = (Field_SwingEffectRecord *)(data);
        entry->originalData = (Field_SwingEffectRecord *)(data);

        if (entry->isActive != 0) {
            Field_CreateRoomEffectObjects(entry);
        }

        size = entry->kindOrObjectCount + 4;

        if ((entry->kindOrObjectCount & 3) != 0) {
            int alignedSize = entry->kindOrObjectCount + 8;

            size = alignedSize - (entry->kindOrObjectCount & 3);
        }

        data = (u_char*)(data) + (size);
        length += size;

        chunk = (Field_RoomEffectChunkHeader *)(data);
        size = chunk->type == 11 ? 4 : chunk->size;

        while (size != 0) {
            data = (u_char*)(data) + (size);
            length += size;
            chunk = (Field_RoomEffectChunkHeader *)(data);
            size = chunk->type == 11 ? 4 : chunk->size;
        }

        length += 4;
        data = (u_char*)(data) + (4);
        ++Field_roomEffectScriptCount;
    }
}

// decomp: BATTLE.PRG func_800927AC (src/BATTLE/BATTLE.PRG/2842C.c:951)
void Field_CreateRoomEffectObjects(Field_RoomEffectInstance* effect_instance)
{
    int _[32] __attribute__((unused));
    int i;
    signed char* script;
    Field_RoomEffectObject* elem = (Field_RoomEffectObject *)(Core_Memory_AllocateFromLowEnd(sizeof *elem * effect_instance->kindOrObjectCount));
    signed char* ids = (signed char*)effect_instance->currentData;

    effect_instance->currentData = (Field_SwingEffectRecord*)elem;
    ids += 4;
    script = ids + effect_instance->kindOrObjectCount;

    if (effect_instance->kindOrObjectCount % 4) {
        script = &script[4 - effect_instance->kindOrObjectCount % 4];
    }

    for (i = 0; i < effect_instance->kindOrObjectCount; ++i, ++elem, ++ids) {
        elem->objectId = *ids;
        elem->scriptCursor = script;
        if (elem->objectId >= 0) {
            int* entry = (int *)(Field_FindRoomGeometryGroup(elem->objectId));
            elem->sceneEntry = entry;
            if (entry != NULL) {
                setVector(&elem->position, entry[1] * 16, entry[2] * 16, entry[3] * 16);
                setVector(&elem->rotation, entry[4], entry[5], entry[6]);
            }
        } else {
            elem->sceneEntry = NULL;
        }
        elem->position.pad = 0;
        elem->rotation.pad = 0;
        if (elem->sceneEntry == NULL) {
            elem->objectId = -1;
        }
    }
}

// decomp: BATTLE.PRG func_80092914 (src/BATTLE/BATTLE.PRG/2842C.c:988)
void Field_FinishRoomEffectSetup(void) { }

// decomp: BATTLE.PRG func_8009291C (src/BATTLE/BATTLE.PRG/2842C.c:990)
int Field_StartRoomEffect(int effect_index)
{
    if (effect_index < Field_textureEffectCount) {
        Field_roomEffectInstances[effect_index].isActive = 1;
        return 1;
    }
    if (effect_index < 0x40) {
        return 0;
    }
    if (effect_index < Field_roomEffectScriptCount + 0x40) {
        if ((Field_roomEffectInstances[effect_index].isActive != 0)
            || (Field_roomEffectInstances[effect_index].currentData != Field_roomEffectInstances[effect_index].originalData)) {
            Core_Memory_FreeFromLowEnd(Field_roomEffectInstances[effect_index].currentData);
        }
        Field_roomEffectInstances[effect_index].isActive = 1;
        Field_roomEffectInstances[effect_index].currentData = Field_roomEffectInstances[effect_index].originalData;
        Field_CreateRoomEffectObjects(&Field_roomEffectInstances[effect_index]);
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_80092A18 (src/BATTLE/BATTLE.PRG/2842C.c:1012)
int Field_ReleaseRoomEffects(void)
{
    int i;

    if (Field_roomEffectScriptCount == 0) {
        return 0;
    }

    for (i = 0; i < Field_roomEffectScriptCount; ++i) {
        if ((Field_roomEffectInstances[i + 64].isActive != 0)
            || (Field_roomEffectInstances[i + 64].currentData != Field_roomEffectInstances[i + 64].originalData)) {
            Field_roomEffectInstances[i + 64].isActive = 0;
            Core_Memory_FreeFromLowEnd(Field_roomEffectInstances[i + 64].currentData);
        }
    }

    if (Field_roomEffectSoundId != -1) {
        if (Field_roomData.soundData != NULL) {
            Audio_StopBankSfx(0x200, (int *)(Field_roomData.soundData), Field_roomEffectSoundId);
            Field_roomEffectSoundId = -1;
        }
    }
    Field_roomEffectScriptCount = 0;
    return 1;
}

// decomp: BATTLE.PRG func_80092B04 (src/BATTLE/BATTLE.PRG/2842C.c:1038)
int Field_ExecuteRoomEffectCommands(Field_RoomEffectObjectView* effect_object_cursor, int object_count, Field_RoomEffectInstance* arg2)
{
    int i;
    int idle = 0;

    for (i = 0; i < object_count; ++i, ++effect_object_cursor) {
        if (effect_object_cursor->scriptCursor != NULL) {
            while (1) {
                Field_RoomEffectCommandHeader* command_header = (Field_RoomEffectCommandHeader *)(effect_object_cursor->scriptCursor);

                if ((command_header->size != 0) && (command_header->opcode < 0xD)) {
                    if (Audio_Seq_unusedStateFlags.opaqueA8 != 0) {
                        if (command_header->opcode == 0xB) {
                            break;
                        }
                        while (Field_kRoomEffectOpcodeHandlers[command_header->opcode](effect_object_cursor, command_header) != 0) { }
                    } else if (Field_kRoomEffectOpcodeHandlers[command_header->opcode](effect_object_cursor, command_header) != 0) {
                        break;
                    }
                    effect_object_cursor->scriptCursor = (char*)effect_object_cursor->scriptCursor + command_header->size;
                } else {
                    effect_object_cursor->scriptCursor = NULL;
                    break;
                }
            }
        } else {
            ++idle;
        }
    }
    return idle != object_count;
}

// decomp: BATTLE.PRG func_80092C68 (src/BATTLE/BATTLE.PRG/2842C.c:1070)
int Field_SetRoomEffectRadialVelocity(Field_RoomEffectObject* effect_object, Field_RoomEffectCommand* effect_command)
{
    int x = effect_object->position.vx / 16 - effect_command->operands[0].signedHalves[0];
    int y;
    int z;
    int d;

    effect_object->velocity.vx = effect_command->operands[1].unsignedHalves[1] - ABS(x);
    y = effect_object->position.vy / 16 - effect_command->operands[0].signedHalves[1];
    effect_object->velocity.vy = effect_command->operands[1].unsignedHalves[1] - ABS(y);
    z = effect_object->position.vz / 16;
    z -= effect_command->operands[1].signedHalves[0];
    effect_object->velocity.vz = effect_command->operands[1].unsignedHalves[1] - ABS(z);

    d = effect_object->velocity.vx;

    if (d > 0) {
        z = effect_object->position.vx - (effect_command->operands[0].signedHalves[0] << 4);
        if (z < 0) {
            effect_object->velocity.vx = -d;
        }
    } else {
        effect_object->velocity.vx = 0;
    }

    d = effect_object->velocity.vy;

    if (d > 0) {
        if (effect_object->position.vy - (effect_command->operands[0].signedHalves[1] << 4) < 0) {
            effect_object->velocity.vy = -d;
        }
    } else {
        effect_object->velocity.vy = 0;
    }

    d = effect_object->velocity.vz;

    if (d > 0) {
        if (effect_object->position.vz - (effect_command->operands[1].signedHalves[0] << 4) < 0) {
            effect_object->velocity.vz = -d;
        }
    } else {
        effect_object->velocity.vz = 0;
    }

    applyVector(&effect_object->velocity, 4, 4, 4, /=);
    effect_object->velocity.pad = effect_command->operands[2].unsignedHalves[0] + 1;
    setVector(
        &effect_object->angularVelocity, (effect_object->velocity.vx & 0x1F) - 16, (effect_object->velocity.vy & 0x3F) - 32, 0);
    effect_object->angularVelocity.pad = 0;
    return 0;
}

// decomp: BATTLE.PRG func_80092E14 (src/BATTLE/BATTLE.PRG/2842C.c:1123)
int Field_SetRoomEffectVelocity(Field_RoomEffectObject* effect_object, Field_RoomEffectCommand* effect_command)
{
    setVector(
        &effect_object->velocity, effect_command->operands[0].unsignedHalves[0], effect_command->operands[0].unsignedHalves[1], effect_command->operands[1].unsignedHalves[0]);
    effect_object->velocity.pad = effect_command->operands[1].unsignedHalves[1];
    setVector(
        &effect_object->angularVelocity, effect_command->operands[2].unsignedHalves[0], effect_command->operands[2].unsignedHalves[1], effect_command->operands[3].unsignedHalves[0]);
    effect_object->angularVelocity.pad = effect_command->operands[3].unsignedHalves[1];
    return 0;
}

// decomp: BATTLE.PRG func_80092E7C (src/BATTLE/BATTLE.PRG/2842C.c:1134)
int Field_SetRoomEffectTransform(Field_RoomEffectObject* effect_object, Field_RoomEffectCommand* effect_command)
{
    setVector(&effect_object->position, effect_command->operands[0].word, effect_command->operands[1].word, effect_command->operands[2].word);
    setVector(&effect_object->rotation, effect_command->operands[3].word, effect_command->extraOperandOne, effect_command->extraOperandTwo);
    Field_WriteRoomEffectTransform(effect_object);
    return 0;
}

// decomp: BATTLE.PRG func_80092EDC (src/BATTLE/BATTLE.PRG/2842C.c:1142)
void Field_WriteRoomEffectTransform(Field_RoomEffectObject* effect_object)
{
    if (effect_object->objectId >= 0) {
        int* geometry_group_words = effect_object->sceneEntry;
        geometry_group_words[1] = effect_object->position.vx / 16;
        geometry_group_words[2] = effect_object->position.vy / 16;
        geometry_group_words[3] = effect_object->position.vz / 16;

        geometry_group_words[4] = effect_object->rotation.vx;
        geometry_group_words[5] = effect_object->rotation.vy;
        geometry_group_words[6] = effect_object->rotation.vz;
    }
}

// decomp: BATTLE.PRG func_80092F74 (src/BATTLE/BATTLE.PRG/2842C.c:1162)
int Field_HideRoomEffectObject(Field_RoomEffectObject* effect_object, Field_RoomEffectCommand* arg1 __attribute__((unused)))
{
    if (effect_object->objectId >= 0) {
        *effect_object->sceneEntry = 0x100;
    }
    return 0;
}

// decomp: BATTLE.PRG func_80092F98 (src/BATTLE/BATTLE.PRG/2842C.c:1170)
int Field_ExecuteRoomEffectShowGroup(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* effect_command)
{
    Field_ShowRoomGeometryGroup(effect_command->operands[0].word);
    return 0;
}

// decomp: BATTLE.PRG func_80092FBC (src/BATTLE/BATTLE.PRG/2842C.c:1176)
int Field_AdvanceRoomEffectMotion(Field_RoomEffectObject* effect_object, Field_RoomEffectCommand* effect_command)
{
    int motion_still_active = 0;
    Field_RoomEffectCommand* motion_command = effect_command;

    if (motion_command->operands[0].unsignedHalves[1] == 1) {
        motion_still_active = Field_StepRoomEffectGravity(effect_object);
    } else if (motion_command->operands[0].unsignedHalves[1] == 2) {
        motion_still_active = Field_StepRoomEffectDampedMotion(effect_object);
    }

    if (motion_command->operands[0].unsignedHalves[0] == 0x8000) {
        if (motion_still_active == 0) {
            return 0;
        }
    } else {
        ++effect_object->position.pad;
        if (effect_object->position.pad >= motion_command->operands[0].unsignedHalves[0]) {
            effect_object->position.pad = 0;
            return 0;
        }
    }
    return 1;
}

// decomp: BATTLE.PRG func_8009306C (src/BATTLE/BATTLE.PRG/2842C.c:1201)
int Field_StepRoomEffectGravity(Field_RoomEffectObject* effect_object)
{
    int height;

    addVector(&effect_object->position, &effect_object->velocity);
    addVector(&effect_object->rotation, &effect_object->angularVelocity);
    applyVector(&effect_object->rotation, 0xFFF, 0xFFF, 0xFFF, &=);

    height = Field_FloorHeightAt(effect_object->position.vx / 16, effect_object->position.vz / 16) << 17;
    height >>= 17;

    if (height < effect_object->position.vy / 16) {
        ++effect_object->rotation.pad;

        if ((effect_object->rotation.pad < 3) && (effect_object->velocity.pad > 0)) {
            int velocity;

            effect_object->position.vy = height * 16;
            velocity = effect_object->velocity.vy / -(ABS(effect_object->velocity.pad) + 2);
            effect_object->velocity.vx /= 2;
            effect_object->velocity.vz /= 2;
            applyVector(&effect_object->angularVelocity, 2, 2, 2, /=);
            effect_object->velocity.vy = velocity;
        } else {
            effect_object->rotation.pad = 0;
            return 0;
        }
    }

    if (effect_object->velocity.pad != 0) {
        effect_object->velocity.vx -= (effect_object->velocity.vx * effect_object->velocity.pad) / 16;
        effect_object->velocity.vz -= (effect_object->velocity.vz * effect_object->velocity.pad) / 16;
    }

    effect_object->velocity.vy += 16;

    if (effect_object->angularVelocity.pad != 0) {
        applyVector(&effect_object->angularVelocity, (effect_object->angularVelocity.vx * effect_object->angularVelocity.pad) / 16,
            (effect_object->angularVelocity.vy * effect_object->angularVelocity.pad) / 16,
            (effect_object->angularVelocity.vz * effect_object->angularVelocity.pad) / 16, -=);
    }

    Field_WriteRoomEffectTransform(effect_object);
    return 1;
}

// decomp: BATTLE.PRG func_80093364 (src/BATTLE/BATTLE.PRG/2842C.c:1247)
int Field_StepRoomEffectDampedMotion(Field_RoomEffectObject* effect_object)
{
    addVector(&effect_object->position, &effect_object->velocity);
    addVector(&effect_object->rotation, &effect_object->angularVelocity);
    applyVector(&effect_object->rotation, 0xFFF, 0xFFF, 0xFFF, &=);

    if (effect_object->velocity.pad != 0) {
        applyVector(&effect_object->velocity, (effect_object->velocity.vx * effect_object->velocity.pad) / 16,
            (effect_object->velocity.vy * effect_object->velocity.pad) / 16,
            (effect_object->velocity.vz * effect_object->velocity.pad) / 16, -=);
    }

    if (effect_object->angularVelocity.pad != 0) {
        applyVector(&effect_object->angularVelocity, (effect_object->angularVelocity.vx * effect_object->angularVelocity.pad) / 16,
            (effect_object->angularVelocity.vy * effect_object->angularVelocity.pad) / 16,
            (effect_object->angularVelocity.vz * effect_object->angularVelocity.pad) / 16, -=);
    }

    Field_WriteRoomEffectTransform(effect_object);
    return 0;
}

// decomp: BATTLE.PRG func_8009352C (src/BATTLE/BATTLE.PRG/2842C.c:1269)
int Field_ExecuteRoomEffectPlaySound(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* effect_command)
{
    if (Field_roomData.soundData != NULL) {
        if ((effect_command->operands[0].unsignedHalves[0] == 0) || (Field_isRoomEffectSoundRestored != 0)) {
            Audio_PlayBankSfxCentered(0x200, (int *)(Field_roomData.soundData), effect_command->operands[0].unsignedHalves[1]);
        } else {
            Audio_PlayBankSfx(
                0x200, Field_roomData.soundData, effect_command->operands[0].unsignedHalves[1], 0x80, 0);
        }
        Field_roomEffectSoundId = effect_command->operands[0].unsignedHalves[1];
    }
    return 0;
}

// decomp: BATTLE.PRG func_800935B8 (src/BATTLE/BATTLE.PRG/2842C.c:1283)
int Field_ExecuteRoomEffectStopSound(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* effect_command)
{
    if (Field_roomData.soundData != NULL) {
        Audio_StopBankSfx(0x200, (int *)(Field_roomData.soundData), effect_command->operands[0].unsignedHalves[1]);
        Field_roomEffectSoundId = -1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800935F8 (src/BATTLE/BATTLE.PRG/2842C.c:1292)
int Field_ExecuteRoomEffectStartEffect(Field_RoomEffectObject* arg0 __attribute__((unused)), Field_RoomEffectCommand* effect_command)
{
    Field_StartRoomEffect(effect_command->operands[0].word);
    return 0;
}

// decomp: BATTLE.PRG func_8009361C (src/BATTLE/BATTLE.PRG/2842C.c:1298)
int Field_ExecuteRoomEffectHideGroup(Field_RoomEffectObject* unk0 __attribute__((unused)), Field_RoomEffectCommand* effect_command)
{
    Field_HideRoomGeometryGroup(effect_command->operands[0].word);
    return 0;
}

// decomp: BATTLE.PRG func_80093640 (src/BATTLE/BATTLE.PRG/2842C.c:1304)
int Field_ExecuteRoomEffectStopScript(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* effect_command)
{
    if (Field_roomEffectInstances[effect_command->operands[0].word + 0x40].isActive != 0) {
        Core_Memory_FreeFromLowEnd(Field_roomEffectInstances[effect_command->operands[0].word + 0x40].currentData);
        Field_roomEffectInstances[effect_command->operands[0].word + 0x40].currentData =
            Field_roomEffectInstances[effect_command->operands[0].word + 0x40].originalData;
        Field_roomEffectInstances[effect_command->operands[0].word + 0x40].isActive = 0;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800936F8 (src/BATTLE/BATTLE.PRG/2842C.c:1321)
int Field_ToggleRoomEffectUpdates(int effect_index)
{
    if (effect_index < Field_textureEffectCount) {
        Field_roomEffectInstances[effect_index].isUpdateEnabled = Field_roomEffectInstances[effect_index].isUpdateEnabled < 1U;
        return 1;
    }
    if ((effect_index >= 0x40) && (effect_index < (Field_roomEffectScriptCount + 0x40))) {
        Field_roomEffectInstances[effect_index].isUpdateEnabled = Field_roomEffectInstances[effect_index].isUpdateEnabled < 1U;
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_80093764 (src/BATTLE/BATTLE.PRG/2842C.c:1334)
int Field_IsRoomEffectActive(int effect_index) { return Field_roomEffectInstances[effect_index].isActive; }

// decomp: BATTLE.PRG func_80093788 (src/BATTLE/BATTLE.PRG/2842C.c:1336)
void Field_FinishRoomDoorRotation(int effect_index)
{
    Field_SwingEffectRecord* swing_effect;
    int previous_start_angle;
    int reversed_initial_velocity;
    int* geometry_group_words;

    if (effect_index < 0x40) {
        void* room_geometry_base;
        if (Field_roomEffectInstances[effect_index].kindOrObjectCount == 6) {
            swing_effect = Field_roomEffectInstances[effect_index].currentData;
            room_geometry_base = (void*)Field_roomData.geometry;
            if (Field_roomEffectInstances[effect_index].isActive != 0) {
                geometry_group_words = (int *)(((u_char*)(room_geometry_base)) + swing_effect->geometryOffset);
                geometry_group_words[5] = swing_effect->restAngle;
                geometry_group_words[0] = (geometry_group_words[0] | 0x6000);
                reversed_initial_velocity = -swing_effect->initialVelocity;
                previous_start_angle = swing_effect->startAngle;
                swing_effect->initialVelocity = reversed_initial_velocity;
                swing_effect->angularVelocity = reversed_initial_velocity;
                swing_effect->startAngle = swing_effect->restAngle;
                swing_effect->restAngle = previous_start_angle;
                swing_effect->acceleration = -swing_effect->acceleration;
                Field_roomEffectInstances[effect_index].isActive = 0;
            }
        }
    }
}

// decomp: BATTLE.PRG func_80093824 (src/BATTLE/BATTLE.PRG/2842C.c:1365)
void Field_FinishRoomTiltRotation(int effect_index)
{
    int previous_start_angle;
    Field_SwingEffectRecord* swing_effect;
    int reversed_initial_velocity;
    int* geometry_group_words;

    swing_effect = (Field_SwingEffectRecord*)Field_roomEffectInstances[effect_index].currentData;
    geometry_group_words = (int *)(((u_char*)((void*)Field_roomData.geometry)) + swing_effect->geometryOffset);

    if (swing_effect->axis == 2) {
        geometry_group_words[4] = swing_effect->restAngle;
    } else {
        geometry_group_words[6] = swing_effect->restAngle;
    }
    reversed_initial_velocity = -swing_effect->initialVelocity;
    previous_start_angle = swing_effect->startAngle;
    swing_effect->initialVelocity = reversed_initial_velocity;
    swing_effect->angularVelocity = reversed_initial_velocity;
    swing_effect->startAngle = swing_effect->restAngle;
    swing_effect->restAngle = previous_start_angle;
    swing_effect->acceleration = -swing_effect->acceleration;
}

// decomp: BATTLE.PRG func_800938AC (src/BATTLE/BATTLE.PRG/2842C.c:1389)
void Field_DuckRoomEffectSound(int duck_enabled)
{
    if ((Field_roomEffectSoundId != -1) && (Field_roomData.soundData != NULL)) {
        if (duck_enabled != 0) {
            Audio_FadeBankSfxVolume(0x200, (int *)(Field_roomData.soundData), Field_roomEffectSoundId, 0x20, 8);
        } else {
            Audio_FadeBankSfxVolume(0x200, (int *)(Field_roomData.soundData), Field_roomEffectSoundId, 0x7F, 8);
        }
    }
}

// decomp: BATTLE.PRG func_80093914 (src/BATTLE/BATTLE.PRG/2842C.c:1400)
void Field_SwapRoomGeometryRedGreen(int geometry_group_id)
{
    int* base = Field_roomData.geometry;
    int* entry = (int *)(Field_FindRoomGeometryGroup(geometry_group_id));
    int* block;
    int n0;
    int n1;
    Field_MpdTriangleColorView* t;
    Field_MpdQuadColorView* q;
    int i;
    u_char tmp;

    if (entry == NULL) {
        return;
    }
    block = (int*)((u_char*)base + entry[7]);
    n0 = block[0];
    n1 = block[1];
    block += 2;
    t = (Field_MpdTriangleColorView*)block;
    for (i = 0; i < n0; ++i, ++t) {
        tmp = t->red1;
        t->red1 = t->green1;
        t->green1 = tmp;
        tmp = t->red2;
        t->red2 = t->green2;
        t->green2 = tmp;
        tmp = t->red3;
        t->red3 = t->green3;
        t->green3 = tmp;
    }
    q = (Field_MpdQuadColorView*)t;
    for (i = 0; i < n1; ++i, ++q) {
        tmp = q->red1;
        q->red1 = q->green1;
        q->green1 = tmp;
        tmp = q->red2;
        q->red2 = q->green2;
        q->green2 = tmp;
        tmp = q->red3;
        q->red3 = q->green3;
        q->green3 = tmp;
        tmp = q->red4;
        q->red4 = q->green4;
        q->green4 = tmp;
    }
}

// decomp: BATTLE.PRG func_80093A14 (src/BATTLE/BATTLE.PRG/2842C.c:1448)
void Field_RestoreRoomEffectSoundVolume(void)
{
    Field_isRoomEffectSoundRestored = 1;
    if ((Field_roomEffectSoundId != -1) && (Field_roomData.soundData != NULL)) {
        Audio_FadeBankSfxVolume(0x200, (int *)(Field_roomData.soundData), Field_roomEffectSoundId, 0x7F, 0);
    }
}

// decomp: BATTLE.PRG func_80093A70 (src/BATTLE/BATTLE.PRG/2842C.c:1456)
void Field_RestartRoomEffectSound(void)
{
    if (Field_roomEffectSoundId != -1) {
        if (Field_roomData.soundData != 0) {
            Audio_PlayBankSfxCentered(0x200, (int *)(Field_roomData.soundData), Field_roomEffectSoundId);
        }
    }
}

}  // extern "C"
