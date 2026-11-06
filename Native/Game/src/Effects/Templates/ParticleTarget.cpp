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

// decomp: EFFECT VS_PARTICLE_TARGET_FUNCTION (src/EFFECT/particleSetup.h:262).
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
  // decomp: particleSetup.h:272-285, the anchor added as halfwords.
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
// decomp: PLG045.BIN func_800FA098 (src/EFFECT/PLG045.BIN/110.c:2).
void Plg045::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG057.BIN func_800FA258 (src/EFFECT/PLG057.BIN/2D0.c:4).
void Plg057::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG065.BIN func_800FCAC8 (src/EFFECT/PLG065.BIN/F8.c:21).
// decomp: PLG065.BIN func_800FCAC8 (src/EFFECT/PLG065.BIN/F8.c:37).
void Plg065::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG082.BIN func_800FA0B0 (src/EFFECT/PLG082.BIN/128.c:10).
void Plg082::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG087.BIN func_800FE8F0 (src/EFFECT/PLG087.BIN/1F0.c:33).
// decomp: PLG087.BIN func_800FE8F0 (src/EFFECT/PLG087.BIN/1F0.c:49).
void Plg087::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG114.BIN func_800FA190 (src/EFFECT/PLG114.BIN/208.c:4).
void Plg114::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG114.BIN func_800FBE34 (src/EFFECT/PLG114.BIN/208.c:16).
// decomp: PLG114.BIN func_800FBE34 (src/EFFECT/PLG114.BIN/208.c:32).
void Plg114::ParticleTarget2(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG146.BIN func_800FBCCC (src/EFFECT/PLG146.BIN/1D8.c:12).
// decomp: PLG146.BIN func_800FBCCC (src/EFFECT/PLG146.BIN/1D8.c:28).
void Plg146::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG147.BIN func_800FACA4 (src/EFFECT/PLG147.BIN/F8.c:8).
// decomp: PLG147.BIN func_800FACA4 (src/EFFECT/PLG147.BIN/F8.c:24).
void Plg147::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG152.BIN func_800FAC00 (src/EFFECT/PLG152.BIN/128.c:13).
void Plg152::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG153.BIN func_800FB7C8 (src/EFFECT/PLG153.BIN/110.c:14).
void Plg153::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG156.BIN func_800FA0B0 (src/EFFECT/PLG156.BIN/128.c:4).
void Plg156::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG168.BIN func_800FC298 (src/EFFECT/PLG168.BIN/1F0.c:14).
// decomp: PLG168.BIN func_800FC298 (src/EFFECT/PLG168.BIN/1F0.c:30).
void Plg168::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG224.BIN func_800FA080 (src/EFFECT/PLG224.BIN/F8.c:4).
// decomp: PLG224.BIN func_800FA080 (src/EFFECT/PLG224.BIN/F8.c:20).
void Plg224::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG255.BIN func_800FC280 (src/EFFECT/PLG255.BIN/1D8.c:14).
// decomp: PLG255.BIN func_800FC280 (src/EFFECT/PLG255.BIN/1D8.c:30).
void Plg255::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG263.BIN func_800FC794 (src/EFFECT/PLG263.BIN/110.c:346).
void Plg263::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG265.BIN func_800FB168 (src/EFFECT/PLG265.BIN/2E8.c:8).
// decomp: PLG265.BIN func_800FB168 (src/EFFECT/PLG265.BIN/2E8.c:24).
void Plg265::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG265.BIN func_800FE87C (src/EFFECT/PLG265.BIN/2E8.c:42).
void Plg265::ParticleTarget2(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG266.BIN func_800FA270 (src/EFFECT/PLG266.BIN/2E8.c:4).
void Plg266::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG266.BIN func_800FBF14 (src/EFFECT/PLG266.BIN/2E8.c:16).
// decomp: PLG266.BIN func_800FBF14 (src/EFFECT/PLG266.BIN/2E8.c:32).
void Plg266::ParticleTarget2(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG300.BIN func_800FAD0C (src/EFFECT/PLG300.BIN/110.c:14).
void Plg300::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}

// decomp: PLG301.BIN func_800FA098 (src/EFFECT/PLG301.BIN/110.c:10).
void Plg301::ParticleTarget(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleTarget(frame, host);
}
}  // namespace VS::Effects
