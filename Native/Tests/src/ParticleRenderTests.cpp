#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Random.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/EffectInstancesLive.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/EffectScriptLive.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/ParticleEmitterLive.hpp"
#include "Effects/ParticleMotionLive.hpp"
#include "Effects/ParticleRenderLive.hpp"
#include "Effects/ParticleTextureAnimationLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Plugins/Plg045.hpp"
#include "Effects/Plugins/Plg049.hpp"
#include "Effects/Plugins/Plg053.hpp"
#include "Effects/Plugins/Plg065.hpp"
#include "Effects/Plugins/Plg073.hpp"
#include "Effects/Plugins/Plg082.hpp"
#include "Effects/PurgeTables.hpp"
#include "Effects/Runtime.h"
#include "Effects/Templates/ParticlesLive.hpp"
#include "Effects/Templates/RingsLive.hpp"
#include "Effects/Templates/SurfacesLive.hpp"
#include "Effects/Templates/TemplateLiveState.hpp"
#include "Effects/Templates/TrailControl.hpp"
#include "Effects/Templates/TrailInit.hpp"
#include "Effects/Templates/TrailRender.hpp"
#include "Effects/Templates/TrailTarget.hpp"
#include "Effects/Templates/TrailUpdate.hpp"
#include "Effects/Templates/TrailsLive.hpp"
#include "Effects/Timeline.hpp"
#include "Effects/TimelinePlaybackLive.hpp"
#include "Effects/TrailEmitterLive.hpp"
#include "Effects/Trails.hpp"
#include "Legacy/Psx.h"
#include "Render/Camera.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/LegacyPackets.hpp"
#include "Render/PageSpace.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"
#include "nlohmann/json.hpp"

namespace {
struct EffectTraceScope {
  VS::Render::PageSpace& pages;
  std::vector<std::pair<std::uint64_t, std::string>> records;
  VS::Core::LogObserver previous;

  explicit EffectTraceScope(VS::Render::PageSpace& value)
      : pages(value),
        previous(
            VS::Core::SetLogObserver([this](const VS::Core::LogRecord& record) {
              if (record.channel == "effect-path") {
                records.emplace_back(record.tick, record.message);
              }
            })) {
    VS::Core::SetLogLevel(VS::Core::LogLevel::Trace);
    VS::Core::SetLogChannels("*");
  }

  ~EffectTraceScope() {
    pages.ConfigureEffectPathTrace(std::nullopt);
    VS::Core::SetLogObserver(std::move(previous));
    VS::Core::SetLogLevel(VS::Core::LogLevel::Info);
    VS::Core::SetLogTick(0);
  }

  void CheckQuadRecord(std::size_t index, const VS::u32* command) const {
    const auto record = nlohmann::json::parse(records[index].second);
    CHECK(record.at("kind") == "primitive");
    CHECK(record.at("command_address") ==
          reinterpret_cast<std::uintptr_t>(command));
    CHECK(record.at("vertices").size() == 4);
    CHECK(record.at("vertices").at(0).at("screen") ==
          nlohmann::json::array({10, 20}));
    CHECK(record.at("vertices").at(0).at("raster") ==
          nlohmann::json::array({13, 24}));
    CHECK(record.at("draw_offset") == nlohmann::json::array({3, 4}));
  }
};

struct ParticleFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  void* saved_ordering = VS::Render::legacyOrderingTables[0];
  int saved_framebuffer = VS::Render::frameBuf;
  int saved_near = VS::Render::nearClip;
  int saved_projection = VS::Render::projectionDistance;
  Effects_EffectPackage* saved_package = Effects_package;
  Effects_EffectInstance* saved_instance = Effects_currentInstance;
  VS::Effects::LiveParticle* saved_pool = VS::Effects::particlePool;
  int saved_capacity = VS::Effects::particleCapacity;
  int saved_used = VS::Effects::usedParticleCount;
  VS::Effects::LiveParticle* saved_free = VS::Effects::freeParticles;
  Effects_EffectPackage package{};
  Effects_EffectInstance instance{};
  VS::Effects::LiveParticle particle{};
  VS::Effects::LivePackageTexture texture{};
  VS::Effects::LivePackageTextureFrame info{.texture_count = 1,
                                            .first_texture = 0};
  std::array<u_long, 2> script{8u << 16, 0x30000000u};
  std::array<u_long, 2> curve_block{1, 2};
  std::array<u_char, 2> curve{32, 64};
  std::vector<u_long> storage = std::vector<u_long>(0x8002, 0xDEADBEEF);
  std::array<u_long, 2064> ordering{};

  ParticleFixture() {
    Effects_package = &package;
    Effects_currentInstance = &instance;
    VS::Effects::particlePool = &particle;
    VS::Effects::particleCapacity = 1;
    package.textures = &texture;
    package.block1TableCount = 1;
    package.textureInfoTable[0] = &info;
    package.curveBlock =
        reinterpret_cast<VS::Effects::LiveCurveTable*>(curve_block.data());
    package.curves[0] = curve.data();
    VS::Effects::RecordLiveEffectCurves(std::as_bytes(std::span(curve_block)));
    texture.u = 16;
    texture.v = 32;
    texture.w = texture.h = 8;
    texture.clut = 0x1234;
    texture.tpage = 0x567;
    texture.corners = {{{-4, -4}, {4, -4}, {-4, 4}, {4, 4}}};
    particle.texture_animation.program =
        reinterpret_cast<u_char*>(script.data());
    VS::Effects::RecordLiveAnimationTable(
        std::as_writable_bytes(std::span(script)));
    particle.motion.position.vz = 1024 << 12;
    particle.transparency = 16;
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = storage.data() + 1;
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::FrameScratchpad() = {};
    for (int axis = 0; axis < 3; ++axis) {
      VS::Render::FrameScratchpad().view_matrix.m[axis][axis] = 4096;
    }
    instance.transforms[0].view = VS::Render::FrameScratchpad().view_matrix;
    instance.transforms[0].world = VS::Render::FrameScratchpad().view_matrix;
    VS::Render::FrameScratchpad().primitive_cursor = storage.data() + 1;
    VS::Render::FrameScratchpad().ordering_table = ordering.data() + 16;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    VS::Render::nearClip = 0;
    VS::Render::projectionDistance = 256;
    Core_Gte_SetGeomOffset(160, 120);
    Core_Gte_SetGeomScreen(256);
  }

  ~ParticleFixture() {
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::legacyOrderingTables[0] = saved_ordering;
    VS::Render::frameBuf = saved_framebuffer;
    VS::Render::nearClip = saved_near;
    VS::Render::projectionDistance = saved_projection;
    VS::Effects::ForgetLivePackageExtents();
    VS::Effects::ForgetLiveAnimationTable(script.data());
    Effects_package = saved_package;
    Effects_currentInstance = saved_instance;
    VS::Effects::particlePool = saved_pool;
    VS::Effects::particleCapacity = saved_capacity;
    VS::Effects::usedParticleCount = saved_used;
    VS::Effects::freeParticles = saved_free;
    Core_Gte_Reset();
  }

  void CheckEmissionPool(bool occupied) const {
    CHECK(VS::Effects::usedParticleCount == (occupied ? 1 : 0));
    CHECK(instance.particleCount == (occupied ? 1 : 0));
    CHECK(instance.particles == (occupied ? &particle : nullptr));
    CHECK(VS::Effects::freeParticles == (occupied ? nullptr : &particle));
    CHECK(particle.next == nullptr);
  }

  void CheckQuad() const {
    CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
          storage.data() + 17);
    const auto* quad = storage.data() + 4;
    CHECK(quad[1] == 0x2C808080);
    CHECK(quad[2] == static_cast<u_long>(156 | (116 << 16)));
    CHECK(quad[8] == static_cast<u_long>(164 | (124 << 16)));
    CHECK(quad[3] == 0x12342010);
    CHECK(quad[5] == 0x05672018);
    CHECK(storage.front() == 0xDEADBEEF);
    CHECK(storage.back() == 0xDEADBEEF);
  }

  void CheckPathPositions(const nlohmann::json& record) const {
    CHECK(record.at("local_point") == nlohmann::json::array({64, 32, 1024}));
    CHECK(record.at("world_point") == nlohmann::json::array({164, 232, 1024}));
    CHECK(record.at("screen") == nlohmann::json::array({180, 128}));
  }

  void CheckPathIdentity(const nlohmann::json& record) const {
    CHECK(record.at("kind") == "particle");
    CHECK(record.at("slot") == 0);
    CHECK(record.at("attached") == true);
    CHECK(record.at("outcome") == "draw");
  }
};
}  // namespace

TEST_CASE("Effect path intervals reject missing reversed and unbounded ticks",
          "[effects][effect-path]") {
  using VS::Render::EffectPathTraceRange;
  for (const auto invalid : {"", "12", ":12", "12:", "0:12", "12:1", "1:2:3",
                             "-1:2", "1:18446744073709551616"}) {
    CHECK_FALSE(EffectPathTraceRange::Parse(invalid).has_value());
  }
  const auto range = EffectPathTraceRange::Parse("10:11");
  REQUIRE(range);
  CHECK(range->first == 10);
  CHECK(range->last == 11);
  VS::Render::PageSpace pages;
  CHECK_FALSE(pages.IsEffectPathTraceEnabled());
  CHECK_THROWS_AS(pages.ConfigureEffectPathTrace(EffectPathTraceRange{5, 4}),
                  std::invalid_argument);
}

TEST_CASE("Effect primitive paths are opt-in bounded and observational",
          "[effects][effect-path]") {
  VS::Render::PageSpace traced;
  VS::Render::PageSpace baseline;
  EffectTraceScope scope(traced);
  const std::array<VS::u32, 5> quad{0x2A0000FF, 10 | (20u << 16),
                                    14 | (20u << 16), 10 | (24u << 16),
                                    14 | (24u << 16)};
  traced.SetDrawOffset(3, 4);
  baseline.SetDrawOffset(3, 4);
  traced.ConfigureEffectPathTrace(VS::Render::EffectPathTraceRange{10, 11});
  for (std::uint64_t tick : {9, 10, 11, 12}) {
    VS::Core::SetLogTick(tick);
    traced.SetEffectPathTraceTick(tick);
    REQUIRE(traced.Execute(quad) == quad.size());
    REQUIRE(baseline.Execute(quad) == quad.size());
  }
  REQUIRE(scope.records.size() == 4);
  for (std::size_t index : {1u, 3u}) {
    CHECK(scope.records[index].first == (index == 1 ? 10 : 11));
    scope.CheckQuadRecord(index, quad.data());
  }
  CHECK(std::ranges::equal(traced.Pixels(), baseline.Pixels()));
  CHECK_FALSE(traced.IsEffectPathTraceEnabled());
}

TEST_CASE("Particle paths join local world projection and emitted primitives",
          "[effects][particle-render][effect-path]") {
  ParticleFixture fixture;
  auto& pages = VS::Render::GamePages();
  EffectTraceScope scope(pages);
  pages.ConfigureEffectPathTrace(VS::Render::EffectPathTraceRange{10, 10});
  VS::Core::SetLogTick(10);
  pages.SetEffectPathTraceTick(10);
  fixture.particle.emit_flags = 0x20000;
  fixture.particle.motion.position.vx = 64 << 12;
  fixture.particle.motion.position.vy = 32 << 12;
  fixture.instance.transforms[0].world.t[0] = 100;
  fixture.instance.transforms[0].world.t[1] = 200;
  fixture.instance.transforms[0].view.t[0] = 16;
  fixture.texture.clut = std::bit_cast<std::int16_t>(std::uint16_t{0x9234});
  const auto motion = fixture.particle.motion;
  VS::Effects::DrawLiveAnimatedSpriteParticles(&fixture.particle);
  REQUIRE(scope.records.size() == 2);
  const auto particle = nlohmann::json::parse(scope.records.back().second);
  fixture.CheckPathPositions(particle);
  fixture.CheckPathIdentity(particle);
  DrawPrim(fixture.storage.data() + 4);
  REQUIRE(scope.records.size() == 3);
  const auto primitive = nlohmann::json::parse(scope.records.back().second);
  CHECK(particle.at("command_addresses").at(0) ==
        primitive.at("command_address"));
  CHECK(primitive.at("vertices").at(0).at("screen") ==
        nlohmann::json::array({176, 124}));
  CHECK(std::memcmp(&motion, &fixture.particle.motion, sizeof motion) == 0);
}

TEST_CASE("Effect paths do not mistake framebuffer flips for particle motion",
          "[effects][effect-path]") {
  VS::Render::PageSpace pages;
  EffectTraceScope scope(pages);
  pages.ConfigureEffectPathTrace(VS::Render::EffectPathTraceRange{10, 11});
  const std::array<VS::u32, 3> sprite{0x620000FF, 10 | (20u << 16),
                                      5 | (6u << 16)};
  for (std::uint64_t tick : {10, 11}) {
    VS::Core::SetLogTick(tick);
    pages.SetEffectPathTraceTick(tick);
    pages.SetDrawOffset(0, tick == 10 ? 0 : 256);
    REQUIRE(pages.Execute(sprite) == sprite.size());
  }
  REQUIRE(scope.records.size() == 4);
  for (std::size_t index : {1u, 3u}) {
    const auto record = nlohmann::json::parse(scope.records[index].second);
    CHECK(record.at("vertices").at(0).at("screen") ==
          nlohmann::json::array({10, 20}));
    CHECK(record.at("vertices").at(0).at("raster") ==
          nlohmann::json::array({10, index == 1 ? 20 : 276}));
  }
}

TEST_CASE("Effect sprite and line paths include final raster coordinates",
          "[effects][effect-path]") {
  VS::Render::PageSpace pages;
  EffectTraceScope scope(pages);
  pages.ConfigureEffectPathTrace(VS::Render::EffectPathTraceRange{10, 10});
  VS::Core::SetLogTick(10);
  pages.SetEffectPathTraceTick(10);
  const std::array<VS::u32, 3> sprite{0x620000FF, 10 | (20u << 16),
                                      5 | (6u << 16)};
  REQUIRE(pages.Execute(sprite) == sprite.size());
  const std::array<VS::u32, 3> line{0x420000FF, 10 | (20u << 16),
                                    15 | (26u << 16)};
  REQUIRE(pages.Execute(line) == line.size());
  REQUIRE(scope.records.size() == 3);
  const auto rectangle = nlohmann::json::parse(scope.records[1].second);
  CHECK(rectangle.at("vertices").size() == 4);
  CHECK(rectangle.at("vertices").at(3).at("screen") ==
        nlohmann::json::array({15, 26}));
  const auto segment = nlohmann::json::parse(scope.records[2].second);
  CHECK(segment.at("vertices").size() == 2);
  CHECK(segment.at("vertices").at(1).at("screen") ==
        nlohmann::json::array({15, 26}));
}

TEST_CASE("Culled particle paths explain absent screen geometry",
          "[effects][particle-render][effect-path]") {
  ParticleFixture fixture;
  auto& pages = VS::Render::GamePages();
  EffectTraceScope scope(pages);
  pages.ConfigureEffectPathTrace(VS::Render::EffectPathTraceRange{10, 10});
  VS::Core::SetLogTick(10);
  pages.SetEffectPathTraceTick(10);
  fixture.particle.emit_flags = 0x80000;
  fixture.particle.motion.position.vy = 4 << 12;
  VS::Effects::DrawLiveAnimatedSpriteParticles(&fixture.particle);
  REQUIRE(scope.records.size() == 2);
  const auto record = nlohmann::json::parse(scope.records.back().second);
  CHECK(record.at("outcome") == "below-ground");
  CHECK(record.at("screen").is_null());
  CHECK(record.at("command_addresses").empty());
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.storage.data() + 1);
}

TEST_CASE("Particle texture reset clears native pointer tails without overflow",
          "[particle][texture-state]") {
  struct {
    std::uint64_t before = 0x123456789ABCDEF0;
    VS::Effects::LiveParticleTextureAnimation animation;
    std::uint64_t after = 0x0FEDCBA987654321;
  } fixture;
  std::memset(&fixture.animation, 0xCD, sizeof fixture.animation);
  VS::Effects::LiveClearParticleTextureState(&fixture.animation);
  CHECK(fixture.before == 0x123456789ABCDEF0);
  CHECK(fixture.after == 0x0FEDCBA987654321);
  CHECK(fixture.animation.program == nullptr);
  CHECK(fixture.animation.texture_info == nullptr);
  CHECK(fixture.animation.cursor == 0);
  CHECK(fixture.animation.frame_timer == 0);
  CHECK(fixture.animation.frame_offset_x == 0);
  CHECK(fixture.animation.frame_offset_y == 0);
}

TEST_CASE("Particle wrappers forward the real native list and texture program",
          "[effects][particle-render]") {
  ParticleFixture fixture;
  VS::Effects::DrawLiveEffectParticleList(&fixture.particle);
  CHECK(fixture.particle.texture_animation.cursor == 4);
  CHECK(fixture.particle.texture_animation.frame_timer == 8);
  CHECK(fixture.particle.texture_animation.texture_info == &fixture.info);
  fixture.CheckQuad();
}

TEST_CASE("Particles retain each original depth override and native link",
          "[effects][particle-render]") {
  const int mode = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8);
  ParticleFixture fixture;
  fixture.particle.variant = static_cast<unsigned>(mode);
  VS::Effects::DrawLiveAnimatedSpriteParticles(&fixture.particle);
  constexpr std::array<int, 9> kDepths{256,  264, 272, 248, 240,
                                       2046, -15, -13, -11};
  const auto* start = fixture.storage.data() + 1;
  const auto* bucket = fixture.ordering.data() + 16 + kDepths[mode];
  REQUIRE(nextPrim(bucket) == start + 13);
  CHECK(nextPrim(start + 13) == start + 3);
  CHECK(nextPrim(start + 3) == start);
  CHECK(start[1] == 0xE1000200);
  CHECK(start[14] == 0xE1000000);
}

TEST_CASE("Particle colors sample the next wrapped age through shared curves",
          "[effects][particle-render]") {
  ParticleFixture fixture;
  fixture.particle.color_curves[0] = 1;
  fixture.particle.color_curves[1] = 1;
  fixture.particle.color_curves[2] = 1;
  fixture.particle.age = 255;
  VS::Effects::DrawLiveAnimatedSpriteParticles(&fixture.particle);
  CHECK(fixture.storage[5] == 0x2C3F3F3F);
}

TEST_CASE("Particle culling preserves texture advancement without emitting",
          "[effects][particle-render]") {
  const bool attached = GENERATE(false, true);
  ParticleFixture fixture;
  fixture.particle.emit_flags = 0x80000 | (attached ? 0x20000 : 0);
  fixture.particle.motion.position.vy = 4096;
  VS::Effects::DrawLiveAnimatedSpriteParticles(&fixture.particle);
  CHECK(fixture.particle.texture_animation.cursor == 4);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.storage.data() + 1);
}

TEST_CASE("Attached particles project through their own native effect matrix",
          "[effects][particle-render]") {
  ParticleFixture fixture;
  fixture.particle.emit_flags = 0x20000;
  fixture.instance.transforms[0].view.t[0] = 128;
  VS::Effects::DrawLiveAnimatedSpriteParticles(&fixture.particle);
  const auto* quad = fixture.storage.data() + 4;
  CHECK((quad[2] & 0xFFFF) == 188);
  CHECK((quad[8] & 0xFFFF) == 196);
}

TEST_CASE(
    "Particle rotation preserves Q12 geometry and semitransparent commands",
    "[effects][particle-render]") {
  ParticleFixture fixture;
  fixture.particle.texture_animation.rotation = 1024;
  fixture.texture.clut = static_cast<std::int16_t>(-28108);
  VS::Effects::DrawLiveAnimatedSpriteParticles(&fixture.particle);
  const auto* quad = fixture.storage.data() + 4;
  CHECK(quad[1] == 0x2E808080);
  CHECK(quad[2] == static_cast<u_long>(164 | (116 << 16)));
  CHECK(quad[8] == static_cast<u_long>(156 | (124 << 16)));
}

TEST_CASE("Particle texture timers advance once per render without resetting",
          "[effects][particle-render]") {
  ParticleFixture fixture;
  VS::Effects::DrawLiveAnimatedSpriteParticles(&fixture.particle);
  VS::Effects::DrawLiveAnimatedSpriteParticles(&fixture.particle);
  CHECK(fixture.particle.texture_animation.frame_timer == 6);
  CHECK(fixture.particle.texture_animation.cursor == 4);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.storage.data() + 33);
}

TEST_CASE("Particle rendering fits exactly at the native frame buffer boundary",
          "[effects][particle-render]") {
  ParticleFixture fixture;
  VS::Render::FrameScratchpad().primitive_cursor =
      fixture.storage.data() + 1 + 0x8000 - 16;
  VS::Effects::DrawLiveAnimatedSpriteParticles(&fixture.particle);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.storage.data() + 1 + 0x8000);
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

namespace {
struct EmissionFixture : ParticleFixture {
  struct Definitions {
    int reserved;
    VS::Effects::LiveParticleDefinition entries[1];
  } definitions{};
  std::array<u_short, 8> programs{1, 0, 8, 0, 0, 8, 0, 0x3000};

  EmissionFixture() {
    package.block5Data =
        reinterpret_cast<VS::Effects::LiveParticleDefinitionTable*>(
            &definitions);
    VS::Effects::RecordLiveParticleDefinitions(
        std::as_bytes(std::span(&definitions, 1)));
    package.block2Data = programs.data();
    VS::Effects::RecordLiveAnimationTable(
        std::as_writable_bytes(std::span(programs)));
    auto& definition = definitions.entries[0];
    definition.interval_range[0] = definition.interval_range[1] = 1;
    definition.count_range[0] = definition.count_range[1] = 1;
    VS::Effects::usedParticleCount = 0;
    VS::Effects::freeParticles = &particle;
  }
  ~EmissionFixture() { VS::Effects::ForgetLiveAnimationTable(programs.data()); }

  void CheckTexture(const VS::Effects::LiveParticleQuadSet& state) const {
    CHECK(state.texture_animation.program ==
          reinterpret_cast<const u_char*>(programs.data()) + 8);
    CHECK(state.texture_animation.texture_info == nullptr);
    CHECK(state.texture_animation.frame_timer == 0);
    CHECK(state.texture_animation.cursor == 0);
    CHECK(state.texture_animation.texture_index == 0);
  }
};

enum class ChildTrigger { kTick, kLifetimeEnd, kTextureEnd };

struct ChildEmissionFixture : EmissionFixture {
  VECTOR saved_gravity = VS::Effects::particleGravity;
  int saved_drag = VS::Effects::particleDrag;
  int saved_sample = Effects_curveSamples[0];
  std::array<VS::Effects::LiveParticle, 2> nodes{};
  std::array<u_long, 1> terminal{0x30000000u};

  explicit ChildEmissionFixture(ChildTrigger trigger) {
    package.block1TableCount = 1;
    package.block2Count = 1;
    auto& definition = definitions.entries[0];
    definition.flags = 4;
    std::ranges::fill(definition.pair_ranges[0], 1);
    std::ranges::fill(definition.pair_ranges[4], 10);
    Effects_curveSamples[0] = 0;
    VS::Effects::particleGravity = {};
    VS::Effects::particleDrag = 0;
    nodes[0] = particle;
    auto& parent = nodes[0];
    parent.motion.mass = 1;
    parent.motion.position = {100 << 12, -20 * 4096, 1024 << 12};
    parent.transparency = 12;
    parent.variant = 3;
    parent.lifetime = 8;
    if (trigger == ChildTrigger::kTick) {
      parent.tick_event = 1;
    } else {
      parent.end_event = 1;
      parent.lifetime = trigger == ChildTrigger::kLifetimeEnd ? 1 : -1;
    }
    if (trigger == ChildTrigger::kTextureEnd) {
      parent.texture_animation.program =
          reinterpret_cast<u_char*>(terminal.data());
      parent.texture_animation.texture_info = &info;
      VS::Effects::RecordLiveAnimationTable(
          std::as_writable_bytes(std::span(terminal)));
    }
    VS::Effects::particlePool = nodes.data();
    VS::Effects::particleCapacity = static_cast<int>(nodes.size());
    VS::Effects::usedParticleCount = 1;
    VS::Effects::freeParticles = &nodes[1];
    instance.particles = &parent;
    instance.particleCount = 1;
  }

  ~ChildEmissionFixture() {
    VS::Effects::particleGravity = saved_gravity;
    VS::Effects::particleDrag = saved_drag;
    Effects_curveSamples[0] = saved_sample;
    VS::Effects::ForgetLiveAnimationTable(terminal.data());
  }

  void CheckChildCoordinates() const {
    const auto& child = nodes[1];
    CHECK(child.motion.position.vx == 100 * 4096);
    CHECK(child.motion.position.vy == -20 * 4096);
    CHECK(child.motion.position.vz == 1024 * 4096);
  }

  void CheckChildAppearance() const {
    const auto& child = nodes[1];
    CHECK(child.transparency == 12);
    CHECK(child.variant == 3);
    CHECK(child.lifetime == 10);
  }

  void CheckChildPopulation(ChildTrigger trigger) const {
    const bool retained = trigger == ChildTrigger::kTick;
    CHECK(instance.particleCount == (retained ? 2 : 1));
    CHECK(VS::Effects::usedParticleCount == (retained ? 2 : 1));
    CHECK(VS::Effects::freeParticles == (retained ? nullptr : &nodes[0]));
  }
};

struct DispatchHeap {
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 384> storage{};
  VS::Core::Memory::ScopedLiveGameHeap scope{storage};

  static std::size_t Capacity(const void* allocation) {
    const auto capacity = VS::Core::Memory::LivePayloadCapacity(allocation);
    REQUIRE(capacity.has_value());
    return capacity.value_or(0);
  }

  static void CheckPrefix(const VS::Effects::LiveParticleQuadSet& state) {
    CHECK(state.lifetime == 30);
    CHECK(state.definition_index == 0);
    CHECK(state.age == 0);
    REQUIRE(state.control_points != nullptr);
  }
};

// The particle-set entries read definitions only within the package's
// recorded block-5 extent.
struct RecordedDefinitions {
  explicit RecordedDefinitions(const EmissionFixture& fixture) {
    VS::Effects::ForgetLivePackageExtents();
    VS::Effects::RecordLiveParticleDefinitions(
        std::as_bytes(std::span{&fixture.definitions, 1}));
  }
  ~RecordedDefinitions() { VS::Effects::ForgetLivePackageExtents(); }
  RecordedDefinitions(const RecordedDefinitions&) = delete;
  RecordedDefinitions& operator=(const RecordedDefinitions&) = delete;
  RecordedDefinitions(RecordedDefinitions&&) = delete;
  RecordedDefinitions& operator=(RecordedDefinitions&&) = delete;
};
}  // namespace

TEST_CASE(
    "Child particle events inherit native parent coordinates and appearance",
    "[effects][particle-emission][particle-parent]") {
  const auto trigger = GENERATE(ChildTrigger::kTick, ChildTrigger::kLifetimeEnd,
                                ChildTrigger::kTextureEnd);
  ChildEmissionFixture fixture(trigger);
  REQUIRE(VS::Effects::LiveTickEffectParticles(&fixture.instance) == 1);
  REQUIRE(fixture.instance.particles == &fixture.nodes[1]);
  fixture.CheckChildCoordinates();
  fixture.CheckChildAppearance();
  fixture.CheckChildPopulation(trigger);
}

TEST_CASE("Textured ribbon dispatchers allocate and initialize native textures",
          "[effects][particle-dispatch][effects-particles]") {
  EmissionFixture fixture;
  const RecordedDefinitions recorded{fixture};
  DispatchHeap heap;
  Effects_RendererSlot renderer{};
  fixture.package.block1TableCount = 1;
  fixture.package.block2Count = 1;
  const auto dispatch =
      GENERATE(&VS::Effects::Templates::UpdateLivePlg045ParticleDispatch,
               &VS::Effects::Templates::UpdateLivePlg265ParticleDispatch2);
  REQUIRE(dispatch(&renderer, 1, 30) == 1);
  REQUIRE(renderer.rendererState != nullptr);
  const auto capacity = DispatchHeap::Capacity(renderer.rendererState);
  CHECK(capacity >= sizeof(VS::Effects::LiveParticleQuadSet));
  auto* state =
      static_cast<VS::Effects::LiveParticleQuadSet*>(renderer.rendererState);
  DispatchHeap::CheckPrefix(*state);
  if (capacity >= sizeof(VS::Effects::LiveParticleQuadSet)) {
    fixture.CheckTexture(*state);
  }
  CHECK(dispatch(&renderer, 4, 0) == 0);
  VS::Core::Memory::FreeLiveFromHighEnd(renderer.rendererState);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("Untextured line dispatchers retain only the complete native prefix",
          "[effects][particle-dispatch][effects-particles]") {
  EmissionFixture fixture;
  const RecordedDefinitions recorded{fixture};
  DispatchHeap heap;
  Effects_RendererSlot renderer{};
  const auto dispatch =
      &VS::Effects::Templates::UpdateLivePlg265ParticleDispatch;
  REQUIRE(dispatch(&renderer, 1, 30) == 1);
  const auto prefix =
      offsetof(VS::Effects::LiveParticleQuadSet, texture_animation);
  constexpr auto kUnit = VS::Core::Memory::kHeapUnitBytes;
  const auto rounded = (prefix + kUnit - 1) / kUnit * kUnit;
  CHECK(DispatchHeap::Capacity(renderer.rendererState) == rounded);
  const auto* state =
      static_cast<VS::Effects::LiveParticleQuadSet*>(renderer.rendererState);
  DispatchHeap::CheckPrefix(*state);
  CHECK(state->cooldown == 0);
  CHECK(dispatch(&renderer, 4, 0) == 0);
  VS::Core::Memory::FreeLiveFromHighEnd(renderer.rendererState);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

namespace {
using LiveParticleDispatch = int (*)(Effects_RendererSlot*, unsigned int, int);

// A particle-set entry, the GPU words and code of each segment it draws and
// the overlay global its origin steps.
struct LiveParticleCase {
  LiveParticleDispatch dispatch;
  std::size_t words;
  u_long code;
  int* counter;
};

// Restores an overlay's emission cursor after a test steps it.
class SavedCounter {
 public:
  explicit SavedCounter(int& counter) : counter_(counter), saved_(counter) {}
  SavedCounter(const SavedCounter&) = delete;
  SavedCounter& operator=(const SavedCounter&) = delete;
  SavedCounter(SavedCounter&&) = delete;
  SavedCounter& operator=(SavedCounter&&) = delete;
  ~SavedCounter() { counter_ = saved_; }

 private:
  int& counter_;
  int saved_;
};

// One scene table entry and a box-corner origin at z 512 aiming at a target
// at z 1024 on the view axis.
void PrepareLiveCornerSet(EmissionFixture& fixture) {
  fixture.package.block1TableCount = 1;
  fixture.package.block2Count = 1;
  auto& definition = fixture.definitions.entries[0];
  definition.flags = 0x1008;
  definition.position_range[4] = definition.position_range[5] = 512;
  definition.vector_ranges[6][4] = definition.vector_ranges[6][5] = 1024;
}

// The first segment's packet opens with its draw-mode word, carries the
// set's grey and the segment's GPU code, and closes with the reset word.
void CheckLiveSegmentPacket(const u_long* packet,
                            const LiveParticleCase& live) {
  CHECK(packet[1] >> 24 == 0xE1);
  CHECK(packet[2] == ((live.code << 24) | 0x808080));
  CHECK(packet[live.words - 1] == 0xE1000200);
}
}  // namespace

TEST_CASE("Live particle ticks link every ribbon segment into the scene table",
          "[effects][particle-dispatch][effects-particles]") {
  const auto live = GENERATE(
      LiveParticleCase{
          &VS::Effects::Templates::UpdateLivePlg045ParticleDispatch, 15, 0x3E,
          &VS::Effects::Plg045::particleEmissionCounter},
      LiveParticleCase{
          &VS::Effects::Templates::UpdateLivePlg065ParticleDispatch, 6, 0x42,
          &VS::Effects::Plg065::particleEmissionCounter},
      LiveParticleCase{
          &VS::Effects::Templates::UpdateLivePlg152ParticleDispatch, 15, 0x3E,
          &VS::Effects::Plg082::particleEmissionCounter});
  EmissionFixture fixture;
  const RecordedDefinitions recorded{fixture};
  DispatchHeap heap;
  const SavedCounter saved{*live.counter};
  *live.counter = 3;
  PrepareLiveCornerSet(fixture);
  Effects_RendererSlot renderer{};
  REQUIRE(live.dispatch(&renderer, 1, 0) == 1);
  REQUIRE(live.dispatch(&renderer, 2, 0) == 1);
  const auto* state =
      static_cast<VS::Effects::LiveParticleQuadSet*>(renderer.rendererState);
  REQUIRE(state != nullptr);
  CHECK(state->age == 1);
  CHECK(*live.counter == 4);
  const u_long* packet = fixture.storage.data() + 1;
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        packet + (8 * live.words));
  CheckLiveSegmentPacket(packet, live);
  CHECK(live.dispatch(&renderer, 4, 0) == 0);
  VS::Core::Memory::FreeLiveFromHighEnd(renderer.rendererState);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("Particle emission respects empty bursts and exhausted capacity",
          "[effects][particle-emission]") {
  EmissionFixture fixture;
  STATIC_REQUIRE(offsetof(EmissionFixture::Definitions, entries) ==
                 sizeof(VS::Effects::LiveParticleDefinitionTable));
  auto& definition = fixture.definitions.entries[0];
  const int requested = GENERATE(0, 1, 5);
  definition.count_range[0] = definition.count_range[1] =
      static_cast<std::int16_t>(requested);
  const bool full = GENERATE(false, true);
  VS::Effects::usedParticleCount = full ? 1 : 0;
  VS::Effects::freeParticles = full ? nullptr : &fixture.particle;
  fixture.instance.particles = full ? &fixture.particle : nullptr;
  fixture.instance.particleCount = full ? 1 : 0;
  fixture.particle.next = nullptr;
  VS::Core::SeedBiosRand(1234);
  const bool emits = requested > 0 && !full;

  VS::Effects::EmitLiveParticleEvent(&fixture.instance, 0, 0, 0, nullptr);

  fixture.CheckEmissionPool(full || emits);
  if (!emits) {
    CHECK(fixture.particle.age == 0);
    CHECK(VS::Core::NextBiosRand() == VS::Core::Rand15(1234).Next());
  } else {
    CHECK(static_cast<unsigned char>(fixture.particle.age) == 255);
    CHECK(fixture.particle.texture_animation.program ==
          reinterpret_cast<u_char*>(fixture.programs.data()) + 8);
  }
  VS::Core::SeedBiosRand(1);
}

TEST_CASE("Effect renderer slots retain eight independent native callbacks",
          "[effects][renderer-registry]") {
  STATIC_REQUIRE(std::size(Effects_effectRenderers) == 8);
  const auto saved = std::to_array(Effects_effectRenderers);
  std::array<std::byte, sizeof(Effects_actionPackage)> package_before;
  std::memcpy(package_before.data(), &Effects_actionPackage,
              package_before.size());
  for (int index = 0; index < 8; ++index) {
    CHECK(VS::Effects::LiveSetEffectRenderer(
        static_cast<VS::Effects::PluginEntry>(index), index));
  }
  for (int index = 0; index < 8; ++index) {
    CHECK(Effects_effectRenderers[index] ==
          VS::Effects::effectEntryPoints[index]);
  }
  std::array<std::byte, sizeof(Effects_actionPackage)> package_after;
  std::memcpy(package_after.data(), &Effects_actionPackage,
              package_after.size());
  CHECK(package_before == package_after);
  std::copy(saved.begin(), saved.end(), Effects_effectRenderers);

  for (std::size_t effect = 0;
       effect + 1 < std::size(VS::Effects::effectEntryPointIndices); ++effect) {
    const int count = VS::Effects::effectEntryPointIndices[effect + 1] -
                      VS::Effects::effectEntryPointIndices[effect];
    CHECK(count >= 0);
    CHECK(count <= 8);
  }
}

TEST_CASE("Primary effect request aliases the canonical native package field",
          "[effects][request-context]") {
  auto* saved_context = Effects_package;
  auto* saved_primary = Effects_actionPackage.request;
  auto* saved_alias = Effects_actionRequestPointer;
  auto* saved_secondary = Effects_secondaryPackage.request;

  Effects_actionPackage.request = nullptr;
  Effects_secondaryPackage.request = &Effects_secondaryRequest;
  Effects_actionRequestPointer = &Effects_actionRequest;
  CHECK(Effects_actionPackage.request == &Effects_actionRequest);
  CHECK(&Effects_actionRequestPointer == &Effects_actionPackage.request);
  VS::Effects::LiveSelectEffectResourceContext(1);
  CHECK(Effects_package == &Effects_actionPackage);
  CHECK(Effects_package->request == &Effects_actionRequest);

  Effects_actionPackage.request = &Effects_secondaryRequest;
  CHECK(Effects_actionRequestPointer == &Effects_secondaryRequest);
  VS::Effects::LiveSelectEffectResourceContext(0);
  CHECK(Effects_package == &Effects_secondaryPackage);
  CHECK(Effects_package->request == &Effects_secondaryRequest);

  Effects_actionPackage.request = saved_primary;
  Effects_actionRequestPointer = saved_alias;
  Effects_secondaryPackage.request = saved_secondary;
  Effects_package = saved_context;
}

TEST_CASE("Child effect tracks wait for the native outstanding child count",
          "[effects][child-tracks]") {
  ParticleFixture fixture;
  Effects_ActionEffectRequest request{};
  std::array<std::uint32_t, 3> key{};
  VS::Effects::LiveTimelineTrack track{};
  VS::Effects::LiveEffectTimeline timeline{};
  fixture.package.request = &request;
  fixture.instance.context.timeline = &timeline;
  fixture.instance.childCount = 1;
  track.keys = key.data();
  auto* saved_instances = Effects_primaryInstances;
  Effects_primaryInstances = nullptr;

  CHECK(VS::Effects::LiveTickChildEffectTrack(&fixture.instance, &track, 0) ==
        1);
  CHECK(fixture.instance.childCount == 1);
  CHECK(track.key_parameter == 0);
  CHECK(Effects_primaryInstances == nullptr);

  while (Effects_primaryInstances != nullptr) {
    VS::Effects::LiveDestroyPrimaryEffectInstance(Effects_primaryInstances);
  }
  Effects_primaryInstances = saved_instances;
}

TEST_CASE("Cone emission preserves original zero-height quotient and RNG",
          "[effects][particle-emission][particle-cone]") {
  EmissionFixture fixture;
  const auto height = GENERATE(0, 32, -32);
  const auto mode = GENERATE(0u, 0x800u, 0x1000u);
  const auto seed = GENERATE(1u, 1234u);
  CAPTURE(height, mode, seed);
  auto& definition = fixture.definitions.entries[0];
  definition.flags = 16 | mode;
  definition.extent_range[0] = definition.extent_range[1] = 32;
  definition.extent_range[2] = definition.extent_range[3] =
      static_cast<std::int16_t>(height);
  definition.extent_range[4] = definition.extent_range[5] = 48;
  definition.inner_extent_range[0] = definition.inner_extent_range[1] = 4096;
  VS::Core::Rand15 expected(seed);
  const auto angle = expected.Next();
  int vertical = 0;
  if (height != 0) {
    const int low = expected.Next();
    const int high = expected.Next();
    vertical = static_cast<int>(
        std::sqrt((low + (high << 16)) % (height * height + 1)));
  }
  const int x =
      height == 0
          ? -1
          : (((32 * VS::Core::Math::Cosine(angle)) >> 12) * vertical) / height;
  const int z =
      height == 0
          ? -1
          : (((48 * VS::Core::Math::Sine(angle)) >> 12) * vertical) / height;
  VS::Core::SeedBiosRand(seed);

  VS::Effects::EmitLiveParticleEvent(&fixture.instance, 0, 0, 0, nullptr);

  fixture.CheckEmissionPool(true);
  const auto& position = fixture.particle.motion.position;
  const std::array actual_position{position.vx, position.vy, position.vz};
  const std::array expected_position{x * 4096, vertical * 4096, z * 4096};
  CHECK(actual_position == expected_position);
  CHECK(VS::Core::NextBiosRand() == expected.Next());
  VS::Core::SeedBiosRand(1);
}

namespace {
struct ParticleMotionFixture {
  int saved_drag = VS::Effects::particleDrag;
  VECTOR saved_gravity = VS::Effects::particleGravity;
  VS::Effects::LiveParticle particle{};

  ParticleMotionFixture() {
    VS::Effects::particleDrag = 0;
    VS::Effects::particleGravity = {};
    particle.motion.mass = 4096;
  }
  ~ParticleMotionFixture() {
    VS::Effects::particleDrag = saved_drag;
    VS::Effects::particleGravity = saved_gravity;
  }
};
}  // namespace

TEST_CASE("Particle rendering flags cannot invent target attraction",
          "[effects][particle-motion][native-attraction]") {
  ParticleMotionFixture fixture;
  auto& particle = fixture.particle;
  particle.emit_flags = GENERATE(0, 1, 0x40, 0x20000, 0x40000, 0x80000);
  std::array<u_char, 4> program{};
  VS::Effects::LivePackageTextureFrame texture{};
  particle.texture_animation.program = program.data();
  particle.texture_animation.texture_info = &texture;
  particle.motion.velocity.vx = 4096;
  VS::Effects::LiveIntegrateParticleMotion(&particle);
  CHECK(particle.motion.position.vx == 4096);
  CHECK(particle.motion.position.vy == 0);
  CHECK(particle.motion.position.vz == 0);
  CHECK(particle.motion.force.vx == 0);
  CHECK(particle.motion.force.vy == 0);
  CHECK(particle.motion.force.vz == 0);
  CHECK(particle.texture_animation.program == program.data());
  CHECK(particle.texture_animation.texture_info == &texture);
}

TEST_CASE("Particle attraction uses its native target and signed strength",
          "[effects][particle-motion][native-attraction]") {
  ParticleMotionFixture fixture;
  const auto axis = GENERATE(0, 1, 2);
  auto& particle = fixture.particle;
  particle.attraction_target[axis] = 32;
  particle.attraction = GENERATE(-4096, 0, 4096);
  VS::Effects::LiveIntegrateParticleMotion(&particle);
  const std::array force{particle.motion.force.vx, particle.motion.force.vy,
                         particle.motion.force.vz};
  for (int component = 0; component < 3; ++component) {
    CHECK(force[component] == (component == axis ? particle.attraction : 0));
  }
  CHECK(particle.motion.position.vx == 0);
  CHECK(particle.motion.position.vy == 0);
  CHECK(particle.motion.position.vz == 0);
}

TEST_CASE("Linked particles retain independent native attraction state",
          "[effects][particle-motion][native-attraction]") {
  ParticleMotionFixture fixture;
  VS::Effects::LiveParticle second{};
  second.motion.mass = 4096;
  second.attraction = 4096;
  second.attraction_target[2] = 32;
  fixture.particle.emit_flags = 0x20000;
  fixture.particle.next = &second;
  VS::Effects::LiveIntegrateParticleMotion(&fixture.particle);
  CHECK(fixture.particle.motion.force.vx == 0);
  CHECK(fixture.particle.motion.force.vy == 0);
  CHECK(fixture.particle.motion.force.vz == 0);
  CHECK(second.motion.force.vx == 0);
  CHECK(second.motion.force.vy == 0);
  CHECK(second.motion.force.vz == 4096);
  CHECK(fixture.particle.next == &second);
}

namespace {
struct TrailGolden {
  int progress;
  int speed;
  std::array<std::int16_t, 3> point;
};

// Original trails5/9 recurrence and 5BF94.c:6388 fixed-point coefficients,
// including the original ONE*2/3 truncation rather than endpoint snapping.
constexpr std::array<TrailGolden, 12> kTrailGolden{{
    {699050, 699050, {22, -12, -12}},
    {2091373, 1392323, {63, -32, -32}},
    {4170037, 2078664, {115, -55, -52}},
    {6926568, 2756531, {169, -75, -64}},
    {10350375, 3423807, {216, -84, -59}},
    {14427840, 4077465, {247, -76, -28}},
    {19140806, 4712966, {259, -49, 32}},
    {24463853, 5323047, {256, -7, 116}},
    {30358851, 5894998, {251, 42, 211}},
    {36762250, 6403399, {269, 78, 291}},
    {43546949, 6784699, {344, 73, 319}},
    {50331648, 6784699, {511, 0, 255}},
}};

template <class Trail>
void CheckTrailInitialState(const Trail& trail) {
  CHECK(trail.step_count == 12);
  CHECK(trail.spline_sample_count == 1);
  CHECK(trail.trail_collapse_counter == std::size(trail.spline_samples) - 1);
  CHECK(trail.current_spline_sample == 0);
  CHECK(trail.spline_progress == 0);
  CHECK(trail.speed == 0);
}

template <class Trail>
void CheckTrailReferenceStep(const std::array<Trail, 2>& entries,
                             std::size_t frame) {
  const auto& trail = entries[0];
  const auto samples = std::size(trail.spline_samples);
  CHECK(trail.spline_progress == kTrailGolden[frame].progress);
  CHECK(trail.speed == kTrailGolden[frame].speed);
  CHECK(trail.current_spline_sample == (frame + 1) % samples);
  CHECK(trail.spline_sample_count == std::min(frame + 2, samples));
  const auto& point = trail.spline_samples[trail.current_spline_sample];
  const std::array actual{point.vx, point.vy, point.vz};
  CHECK(actual == kTrailGolden[frame].point);
  const auto& separate = entries[1];
  const auto& zero = separate.spline_samples[separate.current_spline_sample];
  CHECK(zero.vx == 0);
  CHECK(zero.vy == 0);
  CHECK(zero.vz == 0);
}

template <class Trail, class State, class Initialize, class Update>
void CheckTrailReferencePath(State& state, Initialize initialize,
                             Update update) {
  constexpr u_long kGuard = 0xA1B2C3D4;
  struct Storage {
    std::array<Trail, 2> entries{};
    u_long guard = kGuard;
  } storage;
  auto& trail = storage.entries[0];
  constexpr std::array<SVECTOR, 4> kPoints{
      {{0, 0, 0, 0}, {256, -64, 0, 0}, {256, 64, 256, 0}, {512, 0, 256, 0}}};
  std::ranges::copy(kPoints, trail.spline_points.begin());
  state.trails = storage.entries.data();
  VS::Effects::LiveParticleDefinition definition{};
  std::ranges::fill(definition.pair_ranges[4], 12);
  VS::Effects::LiveParticleEmitterScratch scratch{};
  Effects_EffectInstance instance{};
  const auto samples = std::size(trail.spline_samples);
  for (int cycle = 0; cycle < 2; ++cycle) {
    state.curve_sample_distance = 0;
    initialize(&definition, &scratch, &instance, &state);
    CheckTrailInitialState(trail);
    for (std::size_t frame = 0; frame < kTrailGolden.size(); ++frame) {
      state.curve_sample_distance = static_cast<int>(frame);
      update(&definition, &scratch, &instance, &state);
      CheckTrailReferenceStep(storage.entries, frame);
      CHECK(storage.guard == kGuard);
    }
    for (std::size_t tail = 0; tail < samples - 1; ++tail) {
      state.curve_sample_distance = 12 + static_cast<int>(tail);
      update(&definition, &scratch, &instance, &state);
      CHECK(trail.trail_collapse_counter == samples - 2 - tail);
      CHECK(storage.guard == kGuard);
    }
    CHECK(trail.spline_sample_count == 1);
  }
}

// The five-sample stages through their live binding, in the pointer shape the
// shared reference checks use.
template <VS::Effects::Templates::TrailStage Stage>
void RunTrailStage(VS::Effects::LiveParticleDefinition* definition,
                   VS::Effects::LiveParticleEmitterScratch* scratch,
                   Effects_EffectInstance* instance,
                   VS::Effects::LiveTrailState* state) {
  VS::Effects::Templates::RunLiveTrailStage(Stage, *definition, *scratch,
                                            *instance, *state);
}

// The nine-sample stages through their live binding, in the same shape.
template <VS::Effects::Templates::TrailNineStage Stage>
void RunTrailNineStage(VS::Effects::LiveParticleDefinition* definition,
                       VS::Effects::LiveParticleEmitterScratch* scratch,
                       Effects_EffectInstance* instance,
                       VS::Effects::LiveTrailNineState* state) {
  VS::Effects::Templates::RunLiveTrailNineStage(Stage, *definition, *scratch,
                                                *instance, *state);
}

// A spline point's x, y and z, compared as one value.
[[nodiscard]] std::array<int, 3> Coordinates(const SVECTOR& point) {
  return {point.vx, point.vy, point.vz};
}
}  // namespace

TEST_CASE(
    "Five-sample trails preserve source trajectory and repeat-entry state",
    "[effects][trails][reference]") {
  VS::Effects::LiveTrailState state{};
  state.trail_count = 2;
  CheckTrailReferencePath<VS::Effects::LiveTrail>(
      state, RunTrailStage<&VS::Effects::Plg057::TrailInit>,
      RunTrailStage<&VS::Effects::Plg057::TrailUpdate>);
}

TEST_CASE("Nine-sample trails preserve source trajectory and separate history",
          "[effects][trails][reference]") {
  VS::Effects::LiveTrailNineState state{};
  state.trail_count = 2;
  CheckTrailReferencePath<VS::Effects::LiveTrail9>(
      state, RunTrailNineStage<&VS::Effects::Templates::TrailNineInit>,
      RunTrailNineStage<&VS::Effects::Templates::TrailNineUpdate>);
}

namespace {
void CheckTrailReferenceQuad(const POLY_GT4& quad, int x, int y, int fade,
                             int samples) {
  CHECK(quad.tag == 0xE1000000);
  CHECK(quad.code ==
        std::to_underlying(VS::Render::LegacyPrimitiveCode::kPolyGt4SemiTrans));
  const std::array<int, 8> positions{quad.x0, quad.y0, quad.x1, quad.y1,
                                     quad.x2, quad.y2, quad.x3, quad.y3};
  const std::array expected_positions{x, y - 4, x + 8, y - 4,
                                      x, y + 4, x + 8, y + 4};
  CHECK(positions == expected_positions);
  const auto u0 = 16 + 8 * fade / (samples - 1);
  const auto u1 = 16 + 8 * (fade - 1) / (samples - 1);
  const std::array<int, 8> texture{quad.u0, quad.v0, quad.u1, quad.v1,
                                   quad.u2, quad.v2, quad.u3, quad.v3};
  const std::array expected_texture{u0, 32, u1, 32, u0, 40, u1, 40};
  CHECK(texture == expected_texture);
  const auto start = (fade + 1) * 8;
  const auto end = fade * 8;
  const std::array<int, 12> colors{quad.r0, quad.g0, quad.b0, quad.r1,
                                   quad.g1, quad.b1, quad.r2, quad.g2,
                                   quad.b2, quad.r3, quad.g3, quad.b3};
  const std::array expected_colors{start, start, start, end, end, end,
                                   start, start, start, end, end, end};
  CHECK(colors == expected_colors);
  CHECK(quad.clut == 0x1234);
  CHECK(quad.tpage == 0x567);
}

template <class Trail>
void InitializeTrailRenderHistory(std::array<Trail, 2>& trails,
                                  std::size_t count, std::size_t current) {
  constexpr auto kSamples = std::size(Trail{}.spline_samples);
  for (std::size_t actor = 0; actor < trails.size(); ++actor) {
    auto& trail = trails[actor];
    trail.spline_sample_count = static_cast<u_char>(count);
    trail.trail_collapse_counter = static_cast<u_char>(count - 1);
    trail.current_spline_sample = static_cast<u_char>(current);
    for (std::size_t index = 0; index < kSamples; ++index) {
      trail.spline_samples[(current + kSamples - index) % kSamples] = {
          static_cast<std::int16_t>(index * 32),
          static_cast<std::int16_t>(actor * 64), 1024, 0};
    }
  }
}

void CheckTrailReferenceRenderState(
    const VS::Effects::LiveParticleTextureAnimation& animation,
    const ParticleFixture& fixture, const u_long* end) {
  CHECK(animation.cursor == 4);
  CHECK(animation.frame_timer == 8);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == end);
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

template <class Trail, class Packet, class ReadQuad>
void CheckTrailReferencePackets(const u_long* begin, std::size_t trail_count,
                                std::size_t count, int origin_x,
                                ReadQuad read_quad) {
  constexpr auto kPacketWords = sizeof(Packet) / sizeof(u_long);
  constexpr auto kSamples = std::size(Trail{}.spline_samples);
  for (std::size_t actor = 0; actor < trail_count; ++actor) {
    for (std::size_t segment = 0; segment < count - 1; ++segment) {
      const auto packet_index = actor * (count - 1) + segment;
      Packet packet{};
      std::memcpy(&packet, begin + packet_index * kPacketWords, sizeof(packet));
      const auto& quad = read_quad(packet);
      const auto x = origin_x + static_cast<int>(segment * 8);
      const auto y = 120 + static_cast<int>(actor * 16);
      CHECK(packet.word_count == 14);
      CHECK(static_cast<std::uint32_t>(packet.draw_mode) == 0xE1000200u);
      const auto fade = count - 1 - segment;
      CheckTrailReferenceQuad(quad, x, y, static_cast<int>(fade),
                              static_cast<int>(kSamples));
    }
  }
}

template <class Trail, class Packet, class State, class Render, class ReadQuad>
void CheckTrailReferenceRendering(
    State& state, VS::Effects::LiveParticleTextureAnimation& animation,
    std::span<u_char> transparency, Render render, ReadQuad read_quad) {
  constexpr auto kSamples = std::size(Trail{}.spline_samples);
  const bool attached = GENERATE(false, true);
  const bool clipped = GENERATE(false, true);
  const bool at_boundary = GENERATE(false, true);
  const auto count = GENERATE_COPY(std::size_t{1}, std::size_t{2}, kSamples);
  const auto current = GENERATE_COPY(std::size_t{0}, kSamples - 1);
  ParticleFixture fixture;
  fixture.package.block1TableCount = 1;
  std::array<Trail, 2> trails{};
  InitializeTrailRenderHistory(trails, count, current);
  state.trails = trails.data();
  animation.program = reinterpret_cast<u_char*>(fixture.script.data());
  for (std::size_t index = 0; index < transparency.size(); ++index) {
    transparency[index] = static_cast<u_char>((index + 1) * 8);
  }
  VS::Effects::LiveParticleDefinition definition{};
  definition.vector_ranges[4][0] = definition.vector_ranges[4][1] = 8;
  definition.transform_index = 1;
  VS::Effects::LiveParticleEmitterScratch scratch{};
  scratch.flags = attached ? 0x20000 : 0;
  VS::Render::FrameScratchpad().view_matrix.t[0] = -64;
  fixture.instance.transforms[1].view =
      VS::Render::FrameScratchpad().view_matrix;
  fixture.instance.transforms[1].view.t[0] = 128;
  VS::Render::nearClip = clipped ? 256 : 255;
  constexpr auto kPacketWords = sizeof(Packet) / sizeof(u_long);
  const auto packets = trails.size() * (count - 1);
  auto* begin = fixture.storage.data() + 1;
  if (at_boundary) begin += 0x8000 - packets * kPacketWords;
  VS::Render::FrameScratchpad().primitive_cursor = begin;
  render(&definition, &scratch, &fixture.instance, &state);
  CheckTrailReferenceRenderState(
      animation, fixture, begin + (clipped ? 0 : packets * kPacketWords));
  if (clipped || count == 1) return;

  // trails5.h:449 / trails9.h:456: horizontal samples at Z1024 project at
  // one quarter scale; width8 gives +/-4 pixels and SZ3/4 gives depth256.
  CheckTrailReferencePackets<Trail, Packet>(begin, trails.size(), count,
                                            attached ? 192 : 144, read_quad);
  const auto* last = begin + (packets - 1) * kPacketWords;
  CHECK((fixture.ordering[16 + 256] & 0xFFFFFF) ==
        (LegacyGpu_Handle(last) & 0xFFFFFF));
}
}  // namespace

TEST_CASE("Five-sample trail ribbons retain source geometry and render spaces",
          "[effects][trails][reference][trail-render]") {
  VS::Effects::LiveTrailState state{};
  state.trail_count = 2;
  CheckTrailReferenceRendering<VS::Effects::LiveTrail,
                               VS::Render::EffectGouraudQuadPacket>(
      state, state.texture, state.transparency,
      RunTrailStage<&VS::Effects::Plg057::TrailRender>,
      [](const auto& packet) -> const POLY_GT4& { return packet.opaque4; });
}

TEST_CASE("Nine-sample trail ribbons retain source geometry and render spaces",
          "[effects][trails][reference][trail-render]") {
  VS::Effects::LiveTrailNineState state{};
  state.trail_count = 2;
  CheckTrailReferenceRendering<VS::Effects::LiveTrail9,
                               VS::Render::Trail9Packet>(
      state, state.texture, state.transparency,
      RunTrailNineStage<&VS::Effects::Templates::TrailNineRender>,
      [](const auto& packet) -> const POLY_GT4& { return packet.poly; });
}

namespace {
template <class Function>
Function Plg074Entry(unsigned address, Function expected) {
  auto* actual = VS::Content::ResolveModuleAddress(
      VS::Content::ModuleSymbols(), "EFFECT/PLG074.BIN", address);
  REQUIRE(actual == reinterpret_cast<void*>(expected));
  return reinterpret_cast<Function>(actual);
}

struct Plg074Fixture : EmissionFixture {
  int saved_sample = Effects_curveSamples[0];
  int saved_corner = VS::Effects::Plg073::trailEmissionCounter;
  int saved_plg074_corner = VS::Content::plg074TrailEmissionCounter[0];
  Effects_ActionEffectRequest saved_request = Effects_actionRequest;
  std::array<CVECTOR, 8> saved_colors{};
  std::array<std::array<u_char, 4>, 8> saved_arc_colors{};
  VS::Effects::LiveParticleEmitterScratch scratch{};
  std::array<VS::Effects::LiveTrail, 16> trails{};
  VS::Effects::LiveTrailState trail_state{};

  Plg074Fixture() {
    package.block1TableCount = 1;
    package.block2Count = 1;
    Effects_curveSamples[0] = 0;
    VS::Effects::Plg073::trailEmissionCounter = 0;
    VS::Content::plg074TrailEmissionCounter[0] = 0;
    Effects_actionRequest.position = {400, -500, 600, 0};
    std::ranges::copy(VS::Effects::Plg073::tiledRingColorFactors,
                      saved_colors.begin());
    std::memcpy(saved_arc_colors.data(), VS::Effects::Plg073::ringColorFactors,
                sizeof saved_arc_colors);
    VS::Effects::Plg073::tiledRingColorFactors[0] = {32, 64, 96, 0};
    for (int mode = 0; mode < 8; ++mode) {
      for (int row = 0; row < 3; ++row) {
        VS::Effects::Plg073::ringColorFactors[mode][row] =
            static_cast<u_char>(32 * (row + 1) + mode);
      }
    }
    instance.transforms[0].world.t[0] = 700;
    instance.transforms[0].world.t[1] = -800;
    instance.transforms[0].world.t[2] = 900;
    trail_state.trails = trails.data();
    trail_state.trail_count = 8;
    VS::Effects::ForgetLivePackageExtents();
    VS::Effects::RecordLiveParticleDefinitions(
        std::as_bytes(std::span{&definitions, 1}));
  }

  ~Plg074Fixture() {
    VS::Effects::ForgetLivePackageExtents();
    Effects_curveSamples[0] = saved_sample;
    VS::Effects::Plg073::trailEmissionCounter = saved_corner;
    VS::Content::plg074TrailEmissionCounter[0] = saved_plg074_corner;
    Effects_actionRequest = saved_request;
    std::ranges::copy(saved_colors, VS::Effects::Plg073::tiledRingColorFactors);
    std::memcpy(VS::Effects::Plg073::ringColorFactors, saved_arc_colors.data(),
                sizeof saved_arc_colors);
  }

  VS::Effects::LiveParticleDefinition& Definition() {
    return definitions.entries[0];
  }

  void ConfigureRing() {
    auto& definition = Definition();
    std::ranges::copy(std::array<std::int16_t, 6>{0, 0, 0, 0, 1024, 1024},
                      definition.position_range.begin());
    std::ranges::copy(std::array<std::int16_t, 6>{128, 128, 64, 64, 128, 128},
                      definition.extent_range.begin());
    std::ranges::fill(definition.inner_extent_range, 2048);
    definition.vector_ranges[0][0] = definition.vector_ranges[0][1] = 1024;
    definition.vector_ranges[1][0] = definition.vector_ranges[1][1] = 4096;
  }

  void ResetPackets() {
    VS::Render::FrameScratchpad().primitive_cursor = storage.data() + 1;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }

  void CheckGuards() const {
    CHECK(storage.front() == 0xDEADBEEF);
    CHECK(storage.back() == 0xDEADBEEF);
  }
};

template <class State>
void CheckPlg074RingInitialState(const State& state) {
  CHECK(state.age == 0);
  CHECK(state.angle == 0);
  CHECK(state.radius_speed == 0);
  CHECK(state.height_speed == 0);
}

template <class State>
void StopPlg074Ring(Effects_EffectRenderer renderer,
                    Effects_RendererSlot& slot) {
  auto* state = static_cast<State*>(slot.rendererState);
  CHECK(renderer(&slot, 3, 0) == 1);
  CHECK(renderer(&slot, 2, 0) == 0);
  CHECK(state->age == 2);
  CHECK(state->lifetime == 0);
  CHECK(renderer(&slot, 4, 0) == 0);
}

template <class State>
void CheckPlg074RingLifecycle(Effects_EffectRenderer renderer,
                              Plg074Fixture& fixture) {
  DispatchHeap heap;
  for (const int lifetime : {2, 0}) {
    Effects_RendererSlot slot{};
    REQUIRE(renderer(&slot, 1, lifetime) == 1);
    REQUIRE(slot.rendererState != nullptr);
    REQUIRE(DispatchHeap::Capacity(slot.rendererState) >= sizeof(State));
    auto* state = static_cast<State*>(slot.rendererState);
    CheckPlg074RingInitialState(*state);
    REQUIRE(renderer(&slot, 2, 0) == 1);
    CHECK(state->age == 1);
    CHECK(state->lifetime == (lifetime == 0 ? 0 : 1));
    fixture.ResetPackets();
    StopPlg074Ring<State>(renderer, slot);
    VS::Core::Memory::FreeLiveFromHighEnd(state);
    CHECK(VS::Core::Memory::GameHeap().Empty());
    fixture.ResetPackets();
  }
}

void CheckPlg074TiledRingPackets(const VS::Render::TiledRingPacket* packets) {
  for (int index = 0; index < 32; ++index) {
    const auto& quad = packets[index].poly;
    CHECK(quad.u0 == 16);
    CHECK(quad.u1 == 24);
    CHECK(quad.v0 == (index < 16 ? 32 : 36));
    CHECK(quad.v2 == (index < 16 ? 36 : 40));
    CHECK(quad.r0 == (index < 16 ? 32 : 64));
    CHECK(quad.r2 == (index < 16 ? 64 : 96));
    CHECK(quad.clut == 0x1234);
    CHECK(quad.tpage == 0x567);
    CHECK(quad.code == std::to_underlying(
                           VS::Render::LegacyPrimitiveCode::kPolyGt4SemiTrans));
  }
}

void CheckPlg074TiledRingGeometry(const VS::Render::TiledRingPacket* packets) {
  CHECK(packets[0].poly.x0 == 192);
  CHECK(packets[0].poly.y0 == 120);
  CHECK(packets[0].poly.y2 == 128);
  CHECK(packets[15].poly.x1 == packets[0].poly.x0);
  CHECK(packets[15].poly.y1 == packets[0].poly.y0);
}

void CheckPlg074RingArcPackets(const VS::Render::RingPacket* packets,
                               int mode) {
  for (int index = 0; index < 16; ++index) {
    const auto& quad = packets[index].poly;
    CHECK(quad.u0 == 16 + index % 8);
    CHECK(quad.u1 == 17 + index % 8);
    CHECK(quad.v0 == (index < 8 ? 32 : 36));
    CHECK(quad.v2 == (index < 8 ? 36 : 40));
    CHECK(quad.r0 == (index < 8 ? 32 : 64) + mode);
    CHECK(quad.r2 == (index < 8 ? 64 : 96) + mode);
  }
}

void CheckGravityRingMaterial(const VS::Render::ClosedRingPacket* packets) {
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == packets + 32);
  CHECK(packets[0].poly.clut == 0x1234);
  CHECK(packets[0].poly.tpage == 0x567);
  CHECK(packets[0].poly.u0 == 16);
  CHECK(packets[0].poly.v0 == 32);
  CHECK(packets[0].poly.code ==
        std::to_underlying(VS::Render::LegacyPrimitiveCode::kPolyGt4SemiTrans));
}
}  // namespace

TEST_CASE("PLG074 F01986 draws tiled rings through the production slot",
          "[effects][plg074][f01986]") {
  const auto entry = Plg074Entry(
      0x800F98F8, &VS::Effects::Templates::UpdateLivePlg073TiledRing);
  REQUIRE(VS::Effects::effectEntryPoints[87] == entry);
  Plg074Fixture fixture;
  fixture.ConfigureRing();
  DispatchHeap heap;
  Effects_RendererSlot slot{};
  REQUIRE(entry(&slot, 1, 2) == 1);
  REQUIRE(entry(&slot, 2, 0) == 1);
  const auto* packets = reinterpret_cast<const VS::Render::TiledRingPacket*>(
      fixture.storage.data() + 1);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == packets + 32);
  CheckPlg074TiledRingGeometry(packets);
  CheckPlg074TiledRingPackets(packets);
  fixture.CheckGuards();
  VS::Core::Memory::FreeLiveFromHighEnd(slot.rendererState);
  fixture.ResetPackets();
  CheckPlg074RingLifecycle<VS::Effects::LiveRingState>(entry, fixture);
}

TEST_CASE("PLG074 F01987 draws an eight-segment arc with continuous UVs",
          "[effects][plg074][f01987]") {
  const auto entry =
      Plg074Entry(0x800FA7F0, &VS::Effects::Templates::UpdateLivePlg073Ring);
  REQUIRE(VS::Effects::effectEntryPoints[88] == entry);
  const auto mode = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  Plg074Fixture fixture;
  fixture.ConfigureRing();
  fixture.Definition().transparency_curve = static_cast<u_int>(mode);
  DispatchHeap heap;
  Effects_RendererSlot slot{};
  REQUIRE(entry(&slot, 1, 2) == 1);
  REQUIRE(entry(&slot, 2, 0) == 1);
  const auto* packets = reinterpret_cast<const VS::Render::RingPacket*>(
      fixture.storage.data() + 1);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == packets + 16);
  CheckPlg074RingArcPackets(packets, mode);
  CHECK(packets[0].poly.x0 == 192);
  CHECK(packets[0].poly.y0 == 120);
  CHECK(packets[7].poly.x1 == packets[0].poly.x0);
  fixture.CheckGuards();
  VS::Core::Memory::FreeLiveFromHighEnd(slot.rendererState);
  fixture.ResetPackets();
  CheckPlg074RingLifecycle<VS::Effects::LiveRingState>(entry, fixture);
}

TEST_CASE("PLG074 F01988 cycles all box corners in each anchor space",
          "[effects][plg074][f01988]") {
  const int anchor = GENERATE(0, 1, 2);
  Plg074Fixture fixture;
  Effects_actionRequest.position = {100, -200, 300, 0};
  auto& definition = fixture.Definition();
  std::ranges::copy(std::array<std::int16_t, 6>{10, 10, 20, 20, 30, 30},
                    definition.extent_range.begin());
  fixture.scratch.flags = 0x1008 | anchor;
  VS::Effects::Plg073::trailEmissionCounter = 5;
  const std::array offsets{std::array{0, 0, 0}, std::array{100, -200, 300},
                           std::array{700, -800, 900}};
  for (int cycle = 0; cycle < 2; ++cycle) {
    VS::Effects::GenerateLivePlg074TrailOrigins(
        definition, fixture.scratch, fixture.instance, fixture.trail_state);
    for (int index = 0; index < 8; ++index) {
      const auto& point = fixture.trails[index].spline_points[0];
      const int corner = index + 1;
      CHECK(point.vx == offsets[anchor][0] + (corner & 1 ? 10 : -10));
      CHECK(point.vy == offsets[anchor][1] + (corner & 2 ? 20 : -20));
      CHECK(point.vz == offsets[anchor][2] + (corner & 4 ? 30 : -30));
    }
    CHECK(VS::Content::plg074TrailEmissionCounter[0] == 0);
    CHECK(VS::Effects::Plg073::trailEmissionCounter == 5);
    CHECK(fixture.trails[8].spline_points[0].vx == 0);
  }
}

TEST_CASE("PLG074 F01989 targets world request and actor anchors",
          "[effects][plg074][f01989]") {
  const auto entry = RunTrailStage<&VS::Effects::Plg073::TrailTarget>;
  const unsigned anchor = GENERATE(0u, 0x80u, 0xC0u);
  Plg074Fixture fixture;
  auto& definition = fixture.Definition();
  std::ranges::copy(std::array<std::int16_t, 6>{10, 10, -20, -20, 30, 30},
                    definition.vector_ranges[6].begin());
  fixture.scratch.flags = anchor;
  entry(&definition, &fixture.scratch, &fixture.instance, &fixture.trail_state);
  const auto offset = anchor == 0x80   ? std::array{400, -500, 600}
                      : anchor == 0xC0 ? std::array{700, -800, 900}
                                       : std::array{0, 0, 0};
  for (int index = 0; index < 8; ++index) {
    const auto& point = fixture.trails[index].spline_points[3];
    CHECK(point.vx == offset[0] + 10);
    CHECK(point.vy == offset[1] - 20);
    CHECK(point.vz == offset[2] + 30);
    CHECK(fixture.trails[index].spline_points[0].vx == 0);
  }
  CHECK(fixture.trails[8].spline_points[3].vx == 0);
}

TEST_CASE("PLG074 F01990 builds axial spline controls without moving endpoints",
          "[effects][plg074][f01990]") {
  const auto entry = RunTrailStage<&VS::Effects::Plg073::TrailControl>;
  Plg074Fixture fixture;
  fixture.trail_state.trail_count = 1;
  auto& points = fixture.trails[0].spline_points;
  points[0] = {10, 20, 30, 0};
  points[3] = {10, 20, 1054, 0};
  std::ranges::copy(std::array<std::int16_t, 4>{64, 64, -64, -64},
                    fixture.Definition().pair_ranges[2].begin());
  entry(&fixture.Definition(), &fixture.scratch, &fixture.instance,
        &fixture.trail_state);
  CHECK(points[1].vx == 10);
  CHECK(points[1].vy == 20);
  CHECK(points[1].vz == 286);
  CHECK(points[2].vx == 10);
  CHECK(points[2].vy == 20);
  CHECK(points[2].vz == 798);
  CHECK(points[0].vz == 30);
  CHECK(points[3].vz == 1054);
  CHECK(fixture.trails[1].spline_points[1].vz == 0);
}

TEST_CASE("Nine-sample live trail targets aim held trails at their anchor",
          "[effects][trails][trail-nine-stages]") {
  const auto entry =
      RunTrailNineStage<&VS::Effects::Templates::TrailNineTarget>;
  const unsigned anchor = GENERATE(0u, 0x80u, 0xC0u);
  Plg074Fixture fixture;
  auto& definition = fixture.Definition();
  std::ranges::copy(std::array<std::int16_t, 6>{10, 10, -20, -20, 30, 30},
                    definition.vector_ranges[6].begin());
  fixture.scratch.flags = anchor;
  std::array<VS::Effects::LiveTrail9, 8> trails{};
  VS::Effects::LiveTrailNineState state{};
  state.trails = trails.data();
  state.trail_count = 7;
  entry(&definition, &fixture.scratch, &fixture.instance, &state);
  const auto offset = anchor == 0x80   ? std::array{400, -500, 600}
                      : anchor == 0xC0 ? std::array{700, -800, 900}
                                       : std::array{0, 0, 0};
  for (std::size_t index = 0; index < 7; ++index) {
    const auto& points = trails.at(index).spline_points;
    CHECK(Coordinates(points[3]) ==
          std::array{offset[0] + 10, offset[1] - 20, offset[2] + 30});
    CHECK(Coordinates(points[0]) == std::array{0, 0, 0});
  }
  CHECK(Coordinates(trails[7].spline_points[3]) == std::array{0, 0, 0});
}

TEST_CASE("Nine-sample live trail controls sit their shares along the spline",
          "[effects][trails][trail-nine-stages]") {
  const auto entry =
      RunTrailNineStage<&VS::Effects::Templates::TrailNineControl>;
  Plg074Fixture fixture;
  std::array<VS::Effects::LiveTrail9, 2> trails{};
  VS::Effects::LiveTrailNineState state{};
  state.trails = trails.data();
  state.trail_count = 1;
  auto& points = trails[0].spline_points;
  points[0] = {10, 20, 30, 0};
  points[3] = {10, 20, 1054, 0};
  std::ranges::copy(std::array<std::int16_t, 4>{64, 64, -64, -64},
                    fixture.Definition().pair_ranges[2].begin());
  entry(&fixture.Definition(), &fixture.scratch, &fixture.instance, &state);
  CHECK(Coordinates(points[1]) == std::array{10, 20, 286});
  CHECK(Coordinates(points[2]) == std::array{10, 20, 798});
  CHECK(Coordinates(points[0]) == std::array{10, 20, 30});
  CHECK(Coordinates(points[3]) == std::array{10, 20, 1054});
  CHECK(Coordinates(trails[1].spline_points[1]) == std::array{0, 0, 0});
}

namespace {
void CheckPlg074TrailInitialization(const VS::Effects::LiveTrail& trail,
                                    int steps) {
  CHECK(trail.step_count == steps);
  CHECK(trail.spline_sample_count == 1);
  CHECK(trail.trail_collapse_counter == 4);
  CHECK(trail.current_spline_sample == 0);
  CHECK(trail.spline_progress == 0);
  CHECK(trail.speed == 8192);
  CHECK(trail.first_step == 0x06000000 / (steps * steps) - 16384 / steps);
}

void CheckPlg074EmitterInitialization(const VS::Effects::LiveTrailState& state,
                                      int count) {
  CHECK(state.trail_count == std::min(count, 16));
  CHECK(state.curve_sample_distance == 0);
  CHECK(state.initialized == 0);
  CHECK(state.transparency[0] == 128);
  CHECK(state.transparency[4] == 128);
}

void CheckPlg074EmitterTick(const VS::Effects::LiveTrailState& state, int count,
                            int cycle) {
  CHECK(state.initialized == 1);
  CHECK(state.curve_sample_distance == 1);
  CHECK(VS::Content::plg074TrailEmissionCounter[0] ==
        ((cycle + 1) * std::min(count, 16)) % 8);
  CHECK(VS::Effects::Plg073::trailEmissionCounter == 0);
}

void StopPlg074Emitter(Effects_EffectRenderer entry, Effects_RendererSlot& slot,
                       bool cancel) {
  if (cancel) {
    CHECK(entry(&slot, 4, 0) == 0);
  } else {
    CHECK(entry(&slot, 3, 0) == 1);
    CHECK(entry(&slot, 2, 0) == 0);
  }
}

void CheckPlg074SurfacePackets(
    const VS::Render::RotatingSurfacePacket* packets) {
  for (std::size_t row = 0; row < 4; ++row) {
    const auto& quad = packets[row * 16].poly;
    CHECK(quad.x0 == 192);
    CHECK(quad.y0 == 120 + row * 16);
    CHECK(quad.y2 == 136 + row * 16);
    CHECK(quad.r0 == 255);
    CHECK(quad.r2 == 255);
    CHECK(quad.u0 == 16);
    CHECK(quad.v0 == 32 + row * 2);
    CHECK(quad.v2 == 34 + row * 2);
  }
}
}  // namespace

TEST_CASE(
    "PLG074 F01991 initializes bounded trail histories and zero durations",
    "[effects][plg074][f01991]") {
  const auto entry = RunTrailStage<&VS::Effects::Plg073::TrailInit>;
  const std::int16_t duration = GENERATE(0, 1, 12, 255);
  Plg074Fixture fixture;
  std::ranges::fill(fixture.Definition().pair_ranges[4], duration);
  std::ranges::fill(fixture.Definition().inner_extent_range, 2);
  fixture.trails[0].spline_points[0] = {10, 20, 30, 0};
  entry(&fixture.Definition(), &fixture.scratch, &fixture.instance,
        &fixture.trail_state);
  for (int index = 0; index < 8; ++index) {
    const auto& trail = fixture.trails[index];
    const int steps = duration == 0 ? 1 : duration;
    CheckPlg074TrailInitialization(trail, steps);
    CHECK(trail.spline_samples[0].vx == (index == 0 ? 10 : 0));
  }
  CHECK(fixture.trails[8].step_count == 0);
}

TEST_CASE("PLG074 F01992 matches the source recurrence and tail collapse",
          "[effects][plg074][f01992]") {
  const auto entry = RunTrailStage<&VS::Effects::Plg073::TrailUpdate>;
  VS::Effects::LiveTrailState state{};
  state.trail_count = 2;
  CheckTrailReferencePath<VS::Effects::LiveTrail>(
      state, RunTrailStage<&VS::Effects::Plg073::TrailInit>, entry);
}

TEST_CASE(
    "PLG074 F01993 preserves ribbon packets clipping and circular history",
    "[effects][plg074][f01993]") {
  const auto entry = RunTrailStage<&VS::Effects::Plg073::TrailRender>;
  VS::Effects::LiveTrailState state{};
  state.trail_count = 2;
  CheckTrailReferenceRendering<VS::Effects::LiveTrail,
                               VS::Render::EffectGouraudQuadPacket>(
      state, state.texture, state.transparency, entry,
      [](const auto& packet) -> const POLY_GT4& { return packet.opaque4; });
}

TEST_CASE("PLG074 F01994 owns trail buffers across expiry cancel and reentry",
          "[effects][plg074][f01994]") {
  const Effects_EffectRenderer entry =
      &VS::Effects::UpdateLivePlg074TrailEmitter;
  REQUIRE(VS::Effects::effectEntryPoints[89] == entry);
  const std::int16_t count = GENERATE(0, 1, 16, 20);
  const bool cancel = GENERATE(false, true);
  Plg074Fixture fixture;
  DispatchHeap heap;
  fixture.Definition().interval_range[0] = count;
  fixture.Definition().flags = 0x1008;
  std::ranges::fill(fixture.Definition().pair_ranges[4], 12);
  for (int cycle = 0; cycle < 2; ++cycle) {
    Effects_RendererSlot slot{};
    REQUIRE(entry(&slot, 1, 2) == 1);
    auto* state = static_cast<VS::Effects::LiveTrailState*>(slot.rendererState);
    REQUIRE(state != nullptr);
    CheckPlg074EmitterInitialization(*state, count);
    REQUIRE(entry(&slot, 2, 0) == 1);
    CheckPlg074EmitterTick(*state, count, cycle);
    StopPlg074Emitter(entry, slot, cancel);
    VS::Core::Memory::FreeLiveFromHighEnd(state);
    CHECK(VS::Core::Memory::GameHeap().Empty());
    fixture.ResetPackets();
  }
}

namespace {
struct LogCapture {
  std::vector<std::string> messages;
  VS::Core::LogObserver previous;

  LogCapture()
      : previous(
            VS::Core::SetLogObserver([this](const VS::Core::LogRecord& record) {
              if (record.channel == "effects") {
                messages.emplace_back(record.message);
              }
            })) {}
  LogCapture(const LogCapture&) = delete;
  LogCapture& operator=(const LogCapture&) = delete;
  LogCapture(LogCapture&&) = delete;
  LogCapture& operator=(LogCapture&&) = delete;
  ~LogCapture() { VS::Core::SetLogObserver(std::move(previous)); }

  [[nodiscard]] bool Contains(std::string_view text) const {
    return std::ranges::any_of(messages, [text](const std::string& message) {
      return message.find(text) != std::string::npos;
    });
  }
};

// A package whose curve block (type 4, 20 bytes) holds three curves of one,
// four and two entries after a four-byte header and three size bytes.
struct CurveBlockStorage {
  alignas(4) std::array<u_char, 24> bytes{20, 0, 4, 0, 3,  0,  8,  0,
                                          1,  4, 2, 0, 90, 10, 20, 30,
                                          40, 5, 6, 0, 0,  0,  0,  0};

  // Binding a package forgets the previous blocks' extents, so the caller's
  // particle definitions are recorded again.
  void Bind(std::span<const std::byte> definitions) {
    REQUIRE(VS::Effects::BindLiveEffectPackage(
        std::as_writable_bytes(std::span(bytes))));
    VS::Effects::RecordLiveParticleDefinitions(definitions);
  }
};

std::array<u_char, 5> CreatedTransparency(Plg074Fixture& fixture,
                                          u_char curve) {
  fixture.Definition().transparency_curve = curve;
  Effects_RendererSlot slot{};
  REQUIRE(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 1, 0) == 1);
  auto* state = static_cast<VS::Effects::LiveTrailState*>(slot.rendererState);
  REQUIRE(state != nullptr);
  std::array<u_char, 5> samples = state->transparency;
  REQUIRE(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 4, 0) == 0);
  VS::Core::Memory::FreeLiveFromHighEnd(state);
  return samples;
}
}  // namespace

TEST_CASE("PLG074 promoted trail functions no longer resolve as module code",
          "[effects][plg074][trail-emitter]") {
  for (const unsigned address :
       {0x800FB414u, 0x800FBBB0u, 0x800FBDC8u, 0x800FC28Cu, 0x800FC404u,
        0x800FC590u, 0x800FCFECu}) {
    CHECK(VS::Content::ResolveModuleAddress(VS::Content::ModuleSymbols(),
                                            "EFFECT/PLG074.BIN",
                                            address) == nullptr);
  }
}

TEST_CASE("PLG074 trail emitter refuses definitions the package lacks",
          "[effects][plg074][trail-emitter]") {
  Plg074Fixture fixture;
  DispatchHeap heap;
  LogCapture log;
  Effects_RendererSlot slot{};
  CHECK(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 1, 0x102) == 1);
  CHECK(slot.rendererState == nullptr);
  CHECK(log.Contains("definition 1 is outside"));
  CHECK(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 2, 0) == 0);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("PLG074 trail emitter refuses negative trail counts",
          "[effects][plg074][trail-emitter]") {
  Plg074Fixture fixture;
  DispatchHeap heap;
  LogCapture log;
  Effects_RendererSlot slot{};
  fixture.Definition().interval_range[0] = -1;
  CHECK(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 1, 0) == 1);
  CHECK(slot.rendererState == nullptr);
  CHECK(log.Contains("negative trail count"));
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("PLG074 trail emitter stops when its package is replaced",
          "[effects][plg074][trail-emitter]") {
  Plg074Fixture fixture;
  DispatchHeap heap;
  LogCapture log;
  Effects_RendererSlot slot{};
  fixture.Definition().interval_range[0] = 2;
  REQUIRE(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 1, 0) == 1);
  auto* state = static_cast<VS::Effects::LiveTrailState*>(slot.rendererState);
  REQUIRE(state != nullptr);
  VS::Effects::ForgetLivePackageExtents();
  CHECK(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 2, 0) == 0);
  CHECK(state->trails == nullptr);
  CHECK(log.Contains("no longer holds definition 0"));
  VS::Core::Memory::FreeLiveFromHighEnd(state);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("PLG074 trail transparency reads only the package's bound curves",
          "[effects][plg074][trail-emitter]") {
  Plg074Fixture fixture;
  DispatchHeap heap;
  LogCapture log;
  CurveBlockStorage curves;
  curves.Bind(std::as_bytes(std::span{&fixture.definitions, 1}));
  fixture.Definition().interval_range[0] = 1;
  CHECK(CreatedTransparency(fixture, 0) ==
        std::array<u_char, 5>{128, 128, 128, 128, 128});
  CHECK(CreatedTransparency(fixture, 1) ==
        std::array<u_char, 5>{10, 10, 20, 30, 40});
  CHECK(CreatedTransparency(fixture, 2) ==
        std::array<u_char, 5>{5, 5, 5, 5, 6});
  CHECK(CreatedTransparency(fixture, 3) ==
        std::array<u_char, 5>{128, 128, 128, 128, 128});
  CHECK(log.Contains("transparency curve 3 is outside"));
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("PLG074 trail tick events stay within the package's definitions",
          "[effects][plg074][trail-emitter]") {
  Plg074Fixture fixture;
  DispatchHeap heap;
  LogCapture log;
  fixture.Definition().interval_range[0] = 1;
  fixture.Definition().tick_event = 2;
  Effects_RendererSlot slot{};
  REQUIRE(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 1, 0) == 1);
  auto* state = static_cast<VS::Effects::LiveTrailState*>(slot.rendererState);
  REQUIRE(state != nullptr);
  CHECK(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 2, 0) == 1);
  CHECK(log.Contains("tick event 1 is outside"));
  CHECK(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 4, 0) == 0);
  VS::Core::Memory::FreeLiveFromHighEnd(state);
  CHECK(VS::Core::Memory::GameHeap().Empty());
  fixture.ResetPackets();
}

namespace {

// Binds the three-curve package and leaves a curve an earlier package bound
// in the slot just past it, so only the bound check keeps it out.
struct StaleCurveBlock {
  explicit StaleCurveBlock(Plg074Fixture& fixture) {
    curves.Bind(std::as_bytes(std::span{&fixture.definitions, 1}));
    curves.bytes[11] = static_cast<u_char>(stale.size());
    fixture.package.curves[3] = stale.data();
  }

  CurveBlockStorage curves;
  std::array<u_char, 5> stale{200, 200, 200, 200, 200};
};

std::array<u_char, 9> NineSampleTrailTransparency(Plg074Fixture& fixture,
                                                  u_char curve) {
  fixture.Definition().transparency_curve = curve;
  Effects_RendererSlot slot{};
  REQUIRE(VS::Effects::Templates::UpdateLivePlg049TrailNine(&slot, 1, 0) == 1);
  auto* state =
      static_cast<VS::Effects::LiveTrailNineState*>(slot.rendererState);
  REQUIRE(state != nullptr);
  std::array<u_char, 9> samples = state->transparency;
  REQUIRE(VS::Effects::Templates::UpdateLivePlg049TrailNine(&slot, 4, 0) == 0);
  VS::Core::Memory::FreeLiveFromHighEnd(state);
  return samples;
}

std::array<int, 5> RotatingSurfaceRows(Plg074Fixture& fixture, u_char curve) {
  fixture.Definition().transparency_curve = curve;
  Effects_RendererSlot slot{};
  REQUIRE(VS::Effects::Templates::UpdateLivePlg073RotatingSurface(&slot, 1,
                                                                  0) == 1);
  auto* state =
      static_cast<VS::Effects::LiveRotatingSurfaceState*>(slot.rendererState);
  REQUIRE(state != nullptr);
  std::array<int, 5> rows{};
  std::ranges::copy(state->factors, rows.begin());
  VS::Core::Memory::FreeLiveFromHighEnd(state);
  return rows;
}

}  // namespace

TEST_CASE("Nine-sample trail transparency reads only the package's curves",
          "[effects][trail-emitter][trail-surface-refusals]") {
  Plg074Fixture fixture;
  DispatchHeap heap;
  LogCapture log;
  const StaleCurveBlock curves(fixture);
  fixture.Definition().interval_range[0] = 1;
  CHECK(NineSampleTrailTransparency(fixture, 1) ==
        std::array<u_char, 9>{10, 10, 10, 20, 20, 20, 30, 30, 40});
  CHECK_FALSE(log.Contains("transparency curve"));
  for (const u_char curve : {u_char{3}, u_char{63}}) {
    CAPTURE(curve);
    CHECK(NineSampleTrailTransparency(fixture, curve) ==
          std::array<u_char, 9>{128, 128, 128, 128, 128, 128, 128, 128, 128});
  }
  CHECK(
      log.Contains("PLG049 trail emitter: transparency curve 3 is outside "
                   "the package's 3 bound curves"));
  CHECK(log.Contains("transparency curve 63 is outside"));
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

namespace {
// A trail after its emitter's first tick: origin at the effect origin, target
// 1024 along z, controls at their 64/256 shares between them, and 12 steps.
void CheckFirstTickNineSampleTrail(const VS::Effects::LiveTrail9& trail) {
  CHECK(Coordinates(trail.spline_points[0]) == std::array{0, 0, 0});
  CHECK(Coordinates(trail.spline_points[1]) == std::array{0, 0, 256});
  CHECK(Coordinates(trail.spline_points[2]) == std::array{0, 0, 768});
  CHECK(Coordinates(trail.spline_points[3]) == std::array{0, 0, 1024});
  CHECK(trail.step_count == 12);
}
}  // namespace

// trails9.h:745-751: the first tick places each trail's origin, then its
// target, then the controls between them, before the trail starts.
TEST_CASE("Nine-sample emitters aim and spread their trails on the first tick",
          "[effects][trail-emitter][trail-nine-stages]") {
  Plg074Fixture fixture;
  DispatchHeap heap;
  const std::int32_t saved_corner = VS::Effects::Plg049::trailEmissionCounter;
  auto& definition = fixture.Definition();
  definition.flags = 0;
  definition.interval_range[0] = 3;
  std::ranges::copy(std::array<std::int16_t, 6>{0, 0, 0, 0, 1024, 1024},
                    definition.vector_ranges[6].begin());
  std::ranges::copy(std::array<std::int16_t, 4>{64, 64, -64, -64},
                    definition.pair_ranges[2].begin());
  std::ranges::fill(definition.pair_ranges[4], 12);
  Effects_RendererSlot slot{};
  REQUIRE(VS::Effects::Templates::UpdateLivePlg049TrailNine(&slot, 1, 0) == 1);
  auto* state =
      static_cast<VS::Effects::LiveTrailNineState*>(slot.rendererState);
  REQUIRE(state != nullptr);
  REQUIRE(state->trail_count == 3);
  CHECK(VS::Effects::Templates::UpdateLivePlg049TrailNine(&slot, 2, 0) == 1);
  CHECK(state->initialized == 1);
  for (const VS::Effects::LiveTrail9& trail :
       std::span(state->trails, state->trail_count)) {
    CheckFirstTickNineSampleTrail(trail);
  }
  CHECK(VS::Effects::Templates::UpdateLivePlg049TrailNine(&slot, 4, 0) == 0);
  VS::Core::Memory::FreeLiveFromHighEnd(state);
  CHECK(VS::Core::Memory::GameHeap().Empty());
  VS::Effects::Plg049::trailEmissionCounter = saved_corner;
  fixture.ResetPackets();
}

TEST_CASE("Trail emitters stop on a missing instance or transform",
          "[effects][plg074][trail-emitter][trail-surface-refusals]") {
  const bool missing_instance = GENERATE(false, true);
  Plg074Fixture fixture;
  DispatchHeap heap;
  LogCapture log;
  fixture.Definition().interval_range[0] = 2;
  Effects_RendererSlot slot{};
  REQUIRE(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 1, 0) == 1);
  auto* state = static_cast<VS::Effects::LiveTrailState*>(slot.rendererState);
  REQUIRE(state != nullptr);
  if (missing_instance) {
    Effects_currentInstance = nullptr;
  } else {
    fixture.Definition().transform_index = 16;
  }
  const void* before = VS::Render::FrameScratchpad().primitive_cursor;
  CHECK(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 2, 0) == 0);
  CHECK(state->trails == nullptr);
  CHECK(state->initialized == 0);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == before);
  CHECK(log.Contains(missing_instance ? "no current effect instance"
                                      : "names transform 16 of 16"));
  VS::Core::Memory::FreeLiveFromHighEnd(state);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("Trail emitters refuse an exhausted heap without a partial state",
          "[effects][plg074][trail-emitter][trail-surface-refusals]") {
  constexpr std::size_t kArenaBytes = 512;
  static_assert(16 * sizeof(VS::Effects::LiveTrail) > kArenaBytes);
  const bool state_fits = GENERATE(false, true);
  Plg074Fixture fixture;
  LogCapture log;
  fixture.Definition().interval_range[0] = 16;
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, kArenaBytes>
      arena{};
  VS::Core::Memory::ScopedLiveGameHeap exhausted(
      std::span(arena).first(state_fits ? kArenaBytes : 32));
  Effects_RendererSlot slot{};
  CHECK(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 1, 0) == 1);
  CHECK(slot.rendererState == nullptr);
  CHECK(log.Contains(state_fits ? "heap exhausted for 16 trails"
                                : "heap exhausted for state"));
  CHECK(VS::Core::Memory::GameHeap().Empty());
  CHECK(VS::Effects::UpdateLivePlg074TrailEmitter(&slot, 2, 0) == 0);
}

TEST_CASE("Trail emitters ignore a call without a slot",
          "[effects][trail-emitter][trail-surface-refusals]") {
  LogCapture log;
  CHECK(VS::Effects::UpdateLivePlg074TrailEmitter(nullptr, 2, 0) == 0);
  CHECK(VS::Effects::Templates::UpdateLivePlg049TrailNine(nullptr, 1, 0) == 0);
  CHECK(log.Contains("PLG074 trail emitter: renderer called without a slot"));
  CHECK(log.Contains("PLG049 trail emitter: renderer called without a slot"));
}

TEST_CASE("Live surfaces draw full-intensity rows for curves the package lacks",
          "[effects][plg074][trail-surface-refusals]") {
  Plg074Fixture fixture;
  DispatchHeap heap;
  LogCapture log;
  const StaleCurveBlock curves(fixture);
  CHECK(RotatingSurfaceRows(fixture, 1) ==
        std::array<int, 5>{20, 20, 40, 60, 80});
  CHECK_FALSE(log.Contains("transparency curve"));
  for (const u_char curve : {u_char{3}, u_char{63}}) {
    CAPTURE(curve);
    CHECK(RotatingSurfaceRows(fixture, curve) ==
          std::array<int, 5>{255, 255, 255, 255, 255});
  }
  CHECK(
      log.Contains("transparency curve 3 is outside the package's 3 bound "
                   "curves"));
  CHECK(log.Contains("transparency curve 63 is outside"));
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("Live surfaces stop on a missing instance or transform",
          "[effects][plg074][trail-surface-refusals]") {
  const Effects_EffectRenderer entry =
      &VS::Effects::Templates::UpdateLivePlg073RotatingSurface;
  const bool missing_instance = GENERATE(false, true);
  Plg074Fixture fixture;
  DispatchHeap heap;
  LogCapture log;
  Effects_RendererSlot slot{};
  REQUIRE(entry(&slot, 1, 0) == 1);
  REQUIRE(slot.rendererState != nullptr);
  if (missing_instance) {
    Effects_currentInstance = nullptr;
  } else {
    fixture.Definition().transform_index = 16;
  }
  const void* before = VS::Render::FrameScratchpad().primitive_cursor;
  CHECK(entry(&slot, 2, 0) == 0);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == before);
  CHECK(log.Contains(missing_instance ? "no current effect instance"
                                      : "names transform 16 of 16"));
  VS::Core::Memory::FreeLiveFromHighEnd(slot.rendererState);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("Live surfaces refuse an exhausted heap",
          "[effects][plg074][trail-surface-refusals]") {
  Plg074Fixture fixture;
  LogCapture log;
  std::array<std::byte, 32> arena{};
  VS::Core::Memory::ScopedLiveGameHeap exhausted(arena);
  Effects_RendererSlot slot{};
  CHECK(VS::Effects::Templates::UpdateLivePlg073RotatingSurface(&slot, 1, 0) ==
        1);
  CHECK(slot.rendererState == nullptr);
  CHECK(log.Contains("heap exhausted for state"));
  CHECK(VS::Effects::Templates::UpdateLivePlg073RotatingSurface(&slot, 2, 0) ==
        0);
}

TEST_CASE("EFFPURGE's update sweep reaches no renderer state without a slot",
          "[effects][trail-surface-refusals]") {
  Plg074Fixture fixture;
  DispatchHeap heap;
  LogCapture log;
  CHECK(VS::Effects::LiveInvokeAllPluginUpdates(nullptr, 2, 0) == 0);
  CHECK(log.Contains("PLG074 trail emitter: renderer called without a slot"));
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("PLG074 F01995 draws five accelerating surface rows",
          "[effects][plg074][f01995]") {
  const auto entry = Plg074Entry(
      0x800FD534, &VS::Effects::Templates::UpdateLivePlg073RotatingSurface);
  REQUIRE(VS::Effects::effectEntryPoints[90] == entry);
  Plg074Fixture fixture;
  fixture.ConfigureRing();
  std::ranges::fill(fixture.Definition().vector_ranges[1], 0);
  DispatchHeap heap;
  Effects_RendererSlot slot{};
  REQUIRE(entry(&slot, 1, 2) == 1);
  REQUIRE(entry(&slot, 2, 0) == 1);
  const auto* packets =
      reinterpret_cast<const VS::Render::RotatingSurfacePacket*>(
          fixture.storage.data() + 1);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == packets + 64);
  CheckPlg074SurfacePackets(packets);
  fixture.CheckGuards();
  VS::Core::Memory::FreeLiveFromHighEnd(slot.rendererState);
  fixture.ResetPackets();
  CheckPlg074RingLifecycle<VS::Effects::LiveRotatingSurfaceState>(entry,
                                                                  fixture);
}

TEST_CASE(
    "PLG074 production renderer lists expire and release every allocation",
    "[effects][plg074][plg074-lifecycle]") {
  Plg074Fixture fixture;
  fixture.ConfigureRing();
  DispatchHeap heap;
  for (int cycle = 0; cycle < 3; ++cycle) {
    for (int index = 87; index <= 90; ++index) {
      auto* slot = VS::Effects::LiveAttachEffectRenderer(
          &fixture.instance, VS::Effects::effectEntryPoints[index], 2);
      REQUIRE(slot != nullptr);
      REQUIRE(slot->rendererState != nullptr);
    }

    CHECK(VS::Effects::LiveTickEffectRenderers(&fixture.instance) == 1);
    REQUIRE(fixture.instance.context.renderer != nullptr);
    fixture.ResetPackets();
    CHECK(VS::Effects::LiveTickEffectRenderers(&fixture.instance) == 1);
    CHECK(fixture.instance.context.renderer == nullptr);
    CHECK(VS::Core::Memory::GameHeap().Empty());
    fixture.CheckGuards();
    fixture.ResetPackets();
  }
}

TEST_CASE("Gravity closed rings emit textured geometry and release on reentry",
          "[effects][gravity-material][closed-ring]") {
  Plg074Fixture fixture;
  fixture.ConfigureRing();
  fixture.package.block1TableCount = 2;
  fixture.package.textureInfoTable[1] = &fixture.info;
  fixture.Definition().ring_segments = 1;
  fixture.Definition().ring_positions = 0;
  const auto saved_color = VS::Effects::Plg053::closedRingColorFactors[0];
  VS::Effects::Plg053::closedRingColorFactors[0] = {32, 64, 96, 0};
  DispatchHeap heap;
  for (int cycle = 0; cycle < 3; ++cycle) {
    auto* slot = VS::Effects::LiveAttachEffectRenderer(
        &fixture.instance, &VS::Effects::Templates::UpdateLivePlg053ClosedRing,
        2);
    REQUIRE(slot != nullptr);
    CHECK(VS::Effects::LiveTickEffectRenderers(&fixture.instance) == 1);
    const auto* packets = reinterpret_cast<const VS::Render::ClosedRingPacket*>(
        fixture.storage.data() + 1);
    CheckGravityRingMaterial(packets);
    fixture.ResetPackets();
    CHECK(VS::Effects::LiveTickEffectRenderers(&fixture.instance) == 1);
    CHECK(fixture.instance.context.renderer == nullptr);
    CHECK(VS::Core::Memory::GameHeap().Empty());
    fixture.ResetPackets();
  }
  VS::Effects::Plg053::closedRingColorFactors[0] = saved_color;
}
