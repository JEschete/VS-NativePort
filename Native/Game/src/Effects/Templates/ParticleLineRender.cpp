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

// decomp: particleLineRender.h:72-76, the seven inner phases jittered within
// `jitter` (one draw each).
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

// decomp: particleLineRender.h:66-128, one ribbon's eight lines.
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
// decomp: PLG065.BIN func_800FD190 (src/EFFECT/PLG065.BIN/F8.c:23).
// decomp: PLG065.BIN func_800FD190 (src/EFFECT/PLG065.BIN/F8.c:31).
// decomp: PLG065.BIN func_800FD190 (src/EFFECT/PLG065.BIN/F8.c:39).
void Plg065::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG087.BIN func_800FEFB8 (src/EFFECT/PLG087.BIN/1F0.c:35).
// decomp: PLG087.BIN func_800FEFB8 (src/EFFECT/PLG087.BIN/1F0.c:43).
// decomp: PLG087.BIN func_800FEFB8 (src/EFFECT/PLG087.BIN/1F0.c:51).
void Plg087::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG114.BIN func_800FC4FC (src/EFFECT/PLG114.BIN/208.c:18).
// decomp: PLG114.BIN func_800FC4FC (src/EFFECT/PLG114.BIN/208.c:26).
// decomp: PLG114.BIN func_800FC4FC (src/EFFECT/PLG114.BIN/208.c:34).
void Plg114::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG146.BIN func_800FC394 (src/EFFECT/PLG146.BIN/1D8.c:14).
// decomp: PLG146.BIN func_800FC394 (src/EFFECT/PLG146.BIN/1D8.c:22).
// decomp: PLG146.BIN func_800FC394 (src/EFFECT/PLG146.BIN/1D8.c:30).
void Plg146::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG147.BIN func_800FB36C (src/EFFECT/PLG147.BIN/F8.c:10).
// decomp: PLG147.BIN func_800FB36C (src/EFFECT/PLG147.BIN/F8.c:18).
// decomp: PLG147.BIN func_800FB36C (src/EFFECT/PLG147.BIN/F8.c:26).
void Plg147::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG168.BIN func_800FC960 (src/EFFECT/PLG168.BIN/1F0.c:16).
// decomp: PLG168.BIN func_800FC960 (src/EFFECT/PLG168.BIN/1F0.c:24).
// decomp: PLG168.BIN func_800FC960 (src/EFFECT/PLG168.BIN/1F0.c:32).
void Plg168::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG224.BIN func_800FA748 (src/EFFECT/PLG224.BIN/F8.c:6).
// decomp: PLG224.BIN func_800FA748 (src/EFFECT/PLG224.BIN/F8.c:14).
// decomp: PLG224.BIN func_800FA748 (src/EFFECT/PLG224.BIN/F8.c:22).
void Plg224::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG255.BIN func_800FC948 (src/EFFECT/PLG255.BIN/1D8.c:16).
// decomp: PLG255.BIN func_800FC948 (src/EFFECT/PLG255.BIN/1D8.c:24).
// decomp: PLG255.BIN func_800FC948 (src/EFFECT/PLG255.BIN/1D8.c:32).
void Plg255::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG265.BIN func_800FB830 (src/EFFECT/PLG265.BIN/2E8.c:10).
// decomp: PLG265.BIN func_800FB830 (src/EFFECT/PLG265.BIN/2E8.c:18).
// decomp: PLG265.BIN func_800FB830 (src/EFFECT/PLG265.BIN/2E8.c:26).
void Plg265::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}

// decomp: PLG266.BIN func_800FC5DC (src/EFFECT/PLG266.BIN/2E8.c:18).
// decomp: PLG266.BIN func_800FC5DC (src/EFFECT/PLG266.BIN/2E8.c:26).
// decomp: PLG266.BIN func_800FC5DC (src/EFFECT/PLG266.BIN/2E8.c:34).
void Plg266::ParticleLineRender(Templates::ParticleSetFrame& frame,
                                Templates::ParticleSetHost& host) {
  Templates::ParticleLineRender(frame, host);
}
}  // namespace VS::Effects
