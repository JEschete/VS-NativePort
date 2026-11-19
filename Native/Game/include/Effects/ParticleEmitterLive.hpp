#pragma once

struct Effects_EffectInstance;

namespace VS::Effects {
struct LiveParticle;

// Checked binding of F01515 to the current package, curve samples, particle
// pool, transforms and BIOS random source. Refused inputs are diagnosed;
// no stale definition, curve, transform or allocation is substituted.
void EmitLiveParticleEvent(Effects_EffectInstance* instance, int event,
                           int transparency_override, int particle_variant,
                           const LiveParticle* parent);
}  // namespace VS::Effects
