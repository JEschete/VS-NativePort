// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/Textures.h"
#include "Actors/WeaponModel.h"
#include "Audio/Sfx.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "DiscQueue.h"
#include "Field/Actors.h"
#include "Field/Gimmicks.h"
#include "Items/Materials.h"
#include "LegacyFiles.h"
#include "ModelLoading.h"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Textures.h"

// decomp: D_800E8F29 (D_800E8F29@BATTLE/BATTLE.PRG)
u_char Content_modelParseResetFlag; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8FC4 (D_800E8FC4@BATTLE/BATTLE.PRG)
int Content_assetQueuePeakUsage; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F457C (D_800F457C@BATTLE/BATTLE.PRG)
int Content_lastLoadPhaseSnapshot;

// decomp: D_800F4580 (D_800F4580@BATTLE/BATTLE.PRG)
int Content_assetQueueFrameTicks;

// decomp: D_800F45D8 (D_800F45D8@BATTLE/BATTLE.PRG)
int Content_lastAssetRequestType;

// decomp: BATTLE.PRG vs_battle_getEmptyObjectDataSlot (src/BATTLE/BATTLE.PRG/30DB0.c:149)
int Content_FindFreeActorAssetRequest(void)
{
    int i;

    for (i = 0; i < 16; ++i) {
        if (Content_legacyActorAssetRequests[i].assetKind == 0) {
            return i;
        }
    }

    return -1;
}

// decomp: BATTLE.PRG vs_battle_populateDataSlot (src/BATTLE/BATTLE.PRG/30DB0.c:162)
int Content_QueueActorAssetRequest(Content_ActorAssetRequest* objectData)
{
    Content_ActorAssetRequest* dst;
    int slot = Content_FindFreeActorAssetRequest();

    if (slot == -1) {
        return -1;
    }

    dst = &Content_legacyActorAssetRequests[slot];
    Core_Debugf("actors", "queue kind=%u actor=%u model=%u phase=%u parse=%u",
        (unsigned)objectData->assetKind, (unsigned)objectData->targetSlot,
        (unsigned)objectData->modelId, (unsigned)Content_legacyModelLoadPhase,
        (unsigned)Content_modelParseResetFlag);
    *dst = *objectData;
    return slot;
}

// decomp: BATTLE.PRG vs_battle_processObjectDataQueue (src/BATTLE/BATTLE.PRG/30DB0.c:176)
int Content_UpdateActorAssetQueue(void)
{
    int i;
    int ret;
    Content_ActorAssetRequest* slot = Content_legacyActorAssetRequests;

    Content_assetQueueFrameTicks = VSync(1);

    switch (slot->assetKind) {
    case 0:
        return 0;
    case 1:
        ret = Content_UpdateActorModelLoad(slot);
        break;
    case 3:
        ret = Content_UpdateWeaponModelLoad(slot);
        break;
    case 7:
        ret = Content_UpdateActorSequenceLoad(slot);
        break;
    case 2:
        ret = Content_ParseActorModel(slot);
        break;
    case 4:
        ret = Content_ParseWeaponModel(slot);
        break;
    case 6:
        ret = Field_CreateGimmickInstance(slot);
        break;
    case 8:
        ret = Content_ParseActorSequence(slot);
        break;
    case 9:
        ret = Content_UpdateSequenceEffectTextureLoad(slot);
        break;
    default:
        return -1;
    }

    Content_assetQueueFrameTicks = VSync(1) - Content_assetQueueFrameTicks;

    if (Content_legacyModelLoadPhase == 2) {
        if (Content_assetQueuePeakUsage < Content_assetQueueFrameTicks) {
            Content_assetQueuePeakUsage = Content_assetQueueFrameTicks;
            Content_lastAssetRequestType = slot->assetKind;
            Content_lastLoadPhaseSnapshot = Content_legacyModelLoadPhase;
        }
    }

    if (ret == -1) {
        return 1;
    }
    Core_Debugf("actors", "loaded kind=%u actor=%u model=%d result=%d record=%p",
        (unsigned)slot->assetKind, (unsigned)slot->targetSlot, (int)slot->modelId, ret,
        slot->targetSlot < 17 ? (void*)Actors_actorRecords[slot->targetSlot] : NULL);

    for (i = 1; i < 15; ++i) {
        Core_Memory_CopyAlignedWords(&Content_legacyActorAssetRequests[i - 1], &Content_legacyActorAssetRequests[i],
            sizeof Content_legacyActorAssetRequests[i]);
    }

    Content_legacyActorAssetRequests[i - 1].assetKind = 0;

    return ret < 0 ? -1 : 1;
}

// decomp: BATTLE.PRG func_80099854 (src/BATTLE/BATTLE.PRG/30DB0.c:239)
void Content_QueueAshleyWeaponSequence(int sequence_id)
{
    Content_ActorAssetRequest objectData;
    Content_ActorAssetRequest* src;
    int i;
    int slot;
    Content_ActorAssetRequest* dst;

    if (sequence_id == 0) {
        return;
    }

    src = &objectData;
    src->assetKind = 7;
    src->targetSlot = 0;
    src->subKind = 3;
    src->material = sequence_id;

    for (i = 0; i < 16; ++i) {
        if (Content_legacyActorAssetRequests[i].assetKind == 0) {
            slot = i;
            goto found;
        }
    }

    slot = -1;

found:
    if (slot != -1) {
        dst = &Content_legacyActorAssetRequests[slot];
        *dst = *src;
    }
}

// decomp: BATTLE.PRG func_80099E7C (src/BATTLE/BATTLE.PRG/30DB0.c:445)
void Content_OffsetModelTextureCoordinates(void* polygonData, int polygonCount, int colored, int uOffset, int vOffset)
{
    u_char* cursor = (u_char *)(polygonData);
    int polygonIndex;
    int vertexIndex;
    int remainingUvPairs;

    if (colored == 0) {
        for (polygonIndex = 0; polygonIndex < polygonCount; ++polygonIndex) {
            if (*cursor == 0x24) {
                cursor += 0xA;
                remainingUvPairs = 3;
            } else {
                cursor += 0xC;
                remainingUvPairs = 4;
            }

            for (vertexIndex = 0; vertexIndex < remainingUvPairs; ++vertexIndex) {
                cursor[0] += uOffset;
                cursor[1] += vOffset;
                cursor += 2;
            }
        }
    } else {
        for (polygonIndex = 0; polygonIndex < polygonCount; ++polygonIndex) {
            if (cursor[11] == 0x34) {
                cursor[6] += uOffset;
                cursor[7] += vOffset;
                cursor += 0x14;
                remainingUvPairs = 2;
            } else {
                cursor += 0x18;
                remainingUvPairs = 4;
            }

            for (vertexIndex = 0; vertexIndex < remainingUvPairs; ++vertexIndex) {
                cursor[0] += uOffset;
                cursor[1] += vOffset;
                cursor += 2;
            }
        }
    }
}

// decomp: BATTLE.PRG func_80099FA8 (src/BATTLE/BATTLE.PRG/30DB0.c:489)
int Content_FindSharedWeaponModel(Content_ActorAssetRequest* objData)
{
    int i;
    int index = objData->targetSlot;

    for (i = 0; i < 20; ++i) {
        if (i != index) {
            Actors_WeaponModel* model = Actors_weaponModels[i];
            if ((model != NULL) && (model->actorId || (index < 4))
                && (model->modelId == objData->modelId)) {
                objData->dataAddress = i;
                return 1;
            }
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8009A028 (src/BATTLE/BATTLE.PRG/30DB0.c:507)
void* Content_CopyModelGeometry(int* destination, void* source, int byteCount)
{
    if (destination == NULL) {
        destination = (int *)(Core_Memory_AllocateFromLowEnd(byteCount));
    }

    memcpy(destination, source, byteCount);


    return destination;
}

// decomp: BATTLE.PRG _loadWep (src/BATTLE/BATTLE.PRG/30DB0.c:523)
int Content_UpdateWeaponModelLoad(Content_ActorAssetRequest* request)
{
    int wepData;
    int found;
    int wepFileSize;
    int i;

    if ((request->modelId == 0) || (request->modelId >= 0x80)) {
        return 0;
    }

    switch (Content_legacyModelLoadPhase) {
    case 0:
        for (i = 0; i < 20; ++i) {
            if ((i != request->targetSlot) && (Actors_weaponModels[i] != NULL)
                && ((Actors_weaponModels[i]->actorId) || (request->targetSlot < 4))
                && (Actors_weaponModels[i]->modelId == request->modelId)) {
                request->dataAddress = i;
                found = 1;
                goto exit;
            }
        }
        found = 0;
    exit:
        if (found != 0) {
            Content_legacyWeaponFileLoaded = 0;
            break;
        }

        Content_legacyWeaponFileLoaded = 1;
        // Fallthrough

    default:
        wepFileSize = Content_legacyWepSectorCounts[request->modelId];
        wepFileSize <<= 0xB;
        Content_legacyWeaponFileBuffer = Core_Memory_AllocateFromHighEnd(wepFileSize);
        request->dataAddress = (uintptr_t)Content_legacyWeaponFileBuffer;

        if (Content_legacyWeaponFileBuffer == NULL) {
            return -2;
        }

        Content_legacyModelLoadPhase = 1;
        Content_legacyWeaponFileLocation.lba = Content_legacyWepDiscOffsets[request->modelId] + VS_01_WEP_LBA;
        Content_legacyWeaponFileLocation.size = wepFileSize;
        Content_legacyWeaponDiscRequest = Content_AllocateDiscRequest(&Content_legacyWeaponFileLocation);
        Content_EnqueueDiscRequest(Content_legacyWeaponDiscRequest, (void*)request->dataAddress);
        // Fallthrough

    case 1:
        if (Content_legacyWeaponDiscRequest->state != 4) {
            return -1;
        }
        Content_ReleaseDiscRequest(Content_legacyWeaponDiscRequest);
        Content_legacyModelLoadPhase = 2;
        return -1;
    case 2:
        break;
    }

    wepData = Content_ParseWeaponModel(request);

    if (wepData == -1) {
        return -1;
    }

    if (Content_legacyWeaponFileLoaded != 0) {
        Content_legacyWeaponFileLoaded = 0;
        Core_Memory_FreeFromHighEnd(Content_legacyWeaponFileBuffer);
    }

    Content_legacyModelLoadPhase = 0;
    return wepData;
}

// decomp: BATTLE.PRG _parseWep (src/BATTLE/BATTLE.PRG/30DB0.c:598)
int Content_ParseWeaponModel(Content_ActorAssetRequest* objectData)
{
    u_char* pData;
    Actors_WeaponModelData* buf;
    Actors_WeaponModelData* dst;
    RECT rect;
    int i;
    int j;
    int n;
    u_int texW;
    int w;
    int texU;
    int count; /* block-copy unit size, then reused as the primitives count */
    int size; /* model-data byte size, then reused for the texture-format marker byte */
    int texV; /* 0xE0 texture VRAM-Y: the clut V-offset, also stored to rect.y */

    Actors_WeaponModel* model = Actors_weaponModels[objectData->targetSlot];

    u_char* data =
        (u_char*)objectData->dataAddress; /* parse cursor walking the weapon-model file */

    if (Content_modelParseResetFlag == 0) {
        if (model == NULL) {
            Actors_weaponModels[objectData->targetSlot] = (Actors_WeaponModel *)(objectData->destination);
            model = Actors_weaponModels[objectData->targetSlot];

            if (model == NULL) {
                return -2;
            }

            memset(model, 0, sizeof *model);
            model->textureSlot = 0xFF;
        }

        if (model->boneCount != 0) {
            return -3;
        }

        if ((uintptr_t)data < 20) {
        copySlot:
            /* copy model from an already-loaded slot */
            Core_Memory_CopyAlignedWords(model, Actors_weaponModels[(u_long)data], sizeof *model);

            if (model->textureSlot != 0xFF) {
                ++Render_legacyWeaponTextureSlots[model->textureSlot].referenceCount;
                if (model->isWideTexture) {
                    ++Render_legacyWeaponTextureSlots[model->textureSlot + 1].referenceCount;
                }
            }

            if (objectData->targetSlot < 4) {
                data = (u_char *)((void*)model->sectionOffsets);

                if (objectData->targetSlot < 2) {
                    buf = Content_legacyWeaponModelBuffers[objectData->targetSlot];
                    model->sectionOffsets = buf;
                } else {
                    buf = (Actors_WeaponModelData *)(Core_Memory_AllocateFromLowEnd(sizeof *buf));
                    model->sectionOffsets = buf;
                }

                Core_Memory_CopyAlignedWords(buf, data, sizeof *buf);

                /* dst doubles as the relocation delta for the copied pointers */
                dst = (Actors_WeaponModelData*)((uintptr_t)buf - (uintptr_t)data);
            }

            model->paletteSlotOffset = objectData->textureSlot + (objectData->subKind << 1) + 16;

            Content_LoadWeaponMaterialPalette(objectData->targetSlot, objectData->material);

            model->reserved11 = 64;
            model->reserved08Bit4 = 0;
            model->reserved08Bit6 = 0;
            model->reserved13 = 0;
            model->actorId = objectData->subKind;
            model->attachmentSlot = objectData->textureSlot;
            model->attachmentBone = Actors_FindActorBoneTag(objectData->subKind, objectData->textureSlot, 0);
            Content_modelParseResetFlag = 0;

            return 0;
        } else {
            int found;
            for (j = 0; j < 20; j++) {
                if (j != objectData->targetSlot) {
                    Actors_WeaponModel* other = Actors_weaponModels[j];
                    if ((other != NULL) && ((other->actorId) || (objectData->targetSlot < 4))
                        && (other->modelId == objectData->modelId)) {
                        objectData->dataAddress = j;
                        found = 1;
                        goto haveFound;
                    }
                }
            }
            found = 0;
        haveFound:
            if (found) {
                data = (u_char*)objectData->dataAddress;
                goto copySlot;
            }
            model->modelId = objectData->modelId;
        }
    } else if (Content_modelParseResetFlag == 1) {
        data = (u_char *)(Content_legacyWeaponParseCursor);
        texV = 0xE0; /* clut V offset; also lands in rect.y on the parse path */
        goto fixPrims;
    }

    data += 4;
    *(int*)&model->boneCount = *(int*)data;
    *(int*)&model->quadCount = *(int*)(data + 4);

    data += 8;
    size = *(int*)data;

    data += 4;
    buf = NULL;

    if (objectData->targetSlot == 0) {
        buf = Content_legacyWeaponModelBuffers[0];
    } else if (objectData->targetSlot == 1) {
        buf = Content_legacyWeaponModelBuffers[1];
    }

    dst = buf;

    if (dst == NULL) {
        dst = (Actors_WeaponModelData *)(Core_Memory_AllocateFromLowEnd(size));
    }

    memcpy(dst, data, size);

    model->sectionOffsets = dst;

    if (dst == NULL) {
        return -2;
    }

    data += size;
    data += 5;

    texW = *data;
    texW *= 2;

    data += 2;

    /* size is reused here for the 1-byte texture-format marker */
    size = *data++;

    if ((int)texW < 65) {
        model->isWideTexture = 0;
        for (i = 0; i < 20; i++) {
            if (Render_legacyWeaponTextureSlots[i].referenceCount == 0) {
                Render_legacyWeaponTextureSlots[i].referenceCount = 1;
                break;
            }
        }
        if (i == 20) {
            return -2;
        }
    } else {
        model->isWideTexture = 1;
        for (i = 0; i < 20; i += 2) {
            if ((Render_legacyWeaponTextureSlots[i].referenceCount == 0)
                && (Render_legacyWeaponTextureSlots[i + 1].referenceCount == 0)) {
                Render_legacyWeaponTextureSlots[i].referenceCount = 1;
                Render_legacyWeaponTextureSlots[i + 1].referenceCount = 1;
                break;
            }
        }
        if (i >= 20) {
            return -2;
        }
    }

    model->textureSlot = i;
    model->paletteSlotOffset = objectData->textureSlot + (objectData->subKind << 1) + 16;

    if (size == 96) {
        w = 32;
        count = 64;
        model->isDoubleClut = 1;
        model->paletteColorCount = 96;
    } else {
        w = 16;
        count = 32;
        model->isDoubleClut = 0;
        model->paletteColorCount = 48;
    }

    Core_Memory_CopyAlignedWords(model->workingPalette, data, w * 2);

    data += w * 2;

    for (i = 0; i < 7; i++) {
        Core_Memory_CopyAlignedWords(model->palettes[i], data, count * 2);
        data += count * 2;
    }

    texU = (model->textureSlot / 2) * 64 + (model->textureSlot & 1) * 32;
    texV = 0xE0; /* same 0xE0 register the clut path adds to V coords */

    setRECT(&rect, texU, texV, texW / 2, 32);
    LoadImage(&rect, (u_long *)((void*)data));

    data += texW * 32;
    Content_modelParseResetFlag = 1;
    Content_legacyWeaponParseCursor = data;

    return -1;

fixPrims:
    count = model->otherPrimitiveCount + model->triangleCount + model->quadCount;
    pData = (u_char *)((void*)VS_ONDISC(model->sectionOffsets, polygonSection, void*));
    texU = (model->textureSlot & 1) * 64;

    for (n = 0; n < count; n++) {
        int vertices = 4;
        int k;

        if (*pData == Render_LegacyPrimitiveCode_PolyFt3) {
            pData += 10;
            vertices = 3;
        } else {
            pData += 12;
        }

        // Update uv coords to match the loaded slot
        for (k = 0; k < vertices; pData += 2, ++k) {
            pData[0] += texU;
            pData[1] += texV;
        }
    }

    memcpy(model->bones, data, model->boneCount * 8);
    Content_LoadWeaponMaterialPalette(objectData->targetSlot, objectData->material);

    model->reserved11 = 64;
    model->reserved08Bit4 = 0;
    model->reserved08Bit6 = 0;
    model->reserved13 = 0;
    model->actorId = objectData->subKind;
    model->attachmentSlot = objectData->textureSlot;
    model->attachmentBone = Actors_FindActorBoneTag(objectData->subKind, objectData->textureSlot, 0);
    Content_modelParseResetFlag = 0;

    return 0;
}

// decomp: BATTLE.PRG _loadMaterialPalette (src/BATTLE/BATTLE.PRG/30DB0.c:855)
void Content_LoadWeaponMaterialPalette(int index, int material)
{
    int count;
    int offset;
    int palette;
    Actors_WeaponModel* model = Actors_weaponModels[index];

    do {
    } while (0);

    offset = 32;
    model->material = material;
    palette = Items_weaponMaterialPaletteIndices[material];

    if (model->isDoubleClut) {
        count = 64;
    } else {
        offset = 16;
        count = 32;
    }

    Core_Memory_CopyAlignedWords(&model->workingPalette[offset], &model->palettes[palette], count * 2);
    Core_Memory_CopyAlignedWords(&model->uploadPalette, model->workingPalette, sizeof model->uploadPalette);
    Render_LoadClutColors(model->uploadPalette, (model->paletteSlotOffset >> 1) + 4,
        ((model->paletteSlotOffset & 1) * 48) + 160, offset + count);
}

// decomp: BATTLE.PRG func_8009AA84 (src/BATTLE/BATTLE.PRG/30DB0.c:882)
int Content_ReleaseWeaponModel(int index)
{
    Content_ActorAssetRequest objData;
    int i;
    Actors_WeaponModel* model = Actors_weaponModels[index];

    if ((model == NULL) || (model->boneCount == 0)) {
        return -1;
    }

    if (model->actorId) {
        objData.targetSlot = index;
        objData.modelId = model->modelId;

        if (index >= 4) {
            int shared_model_found;
            Content_ActorAssetRequest* obj = &objData;

            for (i = 0; i < 20; ++i) {
                if ((i != obj->targetSlot) && ((Actors_weaponModels[i] != NULL))
                    && ((Actors_weaponModels[i]->actorId) || (obj->targetSlot < 4))
                    && (Actors_weaponModels[i]->modelId == obj->modelId)) {
                    obj->dataAddress = i;
                    shared_model_found = 1;
                    goto exit;
                }
            }

            shared_model_found = 0;
        exit:
            if (shared_model_found == 0) {
                Core_Memory_FreeFromLowEnd(model->sectionOffsets);
            }
        } else {
            Core_Memory_FreeFromLowEnd(model->sectionOffsets);
        }
    }

    --Render_legacyWeaponTextureSlots[model->textureSlot].referenceCount;

    if (model->isWideTexture) {
        --Render_legacyWeaponTextureSlots[model->textureSlot + 1].referenceCount;
    }

    Actors_weaponModels[index] = NULL;
    return 0;
}

// decomp: BATTLE.PRG func_8009AC24 (src/BATTLE/BATTLE.PRG/30DB0.c:930)
int Content_FindSharedActorModel(Content_ActorAssetRequest* request)
{
    int i;

    for (i = 0; i < 17; ++i) {
        if ((i != request->targetSlot)
            && ((Actors_actorRecords[i] != NULL) && (Actors_actorRecords[i]->modelId == request->modelId))) {
            request->dataAddress = i;
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG _loadShp (src/BATTLE/BATTLE.PRG/30DB0.c:944)
int Content_UpdateActorModelLoad(Content_ActorAssetRequest* request)
{
    int shpData;
    int found;
    int i;
    int shpFileSize;

    switch (Content_legacyModelLoadPhase) {
    case 0:
        if (request->modelId == 0x7F) {
            Content_legacyShpAllocated = 0;
            break;
        }

        for (i = 0; i < 17; ++i) {
            if ((i != request->targetSlot) && ((Actors_actorRecords[i] != NULL))
                && (Actors_actorRecords[i]->modelId == request->modelId)) {
                request->dataAddress = i;
                found = 1;
                goto exit;
            }
        }
        found = 0;
    exit:
        if (found != 0) {
            Content_legacyShpAllocated = 0;
            break;
        }
        Content_legacyShpAllocated = 1;
        // Fallthrough

    default:
        shpFileSize = Content_legacyShpSectorCounts[request->modelId];

        if (shpFileSize == 0) {
            request->modelId = 0;
            shpFileSize = Content_legacyShpSectorCounts[0];
        }

        shpFileSize <<= 0xB;
        Content_legacyShpFileBuffer = Core_Memory_AllocateFromHighEnd(shpFileSize);
        request->dataAddress = (uintptr_t)Content_legacyShpFileBuffer;

        if (Content_legacyShpFileBuffer == NULL) {
            return -2;
        }

        Content_legacyModelLoadPhase = 1;
        Content_legacyShpFileLocation.lba = Content_legacyShpDiscOffsets[request->modelId] + VS_00_SHP_LBA;
        Content_legacyShpFileLocation.size = shpFileSize;
        Content_legacyShpDiscRequest = Content_AllocateDiscRequest(&Content_legacyShpFileLocation);

        Content_EnqueueDiscRequest(Content_legacyShpDiscRequest, (void*)request->dataAddress);
    case 1:
        if (Content_legacyShpDiscRequest->state != 4) {
            return -1;
        }

        Content_ReleaseDiscRequest(Content_legacyShpDiscRequest);
        Content_legacyModelLoadPhase = 2;

        return -1;

    case 2:
        break;
    }

    shpData = Content_ParseActorModel(request);

    if (shpData == -1) {
        return -1;
    }

    if (Content_legacyShpAllocated != 0) {
        Content_legacyShpAllocated = 0;
        Core_Memory_FreeFromHighEnd(Content_legacyShpFileBuffer);
    }

    Content_legacyModelLoadPhase = 0;
    return shpData;
}

// decomp: BATTLE.PRG _parseShp (src/BATTLE/BATTLE.PRG/30DB0.c:1026)
int Content_ParseActorModel(Content_ActorAssetRequest* object)
{
    RECT rect;
    Content_ActorAssetRequest queued;
    Actors_ActorRecord* actor;
    Actors_ActorRecord* other;
    Actors_ShpModel* model;
    Actors_ShpBone* armature;
    u_char* data;
    u_char* pData;
    int size;
    int i;
    int j;
    int n;
    int lo;
    int hi;
    int modelId;
    int vertices;
    int k;
    int corner;
    int x;
    int y;
    u_short* joint;
    Actors_TextureScrollBounds* bounds;
    Actors_TextureScrollState* state;
    int texV;

    if (Content_modelParseResetFlag == 0) {
        if (object->targetSlot == 0 && object->modelId == 6) {
            int originX;
            int originY;

            data = (u_char*)object->dataAddress;
            actor = Actors_actorRecords[0];
            data += 0xF4;
            size = *(int*)data;
            data += size + 4;
            data += 4;
            i = *(int*)data;
            Content_legacyShpCopiedWordCount = actor->instance.copiedWordCount;
            Content_legacyShpTextureWidth = 0x80;
            Content_legacyShpTextureHeight = 0x80;
            data += i + 4;
            data += 8;
            Core_Memory_CopyAlignedWords(actor->palettes, data, Content_legacyShpCopiedWordCount * 4);
            originX = *(short*)(Actors_textureSlotOrigins + actor->textureSlot * 4) << 6;
            rect.x = originX;
            originY = (*(short*)(Actors_textureSlotOrigins + actor->textureSlot * 4 + 2) << 8)
                    + (Actors_textureSlotRowOffsets[actor->textureSlot] << 6);
            rect.y = originY;
            rect.w = Content_legacyShpTextureWidth / 2;
            rect.h = Content_legacyShpTextureHeight;
            LoadImage(&rect, (u_long*)(data + Content_legacyShpCopiedWordCount * 4));
            object->variant = 0;
            goto setupPalettes;
        }

        actor = Actors_actorRecords[object->targetSlot];
        if (actor == NULL) {
            Actors_actorRecords[object->targetSlot] = (Actors_ActorRecord *)(object->destination);
            actor = Actors_actorRecords[object->targetSlot];
            if (actor == NULL) {
                return -2;
            }
            memset(actor, 0, sizeof(Actors_ActorRecord));
            actor->textureSlot = 0xFF;
            actor->instance.attachParent = 0xFF;
        }

        modelId = object->modelId;
        if (modelId == 0x7F) {
            actor->modelId = modelId;
            actor->instance.slot = object->targetSlot;
            actor->occupancyRadius = 0x40;
            actor->occupancyHeight = 10000;
            ((void (*)(...))Actors_ResetActorMotionState)(actor->instance.slot);
            actor->instance.isSkipped = 0;
            Actors_PlaceActorInRoom(actor->instance.slot, (Field_ActorPlacement*)&object->placement, 0, 1);
            Content_modelParseResetFlag = 0;
            return 0;
        }

        if ((u_long)object->dataAddress < 17) {
        copyShared:
            Core_Memory_CopyAlignedWords(actor, Actors_actorRecords[object->dataAddress], sizeof(Actors_ActorRecord));
            if (actor->holdsTextureSlots && actor->textureSlot != 0xFF) {
                for (i = 0; i < actor->instance.textureSlotCountMinusOne + 1; ++i) {
                    ++Render_legacyActorTextureSlots[actor->textureSlot + i].referenceCount;
                }
            }
            *(Field_ActorPlacement*)&actor->instance.currentTileX = object->placement;
            if (actor->textureHeaderBit) {
                Content_legacyShpCopiedWordCount = 0xA0;
            } else {
                Content_legacyShpCopiedWordCount = 0x10;
            }
            goto setupPalettes;
        }
        if (Content_FindSharedActorModel(object)) {
            goto copyShared;
        }
        actor->modelId = object->modelId;
        data = (u_char*)object->dataAddress;
    } else if (Content_modelParseResetFlag == 1) {
        actor = Actors_actorRecords[object->targetSlot];
        goto fixUVs;
    }

    data += 4;
    *(Field_ActorPlacement*)&actor->instance.currentTileX = object->placement;
    actor->instance.isSkipped = 1;
    *(int*)&actor->instance.boneCount = *(int*)data;
    *(int*)&actor->instance.quadCount = *(int*)(data + 4);
    data += 8;
    Core_Memory_CopyAlignedWords(actor->textureOverlays, data, 0xE8);
    data += 0xE8;
    size = *(int*)data;
    data += 4;
    if (size < (int)sizeof(Actors_ShpModel)) {
        return -2;
    }
    model = (Actors_ShpModel *)(Core_Memory_AllocateFromLowEnd(size));
    if (model == NULL) {
        return -2;
    }
    memcpy(model, data, size);
    actor->instance.model = model;
    actor->modelDataSize = (unsigned)size;
    data += size;

    queued.assetKind = 8;
    queued.targetSlot = object->targetSlot;
    queued.dataAddress = (uintptr_t)data;
    queued.modelId = 0xFF;
    queued.subKind = 0;
    Content_ParseActorSequence(&queued);
    data += 4;

    i = *(int*)data;
    data += 4;
    data += i;
    size = *(int*)data;
    actor->holdsTextureSlots = 1;
    data += 4;

    if (size == 4) {
        size = *(int*)data;
        for (i = 0; i < 17; ++i) {
            other = Actors_actorRecords[i];
            if (other != NULL && other->modelId == size) {
                break;
            }
        }
        Content_legacyShpCopiedWordCount = actor->instance.copiedWordCount = other->instance.copiedWordCount;
        Core_Memory_CopyAlignedWords(actor->palettes, other->palettes, Content_legacyShpCopiedWordCount * 4);
        actor->textureSlot = other->textureSlot;
        actor->instance.uvLayout = other->instance.uvLayout;
        actor->holdsTextureSlots = 0;
        actor->textureHeaderBit = 1;
        goto fixUVs;
    }

    Content_legacyShpHeaderWord = *data;
    actor->textureHeaderBit = (u_char)Content_legacyShpHeaderWord;
    actor->instance.uvLayout = Content_legacyShpHeaderWord >> 4;
    data++;
    Content_legacyShpTextureWidth = *data++ * 2;
    Content_legacyShpTextureHeight = *data++ * 2;
    actor->instance.copiedWordCount = Content_legacyShpCopiedWordCount = *data++;

    n = Content_legacyShpTextureHeight / 64;
    if (Content_legacyShpTextureWidth == 0x100) {
        n = 8;
    }
    actor->instance.textureSlotCountMinusOne = n - 1;

    if (object->targetSlot == 1 || object->targetSlot == 16) {
        lo = 12;
        hi = 20;
    } else {
        lo = (n >= 3) * 4;
        hi = 12;
    }

    switch (n) {
    case 1:
    case 2:
        for (i = lo; i < hi; i += n) {
            if (Render_legacyActorTextureSlots[i].referenceCount == 0) {
                break;
            }
        }
        if (i == hi) {
            goto fallback;
        }
        break;
    case 3:
    case 4:
        for (i = lo; i < hi; ++i) {
            for (j = 0; j < n; ++j) {
                if (Render_legacyActorTextureSlots[i + j].referenceCount != 0) {
                    break;
                }
            }
            if (j == n) {
                goto found;
            }
            if (((i + n) & 3) == 0) {
                i += n - 1;
            }
        }
        goto fallback;
    case 8:
        for (i = lo; i < hi; ++i) {
            if (Render_legacyActorTextureSlots[i].referenceCount != 0) {
                break;
            }
        }
        if (i == hi) {
            goto allFree;
        }
    fallback:
        if (Render_ReserveTexturePageRange(31, 1, 4) == 0) {
            actor->textureSlot = 0;
            actor->instance.textureSlotCountMinusOne = 0;
            ++Render_legacyActorTextureSlots[0].referenceCount;
            goto setupPalettes;
        }
        i = 20;
        break;
    allFree:
        i = lo;
        break;
    }
found:
    for (j = 0; j < n; ++j) {
        Render_legacyActorTextureSlots[i + j].referenceCount = 1;
    }
    actor->textureSlot = i;
    if (actor->textureHeaderBit) {
        Render_legacyActorTextureSlots[actor->textureSlot].texturePage |= 0x80;
    } else {
        Render_legacyActorTextureSlots[actor->textureSlot].texturePage &= ~0x180;
    }

    Core_Memory_CopyAlignedWords(actor->palettes, data, Content_legacyShpCopiedWordCount * 4);
    data += Content_legacyShpCopiedWordCount * 4;
    x = *(short*)(Actors_textureSlotOrigins + actor->textureSlot * 4) << 6;
    rect.x = x;
    y = (*(short*)(Actors_textureSlotOrigins + actor->textureSlot * 4 + 2) << 8)
      + (Actors_textureSlotRowOffsets[actor->textureSlot] << 6);
    rect.y = y;
    rect.w = Content_legacyShpTextureWidth / 2;
    rect.h = Content_legacyShpTextureHeight;
    LoadImage(&rect, (u_long*)data);
    data += Content_legacyShpTextureWidth * Content_legacyShpTextureHeight;
    Content_modelParseResetFlag = 1;
    Content_legacyShpParseCursor = data;
    return -1;

fixUVs:
    j = actor->instance.otherPolygonCount + actor->instance.triangleCount + actor->instance.quadCount;
    pData = (u_char *)(VS_ONDISC(actor->instance.model, polygonSection, void *));
    texV = Actors_textureSlotRowOffsets[actor->textureSlot] << 6;

    switch (actor->instance.uvLayout) {
    case 0:
        for (k = 0; k < j; k++) {
            vertices = 4;
            if (*pData == Render_LegacyPrimitiveCode_PolyFt3) {
                pData += 10;
                vertices = 3;
            } else {
                pData += 12;
            }
            for (corner = 0; corner < vertices; pData += 2, ++corner) {
                pData[0] += 0;
                pData[1] += texV;
            }
        }
        break;
    default:
        for (k = 0; k < j; k++) {
            if (pData[11] == 0x34) {
                pData[6] += 0;
                pData[7] += texV;
                pData += 20;
                vertices = 2;
            } else {
                pData += 24;
                vertices = 4;
            }
            for (corner = 0; corner < vertices; pData += 2, ++corner) {
                pData[0] += 0;
                pData[1] += texV;
            }
        }
    }

setupPalettes:
    Core_Memory_CopyAlignedWords(actor->basePalette, (u_short*)actor->palettes + object->variant * 0xA0,
        Content_legacyShpCopiedWordCount * 2);
    Core_Memory_CopyAlignedWords(actor->basePalette + 0xA0, actor->basePalette, Content_legacyShpCopiedWordCount * 2);
    Core_Memory_CopyAlignedWords(actor->currentPalette, actor->basePalette, Content_legacyShpCopiedWordCount * 2);

    if (object->targetSlot == 16) {
        Render_LoadClutColors(actor->basePalette, 0x17, 0, Content_legacyShpCopiedWordCount);
        actor->clutA = Render_legacyActorTextureSlots[1].alternatePalette;
        actor->clutB = Render_legacyActorTextureSlots[1].alternatePalette;
    } else {
        Render_LoadClutColors(actor->basePalette, object->targetSlot + 4, 0, Content_legacyShpCopiedWordCount);
        Render_LoadClutColors(actor->basePalette, object->targetSlot + 0x16, 0, Content_legacyShpCopiedWordCount);
        actor->clutA = Render_legacyActorTextureSlots[object->targetSlot].palette;
        actor->clutB = Render_legacyActorTextureSlots[object->targetSlot].alternatePalette;
    }

    if (object->targetSlot == 0 && object->modelId == 6) {
        return 0;
    }

    actor->instance.slot = object->targetSlot;
    actor->instance.variant = object->variant;
    actor->instance.isParsed = 1;
    actor->instance.actionAnimationState = 0;
    actor->travelAnimation = 13;
    actor->commonSequence = NULL;
    actor->battleSequence = NULL;
    actor->sharedSequences[3] = NULL;
    actor->sequenceFlags = 0;
    actor->commonAnimationCount = 0;
    actor->battleAnimationCount = 0;
    actor->locomotionCounter = 0;
    actor->sequenceModelId = 0;
    actor->animationLength = 0;
    actor->opaque6DC = 0;
    actor->instance.weaponDrawn = 0;
    actor->advanceRequested = 0;
    actor->customTint = 0;
    actor->moveRate = 8;
    actor->turnRate = 0x20;
    actor->instance.scaleX = ONE;
    actor->instance.scaleY = ONE;
    actor->instance.scaleZ = ONE;
    actor->framesPerStep = 1;
    actor->instance.supportLayer = 0;
    actor->instance.placementLayer = 0;
    actor->ignoredByCollision = 0;
    actor->instance.soundEnabled = 1;
    *(int*)&actor->fadeBlue = 0;
    actor->opaque648 = 800;
    *(int*)&actor->instance.baseColor = 0x808080;
    *(int*)&actor->instance.tint = 0x404040;
    actor->scale = Actors_roomActorScale.vx;

    for (i = 0; i < actor->instance.boneCount; ++i) {
        armature = &actor->instance.model->bones[i];
        if (armature->auxiliaryTag >= 3 && armature->auxiliaryTag < 7) {
            joint = (u_short*)&actor->jointSlots[(armature->auxiliaryTag - 3) * 4];
            joint[1] = 0;
            joint[0] = armature->jointInitValue;
        }
    }

    for (i = 0; i < 4; ++i) {
        state = &actor->textureScroll[i];
        bounds = &actor->textureScrollBounds[i];
        state->x = bounds->x;
        state->y = bounds->y;
        state->stepX = bounds->initialStepX;
        state->stepY = bounds->initialStepY;
        state->interval = bounds->interval;
        state->timer = 0;
    }

    actor->sharedSequences[0] = NULL;
    actor->sharedSequences[1] = NULL;
    for (i = 0; i < 3; ++i) {
        if (Actors_sharedSequenceData[i] != NULL) {
            Content_BindSharedActorSequence(actor, i);
        }
    }

    Actors_SetActorPaletteFade(object->targetSlot, 0);
    ((void (*)(...))Actors_ResetActorMotionState)(actor->instance.slot);
    actor->instance.attachBone = 0;
    actor->instance.isAttached = 0;

    if (object->subKind == 0xFF) {
        Actors_PlaceActorInRoom(
            object->targetSlot, (Field_ActorPlacement*)&object->placement, 0, object->material);
        actor->instance.attachParent = 0xFF;
    } else if (object->textureSlot == 0xFC) {
        actor->instance.attachParent = object->subKind;
        actor->instance.attachBone = 0xFC;
        Actors_actorRecords[actor->instance.attachParent]->companionSlot = object->targetSlot;
        Actors_actorRecords[actor->instance.attachParent]->companionState.offsetX = 0;
        Actors_actorRecords[actor->instance.attachParent]->companionState.offsetY = 0;
        Actors_actorRecords[actor->instance.attachParent]->companionState.offsetZ = 0;
        Actors_actorRecords[actor->instance.attachParent]->companionState.depthBias = 8;
    } else {
        actor->instance.attachParent = 0xFF;
        Actors_AttachActorToParentBone(object->targetSlot, object->subKind, object->textureSlot, 0);
    }
    Content_modelParseResetFlag = 0;
    return 0;
}

// decomp: BATTLE.PRG func_8009BD90 (src/BATTLE/BATTLE.PRG/30DB0.c:1433)
int Content_FindSharedActorSequence(Content_ActorAssetRequest* request)
{
    Actors_ActorRecord* candidate;
    int candidate_index;
    int model_id = Actors_actorRecords[request->targetSlot]->modelId;
    for (candidate_index = (request->targetSlot >= 2) * 2; candidate_index < 17; ++candidate_index) {
        if (candidate_index != request->targetSlot) {
            candidate = Actors_actorRecords[candidate_index];
            if ((candidate != NULL) && (candidate->modelId == model_id)) {
                if ((request->modelId == 0 && candidate->commonSequence != 0)
                    || (request->modelId != 0 && candidate->sequenceModelId == request->modelId)) {
                    request->dataAddress = candidate_index;
                    return 1;
                }
            }
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8009BE5C (src/BATTLE/BATTLE.PRG/30DB0.c:1474)
int Content_UpdateActorSequenceLoad(Content_ActorAssetRequest* object)
{
    Content_ActorAssetRequest queued;
    Actors_ActorRecord* actor = Actors_actorRecords[object->targetSlot];
    unsigned sectors;
    char* sequences;
    int result;
    int model;
    switch (Content_legacyModelLoadPhase) {
    case 0:
        switch (object->subKind) {
        case 0:
            if (object->targetSlot != 0 && Content_FindSharedActorSequence(object)) {
                Content_legacySequenceBufferOwned = 0;
                goto parse;
            }
            goto allocate;
        case 1:
        case 2:
            Content_legacySequenceBufferOwned = 0;
            break;
        default:
        allocate:
            Content_legacySequenceBufferOwned = 1;
            break;
        }
        break;
    case 1:
        goto wait;
    case 2:
        goto parse;
    }
    switch (object->subKind) {
    case 0:
        sequences = (char*)actor + 0x5F4;
        sectors = actor->chainAttackAnimationIds[object->modelId] >> 11;
        if (!sectors) {
            return 0;
        }
        Content_legacySequenceFileBuffer = Core_Memory_AllocateFromHighEnd(sectors << 11);
        object->dataAddress = (uintptr_t)Content_legacySequenceFileBuffer;
        if (!Content_legacySequenceFileBuffer) {
            return -2;
        }
        Content_legacySequenceFileLocation.lba = actor->battleSequenceLbas[object->modelId];
        if (object->targetSlot == 0 && (model = object->modelId) != 0) {
            queued.assetKind = 7;
            queued.targetSlot = 0;
            queued.subKind = 3;
            queued.material = model;
            Content_QueueActorAssetRequest(&queued);
        }
        break;
    case 1:
    case 2:
        sectors = Content_legacySequenceSectorCounts[object->modelId];
        if (!sectors) {
            return -2;
        }
        Content_legacySequenceFileBuffer = Core_Memory_AllocateFromLowEnd(sectors << 11);
        object->dataAddress = (uintptr_t)Content_legacySequenceFileBuffer;
        if (!Content_legacySequenceFileBuffer) {
            return -2;
        }
        Content_legacySequenceFileLocation.lba = Content_legacySequenceDiscOffsets[object->modelId] + 0x19797;
        break;
    case 3:
        sectors = Content_legacyMaterialSequenceSectorCounts[(u_char)object->material];
        Content_legacySequenceFileBuffer = Core_Memory_AllocateFromHighEnd(sectors << 11);
        object->dataAddress = (uintptr_t)Content_legacySequenceFileBuffer;
        if (!Content_legacySequenceFileBuffer) {
            return -2;
        }
        Content_legacySequenceFileLocation.lba = Content_legacyMaterialSequenceDiscOffsets[(u_char)object->material] + 0x18043;
        break;
    case 4:
    special:
        sectors = Content_legacyMaterialModelSequenceSectorCounts[(u_char)object->material][object->modelId];
        if (!sectors) {
            sectors = Content_legacyMaterialModelSequenceSectorCounts[2][0];
            object->material = 2;
            object->modelId = 0;
        }
        Content_legacySequenceFileBuffer = Core_Memory_AllocateFromHighEnd(sectors << 11);
        object->dataAddress = (uintptr_t)Content_legacySequenceFileBuffer;
        if (!Content_legacySequenceFileBuffer) {
            return -2;
        }
        Content_legacySequenceFileLocation.lba = Content_legacyMaterialModelSequenceDiscOffsets[(u_char)object->material][object->modelId] + 0x1943E;
        break;
    case 5:
        if (object->modelId == 0) {
            return 0;
        }
        --object->modelId;
        if (actor->modelId == 2) {
            goto special;
        }
        sectors = actor->specialSequenceSizes[object->modelId] >> 11;
        if (!sectors) {
            return -2;
        }
        Content_legacySequenceFileLocation.lba = actor->specialSequenceLbas[object->modelId];
        Content_legacySequenceFileBuffer = Core_Memory_AllocateFromHighEnd(sectors << 11);
        object->dataAddress = (uintptr_t)Content_legacySequenceFileBuffer;
        if (!Content_legacySequenceFileBuffer) {
            return -2;
        }
        break;
    }
    Content_legacySequenceFileLocation.size = sectors << 11;
    Content_legacyModelLoadPhase = 1;
    Content_legacySequenceDiscRequest = Content_AllocateDiscRequest(&Content_legacySequenceFileLocation);
    Content_EnqueueDiscRequest(Content_legacySequenceDiscRequest, (void*)object->dataAddress);
wait:
    if (Content_legacySequenceDiscRequest->state != 4) {
        return -1;
    }
    Content_ReleaseDiscRequest(Content_legacySequenceDiscRequest);
    Content_legacyModelLoadPhase = 2;
    return -1;
parse:
    result = Content_ParseActorSequence(object);
    if (result == -1) {
        return -1;
    }
    if (Content_legacySequenceBufferOwned) {
        Content_legacySequenceBufferOwned = 0;
        Core_Memory_FreeFromHighEnd(Content_legacySequenceFileBuffer);
    }
    Content_legacyModelLoadPhase = 0;
    return result;
}

// decomp: BATTLE.PRG func_8009C378 (src/BATTLE/BATTLE.PRG/30DB0.c:1608)
void Content_CopyWeaponSequence(Content_SequenceBuffer* destination, Content_SequenceBuffer* source)
{
    Core_Memory_CopyAlignedWords(destination, source, 0x7800);
}

// decomp: BATTLE.PRG _loadSeq (src/BATTLE/BATTLE.PRG/30DB0.c:1617)
int Content_ParseActorSequence(Content_ActorAssetRequest* object)
{
    int _[2] __attribute__((unused));
    Actors_ActorRecord* actor;
    Actors_ActorRecord* source;
    Content_SequenceBuffer* seq;
    int size;
    int count;
    int slot;
    int i;

    switch (Content_legacySequenceParsePhase) {
    case 0:
        Content_legacySequenceParseData = (u_char*)object->dataAddress;
        break;
    case 1:
        goto loadEtm;
    }

    if (object->subKind == 0) {
        actor = Actors_actorRecords[object->targetSlot];
        if (object->modelId != 0) {
            if (Content_FindSharedActorSequence(object)) {
                Content_legacySequenceParseData = (u_char*)object->dataAddress;
            }
        }
        if (object->modelId == 0) {
            if ((uintptr_t)Content_legacySequenceParseData < 17) {
                source = Actors_actorRecords[(int)Content_legacySequenceParseData];
                actor->sequenceFlags = source->sequenceFlags;
                actor->commonSequence = source->commonSequence;
                return 0;
            }
            actor->sequenceFlags = *Content_legacySequenceParseData;
            Content_legacySequenceParseData += 4;
            size = *(int*)Content_legacySequenceParseData;
            Content_legacySequenceParseData += 4;
            if (size & 3) {
                size += (4 - size) & 3;
            }
            seq = (Content_SequenceBuffer *)(Core_Memory_AllocateFromLowEnd(size));
            if (seq == NULL) {
                return -2;
            }
            actor->commonSequence = seq;
            goto copy;
        }
        if (object->modelId == 0xFF) {
            Content_legacySequenceParseData += 4;
            size = *(int*)Content_legacySequenceParseData;
            Content_legacySequenceParseData += 4;
            if (size & 3) {
                size += (4 - size) & 3;
            }
            seq = (Content_SequenceBuffer *)(Core_Memory_AllocateFromLowEnd(size));
            if (seq == NULL) {
                return -2;
            }
            actor->ownedSequence = seq;
            goto copy;
        }
        if ((uintptr_t)Content_legacySequenceParseData < 17) {
            source = Actors_actorRecords[(int)Content_legacySequenceParseData];
            actor->commonAnimationCount = source->commonAnimationCount;
            seq = (Content_SequenceBuffer *)(source->battleSequence);
            if (object->targetSlot < 2) {
                if (actor->instance.slot == 0) {
                    seq = (Content_SequenceBuffer *)(Actors_primarySequenceBuffer);
                    goto clone;
                }
                if (Content_legacySequenceParseData == 0) {
                    seq = (Content_SequenceBuffer *)(Core_Memory_AllocateFromLowEnd(0x7800));
                clone:
                    Content_CopyWeaponSequence(seq, (Content_SequenceBuffer *)(source->battleSequence));
                }
            }
            actor->battleSequence = seq;
            actor->sequenceModelId = object->modelId;
            return 0;
        }
        actor->commonAnimationCount = *Content_legacySequenceParseData;
        Content_legacySequenceParseData += 2;
        size = *(u_short*)Content_legacySequenceParseData;
        Content_legacySequenceParseData += 2;
        if (size != 0 && size != actor->instance.boneCount) {
            return 0;
        }
        size = *(int*)Content_legacySequenceParseData;
        Content_legacySequenceParseData += 4;
        if (size & 3) {
            size += (4 - size) & 3;
        }
        if (actor->instance.slot != 0) {
            seq = (Content_SequenceBuffer *)(Core_Memory_AllocateFromLowEnd(size));
            if (seq == NULL) {
                return -2;
            }
        } else {
            seq = (Content_SequenceBuffer *)(Actors_primarySequenceBuffer);
        }
        actor->battleSequence = seq;
        actor->sequenceModelId = object->modelId;
    copy:
        Core_Memory_CopyAlignedWords(seq, Content_legacySequenceParseData, size);
                return 0;
    } else if (object->subKind == 3) {
        Content_legacySequenceParseData += 4;
        size = *(int*)Content_legacySequenceParseData;
        Content_legacySequenceParseData += 4;
        if (size & 3) {
            size += (4 - size) & 3;
        }
        seq = (Content_SequenceBuffer *)(Actors_sequenceWorkBuffer);
        goto copy;
    } else if (object->subKind == 4 || object->subKind == 5) {
        actor = Actors_actorRecords[object->targetSlot];
        actor->battleAnimationCount = *Content_legacySequenceParseData;
        Content_legacySequenceParseData += 4;
        size = *(int*)Content_legacySequenceParseData;
        Content_legacySequenceParseData += 4;
        if (size & 3) {
            size += (4 - size) & 3;
        }
        seq = (Content_SequenceBuffer *)(Core_Memory_AllocateFromLowEnd(size));
        if (seq == NULL) {
            return -2;
        }
        actor->sharedSequences[3] = seq;
        goto copy;
    } else {
        slot = object->targetSlot - 1;
        Actors_sharedSequenceBankTexturePages[slot] = 0;
        if (Actors_sharedSequenceData[slot] != NULL) {
            return -2;
        }
        Actors_sharedSequenceData[slot] = Content_legacySequenceParseData;
        count = *(int*)Content_legacySequenceParseData;
        Content_legacySequenceParseData += 4;
        for (i = 0; i < count; ++i) {
            Actors_sharedSequenceAnimationIds[slot][i] = *(u_short*)Content_legacySequenceParseData;
            Content_legacySequenceParseData += 4;
        }
        for (i = 0; i < count; ++i) {
            Actors_sharedSequenceEntryParams[slot][i] = *(u_short*)Content_legacySequenceParseData;
            Content_legacySequenceParseData += 4;
        }
        Actors_sharedSequenceAnimationIds[slot][i] = -1;
        for (i = 0; i < count; ++i) {
            Actors_sharedSequenceAnimationData[slot][i] = (char*)Content_legacySequenceParseData
                + (count - i) * sizeof(u_int) + *(u_int*)Content_legacySequenceParseData;
            Content_legacySequenceParseData += 4;
        }
        for (i = 0; i < count; ++i) {
            seq = (Content_SequenceBuffer*)(Actors_sharedSequenceAnimationData[slot][i] + 8);
                        }
        Actors_sharedSequenceAnimationData[slot][i] = (char*)-1;
        for (i = 0; i < 17; ++i) {
            if (Actors_actorRecords[i] != NULL) {
                Content_BindSharedActorSequence(Actors_actorRecords[i], slot);
            }
        }
        if (object->subKind == 1) {
            return 0;
        }
        Content_legacySequenceParsePhase = 1;
    loadEtm:
        if (Content_UpdateSequenceEffectTextureLoad(object) == -1) {
            return -1;
        }
        Content_legacySequenceParsePhase = 0;
        return 0;
    }
}

// decomp: BATTLE.PRG func_8009CAEC (src/BATTLE/BATTLE.PRG/30DB0.c:1795)
void Content_BindSharedActorSequence(Actors_ActorRecord* actor, int bank_index)
{
    int i = 0;
    int id;
    char* p;

    while (1) {
        if (Actors_sharedSequenceAnimationIds[bank_index][i] == -1) {
            goto tail;
        }
        if (Actors_sharedSequenceAnimationIds[bank_index][i] == Actors_sharedSequenceIdsByKind[actor->modelId]) {
            p = Actors_sharedSequenceAnimationData[bank_index][i];
            p += 8;
            break;
        }
        if (Actors_sharedSequenceAnimationIds[bank_index][i] == actor->modelId) {
            p = Actors_sharedSequenceAnimationData[bank_index][i];
            p += 8;
            break;
        }
        ++i;
    }
    actor->sharedSequences[bank_index] = p;
tail:
    for (i = 0; (id = Actors_sharedSequenceAnimationIds[bank_index][i]) != -1; ++i) {
        if (id == actor->modelId) {
            actor->textureRowOffsets[bank_index] = Actors_sharedSequenceEntryParams[bank_index][i];
            return;
        }
    }
    actor->textureRowOffsets[bank_index] = 0xFF;
}

// decomp: BATTLE.PRG func_8009CC20 (src/BATTLE/BATTLE.PRG/30DB0.c:1828)
int Content_ReleaseActorSequence(int target_id, int sequence_kind)
{
    Content_ActorAssetRequest objectData;
    Actors_ActorRecord* actor;
    int i;

    switch (sequence_kind) {
    case 0:
        if (target_id >= 17) {
            return -1;
        }
        actor = Actors_actorRecords[target_id];
        if (actor == NULL) {
            return -1;
        }
        if (actor->battleSequence == NULL) {
            return 0;
        }
        if (target_id != 0) {
            objectData.targetSlot = target_id;
            objectData.modelId = actor->sequenceModelId;
            if (target_id == 1 && actor->modelId == 0) {
                Core_Memory_FreeFromLowEnd(actor->battleSequence);
            } else if (!Content_FindSharedActorSequence(&objectData)) {
                Core_Memory_FreeFromLowEnd(actor->battleSequence);
            }
        }
        actor->battleSequence = NULL;
        actor->sequenceModelId = 0;
        actor->commonAnimationCount = 0;
        break;
    case 1:
    case 2:
        --target_id;
        for (i = 0; i < 17; ++i) {
            actor = Actors_actorRecords[i];
            if (actor != NULL) {
                actor->sharedSequences[target_id] = NULL;
            }
        }
        i = Content_ReleaseSequenceEffectTexture(target_id + 1);
        if (i < 0) {
            return i;
        }
        if (Actors_sharedSequenceData[target_id] != NULL) {
            Core_Memory_FreeFromLowEnd(Actors_sharedSequenceData[target_id]);
        }
        Actors_sharedSequenceData[target_id] = NULL;
        break;
    case 4:
    case 5:
        actor = Actors_actorRecords[target_id];
        if (actor == NULL) {
            return -1;
        }
        if (actor->sharedSequences[3] == NULL) {
            return 0;
        }
        Core_Memory_FreeFromLowEnd(actor->sharedSequences[3]);
        actor->sharedSequences[3] = NULL;
        actor->battleAnimationCount = 0;
        break;
    }
    return 0;
}

// decomp: BATTLE.PRG func_8009CFB0 (src/BATTLE/BATTLE.PRG/30DB0.c:1920)
int Content_ReleaseActorModel(int actor_index)
{
    Content_ActorAssetRequest sharing_request;
    Actors_ActorRecord* actor;
    int i;

    if (actor_index >= 0x11) {
        return -1;
    }

    actor = Actors_actorRecords[actor_index];

    if (actor == NULL) {
        return -1;
    }

    if (actor->modelId != 0x7F) {
        Content_ActorAssetRequest* request;
        int shared_model_found;
        sharing_request.targetSlot = actor_index;
        sharing_request.modelId = actor->modelId;
        request = &sharing_request;

        for (i = 0; i < 17; ++i) {
            if ((i != request->targetSlot) && ((Actors_actorRecords[i] != NULL))
                && (Actors_actorRecords[i]->modelId == request->modelId)) {
                request->dataAddress = i;
                shared_model_found = 1;
                goto exit;
            }
        }

        if (i >= 17) {
            shared_model_found = 0;
        }
    exit:
        if (shared_model_found == 0) {
            Audio_StopBankSfx(0x180, (int *)(VS_ONDISC(actor->instance.model, soundSection, void *)), 0);
            Core_Memory_FreeFromLowEnd(actor->instance.model);
            Core_Memory_FreeFromLowEnd(actor->ownedSequence);
        }

        if (actor->commonSequence != 0) {
            for (i = 0; i < 17; ++i) {
                if ((i != actor_index) && ((Actors_actorRecords[i] != NULL))
                    && (Actors_actorRecords[i]->commonSequence == actor->commonSequence)) {
                    goto exit2;
                }
            }
            if (i >= 17) {
                Core_Memory_FreeFromLowEnd(actor->commonSequence);
            }
        }
    exit2:

        Content_ReleaseActorSequence(actor_index, 0);

        if (actor_index < 10) {
            Content_ReleaseWeaponModel(actor_index * 2);
            Content_ReleaseWeaponModel(actor_index * 2 | 1);
        }

        if (actor->holdsTextureSlots) {
            for (i = 0; i < (actor->instance.textureSlotCountMinusOne + 1); ++i) {
                --Render_legacyActorTextureSlots[actor->textureSlot + i].referenceCount;
            }

            if (actor->textureSlot == 20) {
                Render_RetireTexturePageRange(31, 1, 4);
            }
        }
    }
    Actors_actorRecords[actor_index] = NULL;
    return 0;
}

// decomp: BATTLE.PRG func_8009D208 (src/BATTLE/BATTLE.PRG/30DB0.c:1996)
int Content_ReleaseSequenceEffectTexture(int bank_id)
{
    if (bank_id < 1 || bank_id > 3) return -1;
    if (Actors_sharedSequenceBankTexturePages[bank_id - 1] != 0) {
        Actors_ResetActorTextureFrames(bank_id - 1);
        if (Render_RetireTexturePageRange(Actors_sharedSequenceBankTexturePages[bank_id - 1], 1, 4) == 0) {
            return -3;
        }
        Actors_sharedSequenceBankTexturePages[bank_id - 1] = 0;
    }
    return 0;
}

// decomp: BATTLE.PRG _loadEtm (src/BATTLE/BATTLE.PRG/30DB0.c:2008)
int Content_UpdateSequenceEffectTextureLoad(Content_ActorAssetRequest* objdata)
{
    RECT rect;
    int fileSize;
    int sectorSize;
    int h;

    if (objdata->targetSlot < 1 || objdata->targetSlot > 3) return -2;
    switch (Content_legacyEtmLoadPhase) {
    default:
        Content_legacyEtmLoadPhase = 1;
        sectorSize = Content_legacyEtmSectorCounts[objdata->modelId];
        if (sectorSize == 0) {
            Content_legacyEtmLoadPhase = 0;
            return -2;
        } else {
            fileSize = sectorSize << 0xB;
            Content_legacyEtmFileBuffer = (u_short *)(Core_Memory_AllocateFromHighEnd(fileSize));
            if (Content_legacyEtmFileBuffer == NULL) {
                return -2;
            }

            Content_legacyEtmFileLocation.lba = Content_legacyEtmDiscOffsets[objdata->modelId] + VS_E001_ETM_LBA;
            Content_legacyEtmFileLocation.size = fileSize;
            Content_legacyEtmDiscRequest = Content_AllocateDiscRequest(&Content_legacyEtmFileLocation);
            Content_EnqueueDiscRequest(Content_legacyEtmDiscRequest, Content_legacyEtmFileBuffer);
        }
        // fallthrough
    case 1:
        if (Content_legacyEtmDiscRequest->state != 4) {
            return -1;
        }

        Content_ReleaseDiscRequest(Content_legacyEtmDiscRequest);

        if (Actors_sharedSequenceBankTexturePages[objdata->targetSlot - 1] != 0) {
            if (Render_RetireTexturePageRange(Actors_sharedSequenceBankTexturePages[objdata->targetSlot - 1], 1, 4) == 0) {
                return -3;
            }
            Actors_sharedSequenceBankTexturePages[objdata->targetSlot - 1] = 0;
        }

        if (Render_ReserveTexturePageRange(objdata->textureSlot, 1, 4) == 0) {
            return -2;
        }

        Actors_sharedSequenceBankTexturePages[objdata->targetSlot - 1] = objdata->textureSlot;
        ++Content_legacyEtmFileBuffer;
        h = *Content_legacyEtmFileBuffer;
        ++Content_legacyEtmFileBuffer;
        setRECT(&rect, (objdata->textureSlot & 0xF) << 6, 256, 64, h);
        LoadImage(&rect, (u_long *)((void*)Content_legacyEtmFileBuffer));
        Content_legacyEtmLoadPhase = 2;
        return -1;

    case 2:
        Core_Memory_FreeFromHighEnd(Content_legacyEtmFileBuffer - 2);
        Content_legacyEtmLoadPhase = 0;
        return 0;
    }
}

}  // extern "C"
