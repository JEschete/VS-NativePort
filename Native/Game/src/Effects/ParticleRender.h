#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_EFFECTS_PARTICLERENDER_H
#define VS_OWNED_EFFECTS_PARTICLERENDER_H
typedef struct Effects_Particle Effects_Particle;
#ifdef __cplusplus
extern "C" {
#endif
void Effects_DrawEffectParticleList(Effects_Particle* head);
void Effects_DrawAnimatedSpriteParticles(Effects_Particle* head);
#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_EFFECTS_PARTICLERENDER_H
