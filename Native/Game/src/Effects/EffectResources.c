// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/SoundBanks.h"
#include "Audio/StreamPlayback.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Math/Interpolation.h"
#include "Core/Memory/Heap.h"
#include "Core/Overlays.h"
#include "DiscQueue.h"
#include "Effects/EffectResources.h"
#include "Effects/Loading.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg005.h"
#include "Effects/Plugins/Plg052.h"
#include "Effects/Plugins/Plg128.h"
#include "Effects/Plugins/Plg162.h"
#include "Effects/Plugins/Plg300.h"
#include "Effects/PurgeTables.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Templates/Grid.h"
#include "Effects/Templates/Mesh10.h"
#include "Effects/Templates/MirrorSurface.h"
#include "Effects/Templates/ParticleDispatch.h"
#include "Effects/Templates/RadialSurge.h"
#include "Effects/Templates/Ring.h"
#include "Effects/Templates/RotatingSurface.h"
#include "Effects/Templates/Surface.h"
#include "Effects/Templates/TiledRing.h"
#include "Effects/Templates/TrailDispatch.h"
#include "Effects/Templates/TrailNineDispatch.h"
#include "Effects/Transforms.h"
#include "OverlayWorkspace.h"
#include "Render/Textures.h"

// decomp: D_800F5630 (D_800F5630@BATTLE/BATTLE.PRG)
int Effects_textureLoadIndex;

// decomp: D_800F5638 (D_800F5638@BATTLE/BATTLE.PRG)
Effects_TextureLoadList Effects_textureLoads;

// decomp: vs_battle_pfileBuf (vs_battle_pfileBuf@BATTLE/BATTLE.PRG)
void* Effects_packageBuffer;

// decomp: D_800F5680 (D_800F5680@BATTLE/BATTLE.PRG)
u_int Effects_textureLoadState;

// decomp: D_800F5684 (D_800F5684@BATTLE/BATTLE.PRG)
int Effects_purgeOverlayLoaded;

// decomp: vs_battle_pFileLoadContext (vs_battle_pFileLoadContext@BATTLE/BATTLE.PRG)
Effects_FileLoadContext Effects_packageFile;

// decomp: D_800F568C (D_800F568C@BATTLE/BATTLE.PRG)
Content_DiscQueueSlot* Effects_resourceReadSlot;

// decomp: _plgLoadState (_plgLoadState@BATTLE/BATTLE.PRG)
u_int Effects_pluginLoadState;

// decomp: _plgLoadContext (_plgLoadContext@BATTLE/BATTLE.PRG)
Effects_FileLoadContext Effects_pluginFile;

// decomp: D_800F56A0 (D_800F56A0@BATTLE/BATTLE.PRG)
u_long* Effects_textureLoadBuffer;

// decomp: _plgEntryPointCount (_plgEntryPointCount@BATTLE/BATTLE.PRG)
int Effects_pluginEntryPointCount;

// decomp: D_800F5874 (D_800F5874@BATTLE/BATTLE.PRG)
int Effects_loadedTextureSlots;

// decomp: BATTLE.PRG func_800D7814 (src/BATTLE/BATTLE.PRG/6F014.c:446)
void Effects_InitializeEffectResources(void)
{
    Effects_resourceReadSlot = NULL;
    Effects_packageBuffer = NULL;
    Effects_textureLoadBuffer = NULL;
    Effects_loadedTextureSlots = 0;
    Effects_purgeOverlayLoaded = 0;
    Effects_secondaryPackage.curveBlock = NULL;
    Effects_secondaryPackage.request = &Effects_secondaryRequest;
    Effects_actionRequestPointer = &Effects_actionRequest;
    Effects_SelectEffectResourceContext(0);
    Effects_BindEffectPackageBlocks((Effects_PackageFile*)Effects_emptyPackage);
}

// decomp: BATTLE.PRG func_800D7890 (src/BATTLE/BATTLE.PRG/6F014.c:460)
void Effects_SelectEffectResourceContext(int arg0)
{
    if (arg0 == 0) {
        Effects_package = &Effects_secondaryPackage;
    } else {
        Effects_package = &Effects_actionPackage;
    }
}

// decomp: BATTLE.PRG func_800D78B8 (src/BATTLE/BATTLE.PRG/6F014.c:471)
void Effects_SaveEffectResourceContext(void) { Effects_savedPackage = Effects_package; }

// decomp: BATTLE.PRG func_800D78CC (src/BATTLE/BATTLE.PRG/6F014.c:473)
void Effects_RestoreEffectResourceContext(void) { Effects_package = Effects_savedPackage; }

// decomp: BATTLE.PRG _parsePfileBlock5 (src/BATTLE/BATTLE.PRG/6F014.c:475)
void Effects_BindParticleDefinitions(Effects_PackageBlock5* block) { Effects_package->block5Data = block; }

// decomp: BATTLE.PRG func_800D78F0 (src/BATTLE/BATTLE.PRG/6F014.c:477)
void Effects_UpdateParticleGravityAndDrag(void)
{
    VECTOR sp10;
    int sp20;

    Core_Math_InterpolatePairsToWideVector(
        Effects_package->block9Data->gravityRange, Effects_curveSamples[Effects_package->block9Data->gravityCurve], &sp10);
    sp20 = Core_Math_InterpolateShortPair(
        Effects_package->block9Data->dragRange, Effects_curveSamples[Effects_package->block9Data->dragCurve]);
    ((void (*)(VECTOR*))Effects_SetParticleGravity)(&sp10);
    Effects_SetParticleDrag(&sp20);
}

// decomp: BATTLE.PRG _parsePfileBlock3 (src/BATTLE/BATTLE.PRG/6F014.c:490)
void Effects_BindEffectTimelines(void* block) { Effects_package->block3Data = block; }

// decomp: BATTLE.PRG _parsePfileCurveBlock (src/BATTLE/BATTLE.PRG/6F014.c:492)
void Effects_BindEffectCurves(Effects_CurveBlock* block)
{
    u_char* p;
    int i;

    Effects_package->curveBlock = block;
    p = (u_char*)block + block->dataOffset;

    for (i = 0; i < block->count; i++) {
        Effects_package->curves[i] = p;
        p += block->curveSizes[i];
    }
}

// decomp: BATTLE.PRG _parsePfileBlock8 (src/BATTLE/BATTLE.PRG/6F014.c:506)
void Effects_BindEffectPrograms(void* block) { Effects_package->block8Data = block; }

// decomp: BATTLE.PRG _parsePfileBlock9 (src/BATTLE/BATTLE.PRG/6F014.c:508)
void Effects_BindEffectPackageParameters(Effects_PackageBlock9* arg0) { Effects_package->block9Data = arg0; }

// decomp: BATTLE.PRG _parsePfileBlock10 (src/BATTLE/BATTLE.PRG/6F014.c:510)
void Effects_BindEffectTransformDefinitions(void* arg0) { Effects_package->block10Data = arg0; }

// decomp: BATTLE.PRG _parsePfileBlock1 (src/BATTLE/BATTLE.PRG/6F014.c:512)
void Effects_BindEffectTextureTables(Effects_PackageBlock1* block)
{
    int i;

    Effects_package->block1TableCount = block->tableCount;
    Effects_package->textures = (void*)block + block->dataOffset;

    for (i = 0; i < Effects_package->block1TableCount; ++i) {
        Effects_package->textureInfoTable[i] = (void*)block + block->tableOffsets[i];
    }
}

// decomp: BATTLE.PRG _parsePfileBlock2 (src/BATTLE/BATTLE.PRG/6F014.c:524)
void Effects_BindParticleAnimationTable(Effects_PackageBlock2* block)
{
    Effects_package->block2Count = block->animationCount;
    Effects_package->block2Data = (void*)block;
}

// decomp: BATTLE.PRG func_800D7A90 (src/BATTLE/BATTLE.PRG/6F014.c:530)
void* Effects_ParticleAnimationData(int arg0)
{
    return (u_char*)Effects_package->block2Data + Effects_package->block2Data[arg0 + 2];
}

// decomp: BATTLE.PRG _parsePfileBlock11 (src/BATTLE/BATTLE.PRG/6F014.c:535)
void Effects_BindEffectSoundSequences(u_char* arg0) { Effects_package->block11Data = arg0; }

// decomp: BATTLE.PRG _parsePfileBlock12 (src/BATTLE/BATTLE.PRG/6F014.c:537)
void Effects_BindEffectSoundBank(void* arg0)
{
    Effects_package->block12Data = arg0;
    Audio_BindRoomSfxBank((uintptr_t)arg0);
}

// decomp: BATTLE.PRG _parsePfileBlock13 (src/BATTLE/BATTLE.PRG/6F014.c:543)
void Effects_BindEffectAuxiliarySoundBank(void* arg0, size_t byte_count)
{
    Effects_package->block13Data = arg0;
    Effects_package->auxiliarySoundHandle = Audio_BindRoomCutsceneStream(arg0, byte_count);
}

// decomp: BATTLE.PRG _loadfile (src/BATTLE/BATTLE.PRG/6F014.c:549)
int Effects_PollEffectResourceRead(int lba, int size, void* buf)
{
    Content_DiscFile cdFile;

    if (Effects_resourceReadSlot == NULL) {
        cdFile.lba = lba;
        cdFile.size = size;
        Effects_resourceReadSlot = Content_AllocateDiscRequest(&cdFile);
        Content_EnqueueDiscRequest(Effects_resourceReadSlot, buf);
        return 1;
    }

    if (Effects_resourceReadSlot->state == Content_DiscRequestState_Loaded) {
        Content_ReleaseDiscRequest(Effects_resourceReadSlot);
        Effects_resourceReadSlot = NULL;
        return 0;
    }

    return 1;
}

// decomp: BATTLE.PRG vs_battle_loadEffPurge (src/BATTLE/BATTLE.PRG/6F014.c:570)
int Effects_LoadEffectPurgeOverlay(void)
{
    if (Effects_purgeOverlayLoaded == 0) {
        Content_AcquireOverlayWorkspace(2);
        Effects_purgeOverlayLoaded = 1;
    }

    return Effects_PollEffectResourceRead(VS_EFFPURGE_BIN_LBA, VS_EFFPURGE_BIN_SIZE, Core_overlaySlots[3]);
}

// decomp: BATTLE.PRG func_800D7BF8 (src/BATTLE/BATTLE.PRG/6F014.c:580)
int Effects_LoadEffectPackage(void)
{
    int _[16];

    if (Effects_packageBuffer == NULL) {
        Effects_packageBuffer = Core_Memory_AllocateFromHighEnd(Effects_packageFile.size);
    }

    return Effects_PollEffectResourceRead(Effects_packageFile.lba + VS_E000_P_LBA,
        Effects_packageFile.size, Effects_packageBuffer);
}

// decomp: BATTLE.PRG func_800D7C5C (src/BATTLE/BATTLE.PRG/6F014.c:592)
void Effects_ReleaseEffectPackageResources(void)
{
    int i;
    int bit;
    int flags;

    for (i = 24; i < 32; i++) {
        flags = Effects_loadedTextureSlots;
        bit = 1 << i;
        if (flags & bit) {
            Render_RetireTexturePageRange(i, 1, 2);
            Effects_loadedTextureSlots &= ~bit;
        }
    }
    if (Effects_packageBuffer != NULL) {
        Core_Memory_FreeFromHighEnd(Effects_packageBuffer);
        Effects_packageBuffer = NULL;
    }
}

// decomp: BATTLE.PRG func_800D7CFC (src/BATTLE/BATTLE.PRG/6F014.c:612)
int Effects_UpdateEffectTextureLoading(void)
{
    int _[16] __attribute__((unused));
    RECT sp50;
    u_int temp_s1;
    int var_s0;

    if ((Effects_textureLoadState == 0) && (Effects_textureLoads.assetCount != 0)) {
        Effects_textureLoadIndex = 0;
        Effects_SetEffectTextureLoadState(1);
    }

    temp_s1 = Effects_textureLoadState;

    switch (temp_s1) {
    case 1:
        if (Effects_textureLoadBuffer == NULL) {
            // FBT sizes are all equal
            Effects_textureLoadBuffer = Core_Memory_AllocateFromHighEnd(VS_E000_0_FBT_SIZE);
        }

        var_s0 = Effects_PollEffectResourceRead(Effects_textureLoads.loadContexts[Effects_textureLoadIndex].lba + VS_E000_P_LBA,
            Effects_textureLoads.loadContexts[Effects_textureLoadIndex].size, Effects_textureLoadBuffer);

        if (var_s0 == 0) {
            void* temp_v0_3 = (Effects_textureLoadIndex * 2) + &Effects_textureLoads;
            int slot = Effects_textureLoads.slots[Effects_textureLoadIndex];

            if (Effects_textureLoads.isClut[Effects_textureLoadIndex] == 0) {
                slot += 24;
                Render_ReserveTexturePageRange(slot, 1, 2);
                Effects_loadedTextureSlots |= temp_s1 << slot;
                sp50.x = (slot - (slot / 16 * 16)) << 6;
                sp50.y = slot / 16 << 8;
                sp50.w = 64;
                sp50.h = 256;
                LoadImage(&sp50, Effects_textureLoadBuffer);
            } else {
                sp50.x = 768;
                sp50.y = slot + 240;
                sp50.w = 256;
                sp50.h = 5 - slot;
                LoadImage(&sp50, Effects_textureLoadBuffer);
            }

            ++Effects_textureLoadIndex;

            if (Effects_textureLoads.assetCount == Effects_textureLoadIndex) {
                Effects_SetEffectTextureLoadState(2);
            }

            var_s0 = 1;
        }
        break;

    case 2:
        if (Effects_textureLoadBuffer != NULL) {
            Core_Memory_FreeFromHighEnd(Effects_textureLoadBuffer);
            Effects_textureLoadBuffer = NULL;
        }
        /* fallthrough */

    case 0:
        var_s0 = 0;
        break;
    }
    return var_s0;
}

// decomp: BATTLE.PRG func_800D7EF4 (src/BATTLE/BATTLE.PRG/6F014.c:681)
int Effects_UpdateEffectPluginLoading(void)
{
    int _[16];
    int ret;
    u_int state = Effects_pluginLoadState;

    switch (state) {
    case 1:
        if (Effects_purgeOverlayLoaded == 0) {
            Content_AcquireOverlayWorkspace(2);
            Effects_purgeOverlayLoaded = state;
        }
        // Fallthrough

    case 2:
        ret = Effects_PollEffectResourceRead(Effects_pluginFile.lba + VS_E000_P_LBA, Effects_pluginFile.size,
            Core_overlaySlots[3]);
        if (ret == 0) {
            Effects_SetEffectPluginLoadState(3);
        }
        break;
    case 0:
    case 3:
        ret = 0;
        break;
    }

    return ret;
}

// decomp: BATTLE.PRG func_800D7FB4 (src/BATTLE/BATTLE.PRG/6F014.c:712)
void Effects_ConfigureEffectPackageLoad(int lba, int size)
{
    Effects_packageFile.lba = lba;
    Effects_packageFile.size = size;
}

// decomp: BATTLE.PRG vs_battle_configurePlg (src/BATTLE/BATTLE.PRG/6F014.c:718)
void Effects_ConfigureEffectPluginLoad(int lba, int size, int entryPointCount)
{
    Effects_pluginFile.lba = lba;
    Effects_pluginFile.size = size;
    Effects_pluginEntryPointCount = entryPointCount;
}

// decomp: BATTLE.PRG vs_battle_setEffectExec (src/BATTLE/BATTLE.PRG/6F014.c:725)
void Effects_SetEffectRenderer(Effects_EffectRenderer func, int index)
{
    Effects_effectRenderers[index] = func;
}

// decomp: BATTLE.PRG func_800D7FFC (src/BATTLE/BATTLE.PRG/6F014.c:730)
void Effects_SetEffectTextureAssetCount(int arg0) { Effects_textureLoads.assetCount = arg0; }

// decomp: BATTLE.PRG vs_battle_configureEffectFbLoad (src/BATTLE/BATTLE.PRG/6F014.c:732)
void Effects_ConfigureEffectTextureAsset(int lba, int size, int slot, int isClut, int index)
{
    Effects_textureLoads.loadContexts[index].lba = lba;
    Effects_textureLoads.loadContexts[index].size = size;
    Effects_textureLoads.slots[index] = slot;
    Effects_textureLoads.isClut[index] = isClut;
}

// decomp: BATTLE.PRG func_800D8038 (src/BATTLE/BATTLE.PRG/6F014.c:740)
void Effects_SetEffectTextureLoadState(int arg0) { Effects_textureLoadState = arg0; }

// decomp: BATTLE.PRG func_800D8044 (src/BATTLE/BATTLE.PRG/6F014.c:742)
u_int Effects_EffectTextureLoadState(void) { return Effects_textureLoadState; }

// decomp: BATTLE.PRG vs_battle_setPlgLoadState (src/BATTLE/BATTLE.PRG/6F014.c:744)
void Effects_SetEffectPluginLoadState(int arg0) { Effects_pluginLoadState = arg0; }

// decomp: BATTLE.PRG func_800D8060 (src/BATTLE/BATTLE.PRG/6F014.c:746)
void Effects_BindEffectPackageBlocks(Effects_PackageFile* arg0)
{
    while (1) {
        int meta = arg0->meta;

        switch (meta & 0xFFFF0000) {
        case 0:
            return;

        case 0x10000:
            Effects_BindEffectTextureTables((Effects_PackageBlock1*)arg0->blocks);
            break;

        case 0x20000:
            Effects_BindParticleAnimationTable((Effects_PackageBlock2*)arg0->blocks);
            break;

        case 0x30000:
            Effects_BindEffectTimelines(arg0->blocks);
            break;

        case 0x40000:
            Effects_BindEffectCurves((Effects_CurveBlock*)arg0->blocks);
            break;

        case 0x50000:
            Effects_BindParticleDefinitions((Effects_PackageBlock5*)arg0->blocks);
            break;

        case 0x80000:
            Effects_BindEffectPrograms(arg0->blocks);
            break;

        case 0x90000:
            Effects_BindEffectPackageParameters((Effects_PackageBlock9*)arg0->blocks);
            break;

        case 0xA0000:
            Effects_BindEffectTransformDefinitions(arg0->blocks);
            break;

        case 0xB0000:
            Effects_BindEffectSoundSequences(arg0->blocks);
            break;

        case 0xC0000:
            Effects_BindEffectSoundBank(arg0->blocks);
            break;

        case 0xD0000:
            Effects_BindEffectAuxiliarySoundBank(
                arg0->blocks, (meta & 0xFFFF) - sizeof(u_int));
            break;
        }

        arg0 = (void*)arg0 + (meta & 0xFFFF);
    }
}

// decomp: EFFPURGE.BIN func_800F9800 (src/EFFECT/EFFPURGE.BIN/0.c:590)
int Effects_InvokeAllPluginUpdates(Effects_RendererSlot* arg0, u_int arg1, int arg2)
{
    Effects_Plg005_UpdateAnalyzeGrid(NULL, 2, 0);
    Effects_Plg044_TiledRing(NULL, 2, 0);
    Effects_Plg045_ParticleDispatch(NULL, 2, 0);
    Effects_Plg047_ClosedRing(NULL, 2, 0);
    Effects_Plg044_TiledRing(NULL, 2, 0);
    Effects_Plg049_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg049_Ring(NULL, 2, 0);
    Effects_Plg049_TiledRing(NULL, 2, 0);
    Effects_Plg049_RotatingSurface(NULL, 2, 0);
    Effects_Plg050_Ring(NULL, 2, 0);
    Effects_Plg050_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg051_ClosedRing(NULL, 2, 0);
    Effects_Plg052_UpdateRepeatingRing(NULL, 2, 0);
    Effects_Plg053_Grid(NULL, 2, 0);
    Effects_Plg053_ClosedRing(NULL, 2, 0);
    Effects_Plg005_UpdateAnalyzeGrid(NULL, 2, 0);
    Effects_Plg053_ClosedRing(NULL, 2, 0);
    Effects_Plg005_UpdateAnalyzeGrid(NULL, 2, 0);
    Effects_Plg053_ClosedRing(NULL, 2, 0);
    Effects_Plg053_Grid(NULL, 2, 0);
    Effects_Plg053_ClosedRing(NULL, 2, 0);
    Effects_Plg057_ParticleDispatch(NULL, 2, 0);
    Effects_Plg057_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg057_TrailDispatch(NULL, 2, 0);
    Effects_Plg057_ParticleDispatch(NULL, 2, 0);
    Effects_Plg057_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg057_TrailDispatch(NULL, 2, 0);
    Effects_Plg057_ParticleDispatch(NULL, 2, 0);
    Effects_Plg057_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg057_TrailDispatch(NULL, 2, 0);
    Effects_Plg057_ParticleDispatch(NULL, 2, 0);
    Effects_Plg057_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg057_TrailDispatch(NULL, 2, 0);
    Effects_Plg061_Surface(NULL, 2, 0);
    Effects_Plg061_Grid(NULL, 2, 0);
    Effects_Plg061_Ring(NULL, 2, 0);
    Effects_Plg061_ClosedRing(NULL, 2, 0);
    Effects_Plg061_Surface(NULL, 2, 0);
    Effects_Plg061_Grid(NULL, 2, 0);
    Effects_Plg061_Ring(NULL, 2, 0);
    Effects_Plg061_ClosedRing(NULL, 2, 0);
    Effects_Plg061_Surface(NULL, 2, 0);
    Effects_Plg061_Grid(NULL, 2, 0);
    Effects_Plg061_Ring(NULL, 2, 0);
    Effects_Plg061_ClosedRing(NULL, 2, 0);
    Effects_Plg061_Surface(NULL, 2, 0);
    Effects_Plg061_Grid(NULL, 2, 0);
    Effects_Plg061_Ring(NULL, 2, 0);
    Effects_Plg061_ClosedRing(NULL, 2, 0);
    Effects_Plg065_RotatingSurface(NULL, 2, 0);
    Effects_Plg065_ClosedRing(NULL, 2, 0);
    Effects_Plg065_Ring(NULL, 2, 0);
    Effects_Plg065_ParticleDispatch(NULL, 2, 0);
    Effects_Plg065_RotatingSurface(NULL, 2, 0);
    Effects_Plg065_ClosedRing(NULL, 2, 0);
    Effects_Plg065_Ring(NULL, 2, 0);
    Effects_Plg065_ParticleDispatch(NULL, 2, 0);
    Effects_Plg065_RotatingSurface(NULL, 2, 0);
    Effects_Plg065_ClosedRing(NULL, 2, 0);
    Effects_Plg065_Ring(NULL, 2, 0);
    Effects_Plg065_ParticleDispatch(NULL, 2, 0);
    Effects_Plg065_RotatingSurface(NULL, 2, 0);
    Effects_Plg065_ClosedRing(NULL, 2, 0);
    Effects_Plg065_Ring(NULL, 2, 0);
    Effects_Plg065_ParticleDispatch(NULL, 2, 0);
    Effects_Plg069_Mesh10(NULL, 2, 0);
    Effects_Plg069_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg069_TiledRing(NULL, 2, 0);
    Effects_Plg069_Mesh10(NULL, 2, 0);
    Effects_Plg069_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg069_TiledRing(NULL, 2, 0);
    Effects_Plg069_Mesh10(NULL, 2, 0);
    Effects_Plg069_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg069_TiledRing(NULL, 2, 0);
    Effects_Plg069_Mesh10(NULL, 2, 0);
    Effects_Plg069_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg069_TiledRing(NULL, 2, 0);
    Effects_Plg073_TiledRing(NULL, 2, 0);
    Effects_Plg073_Ring(NULL, 2, 0);
    Effects_Plg073_TrailDispatch(NULL, 2, 0);
    Effects_Plg073_RotatingSurface(NULL, 2, 0);
    Effects_Plg073_Grid(NULL, 2, 0);
    Effects_Plg073_TiledRing(NULL, 2, 0);
    Effects_Plg073_Ring(NULL, 2, 0);
    Effects_Plg074_UpdateTrailEmitter(NULL, 2, 0);
    Effects_Plg073_RotatingSurface(NULL, 2, 0);
    Effects_Plg005_UpdateAnalyzeGrid(NULL, 2, 0);
    Effects_Plg073_TiledRing(NULL, 2, 0);
    Effects_Plg073_Ring(NULL, 2, 0);
    Effects_Plg073_TrailDispatch(NULL, 2, 0);
    Effects_Plg073_RotatingSurface(NULL, 2, 0);
    Effects_Plg073_Grid(NULL, 2, 0);
    Effects_Plg073_TiledRing(NULL, 2, 0);
    Effects_Plg073_Ring(NULL, 2, 0);
    Effects_Plg073_TrailDispatch(NULL, 2, 0);
    Effects_Plg073_RotatingSurface(NULL, 2, 0);
    Effects_Plg073_Grid(NULL, 2, 0);
    Effects_Plg077_Mesh10(NULL, 2, 0);
    Effects_Plg052_UpdateRepeatingRing(NULL, 2, 0);
    Effects_Plg077_Grid(NULL, 2, 0);
    Effects_Plg077_Surface(NULL, 2, 0);
    Effects_Plg053_ClosedRing(NULL, 2, 0);
    Effects_Plg077_Mesh10(NULL, 2, 0);
    Effects_Plg052_UpdateRepeatingRing(NULL, 2, 0);
    Effects_Plg077_Grid(NULL, 2, 0);
    Effects_Plg077_Surface(NULL, 2, 0);
    Effects_Plg053_ClosedRing(NULL, 2, 0);
    Effects_Plg077_Mesh10(NULL, 2, 0);
    Effects_Plg052_UpdateRepeatingRing(NULL, 2, 0);
    Effects_Plg077_Grid(NULL, 2, 0);
    Effects_Plg077_Surface(NULL, 2, 0);
    Effects_Plg053_ClosedRing(NULL, 2, 0);
    Effects_Plg077_Mesh10(NULL, 2, 0);
    Effects_Plg052_UpdateRepeatingRing(NULL, 2, 0);
    Effects_Plg077_Grid(NULL, 2, 0);
    Effects_Plg077_Surface(NULL, 2, 0);
    Effects_Plg053_ClosedRing(NULL, 2, 0);
    Effects_Plg081_RotatingSurface(NULL, 2, 0);
    Effects_Plg081_Ring(NULL, 2, 0);
    Effects_Plg081_ClosedRing(NULL, 2, 0);
    Effects_Plg082_ParticleDispatch(NULL, 2, 0);
    Effects_Plg082_Grid(NULL, 2, 0);
    Effects_Plg082_ClosedRing(NULL, 2, 0);
    Effects_Plg044_TiledRing(NULL, 2, 0);
    Effects_Plg053_Grid(NULL, 2, 0);
    Effects_Plg053_ClosedRing(NULL, 2, 0);
    Effects_Plg085_Grid(NULL, 2, 0);
    Effects_Plg085_TiledRing(NULL, 2, 0);
    Effects_Plg086_ClosedRing(NULL, 2, 0);
    Effects_Plg086_RotatingSurface(NULL, 2, 0);
    Effects_Plg086_Grid(NULL, 2, 0);
    Effects_Plg086_TrailDispatch(NULL, 2, 0);
    Effects_Plg087_Surface(NULL, 2, 0);
    Effects_Plg087_ClosedRing(NULL, 2, 0);
    Effects_Plg087_Grid(NULL, 2, 0);
    Effects_Plg087_TrailDispatch(NULL, 2, 0);
    Effects_Plg087_ParticleDispatch(NULL, 2, 0);
    Effects_Plg045_ParticleDispatch(NULL, 2, 0);
    Effects_Plg114_ParticleDispatch(NULL, 2, 0);
    Effects_Plg114_ParticleDispatch2(NULL, 2, 0);
    Effects_Plg128_Surface(NULL, 2, 0);
    Effects_Plg128_ClosedRing(NULL, 2, 0);
    Effects_Plg128_RotatingSurface(NULL, 2, 0);
    Effects_Plg128_Grid(NULL, 2, 0);
    Effects_Plg128_Surface2(NULL, 2, 0);
    Effects_Plg128_UpdateEffectCamera(NULL, 2, 0);
    Effects_Plg128_TrailDispatch(NULL, 2, 0);
    Effects_Plg129_Ring(NULL, 2, 0);
    Effects_Plg129_Mesh10(NULL, 2, 0);
    Effects_Plg129_Surface(NULL, 2, 0);
    Effects_Plg129_Ring2(NULL, 2, 0);
    Effects_Plg129_RotatingSurface(NULL, 2, 0);
    Effects_Plg044_TiledRing(NULL, 2, 0);
    Effects_Plg044_TiledRing(NULL, 2, 0);
    Effects_Plg134_TrailDispatch(NULL, 2, 0);
    Effects_Plg134_ClosedRing(NULL, 2, 0);
    Effects_Plg135_ClosedRing(NULL, 2, 0);
    Effects_Plg135_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg053_Grid(NULL, 2, 0);
    Effects_Plg053_ClosedRing(NULL, 2, 0);
    Effects_Plg137_Mesh10(NULL, 2, 0);
    Effects_Plg137_TrailDispatch(NULL, 2, 0);
    Effects_Plg137_Grid(NULL, 2, 0);
    Effects_Plg137_TiledRing(NULL, 2, 0);
    Effects_Plg138_RotatingSurface(NULL, 2, 0);
    Effects_Plg138_Ring(NULL, 2, 0);
    Effects_Plg138_Grid(NULL, 2, 0);
    Effects_Plg138_TiledRing(NULL, 2, 0);
    Effects_Plg139_TiledRing(NULL, 2, 0);
    Effects_Plg139_Grid(NULL, 2, 0);
    Effects_Plg045_ParticleDispatch(NULL, 2, 0);
    Effects_Plg141_TrailDispatch(NULL, 2, 0);
    Effects_Plg142_Grid(NULL, 2, 0);
    Effects_Plg142_ClosedRing(NULL, 2, 0);
    Effects_Plg142_Ring(NULL, 2, 0);
    Effects_Plg142_RotatingSurface(NULL, 2, 0);
    Effects_Plg143_TiledRing(NULL, 2, 0);
    Effects_Plg143_ClosedRing(NULL, 2, 0);
    Effects_Plg044_TiledRing(NULL, 2, 0);
    Effects_Plg044_TiledRing(NULL, 2, 0);
    Effects_Plg146_TiledRing(NULL, 2, 0);
    Effects_Plg146_MirrorSurface(NULL, 2, 0);
    Effects_Plg146_ParticleDispatch(NULL, 2, 0);
    Effects_Plg146_Ring(NULL, 2, 0);
    Effects_Plg146_TrailDispatch(NULL, 2, 0);
    Effects_Plg147_Ring(NULL, 2, 0);
    Effects_Plg147_ParticleDispatch(NULL, 2, 0);
    Effects_Plg148_TrailDispatch(NULL, 2, 0);
    Effects_Plg148_TiledRing(NULL, 2, 0);
    Effects_Plg149_Ring(NULL, 2, 0);
    Effects_Plg150_TiledRing(NULL, 2, 0);
    Effects_Plg150_Grid(NULL, 2, 0);
    Effects_Plg150_Surface(NULL, 2, 0);
    Effects_Plg150_Ring(NULL, 2, 0);
    Effects_Plg047_ClosedRing(NULL, 2, 0);
    Effects_Plg152_Grid(NULL, 2, 0);
    Effects_Plg152_ParticleDispatch(NULL, 2, 0);
    Effects_Plg152_ClosedRing(NULL, 2, 0);
    Effects_Plg153_Mesh10(NULL, 2, 0);
    Effects_Plg153_Ring(NULL, 2, 0);
    Effects_Plg153_ParticleDispatch(NULL, 2, 0);
    Effects_Plg154_Mesh10(NULL, 2, 0);
    Effects_Plg154_ClosedRing(NULL, 2, 0);
    Effects_Plg139_TiledRing(NULL, 2, 0);
    Effects_Plg139_Grid(NULL, 2, 0);
    Effects_Plg156_ParticleDispatch(NULL, 2, 0);
    Effects_Plg156_Grid(NULL, 2, 0);
    Effects_Plg149_Ring(NULL, 2, 0);
    Effects_Plg158_Surface(NULL, 2, 0);
    Effects_Plg158_MirrorSurface(NULL, 2, 0);
    Effects_Plg158_ClosedRing(NULL, 2, 0);
    Effects_Plg158_RotatingSurface(NULL, 2, 0);
    Effects_Plg139_TiledRing(NULL, 2, 0);
    Effects_Plg139_Grid(NULL, 2, 0);
    Effects_Plg160_TiledRing(NULL, 2, 0);
    Effects_Plg160_Ring(NULL, 2, 0);
    Effects_Plg161_RotatingSurface(NULL, 2, 0);
    Effects_Plg161_Ring(NULL, 2, 0);
    Effects_Plg161_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg161_Grid(NULL, 2, 0);
    Effects_Plg161_Ring2(NULL, 2, 0);
    Effects_Plg162_UpdateTransposedRing(NULL, 2, 0);
    Effects_Plg162_RotatingSurface(NULL, 2, 0);
    Effects_Plg162_Ring(NULL, 2, 0);
    Effects_Plg162_ClosedRing(NULL, 2, 0);
    Effects_Plg045_ParticleDispatch(NULL, 2, 0);
    Effects_Plg165_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg165_RotatingSurface(NULL, 2, 0);
    Effects_Plg165_Grid(NULL, 2, 0);
    Effects_Plg165_ClosedRing(NULL, 2, 0);
    Effects_Plg166_RotatingSurface(NULL, 2, 0);
    Effects_Plg166_MirrorSurface(NULL, 2, 0);
    Effects_Plg166_ClosedRing(NULL, 2, 0);
    Effects_Plg166_Ring(NULL, 2, 0);
    Effects_Plg167_ClosedRing(NULL, 2, 0);
    Effects_Plg167_RotatingSurface(NULL, 2, 0);
    Effects_Plg167_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg168_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg168_ParticleDispatch(NULL, 2, 0);
    Effects_Plg168_Grid(NULL, 2, 0);
    Effects_Plg169_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg169_Ring(NULL, 2, 0);
    Effects_Plg169_ClosedRing(NULL, 2, 0);
    Effects_Plg170_MirrorSurface(NULL, 2, 0);
    Effects_Plg170_RotatingSurface(NULL, 2, 0);
    Effects_Plg170_Ring(NULL, 2, 0);
    Effects_Plg170_Grid(NULL, 2, 0);
    Effects_Plg170_Surface(NULL, 2, 0);
    Effects_Plg170_ClosedRing(NULL, 2, 0);
    Effects_Plg198_Ring(NULL, 2, 0);
    Effects_Plg198_Ring(NULL, 2, 0);
    Effects_Plg139_TiledRing(NULL, 2, 0);
    Effects_Plg139_Grid(NULL, 2, 0);
    Effects_Plg224_ParticleDispatch(NULL, 2, 0);
    Effects_Plg232_TrailNineDispatch(NULL, 2, 0);
    Effects_Plg236_ClosedRing(NULL, 2, 0);
    Effects_Plg236_RotatingSurface(NULL, 2, 0);
    Effects_Plg162_UpdateTransposedRing(NULL, 2, 0);
    Effects_Plg236_Ring(NULL, 2, 0);
    Effects_Plg224_ParticleDispatch(NULL, 2, 0);
    Effects_Plg224_ParticleDispatch(NULL, 2, 0);
    Effects_Plg242_TiledRing(NULL, 2, 0);
    Effects_Plg242_Grid(NULL, 2, 0);
    Effects_Plg242_TrailDispatch(NULL, 2, 0);
    Effects_Plg141_TrailDispatch(NULL, 2, 0);
    Effects_Plg051_ClosedRing(NULL, 2, 0);
    Effects_Plg246_RotatingSurface(NULL, 2, 0);
    Effects_Plg081_RotatingSurface(NULL, 2, 0);
    Effects_Plg081_Ring(NULL, 2, 0);
    Effects_Plg081_ClosedRing(NULL, 2, 0);
    Effects_Plg047_ClosedRing(NULL, 2, 0);
    Effects_Plg250_RotatingSurface(NULL, 2, 0);
    Effects_Plg250_ClosedRing(NULL, 2, 0);
    Effects_Plg047_ClosedRing(NULL, 2, 0);
    Effects_Plg255_TrailDispatch(NULL, 2, 0);
    Effects_Plg255_ParticleDispatch(NULL, 2, 0);
    Effects_Plg047_ClosedRing(NULL, 2, 0);
    Effects_Plg051_ClosedRing(NULL, 2, 0);
    Effects_Plg260_Mesh10(NULL, 2, 0);
    Effects_Plg263_Surface(NULL, 2, 0);
    Effects_Plg263_Ring(NULL, 2, 0);
    Effects_Plg162_UpdateTransposedRing(NULL, 2, 0);
    Effects_Plg263_ParticleDispatch(NULL, 2, 0);
    Effects_Plg265_TiledRing(NULL, 2, 0);
    Effects_Plg265_ParticleDispatch(NULL, 2, 0);
    Effects_Plg265_TrailDispatch(NULL, 2, 0);
    Effects_Plg265_ParticleDispatch2(NULL, 2, 0);
    Effects_Plg266_ParticleDispatch(NULL, 2, 0);
    Effects_Plg266_ParticleDispatch2(NULL, 2, 0);
    Effects_Plg266_TrailDispatch(NULL, 2, 0);
    Effects_Plg141_TrailDispatch(NULL, 2, 0);
    Effects_Plg255_TrailDispatch(NULL, 2, 0);
    Effects_Plg255_ParticleDispatch(NULL, 2, 0);
    Effects_Plg272_Grid(NULL, 2, 0);
    Effects_Plg272_ClosedRing(NULL, 2, 0);
    Effects_Plg272_Surface(NULL, 2, 0);
    Effects_Plg273_MirrorSurface(NULL, 2, 0);
    Effects_Plg082_ParticleDispatch(NULL, 2, 0);
    Effects_Plg082_Grid(NULL, 2, 0);
    Effects_Plg082_ClosedRing(NULL, 2, 0);
    Effects_Plg045_ParticleDispatch(NULL, 2, 0);
    Effects_Plg291_TrailDispatch(NULL, 2, 0);
    Effects_Plg291_Grid(NULL, 2, 0);
    Effects_Plg291_ClosedRing(NULL, 2, 0);
    Effects_Plg297_MirrorSurface(NULL, 2, 0);
    Effects_Plg297_ClosedRing(NULL, 2, 0);
    Effects_Plg297_Grid(NULL, 2, 0);
    Effects_Plg297_Ring(NULL, 2, 0);
    Effects_Plg300_MirrorSurface(NULL, 2, 0);
    Effects_Plg300_ParticleDispatch(NULL, 2, 0);
    Effects_Plg052_UpdateRepeatingRing(NULL, 2, 0);
    Effects_Plg300_UpdateForegroundRepeatingRing(NULL, 2, 0);
    Effects_Plg301_ParticleDispatch(NULL, 2, 0);
    return Effects_Plg301_ClosedRing(NULL, 2, 0);
}

// decomp: EFFPURGE.BIN vs_effpurge_exec (src/EFFECT/EFFPURGE.BIN/0.c:911)
void Effects_ConfigureEffectResources(void)
{
    Effects_SetEffectTextureLoadState(0);
    Effects_ConfigureTextureLoads();
    Effects_ConfigurePluginLoad();
    Effects_packageFile = Effects_packageFiles[Effects_actionRequest.effectId];
    Effects_AcquireParticlePool(255);
    Effects_package->block9Data = NULL;
    Effects_actionPackage.curveBlock = NULL;
    Effects_actionPackage.block11Data = NULL;
    Effects_actionPackage.block12Data = NULL;
    Effects_actionPackage.block13Data = NULL;
}

// decomp: EFFPURGE.BIN _configureFb (src/EFFECT/EFFPURGE.BIN/0.c:925)
void Effects_ConfigureTextureLoads(void)
{
    int i;
    int texCount;
    u_char* a2;

    int new_var = Effects_effectUsesTextures[Effects_actionRequest.effectId];
    int var_s2 = 0;

    for (i = 0; i < Effects_actionRequest.effectId; ++i) {
        var_s2 += Effects_fbtCounts[i];
    }

    texCount = Effects_fbtCounts[Effects_actionRequest.effectId];

    Effects_SetEffectTextureAssetCount(texCount + (new_var != 0));

    a2 = &Effects_fbcSlots[Effects_actionRequest.effectId];

    Effects_ConfigureEffectTextureAsset(Effects_fbcFiles[Effects_actionRequest.effectId].lba,
        Effects_fbcFiles[Effects_actionRequest.effectId].size, *a2, 1, 0);

    for (i = 0; i < texCount; ++i) {
        Effects_ConfigureEffectTextureAsset(Effects_fbtFiles[var_s2 + i].lba,
            Effects_fbtFiles[var_s2 + i].size, Effects_fbtSlots[var_s2 + i], 0, i + 1);
    }
}

// decomp: EFFPURGE.BIN _configurePlg (src/EFFECT/EFFPURGE.BIN/0.c:953)
void Effects_ConfigurePluginLoad(void)
{
    int i;
    int firstindex = Effects_effectEntryPointIndices[Effects_actionRequest.effectId];
    int count = Effects_effectEntryPointIndices[Effects_actionRequest.effectId + 1] - firstindex;

    Effects_ConfigureEffectPluginLoad(
        Effects_pluginFiles[Effects_actionRequest.effectId].lba, Effects_pluginFiles[Effects_actionRequest.effectId].size, count);

    for (i = 0; i < count; ++i) {
        Effects_SetEffectRenderer(Effects_effectEntryPoints[firstindex + i], i);
    }

    if (count != 0) {
        Effects_SetEffectPluginLoadState(1);
    } else {
        Effects_SetEffectPluginLoadState(0);
    }
}

