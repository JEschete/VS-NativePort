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
void Effects_BranchEffectScriptIf(int arg0, Effects_EffectInstance* arg1)
{
    if (arg0) {
        arg1->scriptOffset = Effects_ReadScriptHalfword(arg1);
    } else {
        arg1->scriptOffset += 2;
    }
}

// decomp: BATTLE.PRG func_800D4720 (src/BATTLE/BATTLE.PRG/5BF94.c:7515)
int Effects_YieldEffectScriptAtOffset(Effects_EffectInstance* arg0)
{
    arg0->scriptOffset = Effects_ReadScriptHalfword(arg0);
    return 0;
}

// decomp: BATTLE.PRG func_800D474C (src/BATTLE/BATTLE.PRG/5BF94.c:7521)
int Effects_JumpEffectScript(Effects_EffectInstance* arg0)
{
    arg0->scriptOffset = Effects_ReadScriptHalfword(arg0);
    return 1;
}

// decomp: BATTLE.PRG func_800D4778 (src/BATTLE/BATTLE.PRG/5BF94.c:7527)
int Effects_SpawnEffectScript(Effects_EffectInstance* arg0)
{
    Effects_EffectSpawnRequest sp10;

    sp10.script = arg0->script;
    sp10.scriptOffset = Effects_ReadScriptHalfword(arg0);
    sp10.source = &arg0->context.source;
    Effects_CreatePrimaryEffectInstance(&sp10);
    return 1;
}

// decomp: BATTLE.PRG func_800D47C0 (src/BATTLE/BATTLE.PRG/5BF94.c:7538)
int Effects_EndEffectScript(Effects_EffectInstance* arg0 __attribute__((unused))) { return 2; }

// decomp: BATTLE.PRG func_800D47C8 (src/BATTLE/BATTLE.PRG/5BF94.c:7540)
int Effects_SetEffectScriptSelector(Effects_EffectInstance* arg0)
{
    Effects_scriptSelector = Effects_ReadScriptByte(arg0);
    return 1;
}

// decomp: BATTLE.PRG func_800D47F4 (src/BATTLE/BATTLE.PRG/5BF94.c:7546)
int Effects_ScriptAttachRenderer(Effects_EffectInstance* arg0)
{
    int temp_s0;
    int temp_s2;

    temp_s2 = Effects_ReadScriptByte(arg0);
    temp_s0 = Effects_ReadScriptHalfword(arg0);
    temp_s0 = temp_s0 + (Effects_ReadScriptHalfword(arg0) << 0x10);

    if (arg0->context.parameter != 0) {
        temp_s0 = arg0->context.parameter;
    }

    Effects_AttachEffectRenderer(arg0, Effects_scriptRenderers[temp_s2], temp_s0);
    return 1;
}

// decomp: BATTLE.PRG func_800D487C (src/BATTLE/BATTLE.PRG/5BF94.c:7563)
int Effects_BranchIfEffectRenderersFinished(Effects_EffectInstance* arg0)
{
    Effects_BranchEffectScriptIf(arg0->context.renderer == 0, arg0);
    return 1;
}

// decomp: BATTLE.PRG func_800D48A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7569)
int Effects_LoadEffectTimelineImmediate(Effects_EffectInstance* arg0)
{
    Effects_LoadEffectTimeline(arg0, Effects_ReadScriptByte(arg0) & 0xFF, 0);
    arg0->scriptOffset = 0xB7;
    return 1;
}

// decomp: BATTLE.PRG func_800D48E4 (src/BATTLE/BATTLE.PRG/5BF94.c:7576)
int Effects_BranchIfActionVariantCleared(Effects_EffectInstance* arg0)
{
    Effects_BranchEffectScriptIf(Effects_actionVariant == 0, arg0);
    return 1;
}

// decomp: BATTLE.PRG func_800D4910 (src/BATTLE/BATTLE.PRG/5BF94.c:7582)
int Effects_TickEffectRenderers(Effects_EffectInstance* arg0)
{
    Effects_RendererSlot* var_s0 = arg0->context.renderer;

    while (var_s0 != 0) {
        Effects_RendererSlot* s1 = var_s0->next;
        if (var_s0->renderer(var_s0, 2, 0) == 0) {
            Effects_DetachEffectRenderer(arg0, var_s0);
        }
        var_s0 = s1;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D4984 (src/BATTLE/BATTLE.PRG/5BF94.c:7596)
int Effects_TickEffectTransforms(Effects_EffectInstance* arg0)
{
    Effects_UpdateEffectTransforms(arg0);
    return 1;
}

// decomp: BATTLE.PRG func_800D49A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7602)
int Effects_SetActionEffectVerticalOffset(Effects_EffectInstance* arg0)
{
    int i;

    if (((u_short)(arg0->context.effectId - 6) < 16) || (arg0->context.effectId == 0x21)
        || (arg0->context.effectId == 0x24)) {
        for (i = 0; i < Effects_package->block9Data->transformCount; ++i) {
            arg0->transforms[i].positionY = -0x80;
        }
    } else if ((arg0->context.effectId == 0x38) && (arg0->registers[3] == 0x2B)) {
        for (i = 0; i < Effects_package->block9Data->transformCount; ++i) {
            arg0->transforms[i].positionY = 0x80;
        }
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D4A94 (src/BATTLE/BATTLE.PRG/5BF94.c:7619)
int Effects_InitializeScriptEffectTransforms(Effects_EffectInstance* arg0)
{
    Effects_InitializeTransformStates(arg0);
    return 1;
}

// decomp: BATTLE.PRG func_800D4AB4 (src/BATTLE/BATTLE.PRG/5BF94.c:7625)
int Effects_LoadActionVariantRegister(Effects_EffectInstance* arg0)
{
    if (Effects_actionVariant < 4) {
        arg0->registers[3] = Effects_actionVariantValues[Effects_actionVariant];
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
int Effects_BranchIfParticleCountEqual(Effects_EffectInstance* arg0)
{
    Effects_BranchEffectScriptIf(arg0->particleCount == Effects_ReadScriptByte(arg0), arg0);
    return 1;
}

// decomp: BATTLE.PRG func_800D4BD0 (src/BATTLE/BATTLE.PRG/5BF94.c:7651)
int Effects_EmitParticlesFromScriptRegister(Effects_EffectInstance* arg0)
{
    Effects_EmitParticleEvent(arg0, arg0->registers[Effects_ReadScriptByte(arg0)], 0, 0, NULL);
    return 1;
}

// decomp: BATTLE.PRG func_800D4D44 (src/BATTLE/BATTLE.PRG/5BF94.c:7691)
int Effects_RefreshScriptParticleEnvironment(Effects_EffectInstance* arg0 __attribute__((unused)))
{
    Effects_UpdateParticleGravityAndDrag();
    return 1;
}

// decomp: BATTLE.PRG func_800D4D64 (src/BATTLE/BATTLE.PRG/5BF94.c:7697)
int Effects_SignalSourceActorFromScript(Effects_EffectInstance* arg0)
{
    if (arg0->context.source.type == 4) {
        Effects_SignalActorEffectAnimation(arg0->context.source.value.actor.actor);
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D4D98 (src/BATTLE/BATTLE.PRG/5BF94.c:7705)
int Effects_ClearScriptActionVariant(Effects_EffectInstance* arg0 __attribute__((unused)))
{
    Effects_actionVariant = 0;
    return 1;
}

// decomp: BATTLE.PRG func_800D4DA8 (src/BATTLE/BATTLE.PRG/5BF94.c:7711)
int Effects_ContinueEffectScript(Effects_EffectInstance* arg0 __attribute__((unused))) { return 1; }

// decomp: BATTLE.PRG func_800D4DB0 (src/BATTLE/BATTLE.PRG/5BF94.c:7713)
int Effects_LoadEffectTimelineFromRegister(Effects_EffectInstance* arg0)
{
    u_char index = arg0->registers[Effects_ReadScriptByte(arg0)];

    if (arg0->parent == NULL && index < 4) {
        int id = Effects_package->block9Data->timelineIds[index];
        if (id != 0 && Effects_package->block3Data != NULL
            && id < Effects_package->block3Data->count) {
            index = id - 1;
        }
    }
    Effects_LoadEffectTimeline(arg0, index, 0);
    return 1;
}

// decomp: BATTLE.PRG func_800D4E5C (src/BATTLE/BATTLE.PRG/5BF94.c:7728)
int Effects_TickEffectTimelineFromScript(Effects_EffectInstance* arg0)
{
    Effects_TickEffectTimeline(arg0, Effects_ReadScriptHalfword(arg0));
    return 1;
}

// decomp: BATTLE.PRG func_800D4E90 (src/BATTLE/BATTLE.PRG/5BF94.c:7734)
int Effects_BranchIfEffectTimelineFinished(Effects_EffectInstance* arg0)
{
    Effects_BranchEffectScriptIf(arg0->context.timeline->activeTracks == 0, arg0);
    return 1;
}

// decomp: BATTLE.PRG func_800D4EC0 (src/BATTLE/BATTLE.PRG/5BF94.c:7740)
int Effects_FreeEffectScriptTimeline(Effects_EffectInstance* arg0)
{
    if (arg0->context.timeline != NULL) {
        Core_Memory_FreeFromHighEnd(arg0->context.timeline);
        arg0->context.timeline = NULL;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D4F00 (src/BATTLE/BATTLE.PRG/5BF94.c:7749)
int Effects_SetParticleColorCurvesFromScript(Effects_EffectInstance* arg0)
{
    u_char first = Effects_ReadScriptByte(arg0);
    u_char second = Effects_ReadScriptByte(arg0);
    u_char third = Effects_ReadScriptByte(arg0);
    Effects_ParticleDefinition* dst = &Effects_package->block5Data->definitions[first & 0x3F];

    dst->rCurve = (first >> 6) + ((second & 0xF) << 2);
    dst->gCurve = (second >> 4) + ((third & 3) << 4);
    dst->bCurve = third >> 2;
    return 1;
}

// decomp: BATTLE.PRG func_800D4FB4 (src/BATTLE/BATTLE.PRG/5BF94.c:7762)
int Effects_ApplyPairedParticleColorPreset(Effects_EffectInstance* arg0)
{
    int i;

    for (i = 0; i < 2; ++i) {
        Effects_ParticleDefinition* dst = &Effects_package->block5Data->definitions[i];
        dst->rCurve = Effects_pairedParticleColorPresets[arg0->context.target.colorPreset][i][0];
        dst->gCurve = Effects_pairedParticleColorPresets[arg0->context.target.colorPreset][i][1];
        dst->bCurve = Effects_pairedParticleColorPresets[arg0->context.target.colorPreset][i][2];
    }

    return 1;
}

// decomp: BATTLE.PRG func_800D5048 (src/BATTLE/BATTLE.PRG/5BF94.c:7776)
int Effects_CompleteChildEffectScript(Effects_EffectInstance* arg0)
{
    if (arg0->parent != 0) {
        --arg0->parent->childCount;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D5088 (src/BATTLE/BATTLE.PRG/5BF94.c:7785)
int Effects_ApplyParticleColorPresetTable(Effects_EffectInstance* arg0)
{
    int i;

    for (i = 0; i < 5; i++) {
        Effects_ParticleDefinition* dst =
            &Effects_package->block5Data->definitions[Effects_particleColorPresetTable[arg0->context.target.colorPreset][i][3]];
        dst->rCurve = Effects_particleColorPresetTable[arg0->context.target.colorPreset][i][0];
        dst->gCurve = Effects_particleColorPresetTable[arg0->context.target.colorPreset][i][1];
        dst->bCurve = Effects_particleColorPresetTable[arg0->context.target.colorPreset][i][2];
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D5150 (src/BATTLE/BATTLE.PRG/5BF94.c:7799)
int Effects_ReportInvalidEffectInstruction(Effects_EffectInstance* arg0 __attribute__((unused)))
{
    return Effects_IgnoreRuntimeDiagnostic(0x1E);
}

// decomp: BATTLE.PRG func_800D5170 (src/BATTLE/BATTLE.PRG/5BF94.c:7804)
u_char Effects_ReadScriptByte(Effects_EffectInstance* arg0) { return arg0->script[arg0->scriptOffset++]; }

// decomp: BATTLE.PRG func_800D5198 (src/BATTLE/BATTLE.PRG/5BF94.c:7806)
u_short Effects_ReadScriptHalfword(Effects_EffectInstance* arg0)
{
    u_char lo = Effects_ReadScriptByte(arg0);
    u_char hi = Effects_ReadScriptByte(arg0);
    return lo | (hi << 8);
}

// decomp: BATTLE.PRG func_800D51D8 (src/BATTLE/BATTLE.PRG/5BF94.c:7813)
int Effects_RunEffectScript(Effects_EffectInstance* arg0)
{
    int miscIndex;

    do {
        miscIndex = Effects_scriptOpcodeHandlers[Effects_ReadScriptByte(arg0)](arg0);
        if (miscIndex == 2) {
            return 0;
        }
    } while (miscIndex == 1);
    return 1;
}

