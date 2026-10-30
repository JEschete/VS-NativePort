// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/SoundBanks.h"
#include "Audio/StreamPlayback.h"
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

// decomp: D_800F57A0 (D_800F57A0@BATTLE/BATTLE.PRG)
Effects_EffectPackage Effects_secondaryPackage;

// decomp: BATTLE.PRG func_800CE644 (src/BATTLE/BATTLE.PRG/5BF94.c:5045)
int Effects_IgnoreRuntimeDiagnostic(int diagnostic_code __attribute__((unused))) { }

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
void Effects_InitEffectInstance(Effects_EffectSpawnRequest* spawn_request, Effects_EffectInstance* effect_instance)
{
    int i;

    effect_instance->parent = spawn_request->parent;
    effect_instance->particleCount = 0;
    effect_instance->priority = spawn_request->priority;
    effect_instance->scriptOffset = spawn_request->scriptOffset;
    effect_instance->script = spawn_request->script;
    *(int*)&effect_instance->registers = 0;
    effect_instance->token = spawn_request->token;
    effect_instance->particles = 0;
    effect_instance->childCount = 0;
    effect_instance->frame = 0;

    for (i = 0; i < 16; ++i) {
        ((short*)&effect_instance->transforms[i].reserved4)[1] = 0;
        effect_instance->transforms[i].reserved4 = 0;
        effect_instance->transforms[i].reserved0 = 0;
        effect_instance->transforms[i].reservedC = 0;
        effect_instance->transforms[i].reserved8 = 0;
        effect_instance->transforms[i].reserved18 = 0;
        effect_instance->transforms[i].reserved14 = 0;
        effect_instance->transforms[i].reserved10 = 0;
        effect_instance->transforms[i].reserved30 = 0;
        effect_instance->transforms[i].reserved2C = 0;
        effect_instance->transforms[i].reserved28 = 0;
        *(int*)&effect_instance->transforms[i].actor = 0;
    }

    effect_instance->context.source = *spawn_request->source;
    effect_instance->context.emitFrom = *spawn_request->emitFrom;
    effect_instance->context.target = *spawn_request->target;
    effect_instance->context.emitTo = *spawn_request->emitTo;

    effect_instance->context.timeline = 0;
    effect_instance->context.renderer = 0;
    effect_instance->context.parameter = 0;
}

// decomp: BATTLE.PRG func_800CE83C (src/BATTLE/BATTLE.PRG/5BF94.c:5114)
Effects_EffectInstance* Effects_CreatePrimaryEffectInstance(Effects_EffectSpawnRequest* spawn_request)
{
    Effects_EffectInstance* currentNode;
    Effects_EffectInstance* newNode = Core_Memory_AllocateFromHighEnd(sizeof *newNode);

    Effects_InitEffectInstance(spawn_request, newNode);

    currentNode = Effects_primaryInstances;

    if ((currentNode == NULL) || (spawn_request->priority < currentNode->priority)) {
        newNode->next = currentNode;
        Effects_primaryInstances = newNode;
    } else {
        while (1) {
            if ((currentNode->next == NULL) || (spawn_request->priority < currentNode->next->priority)) {
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
void Effects_DestroyPrimaryEffectInstance(Effects_EffectInstance* effect_instance)
{
    Effects_RendererSlot* renderer_cursor;
    Effects_EffectInstance* instance_cursor = Effects_primaryInstances;

    if (instance_cursor == effect_instance) {
        Effects_primaryInstances = effect_instance->next;
    } else {
        while (instance_cursor->next != NULL) {
            if (instance_cursor->next == effect_instance) {
                instance_cursor->next = effect_instance->next;
                break;
            }
            instance_cursor = instance_cursor->next;
        }
    }

    Effects_ReleaseAllEffectParticles(effect_instance);

    renderer_cursor = effect_instance->context.renderer;

    while (renderer_cursor != NULL) {
        Effects_RendererSlot* next_renderer = renderer_cursor->next;
        renderer_cursor->renderer(renderer_cursor, 4, 0);
        Effects_DetachEffectRenderer(effect_instance, renderer_cursor);
        renderer_cursor = next_renderer;
    }

    Core_Memory_FreeFromHighEnd(effect_instance);
}

// decomp: BATTLE.PRG func_800CE9B0 (src/BATTLE/BATTLE.PRG/5BF94.c:5170)
int Effects_UpdatePrimaryEffects(void)
{
    int _[10] __attribute__((unused));
    Content_DiscFile menu_reload_file;
    Effects_EffectInstance* next_instance;
    Effects_EffectInstance* instance_cursor;
    int i;
    int primary_busy;

    instance_cursor = Effects_primaryInstances;
    primary_busy = 1;

    switch (Effects_primaryPhase) {
    case 0:
        primary_busy = 0;
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
            menu_reload_file.lba = Content_MainMenuOverlayDiscSector();
            menu_reload_file.size = 0;
            Effects_menuReloadSlot = Content_AllocateDiscRequest(&menu_reload_file);
            Content_EnqueueDiscRequest(Effects_menuReloadSlot, NULL);
        }
        break;

    case 4:
        while (instance_cursor != NULL) {

            next_instance = instance_cursor->next;
            Effects_currentInstance = instance_cursor;

            if (Effects_package->curveBlock != NULL) {
                Effects_SampleAllEffectCurves(instance_cursor->frame);
            }

            Effects_UpdateParticleGravityAndDrag();

            if (Effects_RunEffectScript(instance_cursor) == 0) {
                Effects_DestroyPrimaryEffectInstance(instance_cursor);
            } else {
                ++instance_cursor->frame;
            }

            instance_cursor = next_instance;
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

        primary_busy = 1;
        break;

    case 9:
        if (Effects_menuReloadSlot != NULL) {
            if (Effects_menuReloadSlot->state != 4) {
                // Ugly match hack
                if (Effects_package || primary_busy) {
                    primary_busy = 1;
                    break;
                } else {
                    primary_busy = 1;
                    break;
                }
            }
            Content_ReleaseDiscRequest(Effects_menuReloadSlot);
            Effects_menuReloadSlot = NULL;
        }
        Effects_SetPrimaryEffectPhase(0);
        primary_busy = 0;
        break;
    }

    return primary_busy;
}

// decomp: BATTLE.PRG func_800CED60 (src/BATTLE/BATTLE.PRG/5BF94.c:5321)
int Effects_UpdateSecondaryEffects(void)
{
    Effects_EffectInstance* next_instance;
    Effects_EffectInstance* instance_cursor;
    Effects_RendererSlot* renderer_cursor;
    int secondary_busy_bits;

    instance_cursor = Effects_secondaryInstances;
    secondary_busy_bits = 0;

    while (instance_cursor != NULL) {

        Effects_currentInstance = instance_cursor;
        next_instance = instance_cursor->next;

        switch (instance_cursor->context.phase) {
        case 2:
            break;
        case 3:
            Effects_AcquireParticlePool(0xFF);
            Effects_SetSecondaryEffectPhase(4, instance_cursor);
            break;
        case 4:
            if (Effects_package->curveBlock != NULL) {
                Effects_SampleAllEffectCurves(instance_cursor->frame);
            }

            Effects_UpdateParticleGravityAndDrag();

            renderer_cursor = instance_cursor->context.renderer;

            while (renderer_cursor != NULL) {
                Effects_RendererSlot* next_renderer = renderer_cursor->next;
                if (renderer_cursor->renderer(renderer_cursor, 2, 0) == 0) {
                    Effects_DetachEffectRenderer(instance_cursor, renderer_cursor);
                }
                renderer_cursor = next_renderer;
            }
            if (Effects_RunEffectScript(instance_cursor) == 0) {
                Effects_DestroySecondaryEffectInstance(instance_cursor);
                Effects_ReleaseParticlePool();
            } else {
                ++instance_cursor->frame;
            }
            break;
        }
        secondary_busy_bits |= instance_cursor->busyBits;
        instance_cursor = next_instance;
    }
    return secondary_busy_bits;
}

// decomp: BATTLE.PRG func_800CEEBC (src/BATTLE/BATTLE.PRG/5BF94.c:5373)
int Effects_CanStartEffectTrack(void)
{
    int can_start_track = 0;
    if (((Effects_primaryPhase - 4) < 2) && (Effects_trackCount > Effects_startedTrackCount)) {
        can_start_track = Effects_pendingTracks > 0;
    }
    return can_start_track;
}

// decomp: BATTLE.PRG func_800CEF0C (src/BATTLE/BATTLE.PRG/5BF94.c:5382)
int Effects_PrimaryEffectTrackCount(void)
{
    int active_track_count = 0;
    if ((Effects_primaryPhase - 4) < 2) {
        active_track_count = Effects_trackCount;
    }
    return active_track_count;
}

// decomp: BATTLE.PRG func_800CEF38 (src/BATTLE/BATTLE.PRG/5BF94.c:5391)
void Effects_RequeueEffectTrack(int track_index)
{
    Effects_startedTrackCount -= 1;
    Effects_pendingTracks |= 1 << track_index;
}

// decomp: BATTLE.PRG func_800CEF64 (src/BATTLE/BATTLE.PRG/5BF94.c:5397)
int Effects_IsPrimaryEffectLoaded(void) { return Effects_primaryLoaded != 0; }

// decomp: BATTLE.PRG func_800CEF74 (src/BATTLE/BATTLE.PRG/5BF94.c:5399)
int Effects_StartEffectTrack(int track_index)
{
    Effects_EffectSpawnRequest spawn_request;
    int v;

    Effects_SelectEffectResourceContext(1);
    v = Effects_CanStartEffectTrack();
    if (v != 0) {
        if (Effects_pendingTracks & (1 << track_index)) {
            char* script_bytes = Effects_package->block8Data;
            spawn_request.scriptOffset = track_index * 3;
            spawn_request.source = &Effects_actionRequest.source;
            spawn_request.emitFrom = &Effects_actionRequest.source;
            spawn_request.priority = 0;
            spawn_request.target = &Effects_actionRequest.targets[0];
            spawn_request.emitTo = &Effects_actionRequest.targets[0];
            spawn_request.parent = 0;
            spawn_request.token = Effects_primaryToken;
            spawn_request.script = script_bytes;
            Effects_CreatePrimaryEffectInstance(&spawn_request);
            Effects_SetPrimaryEffectPhase(4);
            ++Effects_startedTrackCount;
            Effects_pendingTracks &= ~(1 << track_index);
        }
    }
    return v;
}

// decomp: BATTLE.PRG func_800CF060 (src/BATTLE/BATTLE.PRG/5BF94.c:5427)
int Effects_UpdateEffectRuntime(void)
{
    int secondary_busy_bits;
    int primary_busy;

    Effects_effectBufferIndex = 1 - Effects_effectBufferIndex;
    Effects_SelectEffectResourceContext(1);
    primary_busy = Effects_UpdatePrimaryEffects();
    Effects_SelectEffectResourceContext(0);
    secondary_busy_bits = Effects_UpdateSecondaryEffects();
    Effects_SelectEffectResourceContext(1);
    if (Effects_primaryPhase == 4) {
        Effects_UpdateEffectCameraMotion();
    }
    return primary_busy | secondary_busy_bits;
}

// decomp: BATTLE.PRG func_800CF0E8 (src/BATTLE/BATTLE.PRG/5BF94.c:5444)
int Effects_BeginActionEffect(Effects_ActionEffectRequest* effect_request, int action_variant, int variant_argument)
{
    Effects_actionRequest = *effect_request;

    if ((action_variant != 0) && (effect_request->source.type == 4)) {
        Effects_actionVariant = action_variant;
        Effects_actionVariantArgument = variant_argument;
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
    Effects_EffectInstance* instance_cursor = Effects_primaryInstances;

    Input_StopVibrationPatterns(0U, 1);

    while (instance_cursor != NULL) {
        Effects_EffectInstance* next_instance = instance_cursor->next;
        if (instance_cursor->context.timeline != NULL) {
            Core_Memory_FreeFromHighEnd(instance_cursor->context.timeline);
            instance_cursor->context.timeline = NULL;
        }
        Effects_DestroyPrimaryEffectInstance(instance_cursor);
        instance_cursor = next_instance;
    }
}

// decomp: BATTLE.PRG func_800CF218 (src/BATTLE/BATTLE.PRG/5BF94.c:5479)
char Effects_SpawnSecondaryEffects(Effects_ActionEffectRequest* effect_request, int arg1, int renderer_parameters)
{
    Effects_EffectSpawnRequest spawn;
    int _[4] __attribute__((unused));
    char token = ((int (*)())Effects_AllocateEffectToken)();
    int i;

    spawn.source = &effect_request->source;
    spawn.script = Effects_package->block8Data;
    spawn.scriptOffset = effect_request->effectId * 3;
    spawn.parent = NULL;
    spawn.token = token;
    Effects_secondaryRequest = *effect_request;

    for (i = 0; i < effect_request->count; ++i) {
        Effects_EffectInstance* child;
        int id = effect_request->effectId;

        if ((id >= 40 && id <= 41) || (id >= 44 && id <= 53)) {
            spawn.emitFrom = &effect_request->targets[i];
            spawn.emitTo = &effect_request->source;
        } else {
            switch ((u_short)id) {
            case 23:
            case 25:
                spawn.emitFrom = &effect_request->targets[i];
                spawn.emitTo = &effect_request->source;
                break;
            default:
                spawn.emitFrom = &effect_request->source;
                spawn.emitTo = &effect_request->targets[i];
                break;
            }
        }

        spawn.target = &effect_request->targets[i];
        spawn.priority = effect_request->targets[i].type;
        child = Effects_CreateSecondaryEffectInstance(&spawn);
        child->context.parameter = renderer_parameters;
        child->context.effectId = effect_request->effectId;

        if (effect_request->effectId == 54) {
            child->busyBits = 4;
        } else {
            child->busyBits = 2;
        }

        Effects_SetSecondaryEffectPhase(3, child);
    }

    return token;
}

// decomp: BATTLE.PRG func_800CF3F8 (src/BATTLE/BATTLE.PRG/5BF94.c:5532)
int Effects_BeginSecondaryEffects(Effects_ActionEffectRequest* effect_request, int renderer_parameters)
{
    int _[4] __attribute__((unused));
    Effects_SelectEffectResourceContext(0);
    Effects_CaptureEffectCamera(&Effects_savedActionCamera);
    Effects_CaptureEffectCamera(&Effects_effectCamera);
    return Effects_SpawnSecondaryEffects(effect_request, 0, renderer_parameters);
}

// decomp: BATTLE.PRG func_800CF458 (src/BATTLE/BATTLE.PRG/5BF94.c:5541)
int Effects_SpawnSecondaryEffectRequest(Effects_ActionEffectRequest* effect_request, int arg1, int renderer_parameters)
{
    return Effects_SpawnSecondaryEffects(effect_request, arg1, renderer_parameters);
}

// decomp: BATTLE.PRG func_800CF478 (src/BATTLE/BATTLE.PRG/5BF94.c:5546)
void Effects_SetPrimaryEffectPhase(int effect_phase) { Effects_primaryPhase = effect_phase; }

// decomp: BATTLE.PRG func_800CF484 (src/BATTLE/BATTLE.PRG/5BF94.c:5548)
void Effects_SetSecondaryEffectPhase(int effect_phase, Effects_EffectInstance* effect_instance) { effect_instance->context.phase = effect_phase; }

// decomp: BATTLE.PRG func_800CF48C (src/BATTLE/BATTLE.PRG/5BF94.c:5550)
int Effects_PrimaryEffectPhase(void) { return Effects_primaryPhase; }

// decomp: BATTLE.PRG func_800CF49C (src/BATTLE/BATTLE.PRG/5BF94.c:5552)
int Effects_AllocateEffectToken(void)
{
    int allocated_bits;
    int j;
    int i;
    int allocated_token;
    int* bitmap_base;

    allocated_token = 0;
    bitmap_base = Effects_tokenBitmaps;

    for (i = 0; i < 4; ++i) {
        allocated_bits = Effects_tokenBitmaps[i];
        if (allocated_bits == -2) {
            continue;
        }

        for (j = 1; j < 32; ++j) {
            int v = 1 << j;
            if (!(allocated_bits & v)) {
                Effects_tokenBitmaps[i] = allocated_bits | v;
                allocated_token = (i << 5) + j;
                goto done;
            }
        }
    }
done:
    return allocated_token;
}

// decomp: BATTLE.PRG func_800CF514 (src/BATTLE/BATTLE.PRG/5BF94.c:5582)
void Effects_ReleaseEffectToken(int effect_token)
{
    int* ptr = Effects_tokenBitmaps;
    int bitmap_word_index = effect_token;
    if (effect_token < 0) {
        bitmap_word_index = effect_token + 0x1F;
    }
    bitmap_word_index >>= 5;
    ptr[bitmap_word_index] &= ~(1 << (effect_token - (bitmap_word_index << 5)));
}

// decomp: BATTLE.PRG func_800CF55C (src/BATTLE/BATTLE.PRG/5BF94.c:5593)
Effects_EffectInstance* Effects_CreateSecondaryEffectInstance(Effects_EffectSpawnRequest* spawn_request)
{
    Effects_EffectInstance* instance_cursor;
    Effects_EffectInstance* new_instance = Core_Memory_AllocateFromHighEnd(sizeof *new_instance);
    Effects_InitEffectInstance(spawn_request, new_instance);
    instance_cursor = Effects_secondaryInstances;

    if ((instance_cursor == NULL) || (spawn_request->priority < instance_cursor->priority)) {
        new_instance->next = instance_cursor;
        Effects_secondaryInstances = new_instance;
    } else {
        while (1) {
            Effects_EffectInstance* next_instance = instance_cursor->next;
            if ((next_instance == NULL) || (spawn_request->priority < next_instance->priority)) {
                new_instance->next = next_instance;
                instance_cursor->next = new_instance;
                break;
            }
            instance_cursor = next_instance;
        }
    }
    return new_instance;
}

// decomp: BATTLE.PRG func_800CF614 (src/BATTLE/BATTLE.PRG/5BF94.c:5617)
void Effects_DestroySecondaryEffectInstance(Effects_EffectInstance* effect_instance)
{
    Effects_EffectInstance* instance_cursor = Effects_secondaryInstances;
    if (instance_cursor == effect_instance) {
        Effects_secondaryInstances = effect_instance->next;
    } else {
        while (instance_cursor->next != NULL) {
            if (instance_cursor->next == effect_instance) {
                instance_cursor->next = effect_instance->next;
                break;
            }
            instance_cursor = instance_cursor->next;
        }
    }
    Effects_ReleaseAllEffectParticles(effect_instance);
    Effects_ReleaseEffectToken(effect_instance->token);
    Core_Memory_FreeFromHighEnd(effect_instance);
}

// decomp: BATTLE.PRG func_800CF694 (src/BATTLE/BATTLE.PRG/5BF94.c:5636)
Effects_RendererSlot* Effects_AttachEffectRenderer(Effects_EffectInstance* effect_instance, Effects_EffectRenderer renderer, int renderer_parameters)
{
    Effects_RendererSlot* new_renderer = Core_Memory_AllocateFromHighEnd(sizeof *new_renderer);
    new_renderer->next = effect_instance->context.renderer;
    effect_instance->context.renderer = new_renderer;
    new_renderer->renderer = renderer;
    new_renderer->rendererState = 0;
    new_renderer->renderer(new_renderer, 1, renderer_parameters);
    return new_renderer;
}

// decomp: BATTLE.PRG func_800CF70C (src/BATTLE/BATTLE.PRG/5BF94.c:5647)
void Effects_DetachEffectRenderer(Effects_EffectInstance* effect_instance, Effects_RendererSlot* renderer_slot)
{
    if (renderer_slot->rendererState != NULL) {
        Core_Memory_FreeFromHighEnd(renderer_slot->rendererState);
    }

    if (effect_instance->context.renderer == renderer_slot) {
        effect_instance->context.renderer = renderer_slot->next;
    } else {
        Effects_RendererSlot* renderer_cursor = effect_instance->context.renderer;
        while (renderer_cursor->next != renderer_slot) {
            renderer_cursor = renderer_cursor->next;
        }
        renderer_cursor->next = renderer_slot->next;
    }

    Core_Memory_FreeFromHighEnd(renderer_slot);
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

