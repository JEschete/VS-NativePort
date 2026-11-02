#pragma once

struct Effects_Particle;

namespace VS::Effects {

// Live bindings for BATTLE.PRG func_800D6E24/func_800D6E44: validate the
// native particle list against Effects_particlePool, then draw it through the
// typed core into the current frame's packets and ordering table.
void DrawLiveEffectParticleList(Effects_Particle* head);
void DrawLiveAnimatedSpriteParticles(Effects_Particle* head);

}  // namespace VS::Effects
