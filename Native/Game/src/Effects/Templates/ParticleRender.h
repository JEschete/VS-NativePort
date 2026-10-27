// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_TEMPLATES_PARTICLERENDER_H_TYPES
#define VS_EFFECTS_TEMPLATES_PARTICLERENDER_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EFFECTS_TEMPLATES_PARTICLERENDER_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_TEMPLATES_PARTICLERENDER_H_DECLS)
#define VS_EFFECTS_TEMPLATES_PARTICLERENDER_H_DECLS
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

void Effects_Plg045_ParticleRender(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set);
void Effects_Plg057_ParticleRender(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set);
void Effects_Plg082_ParticleRender(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set);
void Effects_Plg114_ParticleRender(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set);
void Effects_Plg152_ParticleRender(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set);
void Effects_Plg153_ParticleRender(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set);
void Effects_Plg156_ParticleRender(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set);
void Effects_Plg263_ParticleRender(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set);
void Effects_Plg265_ParticleRender(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set);
void Effects_Plg266_ParticleRender(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set);
void Effects_Plg300_ParticleRender(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set);
void Effects_Plg301_ParticleRender(Effects_ParticleDefinition* particle_definition, Effects_ParticleEmitterScratch* emitter_scratch,
    Effects_EffectInstance* effect_instance, Effects_ParticleQuadSet* particle_set);

#endif // VS_EFFECTS_TEMPLATES_PARTICLERENDER_H_DECLS
