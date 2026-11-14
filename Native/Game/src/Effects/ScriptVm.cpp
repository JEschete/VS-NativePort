// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Core/Memory/HeapLive.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/EffectScriptLive.hpp"
#include "Effects/TransformsLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/ScriptVm.h"
#include "Effects/Timeline.h"
#include "Effects/Transforms.h"

// decomp: BATTLE.PRG func_800D4778 (src/BATTLE/BATTLE.PRG/5BF94.c:7527)
int Effects_SpawnEffectScript(Effects_EffectInstance* effect_instance)
{
    Effects_EffectSpawnRequest spawn_request;

    spawn_request.script = effect_instance->script;
    spawn_request.scriptOffset = VS::Effects::LiveReadScriptHalfword(effect_instance);
    spawn_request.source = &effect_instance->context.source;
    Effects_CreatePrimaryEffectInstance(&spawn_request);
    return 1;
}

// decomp: BATTLE.PRG func_800D47F4 (src/BATTLE/BATTLE.PRG/5BF94.c:7546)
int Effects_ScriptAttachRenderer(Effects_EffectInstance* effect_instance)
{
    int renderer_parameters;
    int renderer_index;

    renderer_index = VS::Effects::LiveReadScriptByte(effect_instance);
    renderer_parameters = VS::Effects::LiveReadScriptHalfword(effect_instance);
    renderer_parameters = renderer_parameters + (VS::Effects::LiveReadScriptHalfword(effect_instance) << 0x10);

    if (effect_instance->context.parameter != 0) {
        renderer_parameters = effect_instance->context.parameter;
    }

    Effects_AttachEffectRenderer(effect_instance, Effects_scriptRenderers[renderer_index], renderer_parameters);
    return 1;
}

// decomp: BATTLE.PRG func_800D48A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7569)
int Effects_LoadEffectTimelineImmediate(Effects_EffectInstance* effect_instance)
{
    Effects_LoadEffectTimeline(effect_instance, VS::Effects::LiveReadScriptByte(effect_instance) & 0xFF, 0);
    effect_instance->scriptOffset = 0xB7;
    return 1;
}

// decomp: BATTLE.PRG func_800D4910 (src/BATTLE/BATTLE.PRG/5BF94.c:7582)
int Effects_TickEffectRenderers(Effects_EffectInstance* effect_instance)
{
    Effects_RendererSlot* renderer_cursor = effect_instance->context.renderer;

    while (renderer_cursor != 0) {
        Effects_RendererSlot* next_renderer = renderer_cursor->next;
        if (renderer_cursor->renderer(renderer_cursor, 2, 0) == 0) {
            Effects_DetachEffectRenderer(effect_instance, renderer_cursor);
        }
        renderer_cursor = next_renderer;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D4984 (src/BATTLE/BATTLE.PRG/5BF94.c:7596)
int Effects_TickEffectTransforms(Effects_EffectInstance* effect_instance)
{
    VS::Effects::LiveUpdateEffectTransforms(effect_instance);
    return 1;
}

// decomp: BATTLE.PRG func_800D4A94 (src/BATTLE/BATTLE.PRG/5BF94.c:7619)
int Effects_InitializeScriptEffectTransforms(Effects_EffectInstance* effect_instance)
{
    VS::Effects::LiveInitializeTransformStates(effect_instance);
    return 1;
}

// decomp: BATTLE.PRG func_800D4BD0 (src/BATTLE/BATTLE.PRG/5BF94.c:7651)
int Effects_EmitParticlesFromScriptRegister(Effects_EffectInstance* effect_instance)
{
    Effects_EmitParticleEvent(effect_instance, effect_instance->registers[VS::Effects::LiveReadScriptByte(effect_instance)], 0, 0, NULL);
    return 1;
}

// decomp: BATTLE.PRG func_800D4D44 (src/BATTLE/BATTLE.PRG/5BF94.c:7691)
int Effects_RefreshScriptParticleEnvironment(Effects_EffectInstance* effect_instance __attribute__((unused)))
{
    VS::Effects::LiveUpdateParticleGravityAndDrag();
    return 1;
}

// decomp: BATTLE.PRG func_800D4DB0 (src/BATTLE/BATTLE.PRG/5BF94.c:7713)
int Effects_LoadEffectTimelineFromRegister(Effects_EffectInstance* effect_instance)
{
    u_char index = effect_instance->registers[VS::Effects::LiveReadScriptByte(effect_instance)];

    if (effect_instance->parent == NULL && index < 4) {
        int id = Effects_package->block9Data->timelineIds[index];
        if (id != 0 && Effects_package->block3Data != NULL
            && id < Effects_package->block3Data->count) {
            index = id - 1;
        }
    }
    Effects_LoadEffectTimeline(effect_instance, index, 0);
    return 1;
}

// decomp: BATTLE.PRG func_800D4E5C (src/BATTLE/BATTLE.PRG/5BF94.c:7728)
int Effects_TickEffectTimelineFromScript(Effects_EffectInstance* effect_instance)
{
    Effects_TickEffectTimeline(effect_instance, VS::Effects::LiveReadScriptHalfword(effect_instance));
    return 1;
}

// decomp: BATTLE.PRG func_800D4E90 (src/BATTLE/BATTLE.PRG/5BF94.c:7734)
int Effects_BranchIfEffectTimelineFinished(Effects_EffectInstance* effect_instance)
{
    VS::Effects::LiveBranchEffectScriptIf(effect_instance->context.timeline->activeTracks == 0, effect_instance);
    return 1;
}

// decomp: BATTLE.PRG func_800D4EC0 (src/BATTLE/BATTLE.PRG/5BF94.c:7740)
int Effects_FreeEffectScriptTimeline(Effects_EffectInstance* effect_instance)
{
    if (effect_instance->context.timeline != NULL) {
        VS::Core::Memory::FreeLiveFromHighEnd(effect_instance->context.timeline);
        effect_instance->context.timeline = NULL;
    }
    return 1;
}

}  // extern "C"
