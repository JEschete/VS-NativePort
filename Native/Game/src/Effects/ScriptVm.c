// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Memory/Heap.h"
#include "Effects/ActionEffects.h"
#include "Effects/EffectResources.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/ScriptVm.h"
#include "Effects/Timeline.h"
#include "Effects/Transforms.h"

// decomp: BATTLE.PRG func_800D46DC (src/BATTLE/BATTLE.PRG/5BF94.c:7506)
void Effects_BranchEffectScriptIf(int condition, Effects_EffectInstance* effect_instance)
{
    if (condition) {
        effect_instance->scriptOffset = Effects_ReadScriptHalfword(effect_instance);
    } else {
        effect_instance->scriptOffset += 2;
    }
}

// decomp: BATTLE.PRG func_800D4720 (src/BATTLE/BATTLE.PRG/5BF94.c:7515)
int Effects_YieldEffectScriptAtOffset(Effects_EffectInstance* effect_instance)
{
    effect_instance->scriptOffset = Effects_ReadScriptHalfword(effect_instance);
    return 0;
}

// decomp: BATTLE.PRG func_800D474C (src/BATTLE/BATTLE.PRG/5BF94.c:7521)
int Effects_JumpEffectScript(Effects_EffectInstance* effect_instance)
{
    effect_instance->scriptOffset = Effects_ReadScriptHalfword(effect_instance);
    return 1;
}

// decomp: BATTLE.PRG func_800D4778 (src/BATTLE/BATTLE.PRG/5BF94.c:7527)
int Effects_SpawnEffectScript(Effects_EffectInstance* effect_instance)
{
    Effects_EffectSpawnRequest spawn_request;

    spawn_request.script = effect_instance->script;
    spawn_request.scriptOffset = Effects_ReadScriptHalfword(effect_instance);
    spawn_request.source = &effect_instance->context.source;
    Effects_CreatePrimaryEffectInstance(&spawn_request);
    return 1;
}

// decomp: BATTLE.PRG func_800D47C0 (src/BATTLE/BATTLE.PRG/5BF94.c:7538)
int Effects_EndEffectScript(Effects_EffectInstance* effect_instance __attribute__((unused))) { return 2; }

// decomp: BATTLE.PRG func_800D47C8 (src/BATTLE/BATTLE.PRG/5BF94.c:7540)
int Effects_SetEffectScriptSelector(Effects_EffectInstance* effect_instance)
{
    Effects_scriptSelector = Effects_ReadScriptByte(effect_instance);
    return 1;
}

// decomp: BATTLE.PRG func_800D47F4 (src/BATTLE/BATTLE.PRG/5BF94.c:7546)
int Effects_ScriptAttachRenderer(Effects_EffectInstance* effect_instance)
{
    int renderer_parameters;
    int renderer_index;

    renderer_index = Effects_ReadScriptByte(effect_instance);
    renderer_parameters = Effects_ReadScriptHalfword(effect_instance);
    renderer_parameters = renderer_parameters + (Effects_ReadScriptHalfword(effect_instance) << 0x10);

    if (effect_instance->context.parameter != 0) {
        renderer_parameters = effect_instance->context.parameter;
    }

    Effects_AttachEffectRenderer(effect_instance, Effects_scriptRenderers[renderer_index], renderer_parameters);
    return 1;
}

// decomp: BATTLE.PRG func_800D487C (src/BATTLE/BATTLE.PRG/5BF94.c:7563)
int Effects_BranchIfEffectRenderersFinished(Effects_EffectInstance* effect_instance)
{
    Effects_BranchEffectScriptIf(effect_instance->context.renderer == 0, effect_instance);
    return 1;
}

// decomp: BATTLE.PRG func_800D48A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7569)
int Effects_LoadEffectTimelineImmediate(Effects_EffectInstance* effect_instance)
{
    Effects_LoadEffectTimeline(effect_instance, Effects_ReadScriptByte(effect_instance) & 0xFF, 0);
    effect_instance->scriptOffset = 0xB7;
    return 1;
}

// decomp: BATTLE.PRG func_800D48E4 (src/BATTLE/BATTLE.PRG/5BF94.c:7576)
int Effects_BranchIfActionVariantCleared(Effects_EffectInstance* effect_instance)
{
    Effects_BranchEffectScriptIf(Effects_actionVariant == 0, effect_instance);
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
    Effects_UpdateEffectTransforms(effect_instance);
    return 1;
}

// decomp: BATTLE.PRG func_800D49A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7602)
int Effects_SetActionEffectVerticalOffset(Effects_EffectInstance* effect_instance)
{
    int i;

    if (((u_short)(effect_instance->context.effectId - 6) < 16) || (effect_instance->context.effectId == 0x21)
        || (effect_instance->context.effectId == 0x24)) {
        for (i = 0; i < Effects_package->block9Data->transformCount; ++i) {
            effect_instance->transforms[i].positionY = -0x80;
        }
    } else if ((effect_instance->context.effectId == 0x38) && (effect_instance->registers[3] == 0x2B)) {
        for (i = 0; i < Effects_package->block9Data->transformCount; ++i) {
            effect_instance->transforms[i].positionY = 0x80;
        }
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D4A94 (src/BATTLE/BATTLE.PRG/5BF94.c:7619)
int Effects_InitializeScriptEffectTransforms(Effects_EffectInstance* effect_instance)
{
    Effects_InitializeTransformStates(effect_instance);
    return 1;
}

// decomp: BATTLE.PRG func_800D4AB4 (src/BATTLE/BATTLE.PRG/5BF94.c:7625)
int Effects_LoadActionVariantRegister(Effects_EffectInstance* effect_instance)
{
    if (Effects_actionVariant < 4) {
        effect_instance->registers[3] = Effects_actionVariantValues[Effects_actionVariant];
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D4AEC (src/BATTLE/BATTLE.PRG/5BF94.c:7633)
int Effects_SetEffectScriptRegister(Effects_EffectInstance* instance)
{
    u_char registerIndex = Effects_ReadScriptByte(instance);
    u_short value = Effects_ReadScriptHalfword(instance);
    instance->registers[registerIndex] = value;
    return 1;
}

// decomp: BATTLE.PRG func_800D4B34 (src/BATTLE/BATTLE.PRG/5BF94.c:7639)
int Effects_BranchIfEffectRegisterEqual(Effects_EffectInstance* instance)
{
    u_char registerIndex = Effects_ReadScriptByte(instance);
    u_short value = Effects_ReadScriptHalfword(instance);
    Effects_BranchEffectScriptIf((u_char)instance->registers[registerIndex] == value, instance);
    return 1;
}

// decomp: BATTLE.PRG func_800D4B90 (src/BATTLE/BATTLE.PRG/5BF94.c:7645)
int Effects_BranchIfParticleCountEqual(Effects_EffectInstance* effect_instance)
{
    Effects_BranchEffectScriptIf(effect_instance->particleCount == Effects_ReadScriptByte(effect_instance), effect_instance);
    return 1;
}

// decomp: BATTLE.PRG func_800D4BD0 (src/BATTLE/BATTLE.PRG/5BF94.c:7651)
int Effects_EmitParticlesFromScriptRegister(Effects_EffectInstance* effect_instance)
{
    Effects_EmitParticleEvent(effect_instance, effect_instance->registers[Effects_ReadScriptByte(effect_instance)], 0, 0, NULL);
    return 1;
}

// decomp: BATTLE.PRG func_800D4D44 (src/BATTLE/BATTLE.PRG/5BF94.c:7691)
int Effects_RefreshScriptParticleEnvironment(Effects_EffectInstance* effect_instance __attribute__((unused)))
{
    Effects_UpdateParticleGravityAndDrag();
    return 1;
}

// decomp: BATTLE.PRG func_800D4D64 (src/BATTLE/BATTLE.PRG/5BF94.c:7697)
int Effects_SignalSourceActorFromScript(Effects_EffectInstance* effect_instance)
{
    if (effect_instance->context.source.type == 4) {
        Effects_SignalActorEffectAnimation(effect_instance->context.source.value.actor.actor);
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D4D98 (src/BATTLE/BATTLE.PRG/5BF94.c:7705)
int Effects_ClearScriptActionVariant(Effects_EffectInstance* effect_instance __attribute__((unused)))
{
    Effects_actionVariant = 0;
    return 1;
}

// decomp: BATTLE.PRG func_800D4DA8 (src/BATTLE/BATTLE.PRG/5BF94.c:7711)
int Effects_ContinueEffectScript(Effects_EffectInstance* effect_instance __attribute__((unused))) { return 1; }

// decomp: BATTLE.PRG func_800D4DB0 (src/BATTLE/BATTLE.PRG/5BF94.c:7713)
int Effects_LoadEffectTimelineFromRegister(Effects_EffectInstance* effect_instance)
{
    u_char index = effect_instance->registers[Effects_ReadScriptByte(effect_instance)];

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
    Effects_TickEffectTimeline(effect_instance, Effects_ReadScriptHalfword(effect_instance));
    return 1;
}

// decomp: BATTLE.PRG func_800D4E90 (src/BATTLE/BATTLE.PRG/5BF94.c:7734)
int Effects_BranchIfEffectTimelineFinished(Effects_EffectInstance* effect_instance)
{
    Effects_BranchEffectScriptIf(effect_instance->context.timeline->activeTracks == 0, effect_instance);
    return 1;
}

// decomp: BATTLE.PRG func_800D4EC0 (src/BATTLE/BATTLE.PRG/5BF94.c:7740)
int Effects_FreeEffectScriptTimeline(Effects_EffectInstance* effect_instance)
{
    if (effect_instance->context.timeline != NULL) {
        Core_Memory_FreeFromHighEnd(effect_instance->context.timeline);
        effect_instance->context.timeline = NULL;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D4F00 (src/BATTLE/BATTLE.PRG/5BF94.c:7749)
int Effects_SetParticleColorCurvesFromScript(Effects_EffectInstance* effect_instance)
{
    u_char first = Effects_ReadScriptByte(effect_instance);
    u_char second = Effects_ReadScriptByte(effect_instance);
    u_char third = Effects_ReadScriptByte(effect_instance);
    Effects_ParticleDefinition* dst = &Effects_package->block5Data->definitions[first & 0x3F];

    dst->rCurve = (first >> 6) + ((second & 0xF) << 2);
    dst->gCurve = (second >> 4) + ((third & 3) << 4);
    dst->bCurve = third >> 2;
    return 1;
}

// decomp: BATTLE.PRG func_800D4FB4 (src/BATTLE/BATTLE.PRG/5BF94.c:7762)
int Effects_ApplyPairedParticleColorPreset(Effects_EffectInstance* effect_instance)
{
    int i;

    for (i = 0; i < 2; ++i) {
        Effects_ParticleDefinition* dst = &Effects_package->block5Data->definitions[i];
        dst->rCurve = Effects_pairedParticleColorPresets[effect_instance->context.target.colorPreset][i][0];
        dst->gCurve = Effects_pairedParticleColorPresets[effect_instance->context.target.colorPreset][i][1];
        dst->bCurve = Effects_pairedParticleColorPresets[effect_instance->context.target.colorPreset][i][2];
    }

    return 1;
}

// decomp: BATTLE.PRG func_800D5048 (src/BATTLE/BATTLE.PRG/5BF94.c:7776)
int Effects_CompleteChildEffectScript(Effects_EffectInstance* effect_instance)
{
    if (effect_instance->parent != 0) {
        --effect_instance->parent->childCount;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D5088 (src/BATTLE/BATTLE.PRG/5BF94.c:7785)
int Effects_ApplyParticleColorPresetTable(Effects_EffectInstance* effect_instance)
{
    int i;

    for (i = 0; i < 5; i++) {
        Effects_ParticleDefinition* dst =
            &Effects_package->block5Data->definitions[Effects_particleColorPresetTable[effect_instance->context.target.colorPreset][i][3]];
        dst->rCurve = Effects_particleColorPresetTable[effect_instance->context.target.colorPreset][i][0];
        dst->gCurve = Effects_particleColorPresetTable[effect_instance->context.target.colorPreset][i][1];
        dst->bCurve = Effects_particleColorPresetTable[effect_instance->context.target.colorPreset][i][2];
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D5150 (src/BATTLE/BATTLE.PRG/5BF94.c:7799)
int Effects_ReportInvalidEffectInstruction(Effects_EffectInstance* effect_instance __attribute__((unused)))
{
    return Effects_IgnoreRuntimeDiagnostic(0x1E);
}

// decomp: BATTLE.PRG func_800D5170 (src/BATTLE/BATTLE.PRG/5BF94.c:7804)
u_char Effects_ReadScriptByte(Effects_EffectInstance* effect_instance) { return effect_instance->script[effect_instance->scriptOffset++]; }

// decomp: BATTLE.PRG func_800D5198 (src/BATTLE/BATTLE.PRG/5BF94.c:7806)
u_short Effects_ReadScriptHalfword(Effects_EffectInstance* effect_instance)
{
    u_char lo = Effects_ReadScriptByte(effect_instance);
    u_char hi = Effects_ReadScriptByte(effect_instance);
    return lo | (hi << 8);
}

// decomp: BATTLE.PRG func_800D51D8 (src/BATTLE/BATTLE.PRG/5BF94.c:7813)
int Effects_RunEffectScript(Effects_EffectInstance* effect_instance)
{
    int miscIndex;

    do {
        miscIndex = Effects_scriptOpcodeHandlers[Effects_ReadScriptByte(effect_instance)](effect_instance);
        if (miscIndex == 2) {
            return 0;
        }
    } while (miscIndex == 1);
    return 1;
}

