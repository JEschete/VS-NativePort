// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/ParticleCore.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"

// The sampling, clamping, depth and packet rules shared by the particle-set
// dispatcher (src/EFFECT/particleDispatch.h:3), its setup stages
// (src/EFFECT/particleSetup.h) and its two render stages
// (src/EFFECT/particleRender.h:17 and particleLineRender.h:13).
namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

constexpr unsigned kByteBits = 8;
constexpr unsigned kHalfwordBits = 16;
constexpr unsigned kCodeShift = 24;
constexpr unsigned kBlendShift = 5;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
constexpr std::uint32_t kHighHalfwordMask = 0xFFFF0000;
// Colour curves sample a half intensity (0-127); a channel without a curve
// draws at 128.
constexpr std::int32_t kColorCurveScale = 2;
constexpr std::int32_t kChannelMaximum = 255;
constexpr std::uint8_t kChannelWithoutCurve = 128;
// decomp: particleRender.h depth bias targets.
constexpr std::int32_t kBackDepth = 0x7FE;
constexpr std::int32_t kFrontDepthA = -15;
constexpr std::int32_t kFrontDepthB = -13;
constexpr std::int32_t kFrontDepthC = -11;
constexpr std::int32_t kNearerStep = 16;
// Packet word positions: the link tag, the draw-mode word, three words per
// quad corner (colour, screen, texture) and the restoring draw-mode word.
constexpr std::size_t kModeWord = 1;
constexpr std::size_t kFirstCornerWord = 2;
constexpr std::size_t kCornerWords = 3;
constexpr std::size_t kQuadRestoreWord = kParticleQuadWords - 1;
constexpr std::size_t kLineColorWord = 2;
constexpr std::size_t kLineStartWord = 3;
constexpr std::size_t kLineEndWord = 4;
constexpr std::size_t kLineRestoreWord = kParticleLineWords - 1;
// Corners 0 and 1 carry the CLUT and texture page above their coordinates;
// corners 2 and 3 store only their coordinates.
constexpr std::size_t kClutCorner = 0;
constexpr std::size_t kTpageCorner = 1;

[[nodiscard]] constexpr std::uint32_t ColorWord(ParticleColor color,
                                                std::uint8_t code) noexcept {
  return color.r | (static_cast<std::uint32_t>(color.g) << kByteBits) |
         (static_cast<std::uint32_t>(color.b) << kHalfwordBits) |
         (static_cast<std::uint32_t>(code) << kCodeShift);
}

[[nodiscard]] constexpr std::uint32_t UvHalfword(ParticleUv uv) noexcept {
  return uv.u | (static_cast<std::uint32_t>(uv.v) << kByteBits);
}
}  // namespace

std::int32_t ParticleSetCapacity(math::ScalarSpan interval) noexcept {
  // decomp: particleDispatch.h:22-30, the greater endpoint as a halfword.
  const std::int32_t greater = std::max(interval.end, interval.start);
  const std::int32_t endpoint =
      math::WrapSigned16(static_cast<std::uint32_t>(greater));
  return std::min(endpoint, static_cast<std::int32_t>(kParticleSetCapacity));
}

std::int16_t ParticleSetCount(std::int32_t sample) noexcept {
  const std::int16_t count =
      math::WrapSigned16(static_cast<std::uint32_t>(sample));
  return std::min(count, static_cast<std::int16_t>(kParticleSetCapacity));
}

std::size_t ParticleRibbons(const ParticleSetFrame& frame) noexcept {
  if (frame.state.count <= 0) {
    return 0;
  }
  return std::min({static_cast<std::size_t>(frame.state.count),
                   frame.points.capacity, kParticleSetCapacity});
}

math::Vec3 SampleParticleRange(const ParticleVectorRange& range,
                               std::uint8_t curve, std::int32_t age,
                               ParticleSetHost& host) {
  return math::InterpolateSevenBitVector(range, host.SampleCurve(curve, age));
}

math::Vec2 SampleParticlePlane(const ParticlePlaneRange& range,
                               std::uint8_t curve, std::int32_t age,
                               ParticleSetHost& host) {
  return math::InterpolateSevenBitPlane(range, host.SampleCurve(curve, age));
}

std::int32_t SampleParticleScalar(math::ScalarSpan range, std::uint8_t curve,
                                  std::int32_t age, ParticleSetHost& host) {
  return math::InterpolateSevenBitScalar(range, host.SampleCurve(curve, age));
}

std::uint8_t ParticleColorChannel(std::int32_t sample) noexcept {
  const std::int32_t doubled = math::WrappingMultiply(sample, kColorCurveScale);
  return static_cast<std::uint8_t>(std::min(doubled, kChannelMaximum));
}

ParticleColor SampleParticleColor(const ParticleSetDefinition& definition,
                                  std::int32_t age, ParticleSetHost& host) {
  std::array<std::uint8_t, kParticleAxes> channels{};
  for (std::size_t channel = 0; channel < kParticleAxes; ++channel) {
    const std::uint8_t curve = definition.curves.color.at(channel);
    channels.at(channel) =
        curve != 0 ? ParticleColorChannel(host.SampleCurve(curve, age))
                   : kChannelWithoutCurve;
  }
  return {.r = channels[0], .g = channels[1], .b = channels[2]};
}

std::int32_t SampleRibbonJitter(const ParticleSetFrame& frame,
                                ParticleSetHost& host) {
  const ParticleSetDefinition& definition = frame.inputs.definition;
  const std::int32_t width = SampleParticleScalar(
      definition.inner_extent, definition.curves.inner_extent, frame.state.age,
      host);
  return width != 0 ? width : 1;
}

std::optional<std::int32_t> ParticleSegmentDepth(
    std::int32_t first, std::int32_t second, std::int16_t depth_bias,
    std::int32_t near_clip) noexcept {
  const std::int32_t depth =
      math::R3000Quotient(math::WrappingAdd(first, second), 2);
  if (depth >= kParticleFarDepth || depth <= near_clip) {
    return std::nullopt;
  }
  switch (static_cast<ParticleDepthBias>(depth_bias)) {
    case ParticleDepthBias::kBack:
      return kBackDepth;
    case ParticleDepthBias::kFrontA:
      return kFrontDepthA;
    case ParticleDepthBias::kFrontB:
      return kFrontDepthB;
    case ParticleDepthBias::kFrontC:
      return kFrontDepthC;
    case ParticleDepthBias::kNearer:
      return near_clip < depth - kNearerStep ? depth - kNearerStep : depth;
    case ParticleDepthBias::kNone:
    default:
      return depth;
  }
}

ParticlePoint ParticleRibbonPoint(const ParticleControlSet& points,
                                  std::int32_t phase) noexcept {
  return ParticlePointAt(math::InterpolateLagrangeVector(
      {ParticleVector(points.front()), ParticleVector(points[1]),
       ParticleVector(points[2]), ParticleVector(points.back())},
      phase));
}

void ProjectParticleRibbon(
    const ParticleControlSet& points,
    std::span<const std::int32_t, kParticleSegments - 1> phases,
    ParticleSetScratch& scratch,
    std::span<ParticleProjection, kParticleProjections> projections,
    ParticleSetHost& host) {
  projections[0] = host.Project(points.front());
  for (std::size_t segment = 1; segment < kParticleSegments; ++segment) {
    scratch.vector = ParticleRibbonPoint(points, phases[segment - 1]);
    projections[segment] = host.Project(scratch.vector);
  }
  projections[kParticleSegments] = host.Project(points.back());
}

void EncodeParticleQuadPacket(
    const ParticleQuad& quad, std::uint32_t link,
    std::span<std::uint32_t, kParticleQuadWords> packet) noexcept {
  packet[0] = (link & kParticleLinkMask) | kParticleQuadLength;
  packet[kModeWord] = kParticleDrawModeWord;
  for (std::size_t corner = 0; corner < kParticleQuadCorners; ++corner) {
    const std::size_t at = kFirstCornerWord + (corner * kCornerWords);
    packet[at] = ColorWord(quad.color, kParticleQuadCode);
    packet[at + 1] = quad.screens.at(corner);
    std::uint32_t high = packet[at + 2] & kHighHalfwordMask;
    if (corner == kClutCorner) {
      high = static_cast<std::uint32_t>(quad.clut) << kHalfwordBits;
    } else if (corner == kTpageCorner) {
      high = static_cast<std::uint32_t>(quad.tpage) << kHalfwordBits;
    }
    packet[at + 2] = high | (UvHalfword(quad.uvs.at(corner)) & kHalfwordMask);
  }
  packet[kQuadRestoreWord] = kParticleRestoreModeWord;
}

std::array<std::uint32_t, kParticleLineWords> EncodeParticleLinePacket(
    const ParticleLine& line, std::uint32_t link) noexcept {
  std::array<std::uint32_t, kParticleLineWords> words{};
  words[0] = (link & kParticleLinkMask) | kParticleLineLength;
  words[kModeWord] = kParticleDrawModeWord |
                     (static_cast<std::uint32_t>(line.blend) << kBlendShift);
  words[kLineColorWord] = ColorWord(line.color, kParticleLineCode);
  words[kLineStartWord] = line.screens[0];
  words[kLineEndWord] = line.screens[1];
  words[kLineRestoreWord] = kParticleRestoreModeWord;
  return words;
}
}  // namespace VS::Effects::Templates
