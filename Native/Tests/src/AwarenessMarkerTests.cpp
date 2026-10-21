#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Battle/Feedback.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
struct MarkerFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  Actors_ActorRecord* saved_player = Actors_actorRecords[0];
  Actors_ActorRecord* saved_enemy = Actors_actorRecords[2];
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  int saved_framebuffer = Render_frameBuf;
  int saved_sides = Battle_actorMarkerSides;
  Actors_ActorRecord player{};
  Actors_ActorRecord enemy{};
  std::vector<u_long> storage = std::vector<u_long>(4098, 0xDEADBEEF);
  std::array<u_long, 2048> ordering{};

  MarkerFixture() {
    Actors_actorRecords[0] = &player;
    Actors_actorRecords[2] = &enemy;
    player.instance.position.vx = -256;
    player.instance.position.vz = enemy.instance.position.vz = 512;
    player.targetPointHeight = enemy.targetPointHeight = 64;
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = storage.data() + 1;
    Render_frameScratch = {};
    for (int axis = 0; axis < 3; ++axis) {
      Render_frameScratch.viewMatrix.m[axis][axis] = 4096;
    }
    Render_frameScratch.viewMatrix.t[2] = 1024;
    Render_frameScratch.primitiveCursor = storage.data() + 1;
    Render_frameScratch.orderingTable = ordering.data();
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Core_Gte_SetGeomOffset(160, 120);
    Core_Gte_SetGeomScreen(256);
    Battle_actorMarkerSides = 0;
  }

  ~MarkerFixture() {
    Actors_actorRecords[0] = saved_player;
    Actors_actorRecords[2] = saved_enemy;
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_frameBuf = saved_framebuffer;
    Battle_actorMarkerSides = saved_sides;
    Core_Gte_Reset();
  }

  void CheckChain(int icons) const {
    const auto* end =
        static_cast<const u_long*>(Render_frameScratch.primitiveCursor);
    const auto* frame = end - 10;
    constexpr unsigned kDepth = 384 - 16;
    REQUIRE(nextPrim(&ordering[kDepth]) == frame);
    CHECK(end == storage.data() + 1 + static_cast<std::size_t>(icons) * 6 + 10);
    CHECK(storage.front() == 0xDEADBEEF);
    CHECK(storage.back() == 0xDEADBEEF);
    const void* packet = &ordering.back();
    unsigned count = 0;
    unsigned steps = 0;
    while (packet != nullptr && steps++ < 4096) {
      const auto* tag = static_cast<const P_TAG*>(packet);
      if (tag->len != 0) ++count;
      packet = isendprim(packet) ? nullptr : nextPrim(packet);
    }
    CHECK(packet == nullptr);
    CHECK(count == static_cast<unsigned>(icons + 1));
  }
};
}  // namespace

TEST_CASE(
    "Awareness bubbles retain high native packet links and complete icons",
    "[render][awareness]") {
  const int kind = GENERATE(1, 2, 3, 4, 5, 6);
  MarkerFixture fixture;
  REQUIRE(reinterpret_cast<std::uintptr_t>(fixture.storage.data()) >
          UINT32_MAX);
  SVECTOR projected{};
  REQUIRE(Actors_ProjectActorTaggedBone(2, 251, &projected, 0) == 0);
  CHECK(projected.vx == 160);
  CHECK(projected.vz == 384);
  Battle_DrawActorMarker(2, kind);
  const int icons = kind <= 3 ? kind : kind <= 5 ? 1 : 0;
  fixture.CheckChain(icons);
}
