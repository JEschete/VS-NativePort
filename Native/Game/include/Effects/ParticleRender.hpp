#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Effects {

inline constexpr int kParticleMaximumDepth = 2048;
inline constexpr int kParticleForegroundSlots = 16;
inline constexpr std::size_t kParticleQuadCorners = 4;

// Source 4-bit depth override; values past kOverlayFront use the projected
// depth unchanged.
enum class ParticleDepthMode : std::uint8_t {
  kActual,
  kFar,
  kFarther,
  kNear,
  kNearer,
  kBackground,
  kOverlayBack,
  kOverlayMiddle,
  kOverlayFront
};

enum class ParticleCullReason : std::uint8_t { kBelowGround, kDepthClip };

struct ParticleScreenPoint {
  std::int16_t x;
  std::int16_t y;
  friend bool operator==(const ParticleScreenPoint&,
                         const ParticleScreenPoint&) = default;
};

struct ParticleProjection {
  ParticleScreenPoint centre;
  std::int32_t depth;
};

struct ParticleAppearance {
  ParticleDepthMode depth_mode;
  bool follows_velocity;
  // Source byte 0x60; the renderer uses it as the sprite scale.
  std::uint8_t size;
  // 4096-per-turn screen rotation.
  std::int16_t rotation;
};

struct ParticleTextureFrame {
  std::uint32_t first_texture;
  std::uint32_t texture_count;
};

struct ParticleTexture {
  std::array<ParticleScreenPoint, kParticleQuadCorners> corners;
  std::uint8_t u;
  std::uint8_t v;
  std::uint8_t w;
  std::uint8_t h;
  std::uint16_t clut;
  std::uint16_t tpage;
};

struct ParticleQuad {
  bool semi_transparent;
  // r | g << 8 | b << 16; channels above 255 carry as in the source.
  std::uint32_t color;
  std::array<ParticleScreenPoint, kParticleQuadCorners> vertices;
  // Packed u | v << 8 per corner; corner 1 keeps the source u + w carry.
  std::array<std::uint32_t, kParticleQuadCorners> uv;
  std::uint16_t clut;
  std::uint16_t tpage;
};

// Raw curve samples for the next age; empty when the channel has no curve.
struct ParticleColorSamples {
  std::optional<std::uint32_t> red;
  std::optional<std::uint32_t> green;
  std::optional<std::uint32_t> blue;
};

class ParticleRenderer {
 public:
  ParticleRenderer() = default;
  ParticleRenderer(const ParticleRenderer&) = delete;
  ParticleRenderer& operator=(const ParticleRenderer&) = delete;
  ParticleRenderer(ParticleRenderer&&) = delete;
  ParticleRenderer& operator=(ParticleRenderer&&) = delete;
  virtual ~ParticleRenderer() = default;

  virtual void AdvanceTextureAnimation(std::size_t particle) = 0;
  // Selects the particle's view and projects it; empty when culled below
  // ground.
  [[nodiscard]] virtual std::optional<ParticleProjection> Project(
      std::size_t particle) = 0;
  [[nodiscard]] virtual ParticleAppearance Appearance(
      std::size_t particle) const = 0;
  [[nodiscard]] virtual std::int32_t NearClip() const = 0;
  [[nodiscard]] virtual std::int32_t ProjectionDistance() const = 0;
  [[nodiscard]] virtual std::optional<ParticleTextureFrame> TextureFrame(
      std::size_t particle) const = 0;
  // Projects the position advanced by velocity >> 5 through the view
  // selected by Project.
  [[nodiscard]] virtual ParticleScreenPoint ProjectVelocityLead(
      std::size_t particle) = 0;
  virtual void SetRotation(std::size_t particle, std::int16_t rotation) = 0;
  [[nodiscard]] virtual ParticleColorSamples ColorSamples(
      std::size_t particle) = 0;
  [[nodiscard]] virtual ParticleTexture Texture(std::uint32_t index) const = 0;
  virtual void Culled(std::size_t particle, ParticleCullReason reason) = 0;
  // Reserves space for quad_count quads and opens the particle's draw mode.
  virtual void BeginParticle(std::size_t particle, int bucket,
                             std::uint32_t quad_count) = 0;
  virtual void DrawQuad(int bucket, const ParticleQuad& quad) = 0;
  virtual void EndParticle(int bucket) = 0;
  virtual void Finish(std::size_t particles, std::size_t quads) = 0;
};

// Ordering-table bucket for a projected depth; empty when depth-clipped.
[[nodiscard]] std::optional<int> ParticleOrderingBucket(
    ParticleDepthMode mode, std::int32_t projected_depth,
    std::int32_t near_clip);

[[nodiscard]] std::uint32_t ParticleColor(const ParticleColorSamples& samples);

[[nodiscard]] ParticleQuad BuildParticleQuad(const ParticleTexture& texture,
                                             ParticleScreenPoint centre,
                                             std::int16_t cosine,
                                             std::int16_t sine,
                                             std::uint32_t color);

void DrawAnimatedSpriteParticles(std::span<const std::size_t> particles,
                                 ParticleRenderer& renderer);

void DrawEffectParticleList(std::span<const std::size_t> particles,
                            ParticleRenderer& renderer);

}  // namespace VS::Effects
