#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/ParticleRender.hpp"

namespace {
using VS::Effects::ParticleAppearance;
using VS::Effects::ParticleColorSamples;
using VS::Effects::ParticleCullReason;
using VS::Effects::ParticleDepthMode;
using VS::Effects::ParticleProjection;
using VS::Effects::ParticleQuad;
using VS::Effects::ParticleScreenPoint;
using VS::Effects::ParticleTexture;
using VS::Effects::ParticleTextureFrame;

struct FakeParticle {
  std::optional<ParticleProjection> projection;
  ParticleAppearance appearance{};
  std::optional<ParticleTextureFrame> frame;
  ParticleScreenPoint lead{};
  ParticleColorSamples colors{};
};

ParticleTexture SquareTexture(std::uint16_t clut) {
  ParticleTexture texture{};
  texture.corners = {{{-8, -8}, {8, -8}, {-8, 8}, {8, 8}}};
  texture.u = 16;
  texture.v = 32;
  texture.w = 15;
  texture.h = 15;
  texture.clut = clut;
  texture.tpage = 0x12;
  return texture;
}

class RecordingRenderer final : public VS::Effects::ParticleRenderer {
 public:
  explicit RecordingRenderer(std::vector<FakeParticle> particles)
      : particles_(std::move(particles)) {}

  void AdvanceTextureAnimation(std::size_t particle) override {
    events.push_back("advance " + std::to_string(particle));
  }
  [[nodiscard]] std::optional<ParticleProjection> Project(
      std::size_t particle) override {
    events.push_back("project " + std::to_string(particle));
    return particles_.at(particle).projection;
  }
  [[nodiscard]] ParticleAppearance Appearance(
      std::size_t particle) const override {
    return particles_.at(particle).appearance;
  }
  [[nodiscard]] std::int32_t NearClip() const override { return 16; }
  [[nodiscard]] std::int32_t ProjectionDistance() const override { return 512; }
  [[nodiscard]] std::optional<ParticleTextureFrame> TextureFrame(
      std::size_t particle) const override {
    return particles_.at(particle).frame;
  }
  [[nodiscard]] ParticleScreenPoint ProjectVelocityLead(
      std::size_t particle) override {
    events.push_back("lead " + std::to_string(particle));
    return particles_.at(particle).lead;
  }
  void SetRotation(std::size_t particle, std::int16_t rotation) override {
    rotations[particle] = rotation;
  }
  [[nodiscard]] ParticleColorSamples ColorSamples(
      std::size_t particle) override {
    return particles_.at(particle).colors;
  }
  [[nodiscard]] ParticleTexture Texture(std::uint32_t index) const override {
    return SquareTexture(static_cast<std::uint16_t>(index));
  }
  void Culled(std::size_t particle, ParticleCullReason reason) override {
    events.push_back(
        (reason == ParticleCullReason::kBelowGround ? "below " : "clip ") +
        std::to_string(particle));
  }
  void BeginParticle(std::size_t particle, int bucket,
                     std::uint32_t quad_count) override {
    events.push_back("begin " + std::to_string(particle) + " @" +
                     std::to_string(bucket) + " x" +
                     std::to_string(quad_count));
  }
  void DrawQuad(int bucket, const ParticleQuad& quad) override {
    events.push_back("quad @" + std::to_string(bucket));
    quads.push_back(quad);
  }
  void EndParticle(int bucket) override {
    events.push_back("end @" + std::to_string(bucket));
  }
  void Finish(std::size_t particles, std::size_t quad_count) override {
    events.push_back("finish " + std::to_string(particles) + " " +
                     std::to_string(quad_count));
  }

  std::vector<std::string> events;
  std::vector<ParticleQuad> quads;
  std::map<std::size_t, std::int16_t> rotations;

 private:
  std::vector<FakeParticle> particles_;
};

FakeParticle DrawnParticle(std::uint32_t textures) {
  FakeParticle particle;
  particle.projection = ParticleProjection{{100, 50}, 1024};
  particle.appearance = {ParticleDepthMode::kActual, false, 4, 0};
  particle.frame = ParticleTextureFrame{0x8000, textures};
  return particle;
}
}  // namespace

TEST_CASE("Particle depth modes bias their ordering bucket",
          "[effects][particles][core]") {
  using VS::Effects::ParticleOrderingBucket;
  CHECK(ParticleOrderingBucket(ParticleDepthMode::kActual, 100, 16) == 100);
  CHECK(ParticleOrderingBucket(ParticleDepthMode::kFar, 100, 16) == 108);
  CHECK(ParticleOrderingBucket(ParticleDepthMode::kFarther, 100, 16) == 116);
  CHECK(ParticleOrderingBucket(ParticleDepthMode::kNear, 100, 16) == 92);
  CHECK(ParticleOrderingBucket(ParticleDepthMode::kNearer, 100, 16) == 84);
  CHECK(ParticleOrderingBucket(ParticleDepthMode::kBackground, 100, 16) ==
        VS::Effects::kParticleMaximumDepth - 2);
  CHECK(ParticleOrderingBucket(ParticleDepthMode::kOverlayBack, 100, 16) ==
        -15);
  CHECK(ParticleOrderingBucket(ParticleDepthMode::kOverlayMiddle, 100, 16) ==
        -13);
  CHECK(ParticleOrderingBucket(ParticleDepthMode::kOverlayFront, 100, 16) ==
        -11);
}

TEST_CASE("Particle depth clipping applies after the depth-mode bias",
          "[effects][particles][core]") {
  using VS::Effects::ParticleOrderingBucket;
  CHECK_FALSE(ParticleOrderingBucket(ParticleDepthMode::kActual, 0, -100));
  CHECK_FALSE(ParticleOrderingBucket(ParticleDepthMode::kNear, 24, 16));
  CHECK(ParticleOrderingBucket(ParticleDepthMode::kNear, 25, 16) == 17);
  CHECK_FALSE(ParticleOrderingBucket(ParticleDepthMode::kFar, 2040, 16));
  CHECK(ParticleOrderingBucket(ParticleDepthMode::kActual, 2047, 16) == 2047);
}

TEST_CASE("Particle colors default absent curves and widen sampled channels",
          "[effects][particles][core]") {
  using VS::Effects::ParticleColor;
  CHECK(ParticleColor({}) == 0x808080U);
  CHECK(ParticleColor({0U, 1U, 64U}) == (0U | (1U << 8) | (127U << 16)));
  CHECK(ParticleColor({std::nullopt, 128U, std::nullopt}) ==
        (128U | (255U << 8) | (128U << 16)));
  CHECK(ParticleColor({200U, std::nullopt, std::nullopt}) ==
        (399U | (128U << 8) | (128U << 16)));
}

TEST_CASE("Particle quads rotate corners and keep source UV carries",
          "[effects][particles][core]") {
  auto texture = SquareTexture(0x8001);
  texture.u = 250;
  texture.w = 10;
  const auto quad =
      VS::Effects::BuildParticleQuad(texture, {100, 50}, 0, 4096, 0x123456);
  CHECK(quad.semi_transparent);
  CHECK(quad.color == 0x123456U);
  CHECK(quad.vertices[0] == ParticleScreenPoint{108, 42});
  CHECK(quad.vertices[3] == ParticleScreenPoint{92, 58});
  const std::uint32_t uv = 250U | (32U << 8);
  CHECK(quad.uv ==
        std::array<std::uint32_t, 4>{uv, uv + 10, (uv + (15U << 8)) & 0xFFFFU,
                                     (uv + 10 + (15U << 8)) & 0xFFFFU});
  CHECK(quad.clut == 0x8001);
  CHECK(quad.tpage == 0x12);
}

TEST_CASE("Particle quads clamp corners to signed 16-bit screen space",
          "[effects][particles][core]") {
  const auto edge = VS::Effects::BuildParticleQuad(SquareTexture(0),
                                                   {32760, -32760}, 4096, 0, 0);
  CHECK_FALSE(edge.semi_transparent);
  CHECK(edge.vertices[0] == ParticleScreenPoint{32752, -32768});
  CHECK(edge.vertices[3] == ParticleScreenPoint{32767, -32752});
}

TEST_CASE("Particle drawing preserves source cull order and quad emission",
          "[effects][particles][core]") {
  auto below = DrawnParticle(1);
  below.projection.reset();
  auto clipped = DrawnParticle(1);
  clipped.projection = ParticleProjection{{100, 50}, 8};
  auto drawn = DrawnParticle(2);
  drawn.colors = {1U, std::nullopt, 0U};
  RecordingRenderer renderer({below, clipped, drawn});
  const std::array<std::size_t, 3> order{2, 0, 1};
  VS::Effects::DrawAnimatedSpriteParticles(order, renderer);
  CHECK(renderer.events == std::vector<std::string>{
                               "advance 2", "project 2", "begin 2 @1024 x2",
                               "quad @1024", "quad @1024", "end @1024",
                               "advance 0", "project 0", "below 0", "advance 1",
                               "project 1", "clip 1", "finish 3 2"});
  CHECK(renderer.rotations.empty());
}

TEST_CASE("Particle drawing scales textured corners by projected depth",
          "[effects][particles][core]") {
  auto drawn = DrawnParticle(2);
  drawn.colors = {1U, std::nullopt, 0U};
  RecordingRenderer renderer({drawn});
  const std::array<std::size_t, 1> order{0};
  VS::Effects::DrawAnimatedSpriteParticles(order, renderer);
  REQUIRE(renderer.quads.size() == 2);
  // Scale = 512 * 1024 / 1024 * (4096 * 4 / 4) >> 12 = 512.
  CHECK(renderer.quads[0].vertices[0] == ParticleScreenPoint{99, 49});
  CHECK(renderer.quads[0].vertices[3] == ParticleScreenPoint{101, 51});
  CHECK(renderer.quads[0].color == (1U | (128U << 8)));
  CHECK(renderer.quads[0].clut == 0x8000);
  CHECK(renderer.quads[1].clut == 0x8001);
}

TEST_CASE("Velocity-following particles face their projected lead",
          "[effects][particles][core]") {
  auto moving = DrawnParticle(1);
  moving.appearance.follows_velocity = true;
  moving.appearance.rotation = 77;
  moving.lead = {100, 60};
  auto still = DrawnParticle(1);
  still.appearance.follows_velocity = true;
  still.appearance.rotation = 77;
  still.lead = {100, 50};
  RecordingRenderer renderer({moving, still});
  const std::array<std::size_t, 2> order{0, 1};
  VS::Effects::DrawEffectParticleList(order, renderer);
  CHECK(renderer.rotations.at(0) ==
        static_cast<std::int16_t>(VS::Core::Math::Arctangent2(-10, 0)));
  CHECK(renderer.rotations.at(1) == 0);
  REQUIRE(renderer.quads.size() == 2);
  // A stationary lead resets the rotation to zero, as in the source.
  CHECK(renderer.quads[1].vertices[0] == ParticleScreenPoint{99, 49});
}
