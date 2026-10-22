#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Random.hpp"
#include "Effects/ParticleRender.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Core/BiosRand.h"
#include "Effects/EffectResources.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/PurgeTables.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ParticleRender.h"
#include "Effects/Timeline.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
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
};
}  // namespace

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

TEST_CASE("Particle emission respects empty bursts and exhausted capacity",
          "[effects][particle-emission]") {
  ParticleFixture fixture;
  struct Definitions {
    int reserved;
    Effects_ParticleDefinition entries[1];
  } definitions{};
  STATIC_REQUIRE(offsetof(Definitions, entries) ==
                 sizeof(Effects_PackageBlock5));
  std::array<u_short, 8> programs{1, 0, 8, 0, 0, 8, 0, 0x3000};
  fixture.package.block5Data =
      reinterpret_cast<Effects_PackageBlock5*>(&definitions);
  fixture.package.block2Data = programs.data();
  auto& definition = definitions.entries[0];
  definition.intervalRange[0] = definition.intervalRange[1] = 1;
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
          reinterpret_cast<u_char*>(programs.data()) + 8);
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
