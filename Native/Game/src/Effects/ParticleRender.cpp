// decomp: func_800D6E44 (src/BATTLE/BATTLE.PRG/6E644.s:19).
// C counterpart: rood-boi pc/src/effect_particles.c:86-368,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Effects/ParticleRender.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Effects {
namespace {
constexpr int kFixedShift = 12;
constexpr int kProjectionShift = 10;
constexpr int kDepthStep = 8;
constexpr int kDepthLargeStep = 16;
constexpr int kBackgroundBucket = kParticleMaximumDepth - 2;
constexpr int kOverlayBack = -15;
constexpr int kOverlayMiddle = -13;
constexpr int kOverlayFront = -11;
constexpr int kScaleDivisor = 4;
constexpr unsigned kByteShift = 8;
constexpr std::uint32_t kDefaultColorChannel = 128;
constexpr std::uint16_t kSemiTransparentClut = 0x8000;

[[noreturn]] void Fail(const char* message) {
  VS_LOG_ERROR("effects", "{}", message);
  std::abort();
}

std::int16_t Half(std::int64_t value) {
  return VS::Core::Math::WrapSigned16(static_cast<std::uint32_t>(value));
}

std::int16_t RotationTowards(ParticleScreenPoint centre,
                             ParticleScreenPoint lead) {
  const int dx = centre.x - lead.x;
  const int dy = centre.y - lead.y;
  return dx == 0 && dy == 0 ? std::int16_t{0}
                            : Half(VS::Core::Math::Arctangent2(dy, dx));
}

std::int16_t Scale(int projection, int trigonometric, unsigned size) {
  const auto factor = trigonometric * static_cast<int>(size) / kScaleDivisor;
  const auto product = static_cast<std::uint32_t>(projection) *
                       static_cast<std::uint32_t>(factor);
  return Half(std::bit_cast<std::int32_t>(product) >> kFixedShift);
}

std::int16_t ClampedAxis(std::int16_t centre, std::int64_t offset) {
  return static_cast<std::int16_t>(std::clamp<std::int64_t>(
      centre + offset, std::numeric_limits<std::int16_t>::min(),
      std::numeric_limits<std::int16_t>::max()));
}
}  // namespace

std::optional<int> ParticleOrderingBucket(ParticleDepthMode mode,
                                          std::int32_t projected_depth,
                                          std::int32_t near_clip) {
  std::int32_t depth = projected_depth;
  switch (mode) {
    case ParticleDepthMode::kFar:
      depth += kDepthStep;
      break;
    case ParticleDepthMode::kFarther:
      depth += kDepthLargeStep;
      break;
    case ParticleDepthMode::kNear:
      depth -= kDepthStep;
      break;
    case ParticleDepthMode::kNearer:
      depth -= kDepthLargeStep;
      break;
    default:
      break;
  }
  if (projected_depth <= 0 || depth >= kParticleMaximumDepth ||
      depth <= near_clip) {
    return std::nullopt;
  }
  switch (mode) {
    case ParticleDepthMode::kBackground:
      return kBackgroundBucket;
    case ParticleDepthMode::kOverlayBack:
      return kOverlayBack;
    case ParticleDepthMode::kOverlayMiddle:
      return kOverlayMiddle;
    case ParticleDepthMode::kOverlayFront:
      return kOverlayFront;
    default:
      return depth;
  }
}

std::uint32_t ParticleColor(const ParticleColorSamples& samples) {
  std::uint32_t color = 0;
  unsigned shift = 0;
  for (const auto& sample : {samples.red, samples.green, samples.blue}) {
    std::uint32_t channel = kDefaultColorChannel;
    if (sample.has_value()) channel = *sample == 0 ? 0 : *sample * 2 - 1;
    color |= channel << shift;
    shift += kByteShift;
  }
  return color;
}

ParticleQuad BuildParticleQuad(const ParticleTexture& texture,
                               ParticleScreenPoint centre, std::int16_t cosine,
                               std::int16_t sine, std::uint32_t color) {
  ParticleQuad quad{};
  quad.semi_transparent = (texture.clut & kSemiTransparentClut) != 0;
  quad.color = color;
  for (std::size_t corner = 0; corner < kParticleQuadCorners; ++corner) {
    const std::int64_t x = texture.corners.at(corner).x;
    const std::int64_t y = texture.corners.at(corner).y;
    const auto dx = (cosine * x - sine * y) >> kFixedShift;
    const auto dy = (sine * x + cosine * y) >> kFixedShift;
    quad.vertices.at(corner) = {ClampedAxis(centre.x, dx),
                                ClampedAxis(centre.y, dy)};
  }
  const std::uint32_t uv =
      texture.u | (static_cast<std::uint32_t>(texture.v) << kByteShift);
  const std::uint32_t down = static_cast<std::uint32_t>(texture.h)
                             << kByteShift;
  quad.uv = {uv, uv + texture.w, static_cast<std::uint16_t>(uv + down),
             static_cast<std::uint16_t>(uv + texture.w + down)};
  quad.clut = texture.clut;
  quad.tpage = texture.tpage;
  return quad;
}

// decomp: func_800D6E44 (src/BATTLE/BATTLE.PRG/6E644.s:19).
void DrawAnimatedSpriteParticles(std::span<const std::size_t> particles,
                                 ParticleRenderer& renderer) {
  std::size_t quads = 0;
  for (const auto particle : particles) {
    renderer.AdvanceTextureAnimation(particle);
    const auto projection = renderer.Project(particle);
    if (!projection) {
      renderer.Culled(particle, ParticleCullReason::kBelowGround);
      continue;
    }
    const auto appearance = renderer.Appearance(particle);
    const auto bucket = ParticleOrderingBucket(
        appearance.depth_mode, projection->depth, renderer.NearClip());
    if (!bucket) {
      renderer.Culled(particle, ParticleCullReason::kDepthClip);
      continue;
    }
    const auto frame = renderer.TextureFrame(particle);
    if (*bucket < -kParticleForegroundSlots || !frame) {
      Fail("Particle rendering has no texture frame or ordering bucket");
    }
    auto rotation = appearance.rotation;
    if (appearance.follows_velocity) {
      rotation = RotationTowards(projection->centre,
                                 renderer.ProjectVelocityLead(particle));
      renderer.SetRotation(particle, rotation);
    }
    renderer.BeginParticle(particle, *bucket, frame->texture_count);
    const auto scale =
        static_cast<int>(std::int64_t{renderer.ProjectionDistance()} *
                         (1 << kProjectionShift) / projection->depth);
    const auto cosine =
        Scale(scale, VS::Core::Math::Cosine(rotation), appearance.size);
    const auto sine =
        Scale(scale, VS::Core::Math::Sine(rotation), appearance.size);
    const auto color = ParticleColor(renderer.ColorSamples(particle));
    for (std::uint32_t index = 0; index < frame->texture_count; ++index) {
      renderer.DrawQuad(
          *bucket,
          BuildParticleQuad(renderer.Texture(frame->first_texture + index),
                            projection->centre, cosine, sine, color));
      ++quads;
    }
    renderer.EndParticle(*bucket);
  }
  renderer.Finish(particles.size(), quads);
}

// decomp: func_800D6E24 (src/BATTLE/BATTLE.PRG/5BF94.c:8554).
void DrawEffectParticleList(std::span<const std::size_t> particles,
                            ParticleRenderer& renderer) {
  DrawAnimatedSpriteParticles(particles, renderer);
}

}  // namespace VS::Effects
