#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Gte.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/ParticleMotion.hpp"
#include "Effects/ParticleMotionLive.hpp"
#include "Effects/ParticleTextureAnimation.hpp"
#include "Effects/ParticleTextureAnimationLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Runtime.hpp"
#include "Legacy/Psx.hpp"

using VS::Core::Gte::MATRIX;
using VS::Core::Gte::SetRotMatrix;
using VS::Core::Gte::VECTOR;

namespace {
namespace fx = VS::Effects;
using VS::Core::Math::Vec3;
using Particle = VS::Effects::LiveParticle;

[[nodiscard]] constexpr std::uint32_t Point(std::int16_t x, std::int16_t y) {
  return static_cast<std::uint16_t>(x) |
         (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y)) << 16);
}

// A rotation that records its input and returns a fixed vector.
class TestRotation final : public fx::ParticleRotation {
 public:
  Vec3 result{};
  std::vector<fx::ParticleOffset> rotated;

  Vec3 Rotate(fx::ParticleOffset offset) override {
    rotated.push_back(offset);
    return result;
  }
};

// Motion records linked through the particles, as in the game heap.
class TestMotionRecords final : public fx::ParticleMotionRecords {
 public:
  std::vector<fx::ParticleMotionState> states;
  std::vector<Particle*> order;

  explicit TestMotionRecords(std::size_t count) : states(count) {}
  Particle* Next(Particle& particle) override { return particle.next; }
  fx::ParticleMotionState Motion(Particle& particle) override {
    order.push_back(&particle);
    return states.at(static_cast<std::size_t>(particle.age));
  }
  void StoreMotion(Particle& particle,
                   const fx::ParticleMotionState& state) override {
    states.at(static_cast<std::size_t>(particle.age)) = state;
  }
};

// A tick host over particles whose lifecycle fields it keeps, logging every
// service call.
class TestTickHost final : public fx::ParticleTickHost {
 public:
  std::vector<std::string> calls;
  std::vector<fx::ParticleLifecycle> lifecycles;
  // A lifetime an event emit stores, as an emitted child's code could.
  std::int16_t lifetime_on_emit = 0;
  bool store_on_emit = false;

  explicit TestTickHost(std::size_t count) : lifecycles(count) {}

  void Integrate(Particle* first) override {
    calls.push_back(first == nullptr ? "integrate none" : "integrate");
  }
  void Draw() override { calls.emplace_back("draw"); }
  Particle* Next(Particle& particle) override { return particle.next; }
  fx::ParticleLifecycle Lifecycle(Particle& particle) override {
    return lifecycles.at(Index(particle));
  }
  void StoreAge(Particle& particle, std::uint8_t age) override {
    lifecycles.at(Index(particle)).age = age;
  }
  void StoreLifetime(Particle& particle, std::int16_t lifetime) override {
    lifecycles.at(Index(particle)).lifetime = lifetime;
    calls.push_back("lifetime " + std::to_string(lifetime));
  }
  void Emit(Particle& particle, std::int32_t event) override {
    calls.push_back("emit " + std::to_string(Index(particle)) + " " +
                    std::to_string(event));
    if (store_on_emit) {
      lifecycles.at(Index(particle)).lifetime = lifetime_on_emit;
    }
  }
  void Release(Particle& particle) override {
    calls.push_back("release " + std::to_string(Index(particle)));
    // A released particle returns to the free list.
    particle.next = nullptr;
  }

 private:
  static std::size_t Index(const Particle& particle) {
    return static_cast<std::size_t>(particle.transparency);
  }
};

// Particles numbered by their transparency, linked in order.
template <std::size_t N>
std::array<Particle, N> LinkedParticles() {
  std::array<Particle, N> particles{};
  for (std::size_t index = 0; index < N; ++index) {
    particles.at(index).transparency = static_cast<std::uint8_t>(index);
    particles.at(index).age = static_cast<std::uint8_t>(index);
    if (index + 1 < N) {
      particles.at(index).next = &particles.at(index + 1);
    }
  }
  return particles;
}

[[nodiscard]] std::span<const std::byte> Bytes(
    std::span<const std::uint32_t> words) {
  return std::as_bytes(words);
}

[[nodiscard]] constexpr std::uint32_t FrameWord(std::uint16_t frame,
                                                std::uint8_t hold,
                                                std::uint8_t flags = 0) {
  return frame | (static_cast<std::uint32_t>(hold) << 16) |
         (static_cast<std::uint32_t>(flags) << 24);
}

[[nodiscard]] constexpr std::uint32_t OffsetWord(std::int16_t x,
                                                 std::int16_t y) {
  return 0x10000000u | (static_cast<std::uint32_t>(x) & 0x3FFF) |
         ((static_cast<std::uint32_t>(y) & 0x3FFF) << 14);
}

constexpr std::uint32_t kRestart = 0x20000000;
constexpr std::uint32_t kStop = 0x30000000;

// The current package with one texture table and a recorded animation table.
class AnimationPackage {
 public:
  fx::LiveEffectPackage package{};
  std::array<fx::LivePackageTextureFrame, 4> frames{};
  // {s32 count = 2, u16 offsets {12, 20}, u16 pad}, then the programs.
  std::array<std::uint32_t, 6> table{
      2, 12 | (20u << 16), 0, FrameWord(3, 4), kStop, kStop};

  AnimationPackage() {
    package.texture_table_count = 1;
    package.texture_tables[0] = frames.data();
    package.animation_table = reinterpret_cast<std::uint16_t*>(table.data());
    fx::package = &package;
    fx::RecordLiveAnimationTable(std::as_writable_bytes(std::span(table)));
  }
  AnimationPackage(const AnimationPackage&) = delete;
  AnimationPackage& operator=(const AnimationPackage&) = delete;
  AnimationPackage(AnimationPackage&&) = delete;
  AnimationPackage& operator=(AnimationPackage&&) = delete;
  ~AnimationPackage() {
    fx::ForgetLiveAnimationTable(table.data());
    fx::package = saved_;
  }

  [[nodiscard]] const std::uint8_t* Program(std::size_t offset) const {
    return reinterpret_cast<const std::uint8_t*>(table.data()) + offset;
  }

 private:
  fx::LiveEffectPackage* saved_ = fx::package;
};
}  // namespace

TEST_CASE("A line endpoint steps one pixel along the major axis",
          "[particle-motion][F01453]") {
  CHECK(fx::StepScreenPointToward(Point(10, 5), Point(0, 5)) == Point(9, 5));
  CHECK(fx::StepScreenPointToward(Point(0, 10), Point(0, 0)) == Point(0, 9));
  CHECK(fx::StepScreenPointToward(Point(-4, 1), Point(4, 2)) == Point(-3, 1));
  CHECK(fx::StepScreenPointToward(Point(7, 7), Point(7, 7)) == Point(7, 7));
}

TEST_CASE("A line endpoint steps diagonally within twice the minor axis",
          "[particle-motion][F01453]") {
  CHECK(fx::StepScreenPointToward(Point(10, 8), Point(0, 0)) == Point(9, 7));
  CHECK(fx::StepScreenPointToward(Point(0, 0), Point(5, -5)) == Point(1, -1));
  CHECK(fx::StepScreenPointToward(Point(3, 0), Point(0, 5)) == Point(2, 1));
  // A minor axis of exactly half the major moves only the major.
  CHECK(fx::StepScreenPointToward(Point(10, 5), Point(0, 0)) == Point(9, 5));
  CHECK(fx::StepScreenPointToward(Point(3, 0), Point(0, 6)) == Point(3, 1));
}

TEST_CASE("A line endpoint wraps within its halfwords",
          "[particle-motion][F01453]") {
  CHECK(fx::StepScreenPointToward(Point(0, 0), Point(-3, 0)) == Point(-1, 0));
  CHECK(fx::StepScreenPointToward(Point(0, 0), Point(0, -3)) == Point(0, -1));
}

TEST_CASE("The live line step moves the packet's second point",
          "[particle-motion][F01453]") {
  fx::LiveParticleLinePacket packet{};
  packet.line.x0 = 0;
  packet.line.y0 = 0;
  packet.line.x1 = 10;
  packet.line.y1 = 8;
  fx::LiveStepParticleEndpointTowardStart(&packet);
  CHECK(packet.line.x1 == 9);
  CHECK(packet.line.y1 == 7);
  CHECK(packet.line.x0 == 0);
  CHECK(packet.line.y0 == 0);
  fx::LiveStepParticleEndpointTowardStart(nullptr);
}

TEST_CASE("A spawn adds the unrotated offset in Q12 units",
          "[particle-motion][F01514]") {
  TestRotation rotation;
  const fx::ParticleSpawn spawn =
      fx::PlaceParticleSpawn(0x3FFFF, {.x = 1, .y = -2, .z = 3},
                             {.x = 100, .y = 200, .z = 300}, rotation);
  CHECK(rotation.rotated.empty());
  CHECK(spawn.aim == Vec3{.x = 1, .y = -2, .z = 3});
  CHECK(spawn.position == Vec3{.x = 4196, .y = -7992, .z = 12588});
}

TEST_CASE("A spawn rotates its offset under flag 0x40000",
          "[particle-motion][F01514]") {
  TestRotation rotation;
  rotation.result = {.x = 7, .y = -8, .z = 9};
  const fx::ParticleSpawn spawn = fx::PlaceParticleSpawn(
      fx::kParticleRotateSpawnOffset, {.x = 1, .y = 2, .z = 3},
      {.x = std::numeric_limits<std::int32_t>::max(), .y = 0, .z = 0},
      rotation);
  REQUIRE(rotation.rotated.size() == 1);
  CHECK(rotation.rotated.front() == fx::ParticleOffset{.x = 1, .y = 2, .z = 3});
  CHECK(spawn.aim == rotation.result);
  CHECK(spawn.position ==
        Vec3{.x = std::numeric_limits<std::int32_t>::min() + 7 * 4096 - 1,
             .y = -8 * 4096,
             .z = 9 * 4096});
}

TEST_CASE("The live spawn rotates through the loaded GTE matrix",
          "[particle-motion][F01514]") {
  const bool rotated = GENERATE(false, true);
  MATRIX swap{};
  swap.m[0][1] = 4096;  // x' = y
  swap.m[1][0] = 4096;  // y' = x
  swap.m[2][2] = 4096;
  SetRotMatrix(&swap);
  VS::Effects::LiveParticleEmitterScratch scratch{};
  scratch.flags = rotated ? fx::kParticleRotateSpawnOffset : 0;
  scratch.vector = {5, -6, 7, 0};
  scratch.position = {10, 20, 30, 0};
  Particle particle{};
  fx::LiveSetParticleSpawnPosition(&scratch, &particle);
  const std::array<std::int32_t, 3> aim{
      static_cast<std::int32_t>(scratch.aim_point.vx),
      static_cast<std::int32_t>(scratch.aim_point.vy),
      static_cast<std::int32_t>(scratch.aim_point.vz)};
  CHECK(aim == (rotated ? std::array{-6, 5, 7} : std::array{5, -6, 7}));
  CHECK(particle.motion.position.vx == 10 + aim[0] * 4096);
  CHECK(particle.motion.position.vy == 20 + aim[1] * 4096);
  CHECK(particle.motion.position.vz == 30 + aim[2] * 4096);
  fx::LiveSetParticleSpawnPosition(nullptr, &particle);
  VS::Core::Gte::Reset();
}

TEST_CASE("A tick integrates and draws before ageing each particle",
          "[particle-motion][F01535]") {
  auto particles = LinkedParticles<2>();
  TestTickHost host(2);
  host.lifecycles[0] = {.age = 4, .lifetime = 5, .tick_event = 3};
  host.lifecycles[1] = {.age = 255, .lifetime = 0, .end_event = 2};
  fx::TickEffectParticles(particles.data(), host);
  CHECK(host.calls == std::vector<std::string>{"integrate", "draw", "emit 0 2",
                                               "lifetime 4", "lifetime -1"});
  CHECK(host.lifecycles[0].age == 5);
  CHECK(host.lifecycles[1].age == 0);
}

TEST_CASE("An expiring particle emits its end event and is released",
          "[particle-motion][F01535]") {
  auto particles = LinkedParticles<3>();
  TestTickHost host(3);
  host.lifecycles[0] = {.lifetime = 1, .end_event = 1};
  host.lifecycles[1] = {.lifetime = -1, .end_event = 4, .texture_timer = 0};
  host.lifecycles[2] = {.lifetime = -1, .end_event = 4, .texture_timer = 2};
  fx::TickEffectParticles(particles.data(), host);
  CHECK(host.calls ==
        std::vector<std::string>{"integrate", "draw", "lifetime 0", "emit 0 0",
                                 "release 0", "emit 1 3", "release 1"});
}

TEST_CASE("A tick reads a particle's lifetime after its tick event",
          "[particle-motion][F01535]") {
  auto particles = LinkedParticles<1>();
  TestTickHost host(1);
  host.lifecycles[0] = {.lifetime = 5, .tick_event = 1};
  host.store_on_emit = true;
  host.lifetime_on_emit = 1;
  fx::TickEffectParticles(particles.data(), host);
  CHECK(host.calls == std::vector<std::string>{"integrate", "draw", "emit 0 0",
                                               "lifetime 0", "release 0"});
}

TEST_CASE("A tick of an empty list only integrates and draws",
          "[particle-motion][F01535]") {
  TestTickHost host(0);
  fx::TickEffectParticles(nullptr, host);
  CHECK(host.calls == std::vector<std::string>{"integrate none", "draw"});
  CHECK(fx::LiveTickEffectParticles(nullptr) == 1);
}

TEST_CASE("Motion drags the velocity and adds force over mass and gravity",
          "[particle-motion][F01554]") {
  fx::ParticleMotionState state{.mass = 4096,
                                .gravity_scale = 2048,
                                .position = {.x = 100, .y = 0, .z = -5},
                                .velocity = {.x = 4096, .y = 0, .z = 8192},
                                .force = {.x = 2, .y = 0, .z = -1},
                                .acceleration = {.x = 1, .y = 2, .z = 3}};
  fx::StepParticleMotion(
      state, {.gravity = {.x = 0, .y = 4096, .z = 0}, .drag = 1024});
  CHECK(state.velocity == Vec3{.x = 3074, .y = 2048, .z = 6143});
  CHECK(state.position == Vec3{.x = 3174, .y = 2048, .z = 6138});
  CHECK(state.force == Vec3{.x = 3, .y = 2, .z = 2});
}

TEST_CASE("Motion with no mass divides as the R3000 does",
          "[particle-motion][F01554]") {
  fx::ParticleMotionState state{.mass = 0,
                                .velocity = {.x = 5, .y = 0, .z = 0},
                                .force = {.x = 0, .y = -1, .z = 0}};
  fx::StepParticleMotion(state, {});
  CHECK(state.velocity == Vec3{.x = -1, .y = 1, .z = -1});
  CHECK(state.position == Vec3{.x = -1, .y = 1, .z = -1});
}

TEST_CASE("Motion wraps its words", "[particle-motion][F01554]") {
  fx::ParticleMotionState state{
      .mass = 1,
      .position = {.x = std::numeric_limits<std::int32_t>::max(),
                   .y = 0,
                   .z = 0},
      .velocity = {.x = 1, .y = 0, .z = 0},
      .force = {.x = 1, .y = 0, .z = 0},
      .acceleration = {
          .x = std::numeric_limits<std::int32_t>::max(), .y = 0, .z = 0}};
  fx::StepParticleMotion(state, {});
  CHECK(state.velocity.x == 4097);
  CHECK(state.position.x == std::numeric_limits<std::int32_t>::min() + 4096);
  CHECK(state.force.x == std::numeric_limits<std::int32_t>::min());
}

TEST_CASE("Attraction pulls the force toward the target from the new position",
          "[particle-motion][F01554]") {
  const std::int32_t attraction = GENERATE(-4096, 2048);
  fx::ParticleMotionState state{.mass = 4096,
                                .velocity = {.x = 64 * 4096, .y = 0, .z = 0},
                                .target = {32, 0, 0},
                                .attraction = attraction};
  fx::StepParticleMotion(state, {});
  CHECK(state.position.x == 64 * 4096);
  CHECK(state.force == Vec3{.x = -attraction, .y = 0, .z = 0});

  fx::ParticleMotionState still{.mass = 4096,
                                .acceleration = {.x = 1, .y = 1, .z = 1},
                                .target = {0, 0, 0},
                                .attraction = attraction};
  fx::StepParticleMotion(still, {});
  CHECK(still.force == Vec3{.x = 1, .y = 1, .z = 1});
}

TEST_CASE("Integration steps every particle of the list in order",
          "[particle-motion][F01554]") {
  auto particles = LinkedParticles<3>();
  TestMotionRecords records(3);
  for (auto& state : records.states) {
    state.mass = 4096;
    state.velocity.y = 4096;
  }
  fx::IntegrateParticleMotion(&particles[1], records, {.drag = 0});
  CHECK(records.order == std::vector<Particle*>{&particles[1], &particles[2]});
  CHECK(records.states[0].position.y == 0);
  CHECK(records.states[1].position.y == 4096);
  CHECK(records.states[2].position.y == 4096);
}

TEST_CASE("Live integration reads the particle forces and keeps acceleration",
          "[particle-motion][F01554]") {
  const VECTOR saved_gravity = VS::Effects::particleGravity;
  const int saved_drag = VS::Effects::particleDrag;
  VS::Effects::particleGravity = {};
  VS::Effects::particleGravity.vy = 4096;
  VS::Effects::particleDrag = 2048;
  Particle particle{};
  particle.motion.mass = 4096;
  particle.motion.gravity_scale = 4096;
  particle.motion.velocity.vx = 4096;
  particle.motion.acceleration.vz = 7;
  fx::LiveIntegrateParticleMotion(&particle);
  CHECK(particle.motion.velocity.vx == 2048);
  CHECK(particle.motion.velocity.vy == 4096);
  CHECK(particle.motion.position.vx == 2048);
  CHECK(particle.motion.position.vy == 4096);
  CHECK(particle.motion.force.vz == 7);
  CHECK(particle.motion.acceleration.vz == 7);
  fx::LiveIntegrateParticleMotion(nullptr);
  VS::Effects::particleGravity = saved_gravity;
  VS::Effects::particleDrag = saved_drag;
}

TEST_CASE("A cleared texture animation holds nothing",
          "[particle-texture][F01590]") {
  fx::TextureAnimationState state{.frame_timer = 3,
                                  .frame_flags = 1,
                                  .frame_offset_x = -2,
                                  .frame_offset_y = 4,
                                  .cursor = 12,
                                  .texture_index = 2};
  fx::ClearTextureAnimation(state);
  CHECK(state == fx::TextureAnimationState{});

  VS::Effects::LiveParticleTextureAnimation animation{};
  animation.frame_offset_x = 5;
  animation.cursor = 8;
  animation.texture_index = 3;
  animation.rotation = 9;
  fx::LivePackageTextureFrame frame{};
  animation.program = reinterpret_cast<std::uint8_t*>(&frame);
  animation.texture_info = &frame;
  fx::LiveClearParticleTextureState(&animation);
  CHECK(animation.frame_offset_x == 0);
  CHECK(animation.cursor == 0);
  CHECK(animation.texture_index == 0);
  CHECK(animation.rotation == 0);
  CHECK(animation.program == nullptr);
  CHECK(animation.texture_info == nullptr);
  fx::LiveClearParticleTextureState(nullptr);
}

TEST_CASE("A restarted texture animation keeps its offset and flags",
          "[particle-texture][F01591]") {
  fx::TextureAnimationState state{.frame_timer = 3,
                                  .frame_flags = 1,
                                  .frame_offset_x = -2,
                                  .frame_offset_y = 4,
                                  .cursor = 12,
                                  .texture_index = 2};
  fx::RestartTextureAnimation(state, 1);
  CHECK(state == fx::TextureAnimationState{.frame_timer = 0,
                                           .frame_flags = 1,
                                           .frame_offset_x = -2,
                                           .frame_offset_y = 4,
                                           .cursor = 0,
                                           .texture_index = 1});
}

TEST_CASE("The live restart selects the package's animation program",
          "[particle-texture][F01591]") {
  AnimationPackage package;
  VS::Effects::LiveParticleTextureAnimation animation{};
  animation.cursor = 8;
  animation.frame_timer = 6;
  fx::LiveInitializeParticleTextureAnimation(&animation, 0x100, 1);
  CHECK(animation.texture_index == 0);
  CHECK(animation.cursor == 0);
  CHECK(animation.frame_timer == 0);
  CHECK(animation.program == package.Program(20));
  fx::LiveInitializeParticleTextureAnimation(&animation, 0, 2);
  CHECK(animation.program == nullptr);
  fx::LiveInitializeParticleTextureAnimation(nullptr, 0, 0);
}

TEST_CASE("A held frame counts down by two ticks",
          "[particle-texture][F01592]") {
  const std::array<std::uint32_t, 1> program{FrameWord(1, 2)};
  fx::TextureAnimationState state{.frame_timer = 6, .cursor = 0};
  CHECK(fx::AdvanceTextureAnimation(state, Bytes(program)).step ==
        fx::TextureAnimationStep::kHolding);
  CHECK(state.frame_timer == 4);
  state.frame_timer = 1;
  CHECK(fx::AdvanceTextureAnimation(state, Bytes(program)).step ==
        fx::TextureAnimationStep::kHolding);
  CHECK(state.frame_timer == 255);
  CHECK(state.cursor == 0);
}

TEST_CASE("A program offsets and selects frames, restarting and stopping",
          "[particle-texture][F01592]") {
  const std::array<std::uint32_t, 4> program{
      OffsetWord(-3, 5), FrameWord(0x1234, 7, 0x0B), FrameWord(2, 2), kRestart};
  fx::TextureAnimationState state{.frame_timer = 2};
  CHECK(fx::AdvanceTextureAnimation(state, Bytes(program)) ==
        fx::TextureAnimationAdvance{.step = fx::TextureAnimationStep::kFrame,
                                    .frame = 0x1234});
  CHECK(state == fx::TextureAnimationState{.frame_timer = 7,
                                           .frame_flags = 0x0B,
                                           .frame_offset_x = -3,
                                           .frame_offset_y = 5,
                                           .cursor = 8});
  state.frame_timer = 0;
  CHECK(fx::AdvanceTextureAnimation(state, Bytes(program)).frame == 2);
  CHECK(state.cursor == 12);
  state.frame_timer = 0;
  CHECK(fx::AdvanceTextureAnimation(state, Bytes(program)).frame == 0x1234);
  CHECK(state.cursor == 8);

  const std::array<std::uint32_t, 2> stopping{OffsetWord(1, -1), kStop};
  fx::TextureAnimationState stopped{.cursor = 0};
  CHECK(fx::AdvanceTextureAnimation(stopped, Bytes(stopping)).step ==
        fx::TextureAnimationStep::kStopped);
  CHECK(stopped == fx::TextureAnimationState{
                       .frame_offset_x = 1, .frame_offset_y = -1, .cursor = 4});
}

TEST_CASE("Endless and unknown programs are refused without a change",
          "[particle-texture][F01592]") {
  // Static: the generator outlives the first run, so its spans must not name
  // locals.
  static const std::array<std::uint32_t, 2> looping{OffsetWord(4, 4), kRestart};
  static const std::array<std::uint32_t, 2> unknown{OffsetWord(4, 4),
                                                    0x40000000};
  static const std::array<std::uint32_t, 1> unterminated{OffsetWord(4, 4)};
  const auto program =
      GENERATE_COPY(std::span<const std::uint32_t>(looping),
                    std::span<const std::uint32_t>(unknown),
                    std::span<const std::uint32_t>(unterminated),
                    std::span<const std::uint32_t>());
  const fx::TextureAnimationState before{.frame_timer = 2, .cursor = 0};
  fx::TextureAnimationState state = before;
  CHECK(fx::AdvanceTextureAnimation(state, Bytes(program)).step ==
        fx::TextureAnimationStep::kRefused);
  CHECK(state == fx::TextureAnimationState{.cursor = 0});
}

TEST_CASE("A wrapping cursor returns to the program's first word",
          "[particle-texture][F01592]") {
  std::vector<std::uint32_t> program(0x4000, kRestart);
  program.front() = FrameWord(9, 2);
  program.back() = OffsetWord(2, 3);
  fx::TextureAnimationState state{.cursor = 0xFFFC};
  CHECK(fx::AdvanceTextureAnimation(state, Bytes(program)).frame == 9);
  CHECK(state.cursor == 4);
  CHECK(state.frame_offset_x == 2);
  CHECK(state.frame_offset_y == 3);
}

TEST_CASE("The live step selects the frame of its texture table",
          "[particle-texture][F01592]") {
  AnimationPackage package;
  VS::Effects::LiveParticleTextureAnimation animation{};
  animation.program = const_cast<std::uint8_t*>(package.Program(12));
  fx::LiveAdvanceParticleTextureAnimation(&animation);
  CHECK(animation.texture_info == &package.frames[3]);
  CHECK(animation.frame_timer == 4);
  CHECK(animation.cursor == 4);

  // An unbound table keeps the previous frame.
  animation.frame_timer = 0;
  animation.cursor = 0;
  animation.texture_index = 1;
  fx::LiveAdvanceParticleTextureAnimation(&animation);
  CHECK(animation.texture_info == &package.frames[3]);
  CHECK(animation.cursor == 4);
}

TEST_CASE("The live step refuses a program outside every recorded table",
          "[particle-texture][F01592]") {
  AnimationPackage package;
  std::array<std::uint32_t, 1> unrecorded{FrameWord(1, 2)};
  VS::Effects::LiveParticleTextureAnimation animation{};
  animation.program = reinterpret_cast<std::uint8_t*>(unrecorded.data());
  fx::LiveAdvanceParticleTextureAnimation(&animation);
  CHECK(animation.texture_info == nullptr);
  CHECK(animation.cursor == 0);
  CHECK(animation.frame_timer == 0);
  fx::LiveAdvanceParticleTextureAnimation(nullptr);
}

TEST_CASE("Recorded animation tables withdraw overlaps and replace the oldest",
          "[particle-texture][package-extents]") {
  std::array<std::array<std::byte, 16>, 10> tables{};
  for (auto& table : tables) {
    fx::RecordLiveAnimationTable(table);
  }
  CHECK(fx::LiveAnimationTable(tables[0].data()).empty());
  CHECK(fx::LiveAnimationTable(tables[1].data()).empty());
  CHECK(fx::LiveAnimationTable(tables[2].data()).size() == 16);
  CHECK(fx::LiveAnimationProgram(tables[9].data() + 4).size() == 12);

  // A table over two recorded ones withdraws both and takes one extent.
  const std::span<std::byte> joined(tables[2].data(), 32);
  fx::RecordLiveAnimationTable(joined);
  CHECK(fx::LiveAnimationTable(tables[3].data()).empty());
  CHECK(fx::LiveAnimationProgram(tables[3].data()).size() == 16);
  fx::RecordLiveAnimationTable(tables[0]);
  CHECK(fx::LiveAnimationTable(tables[4].data()).size() == 16);
  for (const auto& table : tables) {
    fx::ForgetLiveAnimationTable(table.data());
  }
  CHECK(fx::LiveAnimationProgram(tables[2].data()).empty());
}
