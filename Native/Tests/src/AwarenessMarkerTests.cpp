#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorRecord.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/ActorMarkersLive.hpp"
#include "Battle/Feedback.hpp"
#include "Legacy/Psx.h"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

namespace {
struct MarkerFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  VS::Actors::ActorRecord* saved_player = VS::Actors::actorRecords[0];
  VS::Actors::ActorRecord* saved_enemy = VS::Actors::actorRecords[2];
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  void* saved_ordering = VS::Render::legacyOrderingTables[0];
  int saved_framebuffer = VS::Render::frameBuf;
  int saved_sides = VS::Battle::actorMarkerSides;
  VS::Actors::ActorRecord player{};
  VS::Actors::ActorRecord enemy{};
  std::vector<u_long> storage = std::vector<u_long>(4098, 0xDEADBEEF);
  std::array<u_long, 2048> ordering{};

  MarkerFixture() {
    VS::Actors::actorRecords[0] = &player;
    VS::Actors::actorRecords[2] = &enemy;
    player.instance.position.vx = -256;
    player.instance.position.vz = enemy.instance.position.vz = 512;
    player.target_point_height = enemy.target_point_height = 64;
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = storage.data() + 1;
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::FrameScratchpad() = {};
    for (int axis = 0; axis < 3; ++axis) {
      VS::Render::FrameScratchpad().view_matrix.m[axis][axis] = 4096;
    }
    VS::Render::FrameScratchpad().view_matrix.t[2] = 1024;
    VS::Render::FrameScratchpad().primitive_cursor = storage.data() + 1;
    VS::Render::FrameScratchpad().ordering_table = ordering.data();
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Core_Gte_SetGeomOffset(160, 120);
    Core_Gte_SetGeomScreen(256);
    VS::Battle::actorMarkerSides = 0;
  }

  ~MarkerFixture() {
    VS::Actors::actorRecords[0] = saved_player;
    VS::Actors::actorRecords[2] = saved_enemy;
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::legacyOrderingTables[0] = saved_ordering;
    VS::Render::frameBuf = saved_framebuffer;
    VS::Battle::actorMarkerSides = saved_sides;
    Core_Gte_Reset();
  }

  void CheckChain(int icons) const {
    const auto* end = static_cast<const u_long*>(
        VS::Render::FrameScratchpad().primitive_cursor);
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
  REQUIRE(VS::Actors::ProjectLiveActorTaggedBone(2, 251, &projected, 0) == 0);
  CHECK(projected.vx == 160);
  CHECK(projected.vz == 384);
  VS::Battle::LiveDrawActorMarker(2, kind);
  const int icons = kind <= 3 ? kind : kind <= 5 ? 1 : 0;
  fixture.CheckChain(icons);
}
