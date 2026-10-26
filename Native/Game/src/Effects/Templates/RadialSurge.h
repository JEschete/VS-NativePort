#pragma once

#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Trails.h"

extern int Content_plg074TrailEmissionCounter[1];

void Effects_Plg074_GenerateTrailOrigins(
    Effects_ParticleDefinition* definition,
    Effects_ParticleEmitterScratch* scratch, Effects_EffectInstance* instance,
    Effects_TrailQuadSet* state);

int Effects_Plg074_UpdateTrailEmitter(Effects_RendererSlot* slot, u_int command,
                                      int parameter);
