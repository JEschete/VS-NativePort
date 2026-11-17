#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/Matrix.hpp"
#include "Effects/ParticleEmitter.hpp"

namespace {
namespace effects = VS::Effects;
namespace math = VS::Core::Math;
using effects::EmissionPair;
using effects::EmissionVector;
using effects::ParticleEmissionHandle;
using effects::ParticleEmissionStatus;
using math::Vec3;

effects::EmissionVectorRange Constant(Vec3 value) {
  return {math::ScalarSpan{value.x, value.x},
          math::ScalarSpan{value.y, value.y},
          math::ScalarSpan{value.z, value.z}};
}

class EmissionHost final : public effects::ParticleEmissionHost {
 public:
  std::array<effects::EmittedParticle, 4> particles{};
  std::optional<ParticleEmissionHandle> first;
  std::size_t allocations{};
  std::size_t allocation_limit = particles.size();
  bool truncated_chain = false;
  std::vector<std::size_t> released;
  std::vector<std::array<std::size_t, 3>> textures;

  [[nodiscard]] std::optional<ParticleEmissionHandle> Allocate() override {
    if (allocations >= allocation_limit) return std::nullopt;
    const ParticleEmissionHandle handle{allocations++};
    auto& particle = particles.at(handle.index);
    particle.next = first;
    particle.age = 255;
    first = handle;
    return handle;
  }
  [[nodiscard]] std::optional<effects::EmittedParticle> Read(
      ParticleEmissionHandle handle) const override {
    if (handle.index >= particles.size()) return std::nullopt;
    auto particle = particles.at(handle.index);
    if (truncated_chain && allocations == 3 && handle.index == 2) {
      particle.next.reset();
    }
    return particle;
  }
  void Store(ParticleEmissionHandle handle,
             const effects::EmittedParticle& value) override {
    auto& particle = particles.at(handle.index);
    const auto next = particle.next;
    particle = value;
    particle.next = next;
  }
  void InitializeTexture(ParticleEmissionHandle handle, std::uint8_t texture,
                         std::uint8_t animation) override {
    textures.push_back({handle.index, texture, animation});
  }
  void Release(ParticleEmissionHandle handle) override {
    first = particles.at(handle.index).next;
    released.push_back(handle.index);
  }
};

struct Emission {
  std::array<std::int32_t, 33> samples{};
  effects::ParticleEmissionDefinition definition{};
  effects::ParticleEmissionContext context{};
  effects::ParticleEmissionScratch scratch{};
  std::int32_t counter{};
  VS::Core::Rand15 random{1};
  EmissionHost host;

  Emission() {
    definition.flags = 0xA038;
    definition.count = {1, 1};
    definition.interval = {1, 1};
    definition.texture = 9;
    definition.animation = 2;
    context.capacity = 4;
    context.samples = samples;
    context.transform.rotation = math::IdentityRotation();
  }
  [[nodiscard]] effects::ParticleEmissionResult Run() {
    return effects::EmitParticleEvent(definition, context, scratch, counter,
                                      random, host);
  }
  void Vector(EmissionVector field, Vec3 value) {
    definition.vectors.at(static_cast<std::size_t>(field)) = Constant(value);
  }
  void Pair(EmissionPair field, std::int32_t first, std::int32_t second) {
    definition.pairs.at(static_cast<std::size_t>(field)) = {
        math::ScalarSpan{first, first}, math::ScalarSpan{second, second}};
  }
};
}  // namespace

TEST_CASE("F01515 prepends allocations then visits new particles newest first",
          "[F01515][emitter-rings][effects]") {
  Emission emission;
  emission.definition.flags = 0xB008;
  emission.definition.count = {3, 3};
  emission.definition.position = Constant({10, 20, 30});
  emission.definition.extent = Constant({2, 3, 4});
  emission.counter = 6;
  const auto result = emission.Run();
  CHECK(result.count == 3);
  CHECK(emission.host.first == ParticleEmissionHandle{2});
  CHECK(emission.host.textures == std::vector<std::array<std::size_t, 3>>{
                                      {0, 9, 2}, {1, 9, 2}, {2, 9, 2}});
  CHECK(emission.host.particles[2].motion.position ==
        Vec3{12 * 4096, 23 * 4096, 34 * 4096});
  CHECK(emission.host.particles[1].motion.position ==
        Vec3{8 * 4096, 17 * 4096, 26 * 4096});
  CHECK(emission.host.particles[0].motion.position ==
        Vec3{12 * 4096, 17 * 4096, 26 * 4096});
  CHECK(emission.counter == 1);
  CHECK(emission.random.State() == 1);
}

TEST_CASE("F01515 random draws remain phase ordered across the new list",
          "[F01515][emitter-rings][effects]") {
  Emission emission;
  emission.definition.flags = 0xA008;
  emission.definition.count = {2, 2};
  emission.definition.extent = Constant({2, 3, 4});
  emission.Vector(EmissionVector::kForceMin, {10, 20, 30});
  emission.Vector(EmissionVector::kForceMax, {14, 24, 34});
  emission.Pair(EmissionPair::kMass, 10, 14);
  REQUIRE(emission.Run().status == ParticleEmissionStatus::kEmitted);
  CHECK(emission.host.particles[1].motion.position == Vec3{-8192, 0, 12288});
  CHECK(emission.host.particles[0].motion.position == Vec3{4096, 0, -4096});
  CHECK(emission.host.particles[1].motion.force == Vec3{10, 22, 31});
  CHECK(emission.host.particles[0].motion.force == Vec3{13, 20, 30});
  CHECK(emission.host.particles[1].motion.mass == 11);
  CHECK(emission.host.particles[0].motion.mass == 13);
  VS::Core::Rand15 expected(1);
  for (int draw = 0; draw < 16; ++draw) expected.Next();
  CHECK(emission.random.State() == expected.State());
}

TEST_CASE("F01515 interval zero retains the R3000 remainder gate",
          "[F01515][emitter-rings][effects]") {
  Emission emission;
  emission.definition.interval = {};
  emission.scratch.position = {11, 12, 13};
  SECTION("nonzero frame does not allocate or clear translation") {
    emission.context.frame = 1;
    const auto result = emission.Run();
    CHECK(result.status == ParticleEmissionStatus::kIntervalSkipped);
    CHECK_FALSE(result.clear_translation);
    CHECK(emission.scratch.position == Vec3{11, 12, 13});
    CHECK(emission.host.allocations == 0);
    CHECK(emission.random.State() == 1);
  }
  SECTION("frame zero still emits") {
    CHECK(emission.Run().count == 1);
    CHECK(emission.host.allocations == 1);
  }
}

TEST_CASE(
    "F01515 clamps to available slots and keeps the repaired empty contract",
    "[F01515][emitter-rings][effects]") {
  Emission emission;
  SECTION("oversized request consumes only available slots") {
    emission.definition.count = {9, 9};
    emission.context.used = 2;
    CHECK(emission.Run().count == 2);
    CHECK(emission.host.allocations == 2);
  }
  SECTION("full pool returns before allocation") {
    emission.context.used = emission.context.capacity;
    CHECK(emission.Run().status == ParticleEmissionStatus::kEmpty);
    CHECK(emission.host.allocations == 0);
  }
  SECTION("negative and zero requests do not touch an uninitialized node") {
    for (const int count : {-1, 0}) {
      emission.definition.count = {count, count};
      const auto result = emission.Run();
      CHECK(result.status == ParticleEmissionStatus::kEmpty);
      CHECK(result.clear_translation);
    }
    CHECK(emission.host.allocations == 0);
  }
}

TEST_CASE(
    "F01515 refusal rolls back only this emission even with a broken chain",
    "[F01515][emitter-rings][effects]") {
  Emission emission;
  emission.definition.count = {3, 3};
  emission.host.first = ParticleEmissionHandle{3};
  emission.host.particles[3].motion.position = {41, 42, 43};
  SECTION("partial allocation") {
    emission.host.allocation_limit = 2;
    CHECK(emission.Run().status == ParticleEmissionStatus::kAllocationRefused);
    CHECK(emission.host.released == std::vector<std::size_t>{1, 0});
  }
  SECTION("new list truncated after allocation") {
    emission.host.truncated_chain = true;
    CHECK(emission.Run().status == ParticleEmissionStatus::kInvalidLinks);
    CHECK(emission.host.released == std::vector<std::size_t>{2, 1, 0});
  }
  CHECK(emission.host.first == ParticleEmissionHandle{3});
  CHECK(emission.host.particles[3].motion.position == Vec3{41, 42, 43});
}

TEST_CASE("F01515 validates samples pool counts and required aim before writes",
          "[F01515][emitter-rings][effects]") {
  Emission emission;
  auto expected = ParticleEmissionStatus::kMissingSamples;
  SECTION("curve outside current sample extent") {
    emission.definition.curves[0] = 33;
  }
  SECTION("impossible used count") {
    emission.context.used = 5;
    expected = ParticleEmissionStatus::kInvalidPool;
  }
  SECTION("missing target for aimed direction") {
    emission.definition.flags = 0x4000;
    expected = ParticleEmissionStatus::kMissingAim;
  }
  CHECK(emission.Run().status == expected);
  CHECK(emission.host.allocations == 0);
  CHECK(emission.scratch.flags == 0);
  CHECK(emission.random.State() == 1);
}

TEST_CASE(
    "F01515 explicit transparency and variant precede definition and parent",
    "[F01515][emitter-rings][effects]") {
  Emission emission;
  emission.context.parent =
      effects::ParticleEmissionParent{.transparency = 12, .variant = 13};
  emission.context.transparency_override = 257;
  emission.context.variant = 31;
  emission.definition.transparency = 6;
  REQUIRE(emission.Run().count == 1);
  CHECK(emission.host.particles[0].transparency == 1);
  CHECK(emission.host.particles[0].variant == 15);
}

TEST_CASE(
    "F01515 inherited transparency and variant retain definition precedence",
    "[F01515][emitter-rings][effects]") {
  Emission emission;
  emission.context.parent =
      effects::ParticleEmissionParent{.transparency = 12, .variant = 13};
  SECTION("definition wins transparency and parent supplies variant") {
    emission.definition.transparency = 6;
    REQUIRE(emission.Run().count == 1);
    CHECK(emission.host.particles[0].transparency == 6);
    CHECK(emission.host.particles[0].variant == 13);
  }
  SECTION("parent values are otherwise inherited") {
    REQUIRE(emission.Run().count == 1);
    CHECK(emission.host.particles[0].transparency == 12);
  }
  SECTION("no parent uses default transparency") {
    emission.context.parent.reset();
    REQUIRE(emission.Run().count == 1);
    CHECK(emission.host.particles[0].transparency == 4);
  }
}

TEST_CASE("F01515 unselected modes retain particle motion fields",
          "[F01515][emitter-rings][effects]") {
  Emission emission;
  emission.definition.flags = 0xA1B8;
  auto& particle = emission.host.particles[0];
  particle.motion.position = {11, 12, 13};
  particle.motion.velocity = {21, 22, 23};
  particle.motion.target = {31, 32, 33};
  REQUIRE(emission.Run().count == 1);
  CHECK(particle.motion.position == Vec3{11, 12, 13});
  CHECK(particle.motion.velocity == Vec3{21, 22, 23});
  CHECK(particle.motion.target == math::Position3{31, 32, 33});
}

TEST_CASE("F01515 unselected modes still publish particle attributes",
          "[F01515][emitter-rings][effects]") {
  Emission emission;
  emission.definition.flags = 0xA1B8;
  emission.definition.color_curves = {3, 4, 5};
  emission.definition.end_event = 7;
  emission.definition.tick_event = 9;
  emission.definition.transform = 2;
  emission.definition.attraction_transform = 3;
  REQUIRE(emission.Run().count == 1);
  const auto& particle = emission.host.particles[0];
  CHECK(particle.color_curves == std::array<std::uint8_t, 3>{3, 4, 5});
  CHECK(particle.age == 255);
  CHECK(particle.end_event == 7);
  CHECK(particle.tick_event == 9);
  CHECK(particle.transform == 2);
  CHECK(particle.attraction_transform == 3);
}

TEST_CASE(
    "F01515 zero-height cone and inward zero-vector keep original arithmetic",
    "[F01515][emitter-rings][effects]") {
  Emission emission;
  SECTION("cone keeps zero-divisor quotient instead of suppressing spawn") {
    emission.definition.flags = 0xA810;
    emission.definition.extent = Constant({3, 0, 5});
    REQUIRE(emission.Run().count == 1);
    CHECK(emission.host.particles[0].motion.position == Vec3{-4096, 0, -4096});
  }
  SECTION("inward zero vector points up at the sampled speed") {
    emission.definition.flags = 0x2000;
    emission.Pair(EmissionPair::kSpeed, 2, 2);
    REQUIRE(emission.Run().count == 1);
    CHECK(emission.host.particles[0].motion.velocity == Vec3{0, 64, 0});
  }
}

TEST_CASE("F01515 aimed and actor direction modes respect the rotation flag",
          "[F01515][emitter-rings][effects]") {
  Emission emission;
  emission.Pair(EmissionPair::kSpeed, 10, 10);
  emission.context.aim_target = Vec3{100, 0, 0};
  SECTION("aim orientation is not applied without the rotate flag") {
    emission.definition.flags = 0x4038;
    REQUIRE(emission.Run().count == 1);
    CHECK(emission.host.particles[0].motion.velocity == Vec3{0, 0, 320});
  }
  SECTION("aim orientation is applied with the rotate flag") {
    emission.definition.flags = 0x44038;
    REQUIRE(emission.Run().count == 1);
    CHECK(emission.host.particles[0].motion.velocity.x > 0);
    CHECK(emission.host.particles[0].motion.velocity.z == 0);
  }
  SECTION("actor direction always applies its world orientation") {
    emission.definition.flags = 0x8038;
    emission.context.transform.rotation = math::BuildRotationXyz({0, 1024, 0});
    REQUIRE(emission.Run().count == 1);
    CHECK(emission.host.particles[0].motion.velocity == Vec3{320, 0, 0});
  }
}

TEST_CASE(
    "F01515 actor attraction retains the original cross-axis X jitter bound",
    "[F01515][emitter-rings][effects]") {
  Emission emission;
  emission.definition.flags = 0xA0F8;
  emission.context.attraction_translation = {100, 200, 300};
  emission.Vector(EmissionVector::kAttractionOffset, {1, 2, 3});
  emission.Vector(EmissionVector::kAttractionJitter, {5, 2, 0});
  REQUIRE(emission.Run().count == 1);
  // Seed 1: 16838 % 7 - 2 = 1; using -X would instead give 3.
  CHECK(emission.host.particles[0].motion.target ==
        math::Position3{102, 202, 303});
}

TEST_CASE(
    "F01515 spawn rotation saturates IR outputs without changing velocity MACs",
    "[F01515][emitter-rings][effects]") {
  Emission emission;
  SECTION("spawn stores saturated IR values") {
    emission.definition.flags = 0x4B008;
    emission.definition.extent = Constant({32767, 32767, 0});
    emission.context.transform.rotation.m[0] = {4096, -4096, 0};
    REQUIRE(emission.Run().count == 1);
    CHECK(emission.host.particles[0].motion.position.x == 32767 * 4096);
    CHECK(emission.scratch.aim.x == 32767);
  }
  SECTION("velocity retains full-width MAC values") {
    emission.definition.flags = 0x40038;
    emission.Pair(EmissionPair::kSpeed, 1024, 1024);
    emission.context.transform.rotation.m[0] = {0, 0, 8192};
    REQUIRE(emission.Run().count == 1);
    CHECK(emission.host.particles[0].motion.velocity == Vec3{65536, 0, 32768});
  }
}

TEST_CASE(
    "F01515 zero polygon dimensions normalize counts and retain vertex cycling",
    "[F01515][emitter-rings][effects]") {
  Emission emission;
  emission.definition.flags = 0xB030;
  emission.definition.count = {2, 2};
  emission.definition.extent = Constant({100, 50, 200});
  emission.counter = 17;
  REQUIRE(emission.Run().count == 2);
  CHECK(emission.scratch.ring_segments == 1);
  CHECK(emission.scratch.ring_positions == 1);
  CHECK(emission.counter == 0);
  CHECK(emission.host.particles[0].motion.position ==
        emission.host.particles[1].motion.position);
  CHECK(emission.host.particles[0].motion.position.y == 0);
  CHECK(emission.random.State() == 1);
}
