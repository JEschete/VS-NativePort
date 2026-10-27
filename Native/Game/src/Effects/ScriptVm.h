// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_SCRIPTVM_H_TYPES
#define VS_EFFECTS_SCRIPTVM_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EFFECTS_SCRIPTVM_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_SCRIPTVM_H_DECLS)
#define VS_EFFECTS_SCRIPTVM_H_DECLS
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

void Effects_BranchEffectScriptIf(int condition, Effects_EffectInstance* effect_instance);
int Effects_YieldEffectScriptAtOffset(Effects_EffectInstance* effect_instance);
int Effects_JumpEffectScript(Effects_EffectInstance* effect_instance);
int Effects_SpawnEffectScript(Effects_EffectInstance* effect_instance);
int Effects_EndEffectScript(Effects_EffectInstance* effect_instance __attribute__((unused)));
int Effects_SetEffectScriptSelector(Effects_EffectInstance* effect_instance);
int Effects_ScriptAttachRenderer(Effects_EffectInstance* effect_instance);
int Effects_BranchIfEffectRenderersFinished(Effects_EffectInstance* effect_instance);
int Effects_LoadEffectTimelineImmediate(Effects_EffectInstance* effect_instance);
int Effects_BranchIfActionVariantCleared(Effects_EffectInstance* effect_instance);
int Effects_TickEffectRenderers(Effects_EffectInstance* effect_instance);
int Effects_TickEffectTransforms(Effects_EffectInstance* effect_instance);
int Effects_SetActionEffectVerticalOffset(Effects_EffectInstance* effect_instance);
int Effects_InitializeScriptEffectTransforms(Effects_EffectInstance* effect_instance);
int Effects_LoadActionVariantRegister(Effects_EffectInstance* effect_instance);
int Effects_SetEffectScriptRegister(Effects_EffectInstance* instance);
int Effects_BranchIfEffectRegisterEqual(Effects_EffectInstance* instance);
int Effects_BranchIfParticleCountEqual(Effects_EffectInstance* effect_instance);
int Effects_EmitParticlesFromScriptRegister(Effects_EffectInstance* effect_instance);
int Effects_RefreshScriptParticleEnvironment(Effects_EffectInstance* effect_instance __attribute__((unused)));
int Effects_SignalSourceActorFromScript(Effects_EffectInstance* effect_instance);
int Effects_ClearScriptActionVariant(Effects_EffectInstance* effect_instance __attribute__((unused)));
int Effects_ContinueEffectScript(Effects_EffectInstance* effect_instance __attribute__((unused)));
int Effects_LoadEffectTimelineFromRegister(Effects_EffectInstance* effect_instance);
int Effects_TickEffectTimelineFromScript(Effects_EffectInstance* effect_instance);
int Effects_BranchIfEffectTimelineFinished(Effects_EffectInstance* effect_instance);
int Effects_FreeEffectScriptTimeline(Effects_EffectInstance* effect_instance);
int Effects_SetParticleColorCurvesFromScript(Effects_EffectInstance* effect_instance);
int Effects_ApplyPairedParticleColorPreset(Effects_EffectInstance* effect_instance);
int Effects_CompleteChildEffectScript(Effects_EffectInstance* effect_instance);
int Effects_ApplyParticleColorPresetTable(Effects_EffectInstance* effect_instance);
int Effects_ReportInvalidEffectInstruction(Effects_EffectInstance* effect_instance __attribute__((unused)));
u_char Effects_ReadScriptByte(Effects_EffectInstance* effect_instance);
u_short Effects_ReadScriptHalfword(Effects_EffectInstance* effect_instance);
int Effects_RunEffectScript(Effects_EffectInstance* effect_instance);

#endif // VS_EFFECTS_SCRIPTVM_H_DECLS
