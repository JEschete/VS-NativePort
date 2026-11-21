// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/ParticleTarget.hpp"

#include <cstddef>
#include <cstdint>

#include "Core/Math/ScalarMath.hpp"
#include "Core/Math/Vector.hpp"
#include "Effects/Templates/ParticleCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

// The target offset plus a draw across [-jitter, jitter) on one axis; the
// original negated the halfword jitter as a word.
[[nodiscard]] std::int16_t Jittered(std::int16_t offset, std::int16_t jitter,
                                    ParticleSetHost& host) {
  const std::int32_t draw = host.RandomRange(jitter, -std::int32_t{jitter});
  return math::WrapSigned16(
      static_cast<std::uint32_t>(math::WrappingAdd(offset, draw)));
}
}  // namespace

// decomp: EFFECT VS_PARTICLE_TARGET_FUNCTION (src/EFFECT/particleSetup.h:221).
void ParticleTarget(ParticleSetFrame& frame, ParticleSetHost& host) {
  const ParticleSetDefinition& definition = frame.inputs.definition;
  ParticleSetScratch& scratch = frame.scratch;
  const std::int32_t age = frame.state.age;
  scratch.target_offset = ParticlePointAt(
      SampleParticleRange(definition.target_offset,
                          definition.curves.attraction_offset, age, host));
  scratch.target_jitter = ParticlePointAt(
      SampleParticleRange(definition.target_jitter,
                          definition.curves.attraction_jitter, age, host));
  // decomp: src/EFFECT/particleSetup.h:272-285, the anchor added as halfwords.
  switch (
      static_cast<ParticleTargetAnchor>(scratch.flags & kParticleTargetMask)) {
    case ParticleTargetAnchor::kTransform:
      scratch.target_offset = ParticlePointAt(
          math::AddVectors(ParticleVector(scratch.target_offset),
                           frame.inputs.target_translation));
      break;
    case ParticleTargetAnchor::kEffectOrigin:
      scratch.target_offset = ParticlePointAt(math::AddVectors(
          ParticleVector(scratch.target_offset), frame.inputs.effect_origin));
      break;
    case ParticleTargetAnchor::kNone:
    default:
      break;
  }
  const ParticlePoint offset = scratch.target_offset;
  const ParticlePoint jitter = scratch.target_jitter;
  const std::size_t ribbons = ParticleRibbons(frame);
  for (std::size_t ribbon = 0; ribbon < ribbons; ++ribbon) {
    ParticlePoint& target = frame.points.sets.at(ribbon).back();
    target.x = Jittered(offset.x, jitter.x, host);
    target.y = Jittered(offset.y, jitter.y, host);
    target.z = Jittered(offset.z, jitter.z, host);
  }
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG045.BIN func_800FA098_arg3 (src/EFFECT/particleSetup.h:19; was
// func_800FA098).
void Plg045::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG057.BIN vs_effect_thunderBurstLv1_0_target
// (src/EFFECT/particleSetup.h:221; was func_800FA258).
void Plg057::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG065.BIN vs_effect_gaeaStrikeLv1_3_target
// (src/EFFECT/particleSetup.h:221; was func_800FCAC8). decomp: PLG065.BIN
// vs_effect_gaeaStrikeLv1_3_target (src/EFFECT/particleSetup.h:221; was
// func_800FCAC8).
void Plg065::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG082.BIN vs_effect_thunderbolt_0_target
// (src/EFFECT/particleSetup.h:221; was func_800FA0B0).
void Plg082::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG087.BIN vs_effect_apocalypse_4_target
// (src/EFFECT/particleSetup.h:221; was func_800FE8F0). decomp: PLG087.BIN
// vs_effect_apocalypse_4_target (src/EFFECT/particleSetup.h:221; was
// func_800FE8F0).
void Plg087::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG114.BIN vs_effect_numbingNeedle_0_target
// (src/EFFECT/particleSetup.h:221; was func_800FA190).
void Plg114::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG114.BIN vs_effect_numbingNeedle_1_target
// (src/EFFECT/particleSetup.h:221; was func_800FBE34). decomp: PLG114.BIN
// vs_effect_numbingNeedle_1_target (src/EFFECT/particleSetup.h:221; was
// func_800FBE34).
void Plg114::ParticleTarget2(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG146.BIN vs_effect_blackNebula_2_target
// (src/EFFECT/particleSetup.h:221; was func_800FBCCC). decomp: PLG146.BIN
// vs_effect_blackNebula_2_target (src/EFFECT/particleSetup.h:221; was
// func_800FBCCC).
void Plg146::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG147.BIN vs_effect_bearClaw_1_target
// (src/EFFECT/particleSetup.h:221; was func_800FACA4). decomp: PLG147.BIN
// vs_effect_bearClaw_1_target (src/EFFECT/particleSetup.h:221; was
// func_800FACA4).
void Plg147::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG152.BIN vs_effect_riskbreak_1_target
// (src/EFFECT/particleSetup.h:221; was func_800FAC00).
void Plg152::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG153.BIN vs_effect_gravisAether_2_target
// (src/EFFECT/particleSetup.h:221; was func_800FB7C8).
void Plg153::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG156.BIN vs_effect_quickshock_0_target
// (src/EFFECT/particleSetup.h:221; was func_800FA0B0).
void Plg156::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG168.BIN vs_effect_vertigo_1_target
// (src/EFFECT/particleSetup.h:221; was func_800FC298). decomp: PLG168.BIN
// vs_effect_vertigo_1_target (src/EFFECT/particleSetup.h:221; was
// func_800FC298).
void Plg168::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG224.BIN vs_effect_plg224_0_target (src/EFFECT/particleSetup.h:221;
// was func_800FA080). decomp: PLG224.BIN vs_effect_plg224_0_target
// (src/EFFECT/particleSetup.h:221; was func_800FA080).
void Plg224::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG255.BIN vs_effect_plg255_1_target (src/EFFECT/particleSetup.h:221;
// was func_800FC280). decomp: PLG255.BIN vs_effect_plg255_1_target
// (src/EFFECT/particleSetup.h:221; was func_800FC280).
void Plg255::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG263.BIN vs_effect_plg263_3_target (src/EFFECT/particleSetup.h:221;
// was func_800FC794).
void Plg263::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG265.BIN vs_effect_plg265_1_target (src/EFFECT/particleSetup.h:221;
// was func_800FB168). decomp: PLG265.BIN vs_effect_plg265_1_target
// (src/EFFECT/particleSetup.h:221; was func_800FB168).
void Plg265::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG265.BIN vs_effect_plg265_3_target (src/EFFECT/particleSetup.h:221;
// was func_800FE87C).
void Plg265::ParticleTarget2(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG266.BIN vs_effect_plg266_0_target (src/EFFECT/particleSetup.h:221;
// was func_800FA270).
void Plg266::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG266.BIN vs_effect_plg266_1_target (src/EFFECT/particleSetup.h:221;
// was func_800FBF14). decomp: PLG266.BIN vs_effect_plg266_1_target
// (src/EFFECT/particleSetup.h:221; was func_800FBF14).
void Plg266::ParticleTarget2(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG300.BIN vs_effect_plg300_1_target (src/EFFECT/particleSetup.h:221;
// was func_800FAD0C).
void Plg300::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG301.BIN func_800FA098_arg3 (src/EFFECT/particleSetup.h:19; was
// func_800FA098).
void Plg301::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}
}  // namespace VS::Effects
