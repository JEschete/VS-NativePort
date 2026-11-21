// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/ParticleLineRender.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/ParticleCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

// Ribbon phases step 0x600 per segment from the curve's start, 0x1000
// before its phase 0.
constexpr std::int32_t kSegmentPhase = 0x600;
constexpr std::int32_t kCurveStart = 0x1000;

using RibbonScreens = std::array<ParticleProjection, kParticleProjections>;

// decomp: src/EFFECT/particleLineRender.h:72-76, the seven inner phases
// jittered within `jitter` (one draw each).
[[nodiscard]] std::array<std::int32_t, kParticleSegments - 1> LinePhases(
    std::int32_t jitter, ParticleSetHost& host) {
  std::array<std::int32_t, kParticleSegments - 1> phases{};
  for (std::size_t segment = 1; segment < kParticleSegments; ++segment) {
    const std::int32_t base = math::WrappingSubtract(
        math::WrappingMultiply(static_cast<std::int32_t>(segment),
                               kSegmentPhase),
        kCurveStart);
    phases.at(segment - 1) = math::WrappingSubtract(
        math::WrappingAdd(base, ParticleRemainder(host.Rand(), jitter)),
        ParticleHalf(jitter));
  }
  return phases;
}

// decomp: src/EFFECT/particleLineRender.h:66-128, one ribbon's eight lines.
void DrawLines(const ParticleControlSet& points, const ParticleLine& paint,
               std::int32_t jitter, ParticleSetFrame& frame,
               ParticleSetHost& host) {
  const std::array<std::int32_t, kParticleSegments - 1> phases =
      LinePhases(jitter, host);
  RibbonScreens screens{};
  ProjectParticleRibbon(points, phases, frame.scratch, screens, host);
  for (std::size_t segment = 0; segment < kParticleSegments; ++segment) {
    const ParticleProjection& from = screens.at(segment);
    const ParticleProjection& to = screens.at(segment + 1);
    const std::optional<std::int32_t> depth = ParticleSegmentDepth(
        from.depth, to.depth, frame.inputs.definition.depth_bias,
        frame.inputs.near_clip);
    if (depth) {
      ParticleLine line = paint;
      line.screens = {from.screen, to.screen};
      host.SubmitLine(line, *depth);
    }
  }
}
}  // namespace

// decomp: EFFECT VS_PARTICLE_RENDER_FUNCTION
// (src/EFFECT/particleLineRender.h:13).
void ParticleLineRender(ParticleSetFrame& frame, ParticleSetHost& host) {
  const ParticleSetDefinition& definition = frame.inputs.definition;
  const std::int32_t jitter = SampleRibbonJitter(frame, host);
  const ParticleLine paint{
      .color = SampleParticleColor(definition, frame.state.age, host),
      .blend = definition.blend};
  host.BeginProjection((frame.scratch.flags & kParticleEffectView) != 0);
  const std::size_t ribbons = ParticleRibbons(frame);
  for (std::size_t ribbon = 0; ribbon < ribbons; ++ribbon) {
    DrawLines(frame.points.sets.at(ribbon), paint, jitter, frame, host);
  }
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG065.BIN vs_effect_gaeaStrikeLv1_3_render
// (src/EFFECT/particleLineRender.h:15; was func_800FD190). decomp: PLG065.BIN
// vs_effect_gaeaStrikeLv1_3_render (src/EFFECT/particleLineRender.h:15; was
// func_800FD190). decomp: PLG065.BIN vs_effect_gaeaStrikeLv1_3_render
// (src/EFFECT/particleLineRender.h:15; was func_800FD190).
void Plg065::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG087.BIN vs_effect_apocalypse_4_render
// (src/EFFECT/particleLineRender.h:15; was func_800FEFB8). decomp: PLG087.BIN
// vs_effect_apocalypse_4_render (src/EFFECT/particleLineRender.h:15; was
// func_800FEFB8). decomp: PLG087.BIN vs_effect_apocalypse_4_render
// (src/EFFECT/particleLineRender.h:15; was func_800FEFB8).
void Plg087::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG114.BIN vs_effect_numbingNeedle_1_render
// (src/EFFECT/particleLineRender.h:15; was func_800FC4FC). decomp: PLG114.BIN
// vs_effect_numbingNeedle_1_render (src/EFFECT/particleLineRender.h:15; was
// func_800FC4FC). decomp: PLG114.BIN vs_effect_numbingNeedle_1_render
// (src/EFFECT/particleLineRender.h:15; was func_800FC4FC).
void Plg114::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG146.BIN vs_effect_blackNebula_2_render
// (src/EFFECT/particleLineRender.h:15; was func_800FC394). decomp: PLG146.BIN
// vs_effect_blackNebula_2_render (src/EFFECT/particleLineRender.h:15; was
// func_800FC394). decomp: PLG146.BIN vs_effect_blackNebula_2_render
// (src/EFFECT/particleLineRender.h:15; was func_800FC394).
void Plg146::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG147.BIN vs_effect_bearClaw_1_render
// (src/EFFECT/particleLineRender.h:15; was func_800FB36C). decomp: PLG147.BIN
// vs_effect_bearClaw_1_render (src/EFFECT/particleLineRender.h:15; was
// func_800FB36C). decomp: PLG147.BIN vs_effect_bearClaw_1_render
// (src/EFFECT/particleLineRender.h:15; was func_800FB36C).
void Plg147::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG168.BIN vs_effect_vertigo_1_render
// (src/EFFECT/particleLineRender.h:15; was func_800FC960). decomp: PLG168.BIN
// vs_effect_vertigo_1_render (src/EFFECT/particleLineRender.h:15; was
// func_800FC960). decomp: PLG168.BIN vs_effect_vertigo_1_render
// (src/EFFECT/particleLineRender.h:15; was func_800FC960).
void Plg168::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG224.BIN vs_effect_plg224_0_render
// (src/EFFECT/particleLineRender.h:15; was func_800FA748). decomp: PLG224.BIN
// vs_effect_plg224_0_render (src/EFFECT/particleLineRender.h:15; was
// func_800FA748). decomp: PLG224.BIN vs_effect_plg224_0_render
// (src/EFFECT/particleLineRender.h:15; was func_800FA748).
void Plg224::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG255.BIN vs_effect_plg255_1_render
// (src/EFFECT/particleLineRender.h:15; was func_800FC948). decomp: PLG255.BIN
// vs_effect_plg255_1_render (src/EFFECT/particleLineRender.h:15; was
// func_800FC948). decomp: PLG255.BIN vs_effect_plg255_1_render
// (src/EFFECT/particleLineRender.h:15; was func_800FC948).
void Plg255::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG265.BIN vs_effect_plg265_1_render
// (src/EFFECT/particleLineRender.h:15; was func_800FB830). decomp: PLG265.BIN
// vs_effect_plg265_1_render (src/EFFECT/particleLineRender.h:15; was
// func_800FB830). decomp: PLG265.BIN vs_effect_plg265_1_render
// (src/EFFECT/particleLineRender.h:15; was func_800FB830).
void Plg265::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG266.BIN vs_effect_plg266_1_render
// (src/EFFECT/particleLineRender.h:15; was func_800FC5DC). decomp: PLG266.BIN
// vs_effect_plg266_1_render (src/EFFECT/particleLineRender.h:15; was
// func_800FC5DC). decomp: PLG266.BIN vs_effect_plg266_1_render
// (src/EFFECT/particleLineRender.h:15; was func_800FC5DC).
void Plg266::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}
}  // namespace VS::Effects
