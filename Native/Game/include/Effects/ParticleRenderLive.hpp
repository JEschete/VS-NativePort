#pragma once

namespace VS::Effects {
struct LiveParticle;

// Live bindings for BATTLE.PRG func_800D6E24/func_800D6E44: validate the
// native particle list against particlePool, then draw it through the
// typed core into the current frame's packets and ordering table.
void DrawLiveEffectParticleList(LiveParticle* head);
void DrawLiveAnimatedSpriteParticles(LiveParticle* head);

}  // namespace VS::Effects
