// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Effects/EffectInstancesLive.hpp"
#include "Effects/EffectScriptLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/ScriptVm.h"
#include "Effects/Transforms.h"

// decomp: BATTLE.PRG func_800D4778 (src/BATTLE/BATTLE.PRG/5BF94.c:7527)
int Effects_SpawnEffectScript(Effects_EffectInstance* effect_instance)
{
    Effects_EffectSpawnRequest spawn_request;

    spawn_request.script = effect_instance->script;
    spawn_request.scriptOffset = VS::Effects::LiveReadScriptHalfword(effect_instance);
    spawn_request.source = &effect_instance->context.source;
    VS::Effects::LiveCreatePrimaryEffectInstance(&spawn_request);
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

    VS::Effects::LiveAttachEffectRenderer(effect_instance, Effects_scriptRenderers[renderer_index], renderer_parameters);
    return 1;
}

// decomp: BATTLE.PRG func_800D4910 (src/BATTLE/BATTLE.PRG/5BF94.c:7582)
int Effects_TickEffectRenderers(Effects_EffectInstance* effect_instance)
{
    Effects_RendererSlot* renderer_cursor = effect_instance->context.renderer;

    while (renderer_cursor != 0) {
        Effects_RendererSlot* next_renderer = renderer_cursor->next;
        if (renderer_cursor->renderer(renderer_cursor, 2, 0) == 0) {
            VS::Effects::LiveDetachEffectRenderer(effect_instance, renderer_cursor);
        }
        renderer_cursor = next_renderer;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D4BD0 (src/BATTLE/BATTLE.PRG/5BF94.c:7651)
int Effects_EmitParticlesFromScriptRegister(Effects_EffectInstance* effect_instance)
{
    Effects_EmitParticleEvent(effect_instance, effect_instance->registers[VS::Effects::LiveReadScriptByte(effect_instance)], 0, 0, NULL);
    return 1;
}

}  // extern "C"
