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

void Effects_BranchEffectScriptIf(int arg0, Effects_EffectInstance* arg1);
int Effects_YieldEffectScriptAtOffset(Effects_EffectInstance* arg0);
int Effects_JumpEffectScript(Effects_EffectInstance* arg0);
int Effects_SpawnEffectScript(Effects_EffectInstance* arg0);
int Effects_EndEffectScript(Effects_EffectInstance* arg0 __attribute__((unused)));
int Effects_SetEffectScriptSelector(Effects_EffectInstance* arg0);
int Effects_ScriptAttachRenderer(Effects_EffectInstance* arg0);
int Effects_BranchIfEffectRenderersFinished(Effects_EffectInstance* arg0);
int Effects_LoadEffectTimelineImmediate(Effects_EffectInstance* arg0);
int Effects_BranchIfActionVariantCleared(Effects_EffectInstance* arg0);
int Effects_TickEffectRenderers(Effects_EffectInstance* arg0);
int Effects_TickEffectTransforms(Effects_EffectInstance* arg0);
int Effects_SetActionEffectVerticalOffset(Effects_EffectInstance* arg0);
int Effects_InitializeScriptEffectTransforms(Effects_EffectInstance* arg0);
int Effects_LoadActionVariantRegister(Effects_EffectInstance* arg0);
int Effects_SetEffectScriptRegister(Effects_EffectInstance* instance);
int Effects_BranchIfEffectRegisterEqual(Effects_EffectInstance* instance);
int Effects_BranchIfParticleCountEqual(Effects_EffectInstance* arg0);
int Effects_EmitParticlesFromScriptRegister(Effects_EffectInstance* arg0);
int Effects_RefreshScriptParticleEnvironment(Effects_EffectInstance* arg0 __attribute__((unused)));
int Effects_SignalSourceActorFromScript(Effects_EffectInstance* arg0);
int Effects_ClearScriptActionVariant(Effects_EffectInstance* arg0 __attribute__((unused)));
int Effects_ContinueEffectScript(Effects_EffectInstance* arg0 __attribute__((unused)));
int Effects_LoadEffectTimelineFromRegister(Effects_EffectInstance* arg0);
int Effects_TickEffectTimelineFromScript(Effects_EffectInstance* arg0);
int Effects_BranchIfEffectTimelineFinished(Effects_EffectInstance* arg0);
int Effects_FreeEffectScriptTimeline(Effects_EffectInstance* arg0);
int Effects_SetParticleColorCurvesFromScript(Effects_EffectInstance* arg0);
int Effects_ApplyPairedParticleColorPreset(Effects_EffectInstance* arg0);
int Effects_CompleteChildEffectScript(Effects_EffectInstance* arg0);
int Effects_ApplyParticleColorPresetTable(Effects_EffectInstance* arg0);
int Effects_ReportInvalidEffectInstruction(Effects_EffectInstance* arg0 __attribute__((unused)));
u_char Effects_ReadScriptByte(Effects_EffectInstance* arg0);
u_short Effects_ReadScriptHalfword(Effects_EffectInstance* arg0);
int Effects_RunEffectScript(Effects_EffectInstance* arg0);

#endif // VS_EFFECTS_SCRIPTVM_H_DECLS
