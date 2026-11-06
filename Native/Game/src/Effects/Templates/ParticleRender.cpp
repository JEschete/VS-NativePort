// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/ParticleRender.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/ParticleCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

constexpr unsigned kHalfwordBits = 16;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
// Ribbon phases: 0x600 per segment, 0x3000 at the last point; the curve's
// phase 0 is at the second control point, 0x1000 in.
constexpr std::int32_t kSegmentPhase = 0x600;
constexpr std::int32_t kRibbonPhase = 0x3000;
constexpr std::int32_t kCurveStart = 0x1000;
// The quad edges run across the screen tangent: a quarter turn back.
constexpr std::int32_t kQuarterTurn = 0x400;

using RibbonPhases = std::array<std::int32_t, kParticleProjections>;
using RibbonScreens = std::array<ParticleProjection, kParticleProjections>;
using RibbonAngles = std::array<std::int32_t, kParticleSegments>;

// What every quad of a tick shares.
struct RibbonPaint {
  ParticleTexture texture{};
  ParticleColor color{};
  std::int16_t depth_bias{};
  std::int32_t near_clip{};
};

// The two screen corners either side of a ribbon point.
struct RibbonEdge {
  std::uint32_t plus{};
  std::uint32_t minus{};
};

[[nodiscard]] std::int32_t ScreenX(std::uint32_t screen) noexcept {
  return math::WrapSigned16(screen);
}

[[nodiscard]] std::int32_t ScreenY(std::uint32_t screen) noexcept {
  return math::WrapSigned16(screen >> kHalfwordBits);
}

// x and y stored as the halfwords of a screen word.
[[nodiscard]] std::uint32_t PackScreen(std::int32_t x,
                                       std::int32_t y) noexcept {
  return (static_cast<std::uint32_t>(x) & kHalfwordMask) |
         (static_cast<std::uint32_t>(y) << kHalfwordBits);
}

// decomp: particleRender.h:91-105, the nine phases with the seven inner
// ones jittered within `jitter` (a zero jitter keeps the draw whole, as the
// R3000 divide leaves it).
[[nodiscard]] RibbonPhases JitteredPhases(std::int32_t jitter,
                                          ParticleSetHost& host) {
  RibbonPhases phases{};
  for (std::size_t segment = 1; segment < kParticleSegments; ++segment) {
    const std::int32_t base = math::WrappingMultiply(
        static_cast<std::int32_t>(segment), kSegmentPhase);
    phases.at(segment) = math::WrappingSubtract(
        math::WrappingAdd(base, ParticleRemainder(host.Rand(), jitter)),
        ParticleHalf(jitter));
  }
  phases.back() = kRibbonPhase;
  return phases;
}

// decomp: particleRender.h:107-111, the screen direction of each segment
// turned back a quarter.
[[nodiscard]] RibbonAngles ScreenAngles(const RibbonScreens& screens) {
  RibbonAngles angles{};
  for (std::size_t segment = 0; segment < kParticleSegments; ++segment) {
    const std::uint32_t from = screens.at(segment).screen;
    const std::uint32_t to = screens.at(segment + 1).screen;
    angles.at(segment) = math::Arctangent2(ScreenY(to) - ScreenY(from),
                                           ScreenX(to) - ScreenX(from)) -
                         kQuarterTurn;
  }
  return angles;
}

// Half the sampled width plus half a draw below its spread (none for zero).
[[nodiscard]] std::int32_t HalfWidth(ParticlePoint width,
                                     ParticleSetHost& host) {
  std::int32_t half = ParticleHalf(width.x);
  if (width.y != 0) {
    half = math::WrappingAdd(
        half, ParticleHalf(ParticleRemainder(host.Rand(), width.y)));
  }
  return half;
}

[[nodiscard]] RibbonEdge EdgeAt(std::uint32_t screen, std::int32_t angle,
                                std::int32_t half_width) noexcept {
  const std::int32_t across = ParticleFixed(math::Cosine(angle), half_width);
  const std::int32_t down = ParticleFixed(math::Sine(angle), half_width);
  const std::int32_t x = ScreenX(screen);
  const std::int32_t y = ScreenY(screen);
  return {.plus = PackScreen(x + across, y + down),
          .minus = PackScreen(x - across, y - down)};
}

// decomp: particleRender.h:83-245, one ribbon's eight quads. The last half
// width becomes the next ribbon's phase jitter, as the original reused one
// variable for both.
void DrawRibbon(const ParticleControlSet& points, const RibbonPaint& paint,
                std::int32_t& jitter, ParticleSetFrame& frame,
                ParticleSetHost& host) {
  const RibbonPhases phases = JitteredPhases(jitter, host);
  std::array<std::int32_t, kParticleSegments - 1> inner{};
  for (std::size_t segment = 1; segment < kParticleSegments; ++segment) {
    inner.at(segment - 1) =
        math::WrappingSubtract(phases.at(segment), kCurveStart);
  }
  RibbonScreens screens{};
  ProjectParticleRibbon(points, inner, frame.scratch, screens, host);
  const RibbonAngles angles = ScreenAngles(screens);
  const ParticlePoint width = frame.scratch.width;
  const ParticleTexture& texture = paint.texture;
  const auto bottom = static_cast<std::uint8_t>(texture.v + texture.h);
  std::int32_t half_width = HalfWidth(width, host);
  RibbonEdge trail = EdgeAt(screens.front().screen, angles.front(), half_width);
  std::uint8_t trail_u = texture.u;
  for (std::size_t segment = 0; segment < kParticleSegments; ++segment) {
    half_width = HalfWidth(width, host);
    // Inner edges bisect their two segments; the last follows its own.
    const bool last = segment + 1 == kParticleSegments;
    const std::int32_t angle =
        last ? angles.at(segment)
             : ParticleHalf(math::WrappingAdd(angles.at(segment),
                                              angles.at(segment + 1)));
    const RibbonEdge lead =
        EdgeAt(screens.at(segment + 1).screen, angle, half_width);
    // The last edge's phase is kRibbonPhase: it lands on u + w.
    const auto lead_u = static_cast<std::uint8_t>(
        texture.u + math::R3000Quotient(math::WrappingMultiply(
                                            texture.w, phases.at(segment + 1)),
                                        kRibbonPhase));
    const ParticleQuad quad{
        .screens = {trail.plus, lead.plus, trail.minus, lead.minus},
        .uvs = {ParticleUv{trail_u, texture.v}, ParticleUv{lead_u, texture.v},
                ParticleUv{trail_u, bottom}, ParticleUv{lead_u, bottom}},
        .color = paint.color,
        .clut = texture.clut,
        .tpage = texture.tpage};
    const std::optional<std::int32_t> depth = ParticleSegmentDepth(
        screens.at(segment).depth, screens.at(segment + 1).depth,
        paint.depth_bias, paint.near_clip);
    if (depth) {
      host.SubmitQuad(quad, *depth);
    }
    trail = lead;
    trail_u = lead_u;
  }
  jitter = half_width;
}
}  // namespace

// decomp: EFFECT VS_PARTICLE_RENDER_FUNCTION
// (src/EFFECT/particleRender.h:17).
void ParticleRender(ParticleSetFrame& frame, ParticleSetHost& host) {
  const ParticleSetDefinition& definition = frame.inputs.definition;
  std::int32_t jitter = SampleRibbonJitter(frame, host);
  frame.scratch.width = ParticlePointAt(SampleParticleRange(
      definition.width, definition.curves.acceleration, frame.state.age, host));
  const ParticleColor color =
      SampleParticleColor(definition, frame.state.age, host);
  host.BeginProjection((frame.scratch.flags & kParticleEffectView) != 0);
  if (frame.texturing != ParticleTexturing::kTextured) {
    return;
  }
  const std::optional<ParticleTexture> texture = host.AdvanceTexture();
  if (!texture) {
    return;
  }
  const RibbonPaint paint{.texture = *texture,
                          .color = color,
                          .depth_bias = definition.depth_bias,
                          .near_clip = frame.inputs.near_clip};
  const std::size_t ribbons = ParticleRibbons(frame);
  for (std::size_t ribbon = 0; ribbon < ribbons; ++ribbon) {
    DrawRibbon(frame.points.sets.at(ribbon), paint, jitter, frame, host);
  }
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG045.BIN func_800FA760 (src/EFFECT/PLG045.BIN/110.c:4).
void Plg045::ParticleRender(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleRender(frame, host);
}

// decomp: PLG057.BIN func_800FA920 (src/EFFECT/PLG057.BIN/2D0.c:6).
void Plg057::ParticleRender(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleRender(frame, host);
}

// decomp: PLG082.BIN func_800FA778 (src/EFFECT/PLG082.BIN/128.c:12).
void Plg082::ParticleRender(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleRender(frame, host);
}

// decomp: PLG114.BIN func_800FA858 (src/EFFECT/PLG114.BIN/208.c:6).
void Plg114::ParticleRender(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleRender(frame, host);
}

// decomp: PLG152.BIN func_800FB2C8 (src/EFFECT/PLG152.BIN/128.c:15).
void Plg152::ParticleRender(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleRender(frame, host);
}

// decomp: PLG153.BIN func_800FBE90 (src/EFFECT/PLG153.BIN/110.c:16).
void Plg153::ParticleRender(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleRender(frame, host);
}

// decomp: PLG156.BIN func_800FA778 (src/EFFECT/PLG156.BIN/128.c:6).
void Plg156::ParticleRender(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleRender(frame, host);
}

// decomp: PLG263.BIN func_800FCE5C (src/EFFECT/PLG263.BIN/110.c:348).
void Plg263::ParticleRender(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleRender(frame, host);
}

// decomp: PLG265.BIN func_800FEF44 (src/EFFECT/PLG265.BIN/2E8.c:44).
void Plg265::ParticleRender(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleRender(frame, host);
}

// decomp: PLG266.BIN func_800FA938 (src/EFFECT/PLG266.BIN/2E8.c:6).
void Plg266::ParticleRender(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleRender(frame, host);
}

// decomp: PLG300.BIN func_800FB3D4 (src/EFFECT/PLG300.BIN/110.c:16).
void Plg300::ParticleRender(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleRender(frame, host);
}

// decomp: PLG301.BIN func_800FA760 (src/EFFECT/PLG301.BIN/110.c:12).
void Plg301::ParticleRender(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleRender(frame, host);
}
}  // namespace VS::Effects
