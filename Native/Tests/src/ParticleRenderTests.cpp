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

#include "Content/ModuleData.h"
#include "Core/Log.hpp"
#include "Core/Random.hpp"
#include "Effects/ParticleRender.h"
#include "Legacy/Psx.h"
#include "Render/PageSpace.hpp"
#include "nlohmann/json.hpp"

extern "C" {
#include "Core/BiosRand.h"
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"
#include "Effects/EffectResources.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg073.h"
#include "Effects/PurgeTables.h"
#include "Effects/Runtime.h"
#include "Effects/ScriptVm.h"
#include "Effects/Templates/ParticleDispatch.h"
#include "Effects/Templates/ParticleRender.h"
#include "Effects/Templates/RadialSurge.h"
#include "Effects/Templates/Ring.h"
#include "Effects/Templates/RotatingSurface.h"
#include "Effects/Templates/TiledRing.h"
#include "Effects/Templates/TrailControl.h"
#include "Effects/Templates/TrailDispatch.h"
#include "Effects/Templates/TrailInit.h"
#include "Effects/Templates/TrailNineInit.h"
#include "Effects/Templates/TrailNineRender.h"
#include "Effects/Templates/TrailNineUpdate.h"
#include "Effects/Templates/TrailOrigin.h"
#include "Effects/Templates/TrailRender.h"
#include "Effects/Templates/TrailTarget.h"
#include "Effects/Templates/TrailUpdate.h"
#include "Effects/Timeline.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

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
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  int saved_framebuffer = Render_frameBuf;
  int saved_near = Render_nearClip;
  int saved_projection = Render_projectionDistance;
  Effects_EffectPackage* saved_package = Effects_package;
  Effects_EffectInstance* saved_instance = Effects_currentInstance;
  Effects_Particle* saved_pool = Effects_particlePool;
  int saved_capacity = Effects_particleCapacity;
  int saved_used = Effects_usedParticleCount;
  Effects_Particle* saved_free = Effects_freeParticles;
  Effects_EffectPackage package{};
  Effects_EffectInstance instance{};
  Effects_Particle particle{};
  Effects_PackageTexture texture{};
  Effects_PackageTextureInfo info{1, 0};
  std::array<u_long, 2> script{8u << 16, 0x30000000u};
  std::array<u_long, 2> curve_block{1, 2};
  std::array<u_char, 2> curve{32, 64};
  std::vector<u_long> storage = std::vector<u_long>(0x8002, 0xDEADBEEF);
  std::array<u_long, 2064> ordering{};

  ParticleFixture() {
    Effects_package = &package;
    Effects_currentInstance = &instance;
    Effects_particlePool = &particle;
    Effects_particleCapacity = 1;
    package.textures = &texture;
    package.textureInfoTable[0] = &info;
    package.curveBlock =
        reinterpret_cast<Effects_CurveBlock*>(curve_block.data());
    package.curves[0] = curve.data();
    texture.u = 16;
    texture.v = 32;
    texture.w = texture.h = 8;
    texture.clut = 0x1234;
    texture.tpage = 0x567;
    texture.reserved8 = texture.reservedA = -4;
    texture.reservedC = 4;
    texture.reservedE = -4;
    texture.reserved10 = -4;
    texture.reserved12 = texture.reserved14 = texture.reserved16 = 4;
    particle.textureAnimation.program =
        reinterpret_cast<u_char*>(script.data());
    particle.motion.position.vz = 1024 << 12;
    particle.transparency = 16;
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = storage.data() + 1;
    Render_frameScratch = {};
    for (int axis = 0; axis < 3; ++axis) {
      Render_frameScratch.viewMatrix.m[axis][axis] = 4096;
    }
    instance.transforms[0].view = Render_frameScratch.viewMatrix;
    instance.transforms[0].world = Render_frameScratch.viewMatrix;
    Render_frameScratch.primitiveCursor = storage.data() + 1;
    Render_frameScratch.orderingTable = ordering.data() + 16;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Render_nearClip = 0;
    Render_projectionDistance = 256;
    Core_Gte_SetGeomOffset(160, 120);
    Core_Gte_SetGeomScreen(256);
  }

  ~ParticleFixture() {
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_frameBuf = saved_framebuffer;
    Render_nearClip = saved_near;
    Render_projectionDistance = saved_projection;
    Effects_package = saved_package;
    Effects_currentInstance = saved_instance;
    Effects_particlePool = saved_pool;
    Effects_particleCapacity = saved_capacity;
    Effects_usedParticleCount = saved_used;
    Effects_freeParticles = saved_free;
    Core_Gte_Reset();
  }

  void CheckEmissionPool(bool occupied) const {
    CHECK(Effects_usedParticleCount == (occupied ? 1 : 0));
    CHECK(instance.particleCount == (occupied ? 1 : 0));
    CHECK(instance.particles == (occupied ? &particle : nullptr));
    CHECK(Effects_freeParticles == (occupied ? nullptr : &particle));
    CHECK(particle.next == nullptr);
  }

  void CheckQuad() const {
    CHECK(Render_frameScratch.primitiveCursor == storage.data() + 17);
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
  fixture.particle.emitFlags = 0x20000;
  fixture.particle.motion.position.vx = 64 << 12;
  fixture.particle.motion.position.vy = 32 << 12;
  fixture.instance.transforms[0].world.t[0] = 100;
  fixture.instance.transforms[0].world.t[1] = 200;
  fixture.instance.transforms[0].view.t[0] = 16;
  fixture.texture.clut = std::bit_cast<std::int16_t>(std::uint16_t{0x9234});
  const auto motion = fixture.particle.motion;
  Effects_DrawAnimatedSpriteParticles(&fixture.particle);
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
  fixture.particle.emitFlags = 0x80000;
  fixture.particle.motion.position.vy = 4 << 12;
  Effects_DrawAnimatedSpriteParticles(&fixture.particle);
  REQUIRE(scope.records.size() == 2);
  const auto record = nlohmann::json::parse(scope.records.back().second);
  CHECK(record.at("outcome") == "below-ground");
  CHECK(record.at("screen").is_null());
  CHECK(record.at("command_addresses").empty());
  CHECK(Render_frameScratch.primitiveCursor == fixture.storage.data() + 1);
}

TEST_CASE("Particle texture reset clears native pointer tails without overflow",
          "[particle][texture-state]") {
  struct {
    std::uint64_t before = 0x123456789ABCDEF0;
    Effects_ParticleTextureAnimation animation;
    std::uint64_t after = 0x0FEDCBA987654321;
  } fixture;
  std::memset(&fixture.animation, 0xCD, sizeof fixture.animation);
  Effects_ClearParticleTextureState(reinterpret_cast<int*>(&fixture.animation));
  CHECK(fixture.before == 0x123456789ABCDEF0);
  CHECK(fixture.after == 0x0FEDCBA987654321);
  CHECK(fixture.animation.program == nullptr);
  CHECK(fixture.animation.textureInfo == nullptr);
  CHECK(fixture.animation.cursor == 0);
  CHECK(fixture.animation.frameTimer == 0);
  CHECK(fixture.animation.frameOffsetX == 0);
  CHECK(fixture.animation.frameOffsetY == 0);
}

TEST_CASE("Particle wrappers forward the real native list and texture program",
          "[effects][particle-render]") {
  ParticleFixture fixture;
  Effects_DrawEffectParticleList(&fixture.particle);
  CHECK(fixture.particle.textureAnimation.cursor == 4);
  CHECK(fixture.particle.textureAnimation.frameTimer == 8);
  CHECK(fixture.particle.textureAnimation.textureInfo == &fixture.info);
  fixture.CheckQuad();
}

TEST_CASE("Particles retain each original depth override and native link",
          "[effects][particle-render]") {
  const int mode = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8);
  ParticleFixture fixture;
  fixture.particle.opaque61Bit0 = static_cast<unsigned>(mode);
  Effects_DrawAnimatedSpriteParticles(&fixture.particle);
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
  fixture.particle.colorCurves[0] = 1;
  fixture.particle.colorCurves[1] = 1;
  fixture.particle.colorCurves[2] = 1;
  fixture.particle.age = 255;
  Effects_DrawAnimatedSpriteParticles(&fixture.particle);
  CHECK(fixture.storage[5] == 0x2C3F3F3F);
}

TEST_CASE("Particle culling preserves texture advancement without emitting",
          "[effects][particle-render]") {
  const bool attached = GENERATE(false, true);
  ParticleFixture fixture;
  fixture.particle.emitFlags = 0x80000 | (attached ? 0x20000 : 0);
  fixture.particle.motion.position.vy = 4096;
  Effects_DrawAnimatedSpriteParticles(&fixture.particle);
  CHECK(fixture.particle.textureAnimation.cursor == 4);
  CHECK(Render_frameScratch.primitiveCursor == fixture.storage.data() + 1);
}

TEST_CASE("Attached particles project through their own native effect matrix",
          "[effects][particle-render]") {
  ParticleFixture fixture;
  fixture.particle.emitFlags = 0x20000;
  fixture.instance.transforms[0].view.t[0] = 128;
  Effects_DrawAnimatedSpriteParticles(&fixture.particle);
  const auto* quad = fixture.storage.data() + 4;
  CHECK((quad[2] & 0xFFFF) == 188);
  CHECK((quad[8] & 0xFFFF) == 196);
}

TEST_CASE(
    "Particle rotation preserves Q12 geometry and semitransparent commands",
    "[effects][particle-render]") {
  ParticleFixture fixture;
  fixture.particle.textureAnimation.reserved14 = 1024;
  fixture.texture.clut = static_cast<std::int16_t>(-28108);
  Effects_DrawAnimatedSpriteParticles(&fixture.particle);
  const auto* quad = fixture.storage.data() + 4;
  CHECK(quad[1] == 0x2E808080);
  CHECK(quad[2] == static_cast<u_long>(164 | (116 << 16)));
  CHECK(quad[8] == static_cast<u_long>(156 | (124 << 16)));
}

TEST_CASE("Particle texture timers advance once per render without resetting",
          "[effects][particle-render]") {
  ParticleFixture fixture;
  Effects_DrawAnimatedSpriteParticles(&fixture.particle);
  Effects_DrawAnimatedSpriteParticles(&fixture.particle);
  CHECK(fixture.particle.textureAnimation.frameTimer == 6);
  CHECK(fixture.particle.textureAnimation.cursor == 4);
  CHECK(Render_frameScratch.primitiveCursor == fixture.storage.data() + 33);
}

TEST_CASE("Particle rendering fits exactly at the native frame buffer boundary",
          "[effects][particle-render]") {
  ParticleFixture fixture;
  Render_frameScratch.primitiveCursor =
      fixture.storage.data() + 1 + 0x8000 - 16;
  Effects_DrawAnimatedSpriteParticles(&fixture.particle);
  CHECK(Render_frameScratch.primitiveCursor ==
        fixture.storage.data() + 1 + 0x8000);
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

namespace {
struct EmissionFixture : ParticleFixture {
  struct Definitions {
    int reserved;
    Effects_ParticleDefinition entries[1];
  } definitions{};
  std::array<u_short, 8> programs{1, 0, 8, 0, 0, 8, 0, 0x3000};

  EmissionFixture() {
    package.block5Data = reinterpret_cast<Effects_PackageBlock5*>(&definitions);
    package.block2Data = programs.data();
    auto& definition = definitions.entries[0];
    definition.intervalRange[0] = definition.intervalRange[1] = 1;
    definition.countRange[0] = definition.countRange[1] = 1;
    Effects_usedParticleCount = 0;
    Effects_freeParticles = &particle;
  }

  void CheckTexture(const Effects_ParticleQuadSet& state) const {
    CHECK(state.textureAnimation.program ==
          reinterpret_cast<const u_char*>(programs.data()) + 8);
    CHECK(state.textureAnimation.textureInfo == nullptr);
    CHECK(state.textureAnimation.frameTimer == 0);
    CHECK(state.textureAnimation.cursor == 0);
    CHECK(state.textureAnimation.textureIndex == 0);
  }
};

enum class ChildTrigger { kTick, kLifetimeEnd, kTextureEnd };

struct ChildEmissionFixture : EmissionFixture {
  Effects_ParticleGravity saved_gravity = Effects_particleGravity;
  int saved_drag = Effects_particleDrag;
  int saved_sample = Effects_curveSamples[0];
  std::array<Effects_Particle, 2> nodes{};
  std::array<u_long, 1> terminal{0x30000000u};

  explicit ChildEmissionFixture(ChildTrigger trigger) {
    package.block1TableCount = 1;
    package.block2Count = 1;
    auto& definition = definitions.entries[0];
    definition.flags = 4;
    std::ranges::fill(definition.pairRanges[0], 1);
    std::ranges::fill(definition.pairRanges[4], 10);
    Effects_curveSamples[0] = 0;
    Effects_particleGravity = {};
    Effects_particleDrag = 0;
    nodes[0] = particle;
    auto& parent = nodes[0];
    parent.motion.mass = 1;
    parent.motion.position = {100 << 12, -20 * 4096, 1024 << 12};
    parent.transparency = 12;
    parent.opaque61Bit0 = 3;
    parent.lifetime = 8;
    if (trigger == ChildTrigger::kTick) {
      parent.tickEvent = 1;
    } else {
      parent.endEvent = 1;
      parent.lifetime = trigger == ChildTrigger::kLifetimeEnd ? 1 : -1;
    }
    if (trigger == ChildTrigger::kTextureEnd) {
      parent.textureAnimation.program =
          reinterpret_cast<u_char*>(terminal.data());
      parent.textureAnimation.textureInfo = &info;
    }
    Effects_particlePool = nodes.data();
    Effects_particleCapacity = static_cast<int>(nodes.size());
    Effects_usedParticleCount = 1;
    Effects_freeParticles = &nodes[1];
    instance.particles = &parent;
    instance.particleCount = 1;
  }

  ~ChildEmissionFixture() {
    Effects_particleGravity = saved_gravity;
    Effects_particleDrag = saved_drag;
    Effects_curveSamples[0] = saved_sample;
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
    CHECK(child.opaque61Bit0 == 3);
    CHECK(child.lifetime == 10);
  }

  void CheckChildPopulation(ChildTrigger trigger) const {
    const bool retained = trigger == ChildTrigger::kTick;
    CHECK(instance.particleCount == (retained ? 2 : 1));
    CHECK(Effects_usedParticleCount == (retained ? 2 : 1));
    CHECK(Effects_freeParticles == (retained ? nullptr : &nodes[0]));
  }
};

struct DispatchHeap {
  Core_HeapBlockHeader saved = Core_legacyHeapList;
  Core_HeapBlockHeader saved_secondary = Core_legacySecondaryHeapList;
  std::array<Core_HeapBlockHeader, 256> storage{};
  unsigned free_units = 0;

  DispatchHeap() {
    Core_Memory_InitializeHeap(storage.data(), sizeof storage);
    free_units = Core_legacyHeapList.next->blockSize;
  }

  ~DispatchHeap() {
    Core_legacyHeapList = saved;
    Core_legacySecondaryHeapList = saved_secondary;
  }

  static std::size_t Capacity(void* allocation) {
    const auto* header = static_cast<Core_HeapBlockHeader*>(allocation) - 1;
    return (header->blockSize - 1) * sizeof(Core_HeapBlockHeader);
  }

  static void CheckPrefix(const Effects_ParticleQuadSet& state) {
    CHECK(state.opaque0 == 30);
    CHECK(state.definitionIndex == 0);
    CHECK(state.age == 0);
    REQUIRE(state.controlPoints != nullptr);
  }
};
}  // namespace

TEST_CASE(
    "Child particle events inherit native parent coordinates and appearance",
    "[effects][particle-emission][particle-parent]") {
  const auto trigger = GENERATE(ChildTrigger::kTick, ChildTrigger::kLifetimeEnd,
                                ChildTrigger::kTextureEnd);
  ChildEmissionFixture fixture(trigger);
  REQUIRE(Effects_TickEffectParticles(&fixture.instance) == 1);
  REQUIRE(fixture.instance.particles == &fixture.nodes[1]);
  fixture.CheckChildCoordinates();
  fixture.CheckChildAppearance();
  fixture.CheckChildPopulation(trigger);
}

TEST_CASE("Textured ribbon dispatchers allocate and initialize native textures",
          "[effects][particle-dispatch]") {
  EmissionFixture fixture;
  DispatchHeap heap;
  Effects_RendererSlot renderer{};
  fixture.package.block1TableCount = 1;
  fixture.package.block2Count = 1;
  const auto dispatch = GENERATE(Effects_Plg045_ParticleDispatch,
                                 Effects_Plg265_ParticleDispatch2);
  REQUIRE(dispatch(&renderer, 1, 30) == 1);
  REQUIRE(renderer.rendererState != nullptr);
  const auto capacity = DispatchHeap::Capacity(renderer.rendererState);
  CHECK(capacity >= sizeof(Effects_ParticleQuadSet));
  auto* state = static_cast<Effects_ParticleQuadSet*>(renderer.rendererState);
  DispatchHeap::CheckPrefix(*state);
  if (capacity >= sizeof(Effects_ParticleQuadSet)) {
    fixture.CheckTexture(*state);
  }
  CHECK(dispatch(&renderer, 4, 0) == 0);
  Core_Memory_FreeFromHighEnd(renderer.rendererState);
  CHECK(Core_legacyHeapList.next->blockSize == heap.free_units);
}

TEST_CASE("Untextured line dispatchers retain only the complete native prefix",
          "[effects][particle-dispatch]") {
  EmissionFixture fixture;
  DispatchHeap heap;
  Effects_RendererSlot renderer{};
  REQUIRE(Effects_Plg265_ParticleDispatch(&renderer, 1, 30) == 1);
  const auto prefix = offsetof(Effects_ParticleQuadSet, textureAnimation);
  const auto rounded = (prefix + sizeof(Core_HeapBlockHeader) - 1) /
                       sizeof(Core_HeapBlockHeader) *
                       sizeof(Core_HeapBlockHeader);
  CHECK(DispatchHeap::Capacity(renderer.rendererState) == rounded);
  const auto* state =
      static_cast<Effects_ParticleQuadSet*>(renderer.rendererState);
  DispatchHeap::CheckPrefix(*state);
  CHECK(state->opaque8 == 0);
  CHECK(Effects_Plg265_ParticleDispatch(&renderer, 4, 0) == 0);
  Core_Memory_FreeFromHighEnd(renderer.rendererState);
  CHECK(Core_legacyHeapList.next->blockSize == heap.free_units);
}

TEST_CASE("Particle emission respects empty bursts and exhausted capacity",
          "[effects][particle-emission]") {
  EmissionFixture fixture;
  STATIC_REQUIRE(offsetof(EmissionFixture::Definitions, entries) ==
                 sizeof(Effects_PackageBlock5));
  auto& definition = fixture.definitions.entries[0];
  const int requested = GENERATE(0, 1, 5);
  definition.countRange[0] = definition.countRange[1] =
      static_cast<std::int16_t>(requested);
  const bool full = GENERATE(false, true);
  Effects_usedParticleCount = full ? 1 : 0;
  Effects_freeParticles = full ? nullptr : &fixture.particle;
  fixture.instance.particles = full ? &fixture.particle : nullptr;
  fixture.instance.particleCount = full ? 1 : 0;
  fixture.particle.next = nullptr;
  Core_Random_Seed(1234);
  const bool emits = requested > 0 && !full;

  Effects_EmitParticleEvent(&fixture.instance, 0, 0, 0, nullptr);

  fixture.CheckEmissionPool(full || emits);
  if (!emits) {
    CHECK(fixture.particle.age == 0);
    CHECK(Core_Random_Rand() == VS::Core::Rand15(1234).Next());
  } else {
    CHECK(static_cast<unsigned char>(fixture.particle.age) == 255);
    CHECK(fixture.particle.textureAnimation.program ==
          reinterpret_cast<u_char*>(fixture.programs.data()) + 8);
  }
  Core_Random_Seed(1);
}

TEST_CASE("Effect renderer slots retain eight independent native callbacks",
          "[effects][renderer-registry]") {
  STATIC_REQUIRE(std::size(Effects_effectRenderers) == 8);
  const auto saved = std::to_array(Effects_effectRenderers);
  std::array<std::byte, sizeof(Effects_actionPackage)> package_before;
  std::memcpy(package_before.data(), &Effects_actionPackage,
              package_before.size());
  for (int index = 0; index < 8; ++index) {
    Effects_SetEffectRenderer(Effects_effectEntryPoints[index], index);
  }
  for (int index = 0; index < 8; ++index) {
    CHECK(Effects_effectRenderers[index] == Effects_effectEntryPoints[index]);
  }
  std::array<std::byte, sizeof(Effects_actionPackage)> package_after;
  std::memcpy(package_after.data(), &Effects_actionPackage,
              package_after.size());
  CHECK(package_before == package_after);
  std::copy(saved.begin(), saved.end(), Effects_effectRenderers);

  for (std::size_t effect = 0;
       effect + 1 < std::size(Effects_effectEntryPointIndices); ++effect) {
    const int count = Effects_effectEntryPointIndices[effect + 1] -
                      Effects_effectEntryPointIndices[effect];
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
  Effects_SelectEffectResourceContext(1);
  CHECK(Effects_package == &Effects_actionPackage);
  CHECK(Effects_package->request == &Effects_actionRequest);

  Effects_actionPackage.request = &Effects_secondaryRequest;
  CHECK(Effects_actionRequestPointer == &Effects_secondaryRequest);
  Effects_SelectEffectResourceContext(0);
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
  Effects_ChildTrackKey key{};
  Effects_ChildTrackView track{};
  Effects_EffectTimeline timeline{};
  fixture.package.request = &request;
  fixture.instance.context.timeline = &timeline;
  fixture.instance.childCount = 1;
  track.event = &key;
  auto* saved_instances = Effects_primaryInstances;
  Effects_primaryInstances = nullptr;

  CHECK(Effects_TickChildEffectTrack(&fixture.instance, &track, 0) == 1);
  CHECK(fixture.instance.childCount == 1);
  CHECK(track.index == 0);
  CHECK(Effects_primaryInstances == nullptr);

  while (Effects_primaryInstances != nullptr) {
    Effects_DestroyPrimaryEffectInstance(Effects_primaryInstances);
  }
  Effects_primaryInstances = saved_instances;
}

namespace {
struct RibbonFixture : ParticleFixture {
  static constexpr int kSegments = 8;
  Effects_ParticleDefinition definition{};
  Effects_ParticleEmitterScratch scratch{};
  Effects_ParticleQuadSet ribbons{};
  std::array<SVECTOR, 12> points{};

  RibbonFixture() {
    ribbons.controlPoints = points.data();
    ribbons.textureAnimation = particle.textureAnimation;
    // Keep depth independent of extrapolated spline samples.
    Render_frameScratch.viewMatrix.t[2] = 1024;
    for (std::size_t index = 0; index < points.size(); ++index) {
      points[index].vx = static_cast<std::int16_t>((index % 4) * 32);
    }
  }

  const Render_ParticleQuadPacket* CheckPackets() const {
    const auto* packets =
        reinterpret_cast<const Render_ParticleQuadPacket*>(storage.data() + 1);
    const auto packet_count =
        static_cast<std::ptrdiff_t>(ribbons.count) * kSegments;
    REQUIRE(Render_frameScratch.primitiveCursor == packets + packet_count);
    const void* previous_packet = ordering.data() + 16 + 255;
    for (const auto* packet = packets; packet != packets + packet_count;
         ++packet) {
      CHECK(nextPrim(packet) == previous_packet);
      previous_packet = packet;
    }
    CHECK(nextPrim(ordering.data() + 16 + 256) == previous_packet);
    CHECK(storage.front() == 0xDEADBEEF);
    CHECK(storage.back() == 0xDEADBEEF);
    return packets;
  }

  void CheckSamples(int initial_width, int half_width) const {
    constexpr int kCurveEnd = 0x3000;
    const auto* packets = CheckPackets();
    VS::Core::Rand15 expected(1234);
    for (int ribbon = 0; ribbon < ribbons.count; ++ribbon) {
      const int width =
          ribbon == 0 ? (initial_width == 0 ? 1 : initial_width) : half_width;
      int previous = 0;
      for (int segment = 0; segment < kSegments; ++segment) {
        int sample = kCurveEnd;
        if (segment + 1 < kSegments) {
          const int random = expected.Next();
          sample = (segment + 1) * 0x600 +
                   (width == 0 ? random : random % width) - width / 2;
        }
        const auto& quad = packets[ribbon * kSegments + segment].opaque4;
        CHECK(quad.u0 == texture.u + texture.w * previous / kCurveEnd);
        CHECK(quad.u1 == texture.u + texture.w * sample / kCurveEnd);
        previous = sample;
      }
    }
    CHECK(Core_Random_Rand() == expected.Next());
    CHECK(ribbons.textureAnimation.frameTimer == 8);
  }
};
}  // namespace

TEST_CASE(
    "Shared particle ribbons preserve original zero-width remainder and RNG",
    "[effects][particle-ribbon]") {
  const auto renderer =
      GENERATE(&Effects_Plg045_ParticleRender, &Effects_Plg057_ParticleRender,
               &Effects_Plg082_ParticleRender, &Effects_Plg114_ParticleRender,
               &Effects_Plg152_ParticleRender, &Effects_Plg153_ParticleRender,
               &Effects_Plg156_ParticleRender, &Effects_Plg263_ParticleRender,
               &Effects_Plg265_ParticleRender, &Effects_Plg266_ParticleRender,
               &Effects_Plg300_ParticleRender, &Effects_Plg301_ParticleRender);
  const auto initial_width = GENERATE(0, 16, -16);
  const auto half_width = GENERATE(0, 6);
  const auto count = GENERATE(2, 1, 0, 3);
  CAPTURE(initial_width, half_width, count);
  RibbonFixture fixture;
  fixture.definition.innerExtentRange[0] =
      fixture.definition.innerExtentRange[1] =
          static_cast<std::int16_t>(initial_width);
  fixture.definition.vectorRanges[4][0] =
      fixture.definition.vectorRanges[4][1] =
          static_cast<std::int16_t>(half_width * 2);
  fixture.ribbons.count = static_cast<std::int16_t>(count);
  Core_Random_Seed(1234);
  renderer(&fixture.definition, &fixture.scratch, &fixture.instance,
           &fixture.ribbons);
  fixture.CheckSamples(initial_width, half_width);
  Core_Random_Seed(1);
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
  definition.extentRange[0] = definition.extentRange[1] = 32;
  definition.extentRange[2] = definition.extentRange[3] =
      static_cast<std::int16_t>(height);
  definition.extentRange[4] = definition.extentRange[5] = 48;
  definition.innerExtentRange[0] = definition.innerExtentRange[1] = 4096;
  VS::Core::Rand15 expected(seed);
  const auto angle = expected.Next();
  int vertical = 0;
  if (height != 0) {
    const int low = expected.Next();
    const int high = expected.Next();
    vertical = static_cast<int>(
        std::sqrt((low + (high << 16)) % (height * height + 1)));
  }
  const int x = height == 0
                    ? -1
                    : (((32 * Core_Gte_Cos(angle)) >> 12) * vertical) / height;
  const int z = height == 0
                    ? -1
                    : (((48 * Core_Gte_Sin(angle)) >> 12) * vertical) / height;
  Core_Random_Seed(seed);

  Effects_EmitParticleEvent(&fixture.instance, 0, 0, 0, nullptr);

  fixture.CheckEmissionPool(true);
  const auto& position = fixture.particle.motion.position;
  const std::array actual_position{position.vx, position.vy, position.vz};
  const std::array expected_position{x * 4096, vertical * 4096, z * 4096};
  CHECK(actual_position == expected_position);
  CHECK(Core_Random_Rand() == expected.Next());
  Core_Random_Seed(1);
}

namespace {
struct ParticleMotionFixture {
  int saved_drag = Effects_particleDrag;
  Effects_ParticleGravity saved_gravity = Effects_particleGravity;
  Effects_Particle particle{};

  ParticleMotionFixture() {
    Effects_particleDrag = 0;
    Effects_particleGravity = {};
    particle.motion.mass = 4096;
  }
  ~ParticleMotionFixture() {
    Effects_particleDrag = saved_drag;
    Effects_particleGravity = saved_gravity;
  }
};
}  // namespace

TEST_CASE("Particle rendering flags cannot invent target attraction",
          "[effects][particle-motion][native-attraction]") {
  ParticleMotionFixture fixture;
  auto& particle = fixture.particle;
  particle.emitFlags = GENERATE(0, 1, 0x40, 0x20000, 0x40000, 0x80000);
  std::array<u_char, 4> program{};
  Effects_PackageTextureInfo texture{};
  particle.textureAnimation.program = program.data();
  particle.textureAnimation.textureInfo = &texture;
  particle.motion.velocity.vx = 4096;
  Effects_IntegrateParticleMotion(&particle);
  CHECK(particle.motion.position.vx == 4096);
  CHECK(particle.motion.position.vy == 0);
  CHECK(particle.motion.position.vz == 0);
  CHECK(particle.motion.force.vx == 0);
  CHECK(particle.motion.force.vy == 0);
  CHECK(particle.motion.force.vz == 0);
  CHECK(particle.textureAnimation.program == program.data());
  CHECK(particle.textureAnimation.textureInfo == &texture);
}

TEST_CASE("Particle attraction uses its native target and signed strength",
          "[effects][particle-motion][native-attraction]") {
  ParticleMotionFixture fixture;
  const auto axis = GENERATE(0, 1, 2);
  auto& particle = fixture.particle;
  particle.attractionTarget[axis] = 32;
  particle.attraction = GENERATE(-4096, 0, 4096);
  Effects_IntegrateParticleMotion(&particle);
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
  Effects_Particle second{};
  second.motion.mass = 4096;
  second.attraction = 4096;
  second.attractionTarget[2] = 32;
  fixture.particle.emitFlags = 0x20000;
  fixture.particle.next = &second;
  Effects_IntegrateParticleMotion(&fixture.particle);
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
  CHECK(trail.stepCount == 12);
  CHECK(trail.splineSampleCount == 1);
  CHECK(trail.trailCollapseCounter == std::size(trail.splineSamples) - 1);
  CHECK(trail.currentSplineSample == 0);
  CHECK(trail.splineProgress == 0);
  CHECK(trail.speed == 0);
}

template <class Trail>
void CheckTrailReferenceStep(const std::array<Trail, 2>& entries,
                             std::size_t frame) {
  const auto& trail = entries[0];
  const auto samples = std::size(trail.splineSamples);
  CHECK(trail.splineProgress == kTrailGolden[frame].progress);
  CHECK(trail.speed == kTrailGolden[frame].speed);
  CHECK(trail.currentSplineSample == (frame + 1) % samples);
  CHECK(trail.splineSampleCount == std::min(frame + 2, samples));
  const auto& point = trail.splineSamples[trail.currentSplineSample];
  const std::array actual{point.vx, point.vy, point.vz};
  CHECK(actual == kTrailGolden[frame].point);
  const auto& separate = entries[1];
  const auto& zero = separate.splineSamples[separate.currentSplineSample];
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
  std::ranges::copy(kPoints, trail.splinePoints);
  state.trails = storage.entries.data();
  Effects_ParticleDefinition definition{};
  std::ranges::fill(definition.pairRanges[4], 12);
  Effects_ParticleEmitterScratch scratch{};
  Effects_EffectInstance instance{};
  const auto samples = std::size(trail.splineSamples);
  for (int cycle = 0; cycle < 2; ++cycle) {
    state.curveSampleDistance = 0;
    initialize(&definition, &scratch, &instance, &state);
    CheckTrailInitialState(trail);
    for (std::size_t frame = 0; frame < kTrailGolden.size(); ++frame) {
      state.curveSampleDistance = static_cast<int>(frame);
      update(&definition, &scratch, &instance, &state);
      CheckTrailReferenceStep(storage.entries, frame);
      CHECK(storage.guard == kGuard);
    }
    for (std::size_t tail = 0; tail < samples - 1; ++tail) {
      state.curveSampleDistance = 12 + static_cast<int>(tail);
      update(&definition, &scratch, &instance, &state);
      CHECK(trail.trailCollapseCounter == samples - 2 - tail);
      CHECK(storage.guard == kGuard);
    }
    CHECK(trail.splineSampleCount == 1);
  }
}
}  // namespace

TEST_CASE(
    "Five-sample trails preserve source trajectory and repeat-entry state",
    "[effects][trails][reference]") {
  Effects_TrailQuadSet state{};
  state.trailcount = 2;
  CheckTrailReferencePath<Effects_Trail>(state, Effects_Plg057_TrailInit,
                                         Effects_Plg057_TrailUpdate);
}

TEST_CASE("Nine-sample trails preserve source trajectory and separate history",
          "[effects][trails][reference]") {
  Effects_Trail9State state{};
  state.trailCount = 2;
  CheckTrailReferencePath<Effects_Trail9>(state, Effects_Plg057_TrailNineInit,
                                          Effects_Plg057_TrailNineUpdate);
}

namespace {
void CheckTrailReferenceQuad(const POLY_GT4& quad, int x, int y, int fade,
                             int samples) {
  CHECK(quad.tag == 0xE1000000);
  CHECK(quad.code == Render_LegacyPrimitiveCode_PolyGt4SemiTrans);
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
  constexpr auto kSamples = std::size(Trail{}.splineSamples);
  for (std::size_t actor = 0; actor < trails.size(); ++actor) {
    auto& trail = trails[actor];
    trail.splineSampleCount = static_cast<u_char>(count);
    trail.trailCollapseCounter = static_cast<u_char>(count - 1);
    trail.currentSplineSample = static_cast<u_char>(current);
    for (std::size_t index = 0; index < kSamples; ++index) {
      trail.splineSamples[(current + kSamples - index) % kSamples] = {
          static_cast<std::int16_t>(index * 32),
          static_cast<std::int16_t>(actor * 64), 1024, 0};
    }
  }
}

void CheckTrailReferenceRenderState(
    const Effects_ParticleTextureAnimation& animation,
    const ParticleFixture& fixture, const u_long* end) {
  CHECK(animation.cursor == 4);
  CHECK(animation.frameTimer == 8);
  CHECK(Render_frameScratch.primitiveCursor == end);
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

template <class Trail, class Packet, class ReadQuad>
void CheckTrailReferencePackets(const u_long* begin, std::size_t trail_count,
                                std::size_t count, int origin_x,
                                ReadQuad read_quad) {
  constexpr auto kPacketWords = sizeof(Packet) / sizeof(u_long);
  constexpr auto kSamples = std::size(Trail{}.splineSamples);
  for (std::size_t actor = 0; actor < trail_count; ++actor) {
    for (std::size_t segment = 0; segment < count - 1; ++segment) {
      const auto packet_index = actor * (count - 1) + segment;
      Packet packet{};
      std::memcpy(&packet, begin + packet_index * kPacketWords, sizeof(packet));
      const auto& quad = read_quad(packet);
      const auto x = origin_x + static_cast<int>(segment * 8);
      const auto y = 120 + static_cast<int>(actor * 16);
      CHECK(packet.wordCount == 14);
      CHECK(static_cast<std::uint32_t>(packet.drawMode) == 0xE1000200u);
      const auto fade = count - 1 - segment;
      CheckTrailReferenceQuad(quad, x, y, static_cast<int>(fade),
                              static_cast<int>(kSamples));
    }
  }
}

template <class Trail, class Packet, class State, class Render, class ReadQuad>
void CheckTrailReferenceRendering(State& state,
                                  Effects_ParticleTextureAnimation& animation,
                                  std::span<u_char> transparency, Render render,
                                  ReadQuad read_quad) {
  constexpr auto kSamples = std::size(Trail{}.splineSamples);
  const bool attached = GENERATE(false, true);
  const bool clipped = GENERATE(false, true);
  const bool at_boundary = GENERATE(false, true);
  const auto count = GENERATE_COPY(std::size_t{1}, std::size_t{2}, kSamples);
  const auto current = GENERATE_COPY(std::size_t{0}, kSamples - 1);
  ParticleFixture fixture;
  std::array<Trail, 2> trails{};
  InitializeTrailRenderHistory(trails, count, current);
  state.trails = trails.data();
  animation.program = reinterpret_cast<u_char*>(fixture.script.data());
  for (std::size_t index = 0; index < transparency.size(); ++index) {
    transparency[index] = static_cast<u_char>((index + 1) * 8);
  }
  Effects_ParticleDefinition definition{};
  definition.vectorRanges[4][0] = definition.vectorRanges[4][1] = 8;
  definition.transformIndex = 1;
  Effects_ParticleEmitterScratch scratch{};
  scratch.flags = attached ? 0x20000 : 0;
  Render_frameScratch.viewMatrix.t[0] = -64;
  fixture.instance.transforms[1].view = Render_frameScratch.viewMatrix;
  fixture.instance.transforms[1].view.t[0] = 128;
  Render_nearClip = clipped ? 256 : 255;
  constexpr auto kPacketWords = sizeof(Packet) / sizeof(u_long);
  const auto packets = trails.size() * (count - 1);
  auto* begin = fixture.storage.data() + 1;
  if (at_boundary) begin += 0x8000 - packets * kPacketWords;
  Render_frameScratch.primitiveCursor = begin;
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
  struct Storage {
    Effects_TrailQuadSet state{};
    std::array<u_char, 5> transparency{};
  } storage;
  static_assert(offsetof(Storage, transparency) ==
                offsetof(Effects_TrailQuadSet, sampledTransparencyCurve));
  storage.state.trailcount = 2;
  CheckTrailReferenceRendering<Effects_Trail, Render_EffectGouraudQuadPacket>(
      storage.state, storage.state.textureAnimation, storage.transparency,
      Effects_Plg057_TrailRender,
      [](const auto& packet) -> const POLY_GT4& { return packet.opaque4; });
}

TEST_CASE("Nine-sample trail ribbons retain source geometry and render spaces",
          "[effects][trails][reference][trail-render]") {
  struct Storage {
    Effects_Trail9State state{};
    std::array<u_char, 9> transparency{};
  } storage;
  static_assert(offsetof(Storage, transparency) ==
                offsetof(Effects_Trail9State, sampledTransparencyCurve));
  storage.state.trailCount = 2;
  CheckTrailReferenceRendering<Effects_Trail9, Render_Trail9Packet>(
      storage.state, storage.state.texture, storage.transparency,
      Effects_Plg057_TrailNineRender,
      [](const auto& packet) -> const POLY_GT4& { return packet.poly; });
}

namespace {
template <class Function>
Function Plg074Entry(unsigned address, Function expected) {
  auto* actual = VS::Content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, "EFFECT/PLG074.BIN",
      address);
  REQUIRE(actual == reinterpret_cast<void*>(expected));
  return reinterpret_cast<Function>(actual);
}

struct Plg074Fixture : EmissionFixture {
  int saved_sample = Effects_curveSamples[0];
  int saved_corner = Effects_Plg073_trailEmissionCounter;
  int saved_plg074_corner = Content_plg074TrailEmissionCounter[0];
  SVECTOR saved_origin = Effects_effectOrigin[0];
  Effects_ActionEffectRequest saved_request = Effects_actionRequest;
  std::array<CVECTOR, 8> saved_colors{};
  std::array<std::array<u_char, 4>, 8> saved_arc_colors{};
  Effects_ParticleEmitterScratch scratch{};
  std::array<Effects_Trail, 16> trails{};
  Effects_TrailQuadSet trail_state{};

  Plg074Fixture() {
    package.block1TableCount = 1;
    package.block2Count = 1;
    Effects_curveSamples[0] = 0;
    Effects_Plg073_trailEmissionCounter = 0;
    Content_plg074TrailEmissionCounter[0] = 0;
    Effects_effectOrigin[0] = {100, -200, 300, 0};
    Effects_actionRequest.position = {400, -500, 600, 0};
    std::ranges::copy(Effects_Plg073_tiledRingColorFactors,
                      saved_colors.begin());
    std::memcpy(saved_arc_colors.data(), Effects_Plg073_ringColorFactors,
                sizeof saved_arc_colors);
    Effects_Plg073_tiledRingColorFactors[0] = {32, 64, 96, 0};
    for (int mode = 0; mode < 8; ++mode) {
      for (int row = 0; row < 3; ++row) {
        Effects_Plg073_ringColorFactors[mode][row] =
            static_cast<u_char>(32 * (row + 1) + mode);
      }
    }
    instance.transforms[0].world.t[0] = 700;
    instance.transforms[0].world.t[1] = -800;
    instance.transforms[0].world.t[2] = 900;
    trail_state.trails = trails.data();
    trail_state.trailcount = 8;
  }

  ~Plg074Fixture() {
    Effects_curveSamples[0] = saved_sample;
    Effects_Plg073_trailEmissionCounter = saved_corner;
    Content_plg074TrailEmissionCounter[0] = saved_plg074_corner;
    Effects_effectOrigin[0] = saved_origin;
    Effects_actionRequest = saved_request;
    std::ranges::copy(saved_colors, Effects_Plg073_tiledRingColorFactors);
    std::memcpy(Effects_Plg073_ringColorFactors, saved_arc_colors.data(),
                sizeof saved_arc_colors);
  }

  Effects_ParticleDefinition& Definition() { return definitions.entries[0]; }

  void ConfigureRing() {
    auto& definition = Definition();
    std::ranges::copy(std::array<std::int16_t, 6>{0, 0, 0, 0, 1024, 1024},
                      definition.positionRange);
    std::ranges::copy(std::array<std::int16_t, 6>{128, 128, 64, 64, 128, 128},
                      definition.extentRange);
    std::ranges::fill(definition.innerExtentRange, 2048);
    definition.vectorRanges[0][0] = definition.vectorRanges[0][1] = 1024;
    definition.vectorRanges[1][0] = definition.vectorRanges[1][1] = 4096;
  }

  void ResetPackets() {
    Render_frameScratch.primitiveCursor = storage.data() + 1;
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
  CHECK(state.radiusSpeed == 0);
  CHECK(state.heightSpeed == 0);
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
    Core_Memory_FreeFromHighEnd(state);
    CHECK(Core_legacyHeapList.next->blockSize == heap.free_units);
    fixture.ResetPackets();
  }
}

void CheckPlg074TiledRingPackets(const Render_TiledRingPacket* packets) {
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
    CHECK(quad.code == Render_LegacyPrimitiveCode_PolyGt4SemiTrans);
  }
}

void CheckPlg074TiledRingGeometry(const Render_TiledRingPacket* packets) {
  CHECK(packets[0].poly.x0 == 192);
  CHECK(packets[0].poly.y0 == 120);
  CHECK(packets[0].poly.y2 == 128);
  CHECK(packets[15].poly.x1 == packets[0].poly.x0);
  CHECK(packets[15].poly.y1 == packets[0].poly.y0);
}

void CheckPlg074RingArcPackets(const Render_RingPacket* packets, int mode) {
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
}  // namespace

TEST_CASE("PLG074 F01986 draws tiled rings through the production slot",
          "[effects][plg074][f01986]") {
  const auto entry = Plg074Entry(0x800F98F8, Effects_Plg073_TiledRing);
  REQUIRE(Effects_effectEntryPoints[87] == entry);
  Plg074Fixture fixture;
  fixture.ConfigureRing();
  DispatchHeap heap;
  Effects_RendererSlot slot{};
  REQUIRE(entry(&slot, 1, 2) == 1);
  REQUIRE(entry(&slot, 2, 0) == 1);
  const auto* packets = reinterpret_cast<const Render_TiledRingPacket*>(
      fixture.storage.data() + 1);
  CHECK(Render_frameScratch.primitiveCursor == packets + 32);
  CheckPlg074TiledRingGeometry(packets);
  CheckPlg074TiledRingPackets(packets);
  fixture.CheckGuards();
  Core_Memory_FreeFromHighEnd(slot.rendererState);
  fixture.ResetPackets();
  CheckPlg074RingLifecycle<Effects_TiledRingState>(entry, fixture);
}

TEST_CASE("PLG074 F01987 draws an eight-segment arc with continuous UVs",
          "[effects][plg074][f01987]") {
  const auto entry = Plg074Entry(0x800FA7F0, Effects_Plg073_Ring);
  REQUIRE(Effects_effectEntryPoints[88] == entry);
  const auto mode = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  Plg074Fixture fixture;
  fixture.ConfigureRing();
  fixture.Definition().transparencyCurve = static_cast<u_int>(mode);
  DispatchHeap heap;
  Effects_RendererSlot slot{};
  REQUIRE(entry(&slot, 1, 2) == 1);
  REQUIRE(entry(&slot, 2, 0) == 1);
  const auto* packets =
      reinterpret_cast<const Render_RingPacket*>(fixture.storage.data() + 1);
  CHECK(Render_frameScratch.primitiveCursor == packets + 16);
  CheckPlg074RingArcPackets(packets, mode);
  CHECK(packets[0].poly.x0 == 192);
  CHECK(packets[0].poly.y0 == 120);
  CHECK(packets[7].poly.x1 == packets[0].poly.x0);
  fixture.CheckGuards();
  Core_Memory_FreeFromHighEnd(slot.rendererState);
  fixture.ResetPackets();
  CheckPlg074RingLifecycle<Effects_RingState>(entry, fixture);
}

TEST_CASE("PLG074 F01988 cycles all box corners in each anchor space",
          "[effects][plg074][f01988]") {
  const auto entry =
      Plg074Entry(0x800FB414, Effects_Plg074_GenerateTrailOrigins);
  const int anchor = GENERATE(0, 1, 2);
  Plg074Fixture fixture;
  auto& definition = fixture.Definition();
  std::ranges::copy(std::array<std::int16_t, 6>{10, 10, 20, 20, 30, 30},
                    definition.extentRange);
  fixture.scratch.flags = 0x1008 | anchor;
  Effects_Plg073_trailEmissionCounter = 5;
  const std::array offsets{std::array{0, 0, 0}, std::array{100, -200, 300},
                           std::array{700, -800, 900}};
  for (int cycle = 0; cycle < 2; ++cycle) {
    entry(&definition, &fixture.scratch, &fixture.instance,
          &fixture.trail_state);
    for (int index = 0; index < 8; ++index) {
      const auto& point = fixture.trails[index].splinePoints[0];
      const int corner = index + 1;
      CHECK(point.vx == offsets[anchor][0] + (corner & 1 ? 10 : -10));
      CHECK(point.vy == offsets[anchor][1] + (corner & 2 ? 20 : -20));
      CHECK(point.vz == offsets[anchor][2] + (corner & 4 ? 30 : -30));
    }
    CHECK(Content_plg074TrailEmissionCounter[0] == 0);
    CHECK(Effects_Plg073_trailEmissionCounter == 5);
    CHECK(fixture.trails[8].splinePoints[0].vx == 0);
  }
}

TEST_CASE("PLG074 F01989 targets world request and actor anchors",
          "[effects][plg074][f01989]") {
  const auto entry = Plg074Entry(0x800FBBB0, Effects_Plg073_TrailTarget);
  const unsigned anchor = GENERATE(0u, 0x80u, 0xC0u);
  Plg074Fixture fixture;
  auto& definition = fixture.Definition();
  std::ranges::copy(std::array<std::int16_t, 6>{10, 10, -20, -20, 30, 30},
                    definition.vectorRanges[6]);
  fixture.scratch.flags = anchor;
  entry(&definition, &fixture.scratch, &fixture.instance, &fixture.trail_state);
  const auto offset = anchor == 0x80   ? std::array{400, -500, 600}
                      : anchor == 0xC0 ? std::array{700, -800, 900}
                                       : std::array{0, 0, 0};
  for (int index = 0; index < 8; ++index) {
    const auto& point = fixture.trails[index].splinePoints[3];
    CHECK(point.vx == offset[0] + 10);
    CHECK(point.vy == offset[1] - 20);
    CHECK(point.vz == offset[2] + 30);
    CHECK(fixture.trails[index].splinePoints[0].vx == 0);
  }
  CHECK(fixture.trails[8].splinePoints[3].vx == 0);
}

TEST_CASE("PLG074 F01990 builds axial spline controls without moving endpoints",
          "[effects][plg074][f01990]") {
  const auto entry = Plg074Entry(0x800FBDC8, Effects_Plg073_TrailControl);
  Plg074Fixture fixture;
  fixture.trail_state.trailcount = 1;
  auto& points = fixture.trails[0].splinePoints;
  points[0] = {10, 20, 30, 0};
  points[3] = {10, 20, 1054, 0};
  std::ranges::copy(std::array<std::int16_t, 4>{64, 64, -64, -64},
                    fixture.Definition().pairRanges[2]);
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
  CHECK(fixture.trails[1].splinePoints[1].vz == 0);
}

namespace {
void CheckPlg074TrailInitialization(const Effects_Trail& trail, int steps) {
  CHECK(trail.stepCount == steps);
  CHECK(trail.splineSampleCount == 1);
  CHECK(trail.trailCollapseCounter == 4);
  CHECK(trail.currentSplineSample == 0);
  CHECK(trail.splineProgress == 0);
  CHECK(trail.speed == 8192);
  CHECK(trail.opaque54 == 0x06000000 / (steps * steps) - 16384 / steps);
}

void CheckPlg074EmitterInitialization(const Effects_TrailQuadSet& state,
                                      int count) {
  CHECK(state.trailcount == std::min(count, 16));
  CHECK(state.curveSampleDistance == 0);
  CHECK(state.opaque3 == 0);
  CHECK(state.sampledTransparencyCurve[0] == 128);
  CHECK(state.sampledTransparencyCurve[4] == 128);
}

void CheckPlg074EmitterTick(const Effects_TrailQuadSet& state, int count,
                            int cycle) {
  CHECK(state.opaque3 == 1);
  CHECK(state.curveSampleDistance == 1);
  CHECK(Content_plg074TrailEmissionCounter[0] ==
        ((cycle + 1) * std::min(count, 16)) % 8);
  CHECK(Effects_Plg073_trailEmissionCounter == 0);
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

void CheckPlg074SurfacePackets(const Render_RotatingSurfacePacket* packets) {
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
  const auto entry = Plg074Entry(0x800FC28C, Effects_Plg073_TrailInit);
  const std::int16_t duration = GENERATE(0, 1, 12, 255);
  Plg074Fixture fixture;
  std::ranges::fill(fixture.Definition().pairRanges[4], duration);
  std::ranges::fill(fixture.Definition().innerExtentRange, 2);
  fixture.trails[0].splinePoints[0] = {10, 20, 30, 0};
  entry(&fixture.Definition(), &fixture.scratch, &fixture.instance,
        &fixture.trail_state);
  for (int index = 0; index < 8; ++index) {
    const auto& trail = fixture.trails[index];
    const int steps = duration == 0 ? 1 : duration;
    CheckPlg074TrailInitialization(trail, steps);
    CHECK(trail.splineSamples[0].vx == (index == 0 ? 10 : 0));
  }
  CHECK(fixture.trails[8].stepCount == 0);
}

TEST_CASE("PLG074 F01992 matches the source recurrence and tail collapse",
          "[effects][plg074][f01992]") {
  const auto entry = Plg074Entry(0x800FC404, Effects_Plg073_TrailUpdate);
  Effects_TrailQuadSet state{};
  state.trailcount = 2;
  CheckTrailReferencePath<Effects_Trail>(state, Effects_Plg073_TrailInit,
                                         entry);
}

TEST_CASE(
    "PLG074 F01993 preserves ribbon packets clipping and circular history",
    "[effects][plg074][f01993]") {
  const auto entry = Plg074Entry(0x800FC590, Effects_Plg073_TrailRender);
  struct Storage {
    Effects_TrailQuadSet state{};
    std::array<u_char, 5> transparency{};
  } storage;
  static_assert(offsetof(Storage, transparency) ==
                offsetof(Effects_TrailQuadSet, sampledTransparencyCurve));
  storage.state.trailcount = 2;
  CheckTrailReferenceRendering<Effects_Trail, Render_EffectGouraudQuadPacket>(
      storage.state, storage.state.textureAnimation, storage.transparency,
      entry,
      [](const auto& packet) -> const POLY_GT4& { return packet.opaque4; });
}

TEST_CASE("PLG074 F01994 owns trail buffers across expiry cancel and reentry",
          "[effects][plg074][f01994]") {
  const auto entry = Plg074Entry(0x800FCFEC, Effects_Plg074_UpdateTrailEmitter);
  REQUIRE(Effects_effectEntryPoints[89] == entry);
  const std::int16_t count = GENERATE(0, 1, 16, 20);
  const bool cancel = GENERATE(false, true);
  Plg074Fixture fixture;
  DispatchHeap heap;
  fixture.Definition().intervalRange[0] = count;
  fixture.Definition().flags = 0x1008;
  std::ranges::fill(fixture.Definition().pairRanges[4], 12);
  for (int cycle = 0; cycle < 2; ++cycle) {
    Effects_RendererSlot slot{};
    REQUIRE(entry(&slot, 1, 2) == 1);
    auto* state = static_cast<Effects_TrailQuadSet*>(slot.rendererState);
    REQUIRE(state != nullptr);
    CheckPlg074EmitterInitialization(*state, count);
    REQUIRE(entry(&slot, 2, 0) == 1);
    CheckPlg074EmitterTick(*state, count, cycle);
    StopPlg074Emitter(entry, slot, cancel);
    Core_Memory_FreeFromHighEnd(state);
    CHECK(Core_legacyHeapList.next->blockSize == heap.free_units);
    fixture.ResetPackets();
  }
}

TEST_CASE("PLG074 F01995 draws five accelerating surface rows",
          "[effects][plg074][f01995]") {
  const auto entry = Plg074Entry(0x800FD534, Effects_Plg073_RotatingSurface);
  REQUIRE(Effects_effectEntryPoints[90] == entry);
  Plg074Fixture fixture;
  fixture.ConfigureRing();
  std::ranges::fill(fixture.Definition().vectorRanges[1], 0);
  DispatchHeap heap;
  Effects_RendererSlot slot{};
  REQUIRE(entry(&slot, 1, 2) == 1);
  REQUIRE(entry(&slot, 2, 0) == 1);
  const auto* packets = reinterpret_cast<const Render_RotatingSurfacePacket*>(
      fixture.storage.data() + 1);
  CHECK(Render_frameScratch.primitiveCursor == packets + 64);
  CheckPlg074SurfacePackets(packets);
  fixture.CheckGuards();
  Core_Memory_FreeFromHighEnd(slot.rendererState);
  fixture.ResetPackets();
  CheckPlg074RingLifecycle<Effects_RotatingSurfaceState>(entry, fixture);
}

TEST_CASE(
    "PLG074 production renderer lists expire and release every allocation",
    "[effects][plg074][plg074-lifecycle]") {
  Plg074Fixture fixture;
  fixture.ConfigureRing();
  DispatchHeap heap;
  for (int cycle = 0; cycle < 3; ++cycle) {
    for (int index = 87; index <= 90; ++index) {
      auto* slot = Effects_AttachEffectRenderer(
          &fixture.instance, Effects_effectEntryPoints[index], 2);
      REQUIRE(slot != nullptr);
      REQUIRE(slot->rendererState != nullptr);
    }
    CHECK(Effects_TickEffectRenderers(&fixture.instance) == 1);
    REQUIRE(fixture.instance.context.renderer != nullptr);
    fixture.ResetPackets();
    CHECK(Effects_TickEffectRenderers(&fixture.instance) == 1);
    CHECK(fixture.instance.context.renderer == nullptr);
    CHECK(Core_legacyHeapList.next->blockSize == heap.free_units);
    fixture.CheckGuards();
    fixture.ResetPackets();
  }
}
