#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/SkeletonLive.hpp"
#include "Battle/ActorMarkers.hpp"
#include "Battle/ActorMarkersLive.hpp"
#include "Battle/Feedback.h"
#include "Core/Gte.h"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

namespace {
namespace b = VS::Battle;
using b::MarkerScreenPoint;

class FakeWorld final : public b::ActorMarkerWorld {
 public:
  std::array<std::optional<b::ProjectedMarkerAnchor>, b::kActorMarkerCount>
      anchors{};
  std::array<std::optional<b::MarkerWorldPoint>, b::kActorMarkerCount>
      positions{};
  MarkerScreenPoint view_result{.x = 100, .y = 90};
  int subtype = 0;
  std::vector<b::MarkerWorldPoint> view_inputs;
  std::vector<b::ActorMarkerSprite> sprites;
  std::vector<b::ActorMarkerFrame> frames;
  std::vector<b::OffscreenActorArrow> arrows;
  std::string order;
  bool refuse = false;

  std::optional<b::ProjectedMarkerAnchor> ProjectActorAnchor(
      int actor) override {
    return anchors.at(static_cast<std::size_t>(actor));
  }
  std::optional<b::MarkerWorldPoint> ActorAnchorPosition(int actor) override {
    return positions.at(static_cast<std::size_t>(actor));
  }
  MarkerScreenPoint ProjectThroughView(b::MarkerWorldPoint point) override {
    view_inputs.push_back(point);
    return view_result;
  }
  int GimmickSubtype(int /*actor*/) override { return subtype; }
  bool DrawSprite(const b::ActorMarkerSprite& sprite) override {
    sprites.push_back(sprite);
    order += 'S';
    return !refuse;
  }
  bool DrawFrame(const b::ActorMarkerFrame& frame) override {
    frames.push_back(frame);
    order += 'F';
    return !refuse;
  }
  bool DrawArrow(const b::OffscreenActorArrow& arrow) override {
    arrows.push_back(arrow);
    order += 'A';
    return !refuse;
  }
};

FakeWorld OnScreen(MarkerScreenPoint actor, std::int16_t depth = 400,
                   std::int16_t player_x = 160) {
  FakeWorld world;
  world.anchors.at(0) = {.screen = {.x = player_x, .y = 100}, .depth = 500};
  world.anchors.at(3) = {.screen = actor, .depth = depth};
  return world;
}

FakeWorld Talking(std::int16_t dx, std::int16_t dz) {
  FakeWorld world;
  world.positions.at(0) = {.x = 1000, .y = -200, .z = 500};
  world.positions.at(5) = {.x = static_cast<std::int16_t>(1000 - dx),
                           .y = 300,
                           .z = static_cast<std::int16_t>(500 - dz)};
  return world;
}
}  // namespace

TEST_CASE("Marker points pack signed halves", "[actor-markers]") {
  const MarkerScreenPoint point{.x = -2, .y = -16};
  CHECK(b::PackMarkerPoint(point) == 0xFFF0FFFEU);
  CHECK(b::UnpackMarkerPoint(0xFFF0FFFEU) == point);
  CHECK(b::PackMarkerTexture(b::UnpackMarkerTexture(0x373E125AU)) ==
        0x373E125AU);
}

TEST_CASE("Dot markers draw descending dots before their frame",
          "[actor-markers]") {
  auto world = OnScreen({.x = 200, .y = 120});
  b::ActorMarkerState state;
  REQUIRE(b::DrawActorMarker(world, state, 3, 3));
  CHECK(world.order == "SSSF");
  REQUIRE(world.sprites.size() == 3);
  CHECK(world.sprites[0].position == MarkerScreenPoint{.x = 210, .y = 105});
  CHECK(world.sprites[2].position == MarkerScreenPoint{.x = 198, .y = 105});
  CHECK(world.sprites[0].ordering_slot == 384);
  CHECK(b::PackMarkerTexture(world.sprites[0].texture) == 0x373E125AU);
  CHECK(world.sprites[0].size == b::MarkerSize{.width = 5, .height = 5});
  CHECK_FALSE(world.sprites[0].draw_mode.has_value());
}

TEST_CASE("Dot positions carry between the packed halves", "[actor-markers]") {
  const auto [x, sprite] = GENERATE(
      std::pair<std::int16_t, MarkerScreenPoint>{1, {.x = -1, .y = 104}},
      std::pair<std::int16_t, MarkerScreenPoint>{2, {.x = 0, .y = 105}});
  auto world = OnScreen({.x = x, .y = 120}, 400, 0);
  b::ActorMarkerState state;
  REQUIRE(b::DrawActorMarker(world, state, 3, 1));
  REQUIRE(world.sprites.size() == 1);
  // x + 0xFFFE carries into the y half, as the original's 32-bit add.
  CHECK(world.sprites[0].position == sprite);
}

TEST_CASE("Marker side bits follow the player's screen margin",
          "[actor-markers]") {
  b::ActorMarkerState state{.sides = 0xFFFFFFFFU};
  auto right = OnScreen({.x = 193, .y = 120});
  REQUIRE(b::DrawActorMarker(right, state, 3, 6));
  CHECK((state.sides & 8U) == 0);
  auto edge = OnScreen({.x = 128, .y = 120});
  REQUIRE(b::DrawActorMarker(edge, state, 3, 6));
  CHECK((state.sides & 8U) == 0);
  auto left = OnScreen({.x = 127, .y = 120});
  REQUIRE(b::DrawActorMarker(left, state, 3, 6));
  CHECK((state.sides & 8U) == 8U);
  REQUIRE(left.frames.size() == 1);
  CHECK(left.frames[0].vertices[0].position ==
        MarkerScreenPoint{.x = 117, .y = 104});
}

TEST_CASE("Marker ordering slots clamp the biased depth", "[actor-markers]") {
  const auto [depth, slot] = GENERATE(std::pair<std::int16_t, int>{79, 64},
                                      std::pair<std::int16_t, int>{80, 64},
                                      std::pair<std::int16_t, int>{81, 65},
                                      std::pair<std::int16_t, int>{2063, 2047},
                                      std::pair<std::int16_t, int>{2064, 2047},
                                      std::pair<std::int16_t, int>{-32768, 64});
  auto world = OnScreen({.x = 160, .y = 120}, depth);
  b::ActorMarkerState state;
  REQUIRE(b::DrawActorMarker(world, state, 3, 0x10));
  REQUIRE(world.frames.size() == 1);
  CHECK(world.frames[0].ordering_slot == slot);
}

TEST_CASE("Pinned marker kinds outside one to four become questions",
          "[actor-markers]") {
  const auto [kind, sprites] = GENERATE(std::pair{0x81, 1}, std::pair{0x83, 3},
                                        std::pair{0x80, 1}, std::pair{0x87, 1});
  auto world = OnScreen({.x = 160, .y = 120});
  b::ActorMarkerState state;
  REQUIRE(b::DrawActorMarker(world, state, 3, kind));
  REQUIRE(world.sprites.size() == static_cast<std::size_t>(sprites));
  const bool question = (kind & 7) == 0 || (kind & 7) > 4;
  CHECK((world.sprites[0].size.width == 16) == question);
}

TEST_CASE("Question and small frame markers use their offsets",
          "[actor-markers]") {
  auto question = OnScreen({.x = 160, .y = 120});
  b::ActorMarkerState state;
  REQUIRE(b::DrawActorMarker(question, state, 3, 5));
  CHECK(question.order == "SF");
  CHECK(question.sprites[0].position == MarkerScreenPoint{.x = 160, .y = 102});
  CHECK(b::PackMarkerTexture(question.sprites[0].texture) == 0x373E2098U);
  auto small = OnScreen({.x = 160, .y = 120});
  REQUIRE(b::DrawActorMarker(small, state, 3, 6));
  CHECK(small.order == "F");
  CHECK(small.frames[0].vertices[0].position ==
        MarkerScreenPoint{.x = 151, .y = 104});
  CHECK(small.frames[0].vertices[3].u == 0xB4);
}

TEST_CASE("Exclamation markers draw one sprite without a frame",
          "[actor-markers]") {
  auto world = OnScreen({.x = 160, .y = 120});
  b::ActorMarkerState state;
  REQUIRE(b::DrawActorMarker(world, state, 3, 7));
  CHECK(world.order == "S");
  CHECK(world.sprites[0].position == MarkerScreenPoint{.x = 160, .y = 98});
  CHECK(world.sprites[0].draw_mode == std::optional<std::uint32_t>{0xE1000017});
  CHECK(b::PackMarkerTexture(world.sprites[0].texture) == 0x373E11B7U);
}

TEST_CASE("Empty and missing markers draw nothing", "[actor-markers]") {
  auto empty = OnScreen({.x = 160, .y = 120});
  b::ActorMarkerState state;
  REQUIRE(b::DrawActorMarker(empty, state, 3, 0));
  CHECK(empty.order.empty());
  auto missing = OnScreen({.x = 160, .y = 120});
  missing.anchors.at(0).reset();
  REQUIRE(b::DrawActorMarker(missing, state, 3, 1));
  CHECK(missing.order.empty());
  CHECK_FALSE(b::DrawActorMarker(empty, state, 16, 1));
  CHECK_FALSE(b::DrawActorMarker(empty, state, -1, 1));
}

TEST_CASE("Alert markers on screen draw the alert icon and frame",
          "[actor-markers]") {
  auto world = OnScreen({.x = 304, .y = 8});
  b::ActorMarkerState state;
  REQUIRE(b::DrawActorMarker(world, state, 3, 4));
  CHECK(world.order == "SF");
  CHECK(world.sprites[0].position == MarkerScreenPoint{.x = 304, .y = -13});
  CHECK(world.sprites[0].size == b::MarkerSize{.width = 7, .height = 12});
  CHECK(world.frames[0].vertices[1].u == 0x97);
}

TEST_CASE("Alert markers off screen draw only the clamped arrow",
          "[actor-markers]") {
  const auto [x, y, ax, ay] =
      GENERATE(std::array{15, 100, 8, 107}, std::array{305, 30, 304, 29},
               std::array{160, 7, 152, 32}, std::array{160, 209, 167, 215},
               std::array{-500, 300, 16, 218});
  auto world = OnScreen(
      {.x = static_cast<std::int16_t>(x), .y = static_cast<std::int16_t>(y)});
  b::ActorMarkerState state;
  REQUIRE(b::DrawActorMarker(world, state, 3, 4));
  CHECK(world.order == "A");
  REQUIRE(world.arrows.size() == 1);
  CHECK(world.arrows[0].vertices[0] ==
        MarkerScreenPoint{.x = static_cast<std::int16_t>(ax),
                          .y = static_cast<std::int16_t>(ay)});
}

TEST_CASE("Talk markers draw the gimmick bubble near the player",
          "[actor-markers]") {
  auto world = Talking(200, -151);
  b::ActorMarkerState state;
  state.kinds[5] = 3;
  REQUIRE(b::DrawActorMarker(world, state, 5, 8));
  CHECK(world.order == "S");
  REQUIRE(world.view_inputs.size() == 1);
  CHECK(world.view_inputs[0] ==
        b::MarkerWorldPoint{.x = 800, .y = 236, .z = 651});
  const auto& sprite = world.sprites[0];
  CHECK(sprite.position == MarkerScreenPoint{.x = 92, .y = 82});
  CHECK(sprite.ordering_slot == 0);
  CHECK(sprite.draw_mode == std::optional<std::uint32_t>{0xE100000C});
  CHECK(b::PackMarkerTexture(sprite.texture) == 0x37FD2060U);
  CHECK(state.kinds[5] == 4);
}

TEST_CASE("Talk marker frames wrap at eight", "[actor-markers]") {
  auto world = Talking(10, 10);
  b::ActorMarkerState state;
  state.kinds[5] = 0x8F;
  REQUIRE(b::DrawActorMarker(world, state, 5, 8));
  CHECK(state.kinds[5] == 0);
  world.subtype = 2;
  state.kinds[5] = 0x8F;
  REQUIRE(b::DrawActorMarker(world, state, 5, 8));
  CHECK(state.kinds[5] == 0x8F);
  CHECK(b::PackMarkerTexture(world.sprites[1].texture) == 0x37FD2080U);
}

TEST_CASE("Talk markers hide beyond the far distance", "[actor-markers]") {
  b::ActorMarkerState state;
  auto far = Talking(417, 0);
  REQUIRE(b::DrawActorMarker(far, state, 5, 8));
  CHECK(far.order.empty());
  CHECK(state.sides == 0x20U);
  auto between = Talking(-200, 200);
  REQUIRE(b::DrawActorMarker(between, state, 5, 8));
  CHECK(between.order.empty());
  auto near = Talking(351, 0);
  REQUIRE(b::DrawActorMarker(near, state, 5, 8));
  CHECK(near.order == "S");
  CHECK(state.sides == 0);
}

TEST_CASE("Talk markers clear without a speaker or gimmick",
          "[actor-markers]") {
  b::ActorMarkerState state;
  state.kinds[5] = 8;
  auto none = Talking(0, 0);
  none.subtype = -1;
  REQUIRE(b::DrawActorMarker(none, state, 5, 8));
  CHECK(none.order.empty());
  CHECK(state.kinds[5] == 0);
  state.kinds[5] = 8;
  auto absent = Talking(0, 0);
  absent.positions.at(5).reset();
  REQUIRE(b::DrawActorMarker(absent, state, 5, 8));
  CHECK(state.kinds[5] == 0);
  state.kinds[5] = 8;
  auto lost = Talking(0, 0);
  lost.positions.at(0).reset();
  REQUIRE(b::DrawActorMarker(lost, state, 5, 8));
  CHECK(state.kinds[5] == 8);
}

TEST_CASE("Marker sweep skips the player and hidden markers",
          "[actor-markers]") {
  FakeWorld world;
  for (int actor = 0; actor < 16; ++actor) {
    world.anchors.at(static_cast<std::size_t>(actor)) = {
        .screen = {.x = 160, .y = 120}, .depth = 400};
  }
  b::ActorMarkerState state;
  state.kinds[0] = 6;
  state.kinds[2] = 6;
  state.kinds[15] = 0x86;
  REQUIRE(b::DrawActorMarkers(world, state));
  CHECK(world.order == "FSF");
  state.hidden = 1;
  REQUIRE(b::DrawActorMarkers(world, state));
  CHECK(world.order == "FSF");
}

TEST_CASE("Off-screen arrows rotate with the edge flags", "[actor-markers]") {
  const auto [direction, first] =
      GENERATE(std::pair{4, MarkerScreenPoint{.x = 92, .y = 42}},
               std::pair{6, MarkerScreenPoint{.x = 100, .y = 39}},
               std::pair{10, MarkerScreenPoint{.x = 110, .y = 50}},
               std::pair{5, MarkerScreenPoint{.x = 89, .y = 50}});
  FakeWorld world;
  REQUIRE(b::DrawOffscreenActorArrow(world, {.x = 100, .y = 50}, direction));
  REQUIRE(world.arrows.size() == 1);
  CHECK(world.arrows[0].vertices[0] == first);
}

TEST_CASE("Off-screen arrows keep the quad's packet vertex order",
          "[actor-markers]") {
  FakeWorld world;
  REQUIRE(b::DrawOffscreenActorArrow(world, {.x = -32768, .y = 0}, 4));
  const auto& vertices = world.arrows.at(0).vertices;
  CHECK(vertices[0] == MarkerScreenPoint{.x = 32760, .y = -8});
  CHECK(vertices[1] == MarkerScreenPoint{.x = -32761, .y = -8});
  CHECK(vertices[2] == MarkerScreenPoint{.x = 32760, .y = 7});
  CHECK(vertices[3] == MarkerScreenPoint{.x = -32761, .y = 7});
  CHECK(b::DrawOffscreenActorArrow(world, {}, 3));
  CHECK(b::DrawOffscreenActorArrow(world, {}, 0));
  CHECK(world.arrows.size() == 1);
}

TEST_CASE("Arrow packets keep their colour and padding", "[actor-markers]") {
  b::MarkerFramePacket packet{};
  packet.fill(0xA5A5A5A5U);
  const b::OffscreenActorArrow arrow{.vertices = {{{.x = 1, .y = 2},
                                                   {.x = 3, .y = 4},
                                                   {.x = 5, .y = 6},
                                                   {.x = -1, .y = -2}}}};
  b::WriteOffscreenArrowPacket(packet, arrow, 0x12345678U);
  CHECK(packet == b::MarkerFramePacket{0x09345678U, 0x2DA5A5A5U, 0x00020001U,
                                       0x37FD3030U, 0x00040003U, 0x000C303FU,
                                       0x00060005U, 0xA5A53F30U, 0xFFFEFFFFU,
                                       0xA5A53F3FU});
}

TEST_CASE("Marker frame packets carry the page and clut", "[actor-markers]") {
  b::ActorMarkerFrame frame;
  frame.vertices[0] = {.position = {.x = 1, .y = 2}, .u = 0x40, .v = 0x10};
  frame.vertices[3] = {.position = {.x = -1, .y = -1}, .u = 0x58, .v = 0x27};
  const auto packet = b::BuildActorMarkerFramePacket(frame, 0xAB00FF00U);
  CHECK(packet[0] == 0x0900FF00U);
  CHECK(packet[1] == 0x2C808080U);
  CHECK(packet[2] == 0x00020001U);
  CHECK(packet[3] == 0x373E1040U);
  CHECK(packet[5] == 0x00170000U);
  CHECK(packet[8] == 0xFFFFFFFFU);
  CHECK(packet[9] == 0x2758U);
}

TEST_CASE("Ability timing cue lifts above the screen top", "[actor-markers]") {
  const auto [x, y, cx, cy] =
      GENERATE(std::array{100, 30, 110, 50}, std::array{280, 30, 270, 50},
               std::array{277, 29, 266, 51}, std::array{200, 40, 200, 40});
  FakeWorld world;
  world.anchors.at(0) = {.screen = {.x = static_cast<std::int16_t>(x),
                                    .y = static_cast<std::int16_t>(y)},
                         .depth = 77};
  b::AbilityTimingCue cue{.anchor = {}, .frames = 5};
  b::ShowAbilityTimingCue(world, true, cue, 30);
  CHECK(cue.anchor.screen ==
        MarkerScreenPoint{.x = static_cast<std::int16_t>(cx),
                          .y = static_cast<std::int16_t>(cy)});
  CHECK(cue.anchor.depth == 77);
  CHECK(cue.frames == 35);
}

TEST_CASE("Ability timing cue wraps sixteen-bit lifts", "[actor-markers]") {
  FakeWorld world;
  world.anchors.at(0) = {.screen = {.x = 0, .y = -32768}, .depth = 1};
  b::AbilityTimingCue cue{.anchor = {}, .frames = 32767};
  b::ShowAbilityTimingCue(world, true, cue, 1);
  CHECK(cue.anchor.screen == MarkerScreenPoint{.x = 32728, .y = -32688});
  CHECK(cue.frames == -32768);
}

TEST_CASE("Ability timing cue waits for display and an anchor",
          "[actor-markers]") {
  FakeWorld world;
  world.anchors.at(0) = {.screen = {.x = 9, .y = 99}, .depth = 1};
  b::AbilityTimingCue cue{.anchor = {.screen = {.x = 1, .y = 2}, .depth = 3},
                          .frames = 4};
  b::ShowAbilityTimingCue(world, false, cue, 30);
  CHECK(cue.frames == 4);
  CHECK(cue.anchor.screen == MarkerScreenPoint{.x = 1, .y = 2});
  world.anchors.fill(std::nullopt);
  b::ShowAbilityTimingCue(world, true, cue, 30);
  CHECK(cue.frames == 4);
}

namespace {
constexpr std::size_t kLiveOrderingWords = 0x2088 / sizeof(u_long);
constexpr std::size_t kLiveSceneHead = 4;
constexpr std::size_t kLivePrimitiveWords = 0x20000 / sizeof(u_long);
constexpr u_long kLiveEnd = 0xFFFFFF;

struct LiveMarkerFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  Actors_ActorRecord* saved_player = Actors_actorRecords[0];
  Actors_ActorRecord* saved_enemy = Actors_actorRecords[2];
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  void* saved_ordering = Render_legacyOrderingTables[0];
  int saved_framebuffer = Render_frameBuf;
  int saved_sides = Battle_actorMarkerSides;
  decltype(Battle_actorMarkersHidden) saved_hidden = Battle_actorMarkersHidden;
  decltype(Battle_abilityTimingCueFrames) saved_frames =
      Battle_abilityTimingCueFrames;
  SVECTOR saved_cue = Battle_abilityTimingCuePosition;
  std::array<char, 40> saved_markers{};
  decltype(VS::Input::settings) saved_settings = VS::Input::settings;
  Actors_ActorRecord player{};
  Actors_ActorRecord enemy{};
  std::vector<u_long> storage = std::vector<u_long>(kLivePrimitiveWords, 0);
  std::array<u_long, kLiveOrderingWords> ordering{};

  LiveMarkerFixture() {
    std::ranges::copy(Battle_actorMarkers, saved_markers.begin());
    std::ranges::fill(Battle_actorMarkers, char{0});
    Actors_actorRecords[0] = &player;
    Actors_actorRecords[2] = &enemy;
    player.instance.position.vx = -256;
    player.instance.position.vz = enemy.instance.position.vz = 512;
    player.targetPointHeight = enemy.targetPointHeight = 64;
    ordering.fill(kLiveEnd);
    Render_frameBuf = 0;
    Render_legacyOrderingTables[0] = ordering.data();
    Render_legacyPrimitiveBuffers[0] = storage.data();
    Render_frameScratch = {};
    for (int axis = 0; axis < 3; ++axis) {
      Render_frameScratch.viewMatrix.m[axis][axis] = 4096;
    }
    Render_frameScratch.viewMatrix.t[2] = 1024;
    Render_frameScratch.primitiveCursor = storage.data();
    Render_frameScratch.orderingTable = ordering.data() + kLiveSceneHead;
    Core_Gte_SetGeomOffset(160, 120);
    Core_Gte_SetGeomScreen(256);
    Battle_actorMarkerSides = 0;
    Battle_actorMarkersHidden = 0;
  }

  ~LiveMarkerFixture() {
    std::ranges::copy(saved_markers, std::begin(Battle_actorMarkers));
    Actors_actorRecords[0] = saved_player;
    Actors_actorRecords[2] = saved_enemy;
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_legacyOrderingTables[0] = saved_ordering;
    Render_frameBuf = saved_framebuffer;
    Battle_actorMarkerSides = saved_sides;
    Battle_actorMarkersHidden = saved_hidden;
    Battle_abilityTimingCueFrames = saved_frames;
    Battle_abilityTimingCuePosition = saved_cue;
    VS::Input::settings = saved_settings;
    Core_Gte_Reset();
  }
  LiveMarkerFixture(const LiveMarkerFixture&) = delete;
  LiveMarkerFixture& operator=(const LiveMarkerFixture&) = delete;
  LiveMarkerFixture(LiveMarkerFixture&&) = delete;
  LiveMarkerFixture& operator=(LiveMarkerFixture&&) = delete;

  [[nodiscard]] std::size_t UsedWords() const {
    return static_cast<const u_long*>(Render_frameScratch.primitiveCursor) -
           storage.data();
  }
};

SVECTOR LiveAnchor(int actor) {
  SVECTOR projected{};
  REQUIRE(VS::Actors::ProjectLiveActorTaggedBone(actor, 251, &projected, 0) ==
          0);
  return projected;
}
}  // namespace

TEST_CASE("Live actor marker links its dot then its frame at the depth slot",
          "[actor-markers-live]") {
  LiveMarkerFixture fixture;
  const SVECTOR projected = LiveAnchor(2);
  REQUIRE(projected.vz == 384);
  VS::Battle::LiveDrawActorMarker(2, 1);
  REQUIRE(fixture.UsedWords() == 16);
  const u_long* sprite = fixture.storage.data();
  const u_long* frame = sprite + 6;
  CHECK(nextPrim(&fixture.ordering[kLiveSceneHead + 368]) == frame);
  CHECK(nextPrim(frame) == sprite);
  CHECK((sprite[0] & kLiveEnd) == kLiveEnd);
  const std::uint32_t xy =
      b::PackMarkerPoint({.x = projected.vx, .y = projected.vy}) + 0xFFF0FFFEU;
  CHECK(sprite[3] == xy);
  CHECK(sprite[4] == 0x373E125AU);
  CHECK(frame[1] == 0x2C808080U);
  CHECK(Battle_actorMarkerSides == 0);
}

TEST_CASE("Live off-screen arrow links in front of the scene head",
          "[actor-markers-live]") {
  LiveMarkerFixture fixture;
  fixture.storage[1] = 0x00123456;
  SVECTOR position{.vx = 100, .vy = 50, .vz = 0, .pad = 0};
  VS::Battle::LiveDrawOffscreenActorArrow(&position, 4);
  REQUIRE(fixture.UsedWords() == 10);
  const u_long* arrow = fixture.storage.data();
  CHECK(nextPrim(&fixture.ordering[0]) == arrow);
  CHECK(arrow[1] == 0x2D123456U);
  CHECK(arrow[2] == b::PackMarkerPoint({.x = 92, .y = 42}));
  CHECK(arrow[3] == 0x37FD3030U);
  VS::Battle::LiveDrawOffscreenActorArrow(&position, 3);
  CHECK(fixture.UsedWords() == 10);
}

TEST_CASE("Live marker sweep honours the hidden switch",
          "[actor-markers-live]") {
  LiveMarkerFixture fixture;
  Battle_actorMarkers[2] = 6;
  Battle_actorMarkersHidden = 1;
  VS::Battle::LiveDrawActorMarkers();
  CHECK(fixture.UsedWords() == 0);
  Battle_actorMarkersHidden = 0;
  VS::Battle::LiveDrawActorMarkers();
  CHECK(fixture.UsedWords() == 10);
  CHECK(nextPrim(&fixture.ordering[kLiveSceneHead + 368]) ==
        fixture.storage.data());
}

TEST_CASE("Live ability timing cue follows the display setting",
          "[actor-markers-live]") {
  LiveMarkerFixture fixture;
  Battle_abilityTimingCueFrames = 7;
  Battle_abilityTimingCuePosition = {.vx = 1, .vy = 2, .vz = 3, .pad = 99};
  VS::Input::settings.abilityTimingDisplay = 0;
  VS::Battle::LiveShowAbilityTimingCue(30);
  CHECK(Battle_abilityTimingCueFrames == 7);
  VS::Input::settings.abilityTimingDisplay = 1;
  const SVECTOR player = LiveAnchor(0);
  REQUIRE(player.vy >= 40);
  VS::Battle::LiveShowAbilityTimingCue(30);
  CHECK(Battle_abilityTimingCueFrames == 37);
  CHECK(Battle_abilityTimingCuePosition.vx == player.vx);
  CHECK(Battle_abilityTimingCuePosition.vy == player.vy);
  CHECK(Battle_abilityTimingCuePosition.vz == player.vz);
  CHECK(Battle_abilityTimingCuePosition.pad == 99);
}
