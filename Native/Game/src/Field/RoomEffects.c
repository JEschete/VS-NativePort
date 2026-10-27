// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
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
int (*Field_kTextureEffectHandlers[7])(Field_SwingEffectRecord*); /* contents: module image, relocated (ModulePointers.c) */

// decomp: D_800E85E8 (D_800E85E8@BATTLE/BATTLE.PRG)
int (*Field_kRoomEffectOpcodeHandlers[13])(Field_RoomEffectObjectView*, Field_RoomEffectCommandHeader*); /* contents: module image, relocated (ModulePointers.c) */

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
    int* group = Field_FindRoomGeometryGroup(group_index);
    if (group != NULL) {
        *group &= ~0x100;
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800919D8 (src/BATTLE/BATTLE.PRG/2842C.c:572)
int Field_HideRoomGeometryGroup(int group_index)
{
    int* group = Field_FindRoomGeometryGroup(group_index);
    if (group != NULL) {
        *group |= 0x100;
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_80091A1C (src/BATTLE/BATTLE.PRG/2842C.c:582)
int Field_SwapRoomGeometryGroupFlags(int first_group_index, int second_group_index)
{
    int* first_group = Field_FindRoomGeometryGroup(first_group_index);
    int* second_group = Field_FindRoomGeometryGroup(second_group_index);

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
    u_short* group_halfwords = Field_FindRoomGeometryGroup(group_index);
    if (group_halfwords != NULL) {
        group_halfwords[1] = (u_short)(group_halfwords[1] + depth_bias);
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_80091AC0 (src/BATTLE/BATTLE.PRG/2842C.c:606)
int Field_DisableRoomGeometryGroup(int group_index)
{
    int* group = Field_FindRoomGeometryGroup(group_index);
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
int Field_AdvanceRoomTextureFrames(Field_TextureFrameEffect* arg0)
{
    Field_TextureFrameEffect* quad = arg0;
    void* base = Field_roomData.geometry;

    if (++arg0->tick > arg0->delay) {
        int u;
        int v;
        Field_MpdPolygonUvView* polygon;

        arg0->tick = 0;

        if (++arg0->frame >= arg0->frameCount) {
            arg0->frame = 0;
        }

        u = arg0->baseU + arg0->frameWidth * (arg0->frame % arg0->columns);
        v = arg0->baseV + arg0->frameHeight * (arg0->frame / arg0->columns);
        polygon = base + arg0->geometryOffset;

        if ((polygon->type & 0x3C) == 0x34) {
            polygon->u0 = arg0->u[0] + u;
            polygon->u1 = arg0->u[1] + u;
            polygon->u2 = arg0->u[2] + u;
            polygon->v0 = arg0->v[0] + v;
            polygon->v1 = arg0->v[1] + v;
            polygon->v2 = arg0->v[2] + v;
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
int Field_ScrollRoomTextureCoordinates(Field_TextureScrollEffect* arg0)
{
    Field_TextureScrollEffect* quad = arg0;
    void* base = Field_roomData.geometry;

    if (++arg0->tick > arg0->delay) {
        int u = arg0->currentU;
        int v = arg0->currentV;
        Field_MpdPolygonUvView* polygon;

        u += arg0->stepU;
        v += arg0->stepV;
        arg0->tick = 0;

        if (u < arg0->baseU) {
            u = arg0->baseU + arg0->width - arg0->windowWidth;
        }

        if (v < arg0->baseV) {
            v = arg0->baseV + arg0->height - arg0->windowHeight;
        }

        if (arg0->baseU + arg0->width < u + arg0->windowWidth) {
            u = arg0->baseU;
        }

        if (arg0->baseV + arg0->height < v + arg0->windowHeight) {
            v = arg0->baseV;
        }

        arg0->currentU = u;
        arg0->currentV = v;
        polygon = base + arg0->geometryOffset;

        if ((polygon->type & 0x3C) == 0x34) {
            polygon->u0 = arg0->u[0] + u;
            polygon->u1 = arg0->u[1] + u;
            polygon->u2 = arg0->u[2] + u;
            polygon->v0 = arg0->v[0] + v;
            polygon->v1 = arg0->v[1] + v;
            polygon->v2 = arg0->v[2] + v;
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
int Field_AnimateRoomTextureImages(Field_TextureRegionScrollEffect* arg0)
{
    RECT source;
    RECT halfSource;

    if (++arg0->tick > arg0->delay) {
        int destX;
        int destY;
        int halfDestX;
        int halfSourceX;

        arg0->rect.x -= 320;
        arg0->rect.y += 256;
        arg0->tick = 0;

        if (arg0->scrollStep == 0) {
            if (++arg0->frame > arg0->frameCount) {
                arg0->frame = 1;
            }

            setRECT(&source, arg0->rect.x + arg0->rect.w * (arg0->frame % arg0->columns),
                arg0->rect.y + arg0->rect.h * (arg0->frame / arg0->columns), arg0->rect.w,
                arg0->rect.h);
        } else {
            arg0->frame += arg0->scrollStep;

            if (arg0->frame >= arg0->rect.h) {
                arg0->frame = 0;
            }

            setRECT(&source, arg0->rect.x + arg0->rect.w / 2, arg0->rect.y + arg0->frame,
                arg0->rect.w / 2, arg0->rect.h - arg0->frame);
        }

        halfDestX = (arg0->rect.x % 64) / 2 - 64;
        destX = halfDestX + (arg0->rect.x & 0x3C0);
        destY = (arg0->rect.y % 256) / 2 + (arg0->rect.y & 0x100);
        halfSourceX = (source.x % 64) / 2 - 64;
        setRECT(&halfSource, halfSourceX + (source.x & 0x3C0),
            (source.y % 256) / 2 + (source.y & 0x100), source.w / 2, source.h / 2);
        MoveImage(&source, arg0->rect.x, arg0->rect.y);
        MoveImage(&halfSource, destX, destY);

        if (arg0->scrollStep != 0 && arg0->frame != 0) {
            int wrapY;

            source.y = arg0->rect.y;
            source.h = arg0->frame;
            wrapY = arg0->rect.y + arg0->rect.h - arg0->frame;
            destY = (wrapY % 256) / 2 + (wrapY & 0x100);
            halfSource.y = (source.y % 256) / 2 + (source.y & 0x100);
            halfSource.h = source.h / 2;
            MoveImage(&source, arg0->rect.x, wrapY);
            MoveImage(&halfSource, destX, destY);
        }

        arg0->rect.x += 320;
        arg0->rect.y -= 256;
    }

    return 1;
}

// decomp: BATTLE.PRG func_800923F8 (src/BATTLE/BATTLE.PRG/2842C.c:801)
int Field_UpdateRoomRotationEffect(Field_SwingEffectRecord* arg0)
{
    int* base = Field_roomData.geometry;
    int* p;
    int delta;
    int ret = 1;

    if (arg0->needsReset != 0) {
        arg0->needsReset = 0;
        arg0->angle = arg0->startAngle;
        arg0->angularVelocity = arg0->initialVelocity;
    }

    arg0->angle += arg0->angularVelocity;
    arg0->angularVelocity += arg0->acceleration * 8;
    delta = (arg0->angle - arg0->restAngle) & 0xFFF;

    if (delta > ONE / 2) {
        delta = ONE - delta;
    }

    if (delta < ABS(arg0->angularVelocity)) {
        arg0->angularVelocity = -(arg0->angularVelocity >> arg0->dampingShift);

        if (ABS(arg0->angularVelocity) < 0x21) {
            arg0->angle = arg0->restAngle;
            ret = 0;
        }
    }

    p = (void*)base + arg0->geometryOffset;

    if (arg0->axis == 1) {
        p[5] = arg0->angle;
        p[0] |= 0x6000;
    } else if (arg0->axis == 2) {
        p[4] = arg0->angle;
    } else {
        p[6] = arg0->angle;
    }

    if (ret != 0) {
        return 1;
    }

    delta = arg0->startAngle;
    arg0->initialVelocity = -arg0->initialVelocity;
    arg0->angularVelocity = arg0->initialVelocity;
    arg0->startAngle = arg0->restAngle;
    arg0->restAngle = delta;
    arg0->acceleration = -arg0->acceleration;
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
            Field_SwingEffectRecord* effect = data;

            if (effect->kind < 7 && sizes[effect->kind] != 0) {
                Field_roomEffectInstances[count].currentData = effect;
                Field_roomEffectInstances[count].isActive = effect->startsInactive == 0;
                Field_roomEffectInstances[count].isUpdateEnabled = 1;
                Field_roomEffectInstances[count].reserved03 = 0;
                Field_roomEffectInstances[count].kindOrObjectCount = effect->kind;
                effect->needsReset = 1;
                consumed += sizes[effect->kind];
                data += sizes[effect->kind];
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
        Field_RoomEffectScriptHeader* script = data;
        Field_RoomEffectInstance* entry = &Field_roomEffectInstances[index];
        Field_RoomEffectChunkHeader* chunk;
        int size;

        entry->isActive = script->isActive;
        entry->kindOrObjectCount = script->objectCount;
        entry->isUpdateEnabled = 1;
        entry->reserved03 = 0;
        entry->currentData = data;
        entry->originalData = data;

        if (entry->isActive != 0) {
            Field_CreateRoomEffectObjects(entry);
        }

        size = entry->kindOrObjectCount + 4;

        if ((entry->kindOrObjectCount & 3) != 0) {
            int alignedSize = entry->kindOrObjectCount + 8;

            size = alignedSize - (entry->kindOrObjectCount & 3);
        }

        data += size;
        length += size;

        chunk = data;
        size = chunk->type == 11 ? 4 : chunk->size;

        while (size != 0) {
            data += size;
            length += size;
            chunk = data;
            size = chunk->type == 11 ? 4 : chunk->size;
        }

        length += 4;
        data += 4;
        ++Field_roomEffectScriptCount;
    }
}

// decomp: BATTLE.PRG func_800927AC (src/BATTLE/BATTLE.PRG/2842C.c:951)
void Field_CreateRoomEffectObjects(Field_RoomEffectInstance* arg0)
{
    int _[32] __attribute__((unused));
    int i;
    signed char* script;
    Field_RoomEffectObject* elem = Core_Memory_AllocateFromLowEnd(sizeof *elem * arg0->kindOrObjectCount);
    signed char* ids = (signed char*)arg0->currentData;

    arg0->currentData = (Field_SwingEffectRecord*)elem;
    ids += 4;
    script = ids + arg0->kindOrObjectCount;

    if (arg0->kindOrObjectCount % 4) {
        script = &script[4 - arg0->kindOrObjectCount % 4];
    }

    for (i = 0; i < arg0->kindOrObjectCount; ++i, ++elem, ++ids) {
        elem->objectId = *ids;
        elem->scriptCursor = script;
        if (elem->objectId >= 0) {
            int* entry = Field_FindRoomGeometryGroup(elem->objectId);
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
int Field_StartRoomEffect(int arg0)
{
    if (arg0 < Field_textureEffectCount) {
        Field_roomEffectInstances[arg0].isActive = 1;
        return 1;
    }
    if (arg0 < 0x40) {
        return 0;
    }
    if (arg0 < Field_roomEffectScriptCount + 0x40) {
        if ((Field_roomEffectInstances[arg0].isActive != 0)
            || (Field_roomEffectInstances[arg0].currentData != Field_roomEffectInstances[arg0].originalData)) {
            Core_Memory_FreeFromLowEnd(Field_roomEffectInstances[arg0].currentData);
        }
        Field_roomEffectInstances[arg0].isActive = 1;
        Field_roomEffectInstances[arg0].currentData = Field_roomEffectInstances[arg0].originalData;
        Field_CreateRoomEffectObjects(&Field_roomEffectInstances[arg0]);
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
            Audio_StopBankSfx(0x200, Field_roomData.soundData, Field_roomEffectSoundId);
            Field_roomEffectSoundId = -1;
        }
    }
    Field_roomEffectScriptCount = 0;
    return 1;
}

// decomp: BATTLE.PRG func_80092B04 (src/BATTLE/BATTLE.PRG/2842C.c:1038)
int Field_ExecuteRoomEffectCommands(Field_RoomEffectObjectView* arg0, int arg1, Field_RoomEffectInstance* arg2)
{
    int i;
    int idle = 0;

    for (i = 0; i < arg1; ++i, ++arg0) {
        if (arg0->scriptCursor != NULL) {
            while (1) {
                Field_RoomEffectCommandHeader* temp_s0 = arg0->scriptCursor;

                if ((temp_s0->size != 0) && (temp_s0->opcode < 0xD)) {
                    if (Audio_Seq_unusedStateFlags.opaqueA8 != 0) {
                        if (temp_s0->opcode == 0xB) {
                            break;
                        }
                        while (Field_kRoomEffectOpcodeHandlers[temp_s0->opcode](arg0, temp_s0) != 0) { }
                    } else if (Field_kRoomEffectOpcodeHandlers[temp_s0->opcode](arg0, temp_s0) != 0) {
                        break;
                    }
                    arg0->scriptCursor = (char*)arg0->scriptCursor + temp_s0->size;
                } else {
                    arg0->scriptCursor = NULL;
                    break;
                }
            }
        } else {
            ++idle;
        }
    }
    return idle != arg1;
}

// decomp: BATTLE.PRG func_80092C68 (src/BATTLE/BATTLE.PRG/2842C.c:1070)
int Field_SetRoomEffectRadialVelocity(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1)
{
    int x = arg0->position.vx / 16 - arg1->operands[0].signedHalves[0];
    int y;
    int z;
    int d;

    arg0->velocity.vx = arg1->operands[1].unsignedHalves[1] - ABS(x);
    y = arg0->position.vy / 16 - arg1->operands[0].signedHalves[1];
    arg0->velocity.vy = arg1->operands[1].unsignedHalves[1] - ABS(y);
    z = arg0->position.vz / 16;
    z -= arg1->operands[1].signedHalves[0];
    arg0->velocity.vz = arg1->operands[1].unsignedHalves[1] - ABS(z);

    d = arg0->velocity.vx;

    if (d > 0) {
        z = arg0->position.vx - (arg1->operands[0].signedHalves[0] << 4);
        if (z < 0) {
            arg0->velocity.vx = -d;
        }
    } else {
        arg0->velocity.vx = 0;
    }

    d = arg0->velocity.vy;

    if (d > 0) {
        if (arg0->position.vy - (arg1->operands[0].signedHalves[1] << 4) < 0) {
            arg0->velocity.vy = -d;
        }
    } else {
        arg0->velocity.vy = 0;
    }

    d = arg0->velocity.vz;

    if (d > 0) {
        if (arg0->position.vz - (arg1->operands[1].signedHalves[0] << 4) < 0) {
            arg0->velocity.vz = -d;
        }
    } else {
        arg0->velocity.vz = 0;
    }

    applyVector(&arg0->velocity, 4, 4, 4, /=);
    arg0->velocity.pad = arg1->operands[2].unsignedHalves[0] + 1;
    setVector(
        &arg0->angularVelocity, (arg0->velocity.vx & 0x1F) - 16, (arg0->velocity.vy & 0x3F) - 32, 0);
    arg0->angularVelocity.pad = 0;
    return 0;
}

// decomp: BATTLE.PRG func_80092E14 (src/BATTLE/BATTLE.PRG/2842C.c:1123)
int Field_SetRoomEffectVelocity(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1)
{
    setVector(
        &arg0->velocity, arg1->operands[0].unsignedHalves[0], arg1->operands[0].unsignedHalves[1], arg1->operands[1].unsignedHalves[0]);
    arg0->velocity.pad = arg1->operands[1].unsignedHalves[1];
    setVector(
        &arg0->angularVelocity, arg1->operands[2].unsignedHalves[0], arg1->operands[2].unsignedHalves[1], arg1->operands[3].unsignedHalves[0]);
    arg0->angularVelocity.pad = arg1->operands[3].unsignedHalves[1];
    return 0;
}

// decomp: BATTLE.PRG func_80092E7C (src/BATTLE/BATTLE.PRG/2842C.c:1134)
int Field_SetRoomEffectTransform(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1)
{
    setVector(&arg0->position, arg1->operands[0].word, arg1->operands[1].word, arg1->operands[2].word);
    setVector(&arg0->rotation, arg1->operands[3].word, arg1->extraOperandOne, arg1->extraOperandTwo);
    Field_WriteRoomEffectTransform(arg0);
    return 0;
}

// decomp: BATTLE.PRG func_80092EDC (src/BATTLE/BATTLE.PRG/2842C.c:1142)
void Field_WriteRoomEffectTransform(Field_RoomEffectObject* arg0)
{
    if (arg0->objectId >= 0) {
        int* temp_v1 = arg0->sceneEntry;
        temp_v1[1] = arg0->position.vx / 16;
        temp_v1[2] = arg0->position.vy / 16;
        temp_v1[3] = arg0->position.vz / 16;

        temp_v1[4] = arg0->rotation.vx;
        temp_v1[5] = arg0->rotation.vy;
        temp_v1[6] = arg0->rotation.vz;
    }
}

// decomp: BATTLE.PRG func_80092F74 (src/BATTLE/BATTLE.PRG/2842C.c:1162)
int Field_HideRoomEffectObject(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1 __attribute__((unused)))
{
    if (arg0->objectId >= 0) {
        *arg0->sceneEntry = 0x100;
    }
    return 0;
}

// decomp: BATTLE.PRG func_80092F98 (src/BATTLE/BATTLE.PRG/2842C.c:1170)
int Field_ExecuteRoomEffectShowGroup(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1)
{
    Field_ShowRoomGeometryGroup(arg1->operands[0].word);
    return 0;
}

// decomp: BATTLE.PRG func_80092FBC (src/BATTLE/BATTLE.PRG/2842C.c:1176)
int Field_AdvanceRoomEffectMotion(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1)
{
    int var_a0 = 0;
    Field_RoomEffectCommand* new_var = arg1;

    if (new_var->operands[0].unsignedHalves[1] == 1) {
        var_a0 = Field_StepRoomEffectGravity(arg0);
    } else if (new_var->operands[0].unsignedHalves[1] == 2) {
        var_a0 = Field_StepRoomEffectDampedMotion(arg0);
    }

    if (new_var->operands[0].unsignedHalves[0] == 0x8000) {
        if (var_a0 == 0) {
            return 0;
        }
    } else {
        ++arg0->position.pad;
        if (arg0->position.pad >= new_var->operands[0].unsignedHalves[0]) {
            arg0->position.pad = 0;
            return 0;
        }
    }
    return 1;
}

// decomp: BATTLE.PRG func_8009306C (src/BATTLE/BATTLE.PRG/2842C.c:1201)
int Field_StepRoomEffectGravity(Field_RoomEffectObject* arg0)
{
    int height;

    addVector(&arg0->position, &arg0->velocity);
    addVector(&arg0->rotation, &arg0->angularVelocity);
    applyVector(&arg0->rotation, 0xFFF, 0xFFF, 0xFFF, &=);

    height = Field_FloorHeightAt(arg0->position.vx / 16, arg0->position.vz / 16) << 17;
    height >>= 17;

    if (height < arg0->position.vy / 16) {
        ++arg0->rotation.pad;

        if ((arg0->rotation.pad < 3) && (arg0->velocity.pad > 0)) {
            int velocity;

            arg0->position.vy = height * 16;
            velocity = arg0->velocity.vy / -(ABS(arg0->velocity.pad) + 2);
            arg0->velocity.vx /= 2;
            arg0->velocity.vz /= 2;
            applyVector(&arg0->angularVelocity, 2, 2, 2, /=);
            arg0->velocity.vy = velocity;
        } else {
            arg0->rotation.pad = 0;
            return 0;
        }
    }

    if (arg0->velocity.pad != 0) {
        arg0->velocity.vx -= (arg0->velocity.vx * arg0->velocity.pad) / 16;
        arg0->velocity.vz -= (arg0->velocity.vz * arg0->velocity.pad) / 16;
    }

    arg0->velocity.vy += 16;

    if (arg0->angularVelocity.pad != 0) {
        applyVector(&arg0->angularVelocity, (arg0->angularVelocity.vx * arg0->angularVelocity.pad) / 16,
            (arg0->angularVelocity.vy * arg0->angularVelocity.pad) / 16,
            (arg0->angularVelocity.vz * arg0->angularVelocity.pad) / 16, -=);
    }

    Field_WriteRoomEffectTransform(arg0);
    return 1;
}

// decomp: BATTLE.PRG func_80093364 (src/BATTLE/BATTLE.PRG/2842C.c:1247)
int Field_StepRoomEffectDampedMotion(Field_RoomEffectObject* arg0)
{
    addVector(&arg0->position, &arg0->velocity);
    addVector(&arg0->rotation, &arg0->angularVelocity);
    applyVector(&arg0->rotation, 0xFFF, 0xFFF, 0xFFF, &=);

    if (arg0->velocity.pad != 0) {
        applyVector(&arg0->velocity, (arg0->velocity.vx * arg0->velocity.pad) / 16,
            (arg0->velocity.vy * arg0->velocity.pad) / 16,
            (arg0->velocity.vz * arg0->velocity.pad) / 16, -=);
    }

    if (arg0->angularVelocity.pad != 0) {
        applyVector(&arg0->angularVelocity, (arg0->angularVelocity.vx * arg0->angularVelocity.pad) / 16,
            (arg0->angularVelocity.vy * arg0->angularVelocity.pad) / 16,
            (arg0->angularVelocity.vz * arg0->angularVelocity.pad) / 16, -=);
    }

    Field_WriteRoomEffectTransform(arg0);
    return 0;
}

// decomp: BATTLE.PRG func_8009352C (src/BATTLE/BATTLE.PRG/2842C.c:1269)
int Field_ExecuteRoomEffectPlaySound(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1)
{
    if (Field_roomData.soundData != NULL) {
        if ((arg1->operands[0].unsignedHalves[0] == 0) || (Field_isRoomEffectSoundRestored != 0)) {
            Audio_PlayBankSfxCentered(0x200, Field_roomData.soundData, arg1->operands[0].unsignedHalves[1]);
        } else {
            Audio_PlayBankSfx(
                0x200, Field_roomData.soundData, arg1->operands[0].unsignedHalves[1], 0x80, 0);
        }
        Field_roomEffectSoundId = arg1->operands[0].unsignedHalves[1];
    }
    return 0;
}

// decomp: BATTLE.PRG func_800935B8 (src/BATTLE/BATTLE.PRG/2842C.c:1283)
int Field_ExecuteRoomEffectStopSound(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1)
{
    if (Field_roomData.soundData != NULL) {
        Audio_StopBankSfx(0x200, Field_roomData.soundData, arg1->operands[0].unsignedHalves[1]);
        Field_roomEffectSoundId = -1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800935F8 (src/BATTLE/BATTLE.PRG/2842C.c:1292)
int Field_ExecuteRoomEffectStartEffect(Field_RoomEffectObject* arg0 __attribute__((unused)), Field_RoomEffectCommand* arg1)
{
    Field_StartRoomEffect(arg1->operands[0].word);
    return 0;
}

// decomp: BATTLE.PRG func_8009361C (src/BATTLE/BATTLE.PRG/2842C.c:1298)
int Field_ExecuteRoomEffectHideGroup(Field_RoomEffectObject* unk0 __attribute__((unused)), Field_RoomEffectCommand* arg1)
{
    Field_HideRoomGeometryGroup(arg1->operands[0].word);
    return 0;
}

// decomp: BATTLE.PRG func_80093640 (src/BATTLE/BATTLE.PRG/2842C.c:1304)
int Field_ExecuteRoomEffectStopScript(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1)
{
    if (Field_roomEffectInstances[arg1->operands[0].word + 0x40].isActive != 0) {
        Core_Memory_FreeFromLowEnd(Field_roomEffectInstances[arg1->operands[0].word + 0x40].currentData);
        Field_roomEffectInstances[arg1->operands[0].word + 0x40].currentData =
            Field_roomEffectInstances[arg1->operands[0].word + 0x40].originalData;
        Field_roomEffectInstances[arg1->operands[0].word + 0x40].isActive = 0;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800936F8 (src/BATTLE/BATTLE.PRG/2842C.c:1321)
int Field_ToggleRoomEffectUpdates(int arg0)
{
    if (arg0 < Field_textureEffectCount) {
        Field_roomEffectInstances[arg0].isUpdateEnabled = Field_roomEffectInstances[arg0].isUpdateEnabled < 1U;
        return 1;
    }
    if ((arg0 >= 0x40) && (arg0 < (Field_roomEffectScriptCount + 0x40))) {
        Field_roomEffectInstances[arg0].isUpdateEnabled = Field_roomEffectInstances[arg0].isUpdateEnabled < 1U;
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_80093764 (src/BATTLE/BATTLE.PRG/2842C.c:1334)
int Field_IsRoomEffectActive(int arg0) { return Field_roomEffectInstances[arg0].isActive; }

// decomp: BATTLE.PRG func_80093788 (src/BATTLE/BATTLE.PRG/2842C.c:1336)
void Field_FinishRoomDoorRotation(int arg0)
{
    Field_SwingEffectRecord* temp_a2;
    int temp_a1;
    int temp_v0;
    int* temp_v1;

    if (arg0 < 0x40) {
        void* a0;
        if (Field_roomEffectInstances[arg0].kindOrObjectCount == 6) {
            temp_a2 = Field_roomEffectInstances[arg0].currentData;
            a0 = (void*)Field_roomData.geometry;
            if (Field_roomEffectInstances[arg0].isActive != 0) {
                temp_v1 = a0 + temp_a2->geometryOffset;
                temp_v1[5] = temp_a2->restAngle;
                temp_v1[0] = (temp_v1[0] | 0x6000);
                temp_v0 = -temp_a2->initialVelocity;
                temp_a1 = temp_a2->startAngle;
                temp_a2->initialVelocity = temp_v0;
                temp_a2->angularVelocity = temp_v0;
                temp_a2->startAngle = temp_a2->restAngle;
                temp_a2->restAngle = temp_a1;
                temp_a2->acceleration = -temp_a2->acceleration;
                Field_roomEffectInstances[arg0].isActive = 0;
            }
        }
    }
}

// decomp: BATTLE.PRG func_80093824 (src/BATTLE/BATTLE.PRG/2842C.c:1365)
void Field_FinishRoomTiltRotation(int arg0)
{
    int temp_a1;
    Field_SwingEffectRecord* temp_a2;
    int temp_v0;
    int* temp_a0;

    temp_a2 = (Field_SwingEffectRecord*)Field_roomEffectInstances[arg0].currentData;
    temp_a0 = (void*)Field_roomData.geometry + temp_a2->geometryOffset;

    if (temp_a2->axis == 2) {
        temp_a0[4] = temp_a2->restAngle;
    } else {
        temp_a0[6] = temp_a2->restAngle;
    }
    temp_v0 = -temp_a2->initialVelocity;
    temp_a1 = temp_a2->startAngle;
    temp_a2->initialVelocity = temp_v0;
    temp_a2->angularVelocity = temp_v0;
    temp_a2->startAngle = temp_a2->restAngle;
    temp_a2->restAngle = temp_a1;
    temp_a2->acceleration = -temp_a2->acceleration;
}

// decomp: BATTLE.PRG func_800938AC (src/BATTLE/BATTLE.PRG/2842C.c:1389)
void Field_DuckRoomEffectSound(int arg0)
{
    if ((Field_roomEffectSoundId != -1) && (Field_roomData.soundData != NULL)) {
        if (arg0 != 0) {
            Audio_FadeBankSfxVolume(0x200, Field_roomData.soundData, Field_roomEffectSoundId, 0x20, 8);
        } else {
            Audio_FadeBankSfxVolume(0x200, Field_roomData.soundData, Field_roomEffectSoundId, 0x7F, 8);
        }
    }
}

// decomp: BATTLE.PRG func_80093914 (src/BATTLE/BATTLE.PRG/2842C.c:1400)
void Field_SwapRoomGeometryRedGreen(int arg0)
{
    int* base = Field_roomData.geometry;
    int* entry = Field_FindRoomGeometryGroup(arg0);
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
        Audio_FadeBankSfxVolume(0x200, Field_roomData.soundData, Field_roomEffectSoundId, 0x7F, 0);
    }
}

// decomp: BATTLE.PRG func_80093A70 (src/BATTLE/BATTLE.PRG/2842C.c:1456)
void Field_RestartRoomEffectSound(void)
{
    if (Field_roomEffectSoundId != -1) {
        if (Field_roomData.soundData != 0) {
            Audio_PlayBankSfxCentered(0x200, Field_roomData.soundData, Field_roomEffectSoundId);
        }
    }
}

