// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/SoundBanks.h"
#include "Audio/Stream.h"
#include "Boot/Lifecycle.h"
#include "CdDrive.h"
#include "Core/Memory/Heap.h"
#include "DiscQueue.h"
#include "Effects/Camera.h"
#include "Effects/Curves.h"
#include "Effects/EffectResources.h"
#include "Effects/Loading.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/ScriptVm.h"
#include "Effects/Transforms.h"
#include "Input/Controller.h"
#include "OverlayFiles.h"
#include "OverlayWorkspace.h"

// decomp: D_800EC2E4 (D_800EC2E4@BATTLE/BATTLE.PRG)
u_char Effects_effectBufferIndex; /* contents: module image (ModuleData.c) */

// decomp: D_800EC324 (D_800EC324@BATTLE/BATTLE.PRG)
Effects_EffectRenderer Effects_scriptRenderers[2]; /* contents: module image, relocated (ModulePointers.c) */

// decomp: D_800EC32C (D_800EC32C@BATTLE/BATTLE.PRG)
char Effects_actionVariantValues[21824]; /* contents: module image (ModuleData.c) */

// decomp: D_800EC3F4 (D_800EC3F4@BATTLE/BATTLE.PRG)
int (*Effects_scriptOpcodeHandlers[49])(void*); /* contents: module image, relocated (ModulePointers.c) */

// decomp: D_800EC4B8 (D_800EC4B8@BATTLE/BATTLE.PRG)
u_char Effects_lastReactionSoundVariant; /* contents: module image (ModuleData.c) */

// decomp: D_800EC4BC (D_800EC4BC@BATTLE/BATTLE.PRG)
u_int Effects_emptyPackage[0x5200 / sizeof(u_int)]; /* contents: module image (ModuleData.c) */

// decomp: D_800F185C (D_800F185C@BATTLE/BATTLE.PRG)
int Effects_tintedActor;

// decomp: D_800F1868 (D_800F1868@BATTLE/BATTLE.PRG)
int Effects_effectBusyFlags;

// decomp: D_800F5218 (D_800F5218@BATTLE/BATTLE.PRG)
Content_DiscQueueSlot* Effects_menuReloadSlot;

// decomp: D_800F521C (D_800F521C@BATTLE/BATTLE.PRG)
u_int Effects_pendingTracks;

// decomp: D_800F5224 (D_800F5224@BATTLE/BATTLE.PRG)
int Effects_trackCount;

// decomp: D_800F5228 (D_800F5228@BATTLE/BATTLE.PRG)
int Effects_startedTrackCount;

// decomp: D_800F522C (D_800F522C@BATTLE/BATTLE.PRG)
u_char Effects_actionVariant;

// decomp: D_800F5230 (D_800F5230@BATTLE/BATTLE.PRG)
Effects_ActionEffectRequest Effects_actionRequest;

// decomp: D_800F5310 (D_800F5310@BATTLE/BATTLE.PRG)
SVECTOR Effects_effectOrigin[109];

// decomp: D_800F5318 (D_800F5318@BATTLE/BATTLE.PRG)
char Effects_primaryToken;

// decomp: D_800F531C (D_800F531C@BATTLE/BATTLE.PRG)
u_int Effects_primaryPhase;

// decomp: D_800F5320 (D_800F5320@BATTLE/BATTLE.PRG)
int Effects_tokenBitmaps[215];

// decomp: D_800F5330 (D_800F5330@BATTLE/BATTLE.PRG)
int Effects_curveSamples[211];

// decomp: D_800F53B4 (D_800F53B4@BATTLE/BATTLE.PRG)
int Effects_unusedPrimaryWord;

// decomp: D_800F53B8 (D_800F53B8@BATTLE/BATTLE.PRG)
Effects_EffectInstance* Effects_primaryInstances;

// decomp: D_800F53BC (D_800F53BC@BATTLE/BATTLE.PRG)
Effects_EffectInstance* Effects_currentInstance;

// decomp: D_800F53C0 (D_800F53C0@BATTLE/BATTLE.PRG)
Effects_ActionEffectRequest Effects_secondaryRequest;

// decomp: D_800F54A8 (D_800F54A8@BATTLE/BATTLE.PRG)
char Effects_primaryLoaded;

// decomp: D_800F54A9 (D_800F54A9@BATTLE/BATTLE.PRG)
char Effects_actionVariantArgument;

// decomp: D_800F54B0 (D_800F54B0@BATTLE/BATTLE.PRG)
Effects_EffectInstance* Effects_secondaryInstances;

// decomp: D_800F5610 (D_800F5610@BATTLE/BATTLE.PRG)
int Effects_scriptSelector;

// decomp: D_800F5698 (D_800F5698@BATTLE/BATTLE.PRG)
Effects_EffectPackage* Effects_savedPackage;

// decomp: D_800F569C (D_800F569C@BATTLE/BATTLE.PRG)
Effects_EffectPackage* Effects_package;

// decomp: D_800F56A8 (D_800F56A8@BATTLE/BATTLE.PRG)
Effects_EffectRenderer Effects_rendererTableAlias[];

// decomp: vs_battle_effectRenderers (vs_battle_effectRenderers@BATTLE/BATTLE.PRG)
Effects_EffectRenderer Effects_effectRenderers[8];

// decomp: D_800F56C8 (D_800F56C8@BATTLE/BATTLE.PRG)
Effects_EffectPackage Effects_actionPackage;

// decomp: D_800F5798 (D_800F5798@BATTLE/BATTLE.PRG)
Effects_ActionEffectRequest* Effects_actionRequestPointer;

// decomp: D_800F57A0 (D_800F57A0@BATTLE/BATTLE.PRG)
Effects_EffectPackage Effects_secondaryPackage;

// decomp: BATTLE.PRG func_800CE644 (src/BATTLE/BATTLE.PRG/5BF94.c:5045)
int Effects_IgnoreRuntimeDiagnostic(int arg0 __attribute__((unused))) { }

// decomp: BATTLE.PRG func_800CE64C (src/BATTLE/BATTLE.PRG/5BF94.c:5047)
void Effects_InitEffectRuntime(void)
{
    Effects_ResetEffectInstances();
    Effects_InitializeEffectResources();
    Effects_ResetParticlePoolReferences();
}

// decomp: BATTLE.PRG func_800CE67C (src/BATTLE/BATTLE.PRG/5BF94.c:5054)
void Effects_ResetEffectInstances(void)
{
    int i;
    Effects_SetPrimaryEffectPhase(0);
    Effects_startedTrackCount = 0;
    Effects_trackCount = 0;
    Effects_primaryLoaded = 0;
    Effects_primaryToken = 0;
    Effects_secondaryInstances = NULL;
    Effects_currentInstance = 0;
    Effects_unusedPrimaryWord = 0;
    Effects_primaryInstances = 0;
    Effects_curveSamples[0] = 0;
    Effects_actionVariant = 0;
    Effects_menuReloadSlot = NULL;
    for (i = 0; i < 4; ++i) {
        Effects_tokenBitmaps[i] = 0;
    }
}

// decomp: BATTLE.PRG func_800CE714 (src/BATTLE/BATTLE.PRG/5BF94.c:5074)
void Effects_InitEffectInstance(Effects_EffectSpawnRequest* arg0, Effects_EffectInstance* arg1)
{
    int i;

    arg1->parent = arg0->parent;
    arg1->particleCount = 0;
    arg1->priority = arg0->priority;
    arg1->scriptOffset = arg0->scriptOffset;
    arg1->script = arg0->script;
    *(int*)&arg1->registers = 0;
    arg1->token = arg0->token;
    arg1->particles = 0;
    arg1->childCount = 0;
    arg1->frame = 0;

    for (i = 0; i < 16; ++i) {
        ((short*)&arg1->transforms[i].reserved4)[1] = 0;
        arg1->transforms[i].reserved4 = 0;
        arg1->transforms[i].reserved0 = 0;
        arg1->transforms[i].reservedC = 0;
        arg1->transforms[i].reserved8 = 0;
        arg1->transforms[i].reserved18 = 0;
        arg1->transforms[i].reserved14 = 0;
        arg1->transforms[i].reserved10 = 0;
        arg1->transforms[i].reserved30 = 0;
        arg1->transforms[i].reserved2C = 0;
        arg1->transforms[i].reserved28 = 0;
        *(int*)&arg1->transforms[i].actor = 0;
    }

    arg1->context.source = *arg0->source;
    arg1->context.emitFrom = *arg0->emitFrom;
    arg1->context.target = *arg0->target;
    arg1->context.emitTo = *arg0->emitTo;

    arg1->context.timeline = 0;
    arg1->context.renderer = 0;
    arg1->context.parameter = 0;
}

// decomp: BATTLE.PRG func_800CE83C (src/BATTLE/BATTLE.PRG/5BF94.c:5114)
Effects_EffectInstance* Effects_CreatePrimaryEffectInstance(Effects_EffectSpawnRequest* arg0)
{
    Effects_EffectInstance* currentNode;
    Effects_EffectInstance* newNode = Core_Memory_AllocateFromHighEnd(sizeof *newNode);

    Effects_InitEffectInstance(arg0, newNode);

    currentNode = Effects_primaryInstances;

    if ((currentNode == NULL) || (arg0->priority < currentNode->priority)) {
        newNode->next = currentNode;
        Effects_primaryInstances = newNode;
    } else {
        while (1) {
            if ((currentNode->next == NULL) || (arg0->priority < currentNode->next->priority)) {
                newNode->next = currentNode->next;
                currentNode->next = newNode;
                break;
            }
            currentNode = currentNode->next;
        }
    }
    return newNode;
}

// decomp: BATTLE.PRG func_800CE8F4 (src/BATTLE/BATTLE.PRG/5BF94.c:5139)
void Effects_DestroyPrimaryEffectInstance(Effects_EffectInstance* arg0)
{
    Effects_RendererSlot* var_s1;
    Effects_EffectInstance* var_v1 = Effects_primaryInstances;

    if (var_v1 == arg0) {
        Effects_primaryInstances = arg0->next;
    } else {
        while (var_v1->next != NULL) {
            if (var_v1->next == arg0) {
                var_v1->next = arg0->next;
                break;
            }
            var_v1 = var_v1->next;
        }
    }

    Effects_ReleaseAllEffectParticles(arg0);

    var_s1 = arg0->context.renderer;

    while (var_s1 != NULL) {
        Effects_RendererSlot* temp_s0 = var_s1->next;
        var_s1->renderer(var_s1, 4, 0);
        Effects_DetachEffectRenderer(arg0, var_s1);
        var_s1 = temp_s0;
    }

    Core_Memory_FreeFromHighEnd(arg0);
}

// decomp: BATTLE.PRG func_800CE9B0 (src/BATTLE/BATTLE.PRG/5BF94.c:5170)
int Effects_UpdatePrimaryEffects(void)
{
    int _[10] __attribute__((unused));
    Content_DiscFile sp38;
    Effects_EffectInstance* temp_s1;
    Effects_EffectInstance* var_s0;
    int i;
    int var_s2;

    var_s0 = Effects_primaryInstances;
    var_s2 = 1;

    switch (Effects_primaryPhase) {
    case 0:
        var_s2 = 0;
        break;

    case 6:
        if (Effects_LoadEffectPurgeOverlay() == 0) {
            Boot_WaitForOverlayReady();
            Effects_ConfigureEffectResources();
            if ((Effects_actionRequest.effectId != 0) && (Effects_packageFile.lba == 0)) {
                Effects_SetPrimaryEffectPhase(4);
                Effects_trackCount = 0;
                Effects_startedTrackCount = 0;
            } else {
                Effects_SetPrimaryEffectPhase(2);
            }
            if (Effects_purgeOverlayLoaded != 0) {
                Content_ReleaseOverlayWorkspace(2);
                Effects_purgeOverlayLoaded = 0;
            }
        }
        break;

    case 2:
        if (Effects_UpdateEffectTextureLoading() == 0) {
            Effects_SetPrimaryEffectPhase(8);
        }
        break;

    case 8:
        if (Effects_UpdateEffectPluginLoading() == 0) {
            Effects_SetPrimaryEffectPhase(3);
            Boot_WaitForOverlayReady();
        }
        break;

    case 3:
        if (Effects_LoadEffectPackage() == 0) {
            ((void (*)(void*))Effects_BindEffectPackageBlocks)(Effects_packageBuffer);
            Effects_SetPrimaryEffectPhase(5);

            Effects_trackCount = Effects_package->block9Data->frameCount;
            Effects_startedTrackCount = 0;

            for (i = 0; i < Effects_trackCount; ++i) {
                Effects_pendingTracks |= 1 << i;
            }

            Effects_primaryLoaded = 1;
            sp38.lba = Content_MainMenuOverlayDiscSector();
            sp38.size = 0;
            Effects_menuReloadSlot = Content_AllocateDiscRequest(&sp38);
            Content_EnqueueDiscRequest(Effects_menuReloadSlot, NULL);
        }
        break;

    case 4:
        while (var_s0 != NULL) {

            temp_s1 = var_s0->next;
            Effects_currentInstance = var_s0;

            if (Effects_package->curveBlock != NULL) {
                Effects_SampleAllEffectCurves(var_s0->frame);
            }

            Effects_UpdateParticleGravityAndDrag();

            if (Effects_RunEffectScript(var_s0) == 0) {
                Effects_DestroyPrimaryEffectInstance(var_s0);
            } else {
                ++var_s0->frame;
            }

            var_s0 = temp_s1;
        }

        if (Effects_primaryInstances != NULL) {
            break;
        }

        if (Effects_trackCount <= Effects_startedTrackCount) {

            if (Effects_package->block12Data != 0) {
                Audio_UnbindRoomSfxBank();
                Effects_package->block12Data = NULL;
            }

            if (Effects_package->block13Data != NULL) {
                Audio_UnbindRoomCutsceneStream();
                Effects_package->block13Data = NULL;
                Effects_package->auxiliarySoundHandle = 0;
            }

            Input_StopVibrationPatterns(0, 1);
            Effects_ReleaseEffectPackageResources();
            Effects_ReleaseParticlePool();
            Effects_ReleaseEffectToken(Effects_primaryToken);

            Effects_primaryLoaded = 0;
            Effects_primaryToken = 0;

            if (Effects_purgeOverlayLoaded != 0) {
                Content_ReleaseOverlayWorkspace(2);
                Effects_purgeOverlayLoaded = 0;
            }

            Effects_ResetEffectCameraMotion();
            Effects_SetPrimaryEffectPhase(9);
        } else {
            Effects_SetPrimaryEffectPhase(5);
        }

        var_s2 = 1;
        break;

    case 9:
        if (Effects_menuReloadSlot != NULL) {
            if (Effects_menuReloadSlot->state != 4) {
                // Ugly match hack
                if (Effects_package || var_s2) {
                    var_s2 = 1;
                    break;
                } else {
                    var_s2 = 1;
                    break;
                }
            }
            Content_ReleaseDiscRequest(Effects_menuReloadSlot);
            Effects_menuReloadSlot = NULL;
        }
        Effects_SetPrimaryEffectPhase(0);
        var_s2 = 0;
        break;
    }

    return var_s2;
}

// decomp: BATTLE.PRG func_800CED60 (src/BATTLE/BATTLE.PRG/5BF94.c:5321)
int Effects_UpdateSecondaryEffects(void)
{
    Effects_EffectInstance* temp_s4;
    Effects_EffectInstance* var_s1;
    Effects_RendererSlot* var_s0;
    int var_s3;

    var_s1 = Effects_secondaryInstances;
    var_s3 = 0;

    while (var_s1 != NULL) {

        Effects_currentInstance = var_s1;
        temp_s4 = var_s1->next;

        switch (var_s1->context.phase) {
        case 2:
            break;
        case 3:
            Effects_AcquireParticlePool(0xFF);
            Effects_SetSecondaryEffectPhase(4, var_s1);
            break;
        case 4:
            if (Effects_package->curveBlock != NULL) {
                Effects_SampleAllEffectCurves(var_s1->frame);
            }

            Effects_UpdateParticleGravityAndDrag();

            var_s0 = var_s1->context.renderer;

            while (var_s0 != NULL) {
                Effects_RendererSlot* temp_s2 = var_s0->next;
                if (var_s0->renderer(var_s0, 2, 0) == 0) {
                    Effects_DetachEffectRenderer(var_s1, var_s0);
                }
                var_s0 = temp_s2;
            }
            if (Effects_RunEffectScript(var_s1) == 0) {
                Effects_DestroySecondaryEffectInstance(var_s1);
                Effects_ReleaseParticlePool();
            } else {
                ++var_s1->frame;
            }
            break;
        }
        var_s3 |= var_s1->busyBits;
        var_s1 = temp_s4;
    }
    return var_s3;
}

// decomp: BATTLE.PRG func_800CEEBC (src/BATTLE/BATTLE.PRG/5BF94.c:5373)
int Effects_CanStartEffectTrack(void)
{
    int var_a1 = 0;
    if (((Effects_primaryPhase - 4) < 2) && (Effects_trackCount > Effects_startedTrackCount)) {
        var_a1 = Effects_pendingTracks > 0;
    }
    return var_a1;
}

// decomp: BATTLE.PRG func_800CEF0C (src/BATTLE/BATTLE.PRG/5BF94.c:5382)
int Effects_PrimaryEffectTrackCount(void)
{
    int var_v1 = 0;
    if ((Effects_primaryPhase - 4) < 2) {
        var_v1 = Effects_trackCount;
    }
    return var_v1;
}

// decomp: BATTLE.PRG func_800CEF38 (src/BATTLE/BATTLE.PRG/5BF94.c:5391)
void Effects_RequeueEffectTrack(int arg0)
{
    Effects_startedTrackCount -= 1;
    Effects_pendingTracks |= 1 << arg0;
}

// decomp: BATTLE.PRG func_800CEF64 (src/BATTLE/BATTLE.PRG/5BF94.c:5397)
int Effects_IsPrimaryEffectLoaded(void) { return Effects_primaryLoaded != 0; }

// decomp: BATTLE.PRG func_800CEF74 (src/BATTLE/BATTLE.PRG/5BF94.c:5399)
int Effects_StartEffectTrack(int arg0)
{
    Effects_EffectSpawnRequest sp10;
    int v;

    Effects_SelectEffectResourceContext(1);
    v = Effects_CanStartEffectTrack();
    if (v != 0) {
        if (Effects_pendingTracks & (1 << arg0)) {
            char* a1 = Effects_package->block8Data;
            sp10.scriptOffset = arg0 * 3;
            sp10.source = &Effects_actionRequest.source;
            sp10.emitFrom = &Effects_actionRequest.source;
            sp10.priority = 0;
            sp10.target = &Effects_actionRequest.targets[0];
            sp10.emitTo = &Effects_actionRequest.targets[0];
            sp10.parent = 0;
            sp10.token = Effects_primaryToken;
            sp10.script = a1;
            Effects_CreatePrimaryEffectInstance(&sp10);
            Effects_SetPrimaryEffectPhase(4);
            ++Effects_startedTrackCount;
            Effects_pendingTracks &= ~(1 << arg0);
        }
    }
    return v;
}

// decomp: BATTLE.PRG func_800CF060 (src/BATTLE/BATTLE.PRG/5BF94.c:5427)
int Effects_UpdateEffectRuntime(void)
{
    int temp_s0;
    int temp_s1;

    Effects_effectBufferIndex = 1 - Effects_effectBufferIndex;
    Effects_SelectEffectResourceContext(1);
    temp_s1 = Effects_UpdatePrimaryEffects();
    Effects_SelectEffectResourceContext(0);
    temp_s0 = Effects_UpdateSecondaryEffects();
    Effects_SelectEffectResourceContext(1);
    if (Effects_primaryPhase == 4) {
        Effects_UpdateEffectCameraMotion();
    }
    return temp_s1 | temp_s0;
}

// decomp: BATTLE.PRG func_800CF0E8 (src/BATTLE/BATTLE.PRG/5BF94.c:5444)
int Effects_BeginActionEffect(Effects_ActionEffectRequest* arg0, int arg1, int arg2)
{
    Effects_actionRequest = *arg0;

    if ((arg1 != 0) && (arg0->source.type == 4)) {
        Effects_actionVariant = arg1;
        Effects_actionVariantArgument = arg2;
    }

    Effects_primaryToken = ((int (*)())Effects_AllocateEffectToken)();
    Effects_SelectEffectResourceContext(1);
    Effects_CaptureEffectCamera(&Effects_savedActionCamera);
    Effects_CaptureEffectCamera(&Effects_effectCamera);
    Effects_SetPrimaryEffectPhase(6);

    return Effects_primaryToken;
}

// decomp: BATTLE.PRG func_800CF1A8 (src/BATTLE/BATTLE.PRG/5BF94.c:5462)
void Effects_ClearPrimaryEffects(void)
{
    Effects_EffectInstance* var_s0 = Effects_primaryInstances;

    Input_StopVibrationPatterns(0U, 1);

    while (var_s0 != NULL) {
        Effects_EffectInstance* temp_s1 = var_s0->next;
        if (var_s0->context.timeline != NULL) {
            Core_Memory_FreeFromHighEnd(var_s0->context.timeline);
            var_s0->context.timeline = NULL;
        }
        Effects_DestroyPrimaryEffectInstance(var_s0);
        var_s0 = temp_s1;
    }
}

// decomp: BATTLE.PRG func_800CF218 (src/BATTLE/BATTLE.PRG/5BF94.c:5479)
char Effects_SpawnSecondaryEffects(Effects_ActionEffectRequest* arg0, int arg1, int arg2)
{
    Effects_EffectSpawnRequest spawn;
    int _[4] __attribute__((unused));
    char token = ((int (*)())Effects_AllocateEffectToken)();
    int i;

    spawn.source = &arg0->source;
    spawn.script = Effects_package->block8Data;
    spawn.scriptOffset = arg0->effectId * 3;
    spawn.parent = NULL;
    spawn.token = token;
    Effects_secondaryRequest = *arg0;

    for (i = 0; i < arg0->count; ++i) {
        Effects_EffectInstance* child;
        int id = arg0->effectId;

        if ((id >= 40 && id <= 41) || (id >= 44 && id <= 53)) {
            spawn.emitFrom = &arg0->targets[i];
            spawn.emitTo = &arg0->source;
        } else {
            switch ((u_short)id) {
            case 23:
            case 25:
                spawn.emitFrom = &arg0->targets[i];
                spawn.emitTo = &arg0->source;
                break;
            default:
                spawn.emitFrom = &arg0->source;
                spawn.emitTo = &arg0->targets[i];
                break;
            }
        }

        spawn.target = &arg0->targets[i];
        spawn.priority = arg0->targets[i].type;
        child = Effects_CreateSecondaryEffectInstance(&spawn);
        child->context.parameter = arg2;
        child->context.effectId = arg0->effectId;

        if (arg0->effectId == 54) {
            child->busyBits = 4;
        } else {
            child->busyBits = 2;
        }

        Effects_SetSecondaryEffectPhase(3, child);
    }

    return token;
}

// decomp: BATTLE.PRG func_800CF3F8 (src/BATTLE/BATTLE.PRG/5BF94.c:5532)
int Effects_BeginSecondaryEffects(Effects_ActionEffectRequest* arg0, int arg1)
{
    int _[4] __attribute__((unused));
    Effects_SelectEffectResourceContext(0);
    Effects_CaptureEffectCamera(&Effects_savedActionCamera);
    Effects_CaptureEffectCamera(&Effects_effectCamera);
    return Effects_SpawnSecondaryEffects(arg0, 0, arg1);
}

// decomp: BATTLE.PRG func_800CF458 (src/BATTLE/BATTLE.PRG/5BF94.c:5541)
int Effects_SpawnSecondaryEffectRequest(Effects_ActionEffectRequest* arg0, int arg1, int arg2)
{
    return Effects_SpawnSecondaryEffects(arg0, arg1, arg2);
}

// decomp: BATTLE.PRG func_800CF478 (src/BATTLE/BATTLE.PRG/5BF94.c:5546)
void Effects_SetPrimaryEffectPhase(int arg0) { Effects_primaryPhase = arg0; }

// decomp: BATTLE.PRG func_800CF484 (src/BATTLE/BATTLE.PRG/5BF94.c:5548)
void Effects_SetSecondaryEffectPhase(int arg0, Effects_EffectInstance* arg1) { arg1->context.phase = arg0; }

// decomp: BATTLE.PRG func_800CF48C (src/BATTLE/BATTLE.PRG/5BF94.c:5550)
int Effects_PrimaryEffectPhase(void) { return Effects_primaryPhase; }

// decomp: BATTLE.PRG func_800CF49C (src/BATTLE/BATTLE.PRG/5BF94.c:5552)
int Effects_AllocateEffectToken(void)
{
    int temp_a1;
    int j;
    int i;
    int var_t2;
    int* var_a2;

    var_t2 = 0;
    var_a2 = Effects_tokenBitmaps;

    for (i = 0; i < 4; ++i) {
        temp_a1 = Effects_tokenBitmaps[i];
        if (temp_a1 == -2) {
            continue;
        }

        for (j = 1; j < 32; ++j) {
            int v = 1 << j;
            if (!(temp_a1 & v)) {
                Effects_tokenBitmaps[i] = temp_a1 | v;
                var_t2 = (i << 5) + j;
                goto done;
            }
        }
    }
done:
    return var_t2;
}

// decomp: BATTLE.PRG func_800CF514 (src/BATTLE/BATTLE.PRG/5BF94.c:5582)
void Effects_ReleaseEffectToken(int arg0)
{
    int* ptr = Effects_tokenBitmaps;
    int var_v1 = arg0;
    if (arg0 < 0) {
        var_v1 = arg0 + 0x1F;
    }
    var_v1 >>= 5;
    ptr[var_v1] &= ~(1 << (arg0 - (var_v1 << 5)));
}

// decomp: BATTLE.PRG func_800CF55C (src/BATTLE/BATTLE.PRG/5BF94.c:5593)
Effects_EffectInstance* Effects_CreateSecondaryEffectInstance(Effects_EffectSpawnRequest* arg0)
{
    Effects_EffectInstance* var_a1;
    Effects_EffectInstance* temp_v0 = Core_Memory_AllocateFromHighEnd(sizeof *temp_v0);
    Effects_InitEffectInstance(arg0, temp_v0);
    var_a1 = Effects_secondaryInstances;

    if ((var_a1 == NULL) || (arg0->priority < var_a1->priority)) {
        temp_v0->next = var_a1;
        Effects_secondaryInstances = temp_v0;
    } else {
        while (1) {
            Effects_EffectInstance* temp_a0 = var_a1->next;
            if ((temp_a0 == NULL) || (arg0->priority < temp_a0->priority)) {
                temp_v0->next = temp_a0;
                var_a1->next = temp_v0;
                break;
            }
            var_a1 = temp_a0;
        }
    }
    return temp_v0;
}

// decomp: BATTLE.PRG func_800CF614 (src/BATTLE/BATTLE.PRG/5BF94.c:5617)
void Effects_DestroySecondaryEffectInstance(Effects_EffectInstance* arg0)
{
    Effects_EffectInstance* var_v1 = Effects_secondaryInstances;
    if (var_v1 == arg0) {
        Effects_secondaryInstances = arg0->next;
    } else {
        while (var_v1->next != NULL) {
            if (var_v1->next == arg0) {
                var_v1->next = arg0->next;
                break;
            }
            var_v1 = var_v1->next;
        }
    }
    Effects_ReleaseAllEffectParticles(arg0);
    Effects_ReleaseEffectToken(arg0->token);
    Core_Memory_FreeFromHighEnd(arg0);
}

// decomp: BATTLE.PRG func_800CF694 (src/BATTLE/BATTLE.PRG/5BF94.c:5636)
Effects_RendererSlot* Effects_AttachEffectRenderer(Effects_EffectInstance* arg0, Effects_EffectRenderer renderer, int arg2)
{
    Effects_RendererSlot* temp_v0 = Core_Memory_AllocateFromHighEnd(sizeof *temp_v0);
    temp_v0->next = arg0->context.renderer;
    arg0->context.renderer = temp_v0;
    temp_v0->renderer = renderer;
    temp_v0->rendererState = 0;
    temp_v0->renderer(temp_v0, 1, arg2);
    return temp_v0;
}

// decomp: BATTLE.PRG func_800CF70C (src/BATTLE/BATTLE.PRG/5BF94.c:5647)
void Effects_DetachEffectRenderer(Effects_EffectInstance* arg0, Effects_RendererSlot* arg1)
{
    if (arg1->rendererState != NULL) {
        Core_Memory_FreeFromHighEnd(arg1->rendererState);
    }

    if (arg0->context.renderer == arg1) {
        arg0->context.renderer = arg1->next;
    } else {
        Effects_RendererSlot* var_v1 = arg0->context.renderer;
        while (var_v1->next != arg1) {
            var_v1 = var_v1->next;
        }
        var_v1->next = arg1->next;
    }

    Core_Memory_FreeFromHighEnd(arg1);
}

// decomp: BATTLE.PRG func_800CF8BC (src/BATTLE/BATTLE.PRG/5BF94.c:5697)
void Effects_SpawnSourceActorEffect(void)
{
    Effects_ActionEffectRequest request;

    request.effectId = 56;
    request.count = 1;
    request.source.type = 4;
    request.source.value.actor.part = 0;
    request.targets[0].type = 5;
    setVector(&request.targets[0].value.position, 0, 0, 0);
    request.otherCount = 0;
    request.source.value.actor.actor = Effects_actionRequest.source.value.actor.actor;
    Effects_BeginSecondaryEffects(&request, 0);
}

// decomp: BATTLE.PRG func_800CF920 (src/BATTLE/BATTLE.PRG/5BF94.c:5712)
void Effects_ClearActionEffectMode(void) { Effects_actionVariant = 0; }

