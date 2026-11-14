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

int Effects_SpawnEffectScript(Effects_EffectInstance* effect_instance);
int Effects_ScriptAttachRenderer(Effects_EffectInstance* effect_instance);
int Effects_LoadEffectTimelineImmediate(Effects_EffectInstance* effect_instance);
int Effects_TickEffectRenderers(Effects_EffectInstance* effect_instance);
int Effects_TickEffectTransforms(Effects_EffectInstance* effect_instance);
int Effects_InitializeScriptEffectTransforms(Effects_EffectInstance* effect_instance);
int Effects_EmitParticlesFromScriptRegister(Effects_EffectInstance* effect_instance);
int Effects_RefreshScriptParticleEnvironment(Effects_EffectInstance* effect_instance __attribute__((unused)));
int Effects_LoadEffectTimelineFromRegister(Effects_EffectInstance* effect_instance);
int Effects_TickEffectTimelineFromScript(Effects_EffectInstance* effect_instance);
int Effects_BranchIfEffectTimelineFinished(Effects_EffectInstance* effect_instance);
int Effects_FreeEffectScriptTimeline(Effects_EffectInstance* effect_instance);

#endif // VS_EFFECTS_SCRIPTVM_H_DECLS
