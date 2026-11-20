#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Gte.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/MinimapScene.hpp"
#include "Field/Room.h"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/Map.hpp"
#include "Menu/MapDrawing.hpp"
#include "Menu/MapDrawingLive.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/StateFlags.hpp"

namespace map = VS::Menu::map;

namespace {
using Entry = std::array<std::uint8_t, 4>;

struct Vertex {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
};

// A room's count-prefixed vertex, triangle, quad, edge, dim edge and marker
// tables.
struct RoomTables {
  std::vector<Vertex> vertices{};
  std::vector<Entry> triangles{};
  std::vector<Entry> quads{};
  std::vector<Entry> edges{};
  std::vector<Entry> dim_edges{};
  std::vector<Entry> markers{};
};

void AppendWord(std::vector<std::byte>& bytes, std::uint32_t word) {
  const auto at = bytes.size();
  bytes.resize(at + sizeof(word));
  std::memcpy(bytes.data() + at, &word, sizeof(word));
}

void AppendEntries(std::vector<std::byte>& bytes,
                   const std::vector<Entry>& entries) {
  AppendWord(bytes, static_cast<std::uint32_t>(entries.size()));
  for (const Entry& entry : entries) {
    AppendWord(bytes, entry[0] | (entry[1] << 8U) | (entry[2] << 16U) |
                          (static_cast<std::uint32_t>(entry[3]) << 24U));
  }
}

std::vector<std::byte> RoomBytes(const RoomTables& tables) {
  std::vector<std::byte> bytes;
  AppendWord(bytes, static_cast<std::uint32_t>(tables.vertices.size()));
  for (const Vertex& vertex : tables.vertices) {
    AppendWord(
        bytes,
        static_cast<std::uint16_t>(vertex.x) |
            (static_cast<std::uint32_t>(static_cast<std::uint16_t>(vertex.y))
             << 16U));
    AppendWord(bytes, static_cast<std::uint16_t>(vertex.z));
  }
  for (const auto* entries : {&tables.triangles, &tables.quads, &tables.edges,
                              &tables.dim_edges, &tables.markers}) {
    AppendEntries(bytes, *entries);
  }
  return bytes;
}

std::array<std::int32_t, map::kAshleyBrightnessSteps> Brightness() {
  std::array<std::int32_t, map::kAshleyBrightnessSteps> steps{};
  for (std::size_t step = 0; step < steps.size(); ++step) {
    steps[step] = 0x100 + static_cast<std::int32_t>(step);
  }
  return steps;
}

struct DoorStateCall {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t id = 0;
  friend bool operator==(const DoorStateCall&, const DoorStateCall&) = default;
};

struct FeatureCall {
  std::int32_t icon = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  friend bool operator==(const FeatureCall&, const FeatureCall&) = default;
};

struct LabelCall {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t kind = 0;
  std::int32_t id = 0;
  friend bool operator==(const LabelCall&, const LabelCall&) = default;
};

struct AshleyCall {
  std::int32_t position = 0;
  std::int32_t brightness = 0;
  friend bool operator==(const AshleyCall&, const AshleyCall&) = default;
};

template <typename Packet>
struct Placed {
  Packet packet;
  std::int32_t depth = 0;
  friend bool operator==(const Placed&, const Placed&) = default;
};

// Projects (x, y, z) to screen (x, y) at depth z, with interpolation x.
class FakeRoomHost final : public map::RoomDrawHost {
 public:
  map::Projection Project(const map::RoomVertex& vertex) override {
    projected.push_back(vertex.z);
    return {.screen = {.x = vertex.x, .y = vertex.y},
            .depth = vertex.z,
            .interpolation = vertex.x};
  }
  map::Rgb DepthCue(const map::Rgb& color, std::int32_t) override {
    return color;
  }
  void AddTriangle(const map::RoomTriangle& triangle,
                   std::int32_t depth) override {
    triangles.push_back({triangle, depth});
  }
  void AddQuad(const map::RoomQuad& quad, std::int32_t depth) override {
    quads.push_back({quad, depth});
  }
  void AddEdge(const map::RoomEdge& edge, std::int32_t depth) override {
    edges.push_back({edge, depth});
  }
  void AddTile(const map::RoomTile& tile, std::int32_t depth) override {
    tiles.push_back({tile, depth});
  }
  bool ShouldDrawDoorState(std::int32_t state_id, std::int32_t,
                           std::int32_t) override {
    door_queries.push_back(state_id);
    return show_door_state;
  }
  void DrawDoorState(std::int32_t x, std::int32_t y,
                     std::int32_t state_id) override {
    door_states.push_back({x, y, state_id});
  }
  void DrawFeature(std::int32_t icon, std::int32_t x, std::int32_t y) override {
    features.push_back({icon, x, y});
  }
  void QueueLabel(std::int32_t x, std::int32_t y, std::int32_t kind,
                  std::int32_t label_id) override {
    labels.push_back({x, y, kind, label_id});
  }
  void DrawAshley(std::int32_t position, std::int32_t brightness) override {
    ashley.push_back({position, brightness});
  }

  bool show_door_state = false;
  std::vector<std::int32_t> projected;
  std::vector<Placed<map::RoomTriangle>> triangles{};
  std::vector<Placed<map::RoomQuad>> quads{};
  std::vector<Placed<map::RoomEdge>> edges{};
  std::vector<Placed<map::RoomTile>> tiles;
  std::vector<std::int32_t> door_queries;
  std::vector<DoorStateCall> door_states;
  std::vector<FeatureCall> features;
  std::vector<LabelCall> labels;
  std::vector<AshleyCall> ashley;
};

struct RoomRun {
  map::RoomWorkspace workspace{};
  std::array<std::int32_t, map::kAshleyBrightnessSteps> brightness =
      Brightness();
  map::RoomContext context{.room_index = 3,
                           .current_room = 3,
                           .scene_room = 3,
                           .entered_door = -1,
                           .frame_counter = 0,
                           .zone_id = 9,
                           .map_id = 10,
                           .brightness = {}};
  FakeRoomHost host;

  map::RoomDrawResult Draw(const RoomTables& tables) {
    context.brightness = brightness;
    const auto bytes = RoomBytes(tables);
    return map::DrawRoom(host, bytes, context, workspace);
  }
};

// A marker at vertex 0 (10, 20) at depth 0x100.
RoomTables MarkerRoom(Entry marker) {
  return {.vertices = {{10, 20, 0x100}}, .markers = {marker}};
}

constexpr map::Rgb kFeatureCross{0xA0, 0xA0, 0};

std::vector<map::RoomTile> Tiles(const FakeRoomHost& host) {
  std::vector<map::RoomTile> tiles;
  for (const auto& placed : host.tiles) {
    CHECK(placed.depth == 0xFF);
    tiles.push_back(placed.packet);
  }
  return tiles;
}

std::vector<map::RoomTile> Cross(const map::Rgb& color) {
  return {{.position = {9, 20}, .width = 3, .height = 1, .color = color},
          {.position = {10, 19}, .width = 1, .height = 3, .color = color}};
}
}  // namespace

TEST_CASE("A room stops projecting at the first vertex outside the depths",
          "[map-drawing]") {
  const std::int16_t outside =
      GENERATE(std::int16_t{0x3F}, std::int16_t{0x7FF}, std::int16_t{-1});
  RoomRun run;
  run.workspace[3] = {.x = 77, .y = 77, .depth = 77, .interpolation = 77};
  const RoomTables tables{
      .vertices = {{1, 2, 0x40}, {3, 4, 0x7FE}, {5, 6, outside}, {7, 8, 0x50}},
      .triangles = {{0, 1, 3, 0}}};
  CHECK(run.Draw(tables) == map::RoomDrawResult::kOutOfDepthRange);
  CHECK(run.host.projected == std::vector<std::int32_t>{0x40, 0x7FE, outside});
  CHECK(run.workspace[1] == map::ProjectedVertex{3, 4, 0x7FE, 3});
  CHECK(run.workspace[2] == map::ProjectedVertex{5, 6, outside, 5});
  CHECK(run.workspace[3] == map::ProjectedVertex{77, 77, 77, 77});
  CHECK(run.host.triangles.empty());
}

TEST_CASE("A malformed room projects and draws nothing", "[map-drawing]") {
  RoomRun run;
  auto bytes = RoomBytes({.vertices = {{1, 2, 0x100}}});
  SECTION("a vertex count past the bytes") { bytes[0] = std::byte{2}; }
  SECTION("a table count past the bytes") { bytes[12] = std::byte{5}; }
  SECTION("a missing marker table") { bytes.resize(bytes.size() - 4); }
  SECTION("more vertices than the workspace holds") {
    std::vector<Vertex> many(257, Vertex{1, 2, 0x100});
    bytes = RoomBytes({.vertices = many});
  }
  SECTION("missing brightness steps") {
    run.brightness = {};
    run.context.brightness = std::span(run.brightness).first(15);
    CHECK(map::DrawRoom(run.host, bytes, run.context, run.workspace) ==
          map::RoomDrawResult::kMalformed);
    CHECK(run.host.projected.empty());
    return;
  }
  run.context.brightness = run.brightness;
  CHECK(map::DrawRoom(run.host, bytes, run.context, run.workspace) ==
        map::RoomDrawResult::kMalformed);
  CHECK(run.host.projected.empty());
}

TEST_CASE("A negative table count draws none of its entries", "[map-drawing]") {
  RoomRun run;
  auto bytes = RoomBytes({.vertices = {{1, 2, 0x100}}});
  bytes[12] = std::byte{0xFF};
  bytes[13] = std::byte{0xFF};
  bytes[14] = std::byte{0xFF};
  bytes[15] = std::byte{0xFF};
  run.context.brightness = run.brightness;
  CHECK(map::DrawRoom(run.host, bytes, run.context, run.workspace) ==
        map::RoomDrawResult::kDrawn);
  CHECK(run.host.triangles.empty());
}

TEST_CASE(
    "Room fills sit at their farthest corner and swap the quad's last two",
    "[map-drawing]") {
  const bool current = GENERATE(true, false);
  RoomRun run;
  run.context.current_room = current ? 3 : 4;
  const RoomTables tables{.vertices = {{10, 20, 0x100},
                                       {11, 21, 0x130},
                                       {12, 22, 0x120},
                                       {13, 23, 0x110}},
                          .triangles = {{3, 2, 0, 0}},
                          .quads = {{0, 1, 2, 3}}};
  REQUIRE(run.Draw(tables) == map::RoomDrawResult::kDrawn);
  const map::Rgb fill =
      current ? map::Rgb{0, 0x80, 0x40} : map::Rgb{0, 0x40, 0x80};
  REQUIRE(run.host.triangles.size() == 1);
  CHECK(run.host.triangles[0] ==
        Placed<map::RoomTriangle>{{.corners = {{{13, 23}, {12, 22}, {10, 20}}},
                                   .colors = {fill, fill, fill}},
                                  0x120});
  REQUIRE(run.host.quads.size() == 1);
  CHECK(run.host.quads[0] ==
        Placed<map::RoomQuad>{
            {.corners = {{{10, 20}, {11, 21}, {13, 23}, {12, 22}}},
             .colors = {fill, fill, fill, fill}},
            0x130});
}

TEST_CASE("A room outline glows around a brighter core in the current room",
          "[map-drawing]") {
  const bool current = GENERATE(true, false);
  RoomRun run;
  run.context.current_room = current ? 3 : 4;
  const RoomTables tables{.vertices = {{10, 20, 0x100}, {30, 40, 0x110}},
                          .edges = {{0, 1, 0, 0}}};
  REQUIRE(run.Draw(tables) == map::RoomDrawResult::kDrawn);
  constexpr map::Rgb kGlow{0, 0x80, 0xFF};
  const map::Rgb core = current ? map::Rgb{0, 0xFF, 0x80} : kGlow;
  const std::vector<std::array<std::int16_t, 2>> offsets{
      {-1, 0}, {0, -1}, {1, 0}, {0, 1}, {0, 0}};
  REQUIRE(run.host.edges.size() == offsets.size());
  for (std::size_t line = 0; line < offsets.size(); ++line) {
    const auto [dx, dy] = offsets[line];
    const bool glow = line + 1 < offsets.size();
    const map::Rgb color = glow ? kGlow : core;
    CHECK(run.host.edges[line] ==
          Placed<map::RoomEdge>{{.from = {static_cast<std::int16_t>(10 + dx),
                                          static_cast<std::int16_t>(20 + dy)},
                                 .to = {static_cast<std::int16_t>(30 + dx),
                                        static_cast<std::int16_t>(40 + dy)},
                                 .from_color = color,
                                 .to_color = color,
                                 .semi_transparent = glow},
                                0x10F});
  }
}

TEST_CASE("Dim edges sit a step in front of their nearer end",
          "[map-drawing]") {
  const bool current = GENERATE(true, false);
  RoomRun run;
  run.context.current_room = current ? 3 : 4;
  const RoomTables tables{.vertices = {{10, 20, 0x100}, {30, 40, 0x101}},
                          .dim_edges = {{0, 1, 0, 0}, {1, 0, 0, 0}}};
  REQUIRE(run.Draw(tables) == map::RoomDrawResult::kDrawn);
  const map::Rgb color =
      current ? map::Rgb{0, 0xFF, 0x80} : map::Rgb{0, 0x40, 0x80};
  REQUIRE(run.host.edges.size() == 2);
  CHECK(run.host.edges[0] == Placed<map::RoomEdge>{{.from = {10, 20},
                                                    .to = {30, 40},
                                                    .from_color = color,
                                                    .to_color = color,
                                                    .semi_transparent = true},
                                                   0x100});
  CHECK(run.host.edges[1].depth == 0x100);
}

TEST_CASE("A shown door state replaces the marker's cross", "[map-drawing]") {
  RoomRun run;
  run.host.show_door_state = true;
  REQUIRE(run.Draw(MarkerRoom({0, 7, 0, 5})) == map::RoomDrawResult::kDrawn);
  CHECK(run.host.door_queries == std::vector<std::int32_t>{5});
  CHECK(run.host.door_states == std::vector<DoorStateCall>{{10, 6, 5}});
  CHECK(run.host.tiles.empty());
}

TEST_CASE("A door state outside the current room is asked but not drawn",
          "[map-drawing]") {
  RoomRun run;
  run.host.show_door_state = true;
  run.context.current_room = 4;
  REQUIRE(run.Draw(MarkerRoom({0, 7, 0, 5})) == map::RoomDrawResult::kDrawn);
  CHECK(run.host.door_queries == std::vector<std::int32_t>{5});
  CHECK(run.host.door_states.empty());
  CHECK(run.host.tiles.empty());
}

TEST_CASE("A hidden door state leaves the plain cross", "[map-drawing]") {
  RoomRun run;
  REQUIRE(run.Draw(MarkerRoom({0, 7, 0, 5})) == map::RoomDrawResult::kDrawn);
  CHECK(run.host.door_states.empty());
  CHECK(Tiles(run.host) == Cross({0xC0, 0xC0, 0xC0}));
}

TEST_CASE("A door marker queues its area label and blinks when entered",
          "[map-drawing]") {
  RoomRun run;
  const bool entered = GENERATE(true, false);
  const std::int32_t frames = GENERATE(0, 0x10);
  run.context.entered_door = entered ? 0 : 1;
  run.context.frame_counter = frames;
  REQUIRE(run.Draw(MarkerRoom({0, 7, 4, 0})) == map::RoomDrawResult::kDrawn);
  CHECK(run.host.door_queries.empty());
  CHECK(run.host.features == std::vector<FeatureCall>{{0, 10, 6}});
  CHECK(run.host.labels == std::vector<LabelCall>{{10, 20, 1, 7}});
  if (!entered) {
    CHECK(Tiles(run.host) == Cross({0, 0xC0, 0xC0}));
  } else if (frames == 0) {
    CHECK(run.host.tiles.empty());
  } else {
    CHECK(Tiles(run.host) == Cross({0xFF, 0, 0x80}));
  }
}

TEST_CASE("An entered door blinks only in the scene's own room",
          "[map-drawing]") {
  RoomRun run;
  run.context.entered_door = 0;
  run.context.scene_room = 5;
  REQUIRE(run.Draw(MarkerRoom({0, 7, 0, 0})) == map::RoomDrawResult::kDrawn);
  CHECK(Tiles(run.host) == Cross({0xC0, 0xC0, 0xC0}));
}

TEST_CASE("Marker kinds cycle every 32 frames in the original's order",
          "[map-drawing]") {
  const std::int32_t period = GENERATE(0, 1, 2, 3, 4, 5, 6);
  RoomRun run;
  run.context.frame_counter = (period * 32) + 7;
  REQUIRE(run.Draw(MarkerRoom({0, 7, 0x1A, 0})) == map::RoomDrawResult::kDrawn);
  CHECK(Tiles(run.host) == Cross(kFeatureCross));
  // Ashley, save, workshop, save, chest, chest.
  constexpr std::array<std::int32_t, 6> kIcons{-1, 2, 1, 2, 3, 3};
  const std::int32_t icon = kIcons.at(static_cast<std::size_t>(period % 6));
  if (icon < 0) {
    CHECK(run.host.features.empty());
    CHECK(run.host.ashley == std::vector<AshleyCall>{{0x000C0002, 0x107}});
  } else {
    CHECK(run.host.ashley.empty());
    CHECK(run.host.features == std::vector<FeatureCall>{{icon, 10, 6}});
  }
}

TEST_CASE("Outside the current room only Ashley's kind is offered",
          "[map-drawing]") {
  RoomRun run;
  run.context.current_room = 4;
  run.context.frame_counter = 32 * 3;
  REQUIRE(run.Draw(MarkerRoom({0, 7, 0x1A, 0})) == map::RoomDrawResult::kDrawn);
  CHECK(run.host.ashley == std::vector<AshleyCall>{{0x000C0002, 0x100}});
  CHECK(Tiles(run.host) == Cross(kFeatureCross));
}

TEST_CASE("Ashley's kind needs the scene's room", "[map-drawing]") {
  RoomRun run;
  run.context.scene_room = 5;
  REQUIRE(run.Draw(MarkerRoom({0, 7, 1, 0})) == map::RoomDrawResult::kDrawn);
  CHECK(run.host.ashley.empty());
  CHECK(run.host.tiles.empty());
}

TEST_CASE("A negative frame counter picks no kind but keeps the cross",
          "[map-drawing]") {
  RoomRun run;
  run.context.frame_counter = -32;
  REQUIRE(run.Draw(MarkerRoom({0, 7, 0x1A, 0})) == map::RoomDrawResult::kDrawn);
  CHECK(run.host.ashley.empty());
  CHECK(run.host.features.empty());
  CHECK(Tiles(run.host) == Cross(kFeatureCross));
}

namespace {
class FakeVisitedHost final : public map::VisitedRoomsHost {
 public:
  map::ScreenOffset GetScreenOffset() override {
    log.emplace_back("get");
    return {.x = 160, .y = 112};
  }
  void SetScreenOffset(const map::ScreenOffset& offset) override {
    log.push_back("set " + std::to_string(offset.x) + " " +
                  std::to_string(offset.y));
  }
  void ClearLabels() override { log.emplace_back("clear"); }
  void DrawRoom(std::size_t index, std::int32_t zone_id,
                std::int32_t map_id) override {
    log.push_back("room " + std::to_string(index) + " " +
                  std::to_string(zone_id) + " " + std::to_string(map_id));
  }
  void DrawLabels() override { log.emplace_back("labels"); }

  std::vector<std::string> log;
};
}  // namespace

TEST_CASE("Visited rooms draw centred and restore the projection offset",
          "[map-drawing]") {
  FakeVisitedHost host;
  const std::array<map::MapRoom, 3> rooms{
      {{.visited = 1, .zone_id = 4, .map_id = 5},
       {.visited = 0, .zone_id = 6, .map_id = 7},
       {.visited = -1, .zone_id = 8, .map_id = 9}}};
  map::DrawVisitedRooms(host, -10, 20, rooms);
  CHECK(host.log == std::vector<std::string>{"get", "set 134 156", "clear",
                                             "room 0 4 5", "room 2 8 9",
                                             "labels", "set 160 112"});
}

namespace {
class FakeLabelHost final : public map::LabelDrainHost {
 public:
  void DrawDoorLabel(std::int32_t x, std::int32_t y,
                     std::int32_t door_id) override {
    log.push_back({x, y, 0, door_id});
  }
  void DrawAreaLabel(std::int32_t x, std::int32_t y,
                     std::int32_t area_id) override {
    log.push_back({x, y, 1, area_id});
  }
  std::vector<LabelCall> log;
};

std::array<map::MapLabel, map::kMapLabelCapacity> QueuedLabels() {
  std::array<map::MapLabel, map::kMapLabelCapacity> labels{};
  for (std::size_t index = 0; index < labels.size(); ++index) {
    const auto value = static_cast<std::int16_t>(index);
    labels[index] = {.x = value,
                     .y = static_cast<std::int16_t>(value + 10),
                     .kind = static_cast<std::int16_t>(index % 3 == 0 ? 0 : 2),
                     .id = static_cast<std::int16_t>(value + 20)};
  }
  return labels;
}
}  // namespace

TEST_CASE("Queued door labels draw before the area labels", "[map-drawing]") {
  FakeLabelHost host;
  const auto labels = QueuedLabels();
  CHECK(map::DrawLabels(host, 0, false, labels, 4));
  CHECK(host.log ==
        std::vector<LabelCall>{
            {0, 10, 0, 20}, {3, 13, 0, 23}, {1, 11, 1, 21}, {2, 12, 1, 22}});
}

TEST_CASE("Hidden legends draw no labels", "[map-drawing]") {
  FakeLabelHost host;
  const auto labels = QueuedLabels();
  const bool by_state = GENERATE(true, false);
  CHECK(map::DrawLabels(host, by_state ? 5 : 4, !by_state, labels, 4));
  CHECK(host.log.empty());
}

TEST_CASE("A label count past the queue draws the queue and is refused",
          "[map-drawing]") {
  FakeLabelHost host;
  const auto labels = QueuedLabels();
  CHECK_FALSE(map::DrawLabels(host, 0, false, labels, 9));
  CHECK(host.log.size() == map::kMapLabelCapacity);
  host.log.clear();
  CHECK(map::DrawLabels(host, 0, false, labels, -1));
  CHECK(host.log.empty());
}

namespace {
class FakeBackdropHost final : public map::BackdropHost {
 public:
  void AddBackdropLine(const map::BackdropLine& line) override {
    lines.push_back(line);
  }
  void AddBackdropShadow(
      const std::array<map::ScreenPoint, map::kQuadCorners>& corners) override {
    shadows.push_back(corners);
  }
  std::vector<map::BackdropLine> lines;
  std::vector<std::array<map::ScreenPoint, map::kQuadCorners>> shadows;
};
}  // namespace

TEST_CASE("A backdrop narrows each row and slants its shadow",
          "[map-drawing]") {
  FakeBackdropHost host;
  CHECK(map::DrawControlBackdrop(host, 10, 20, 30, 4));
  CHECK(host.lines == std::vector<map::BackdropLine>{{{10, 20}, {39, 20}},
                                                     {{10, 21}, {38, 21}},
                                                     {{10, 22}, {37, 22}}});
  using Corners = std::array<map::ScreenPoint, map::kQuadCorners>;
  CHECK(host.shadows == std::vector<Corners>{
                            Corners{{{12, 22}, {41, 22}, {12, 25}, {38, 25}}}});
}

TEST_CASE("A one-row backdrop draws only its shadow", "[map-drawing]") {
  FakeBackdropHost host;
  const std::int32_t height = GENERATE(1, 0, -5);
  CHECK(map::DrawControlBackdrop(host, 10, 20, 30, height));
  CHECK(host.lines.empty());
  CHECK(host.shadows.size() == 1);
}

TEST_CASE("A backdrop taller than the row limit is refused", "[map-drawing]") {
  FakeBackdropHost host;
  CHECK(map::DrawControlBackdrop(host, 0, 0, 30, 257));
  CHECK(host.lines.size() == 256);
  FakeBackdropHost refused;
  CHECK_FALSE(map::DrawControlBackdrop(refused, 0, 0, 30, 258));
  CHECK(refused.lines.empty());
  CHECK(refused.shadows.empty());
}

namespace {
class FakeControlsHost final : public map::ControlsHost {
 public:
  void ShiftTextBox(std::int32_t delta) override {
    log.push_back("shift " + std::to_string(delta));
  }
  std::optional<bool> IsJammed() override {
    log.emplace_back("jammed?");
    return jammed;
  }
  void DrawButton(VS::Menu::Framework::ButtonIcon button, std::int32_t x,
                  std::int32_t y) override {
    log.push_back("button " + std::to_string(static_cast<int>(button)) + " " +
                  std::to_string(x) + " " + std::to_string(y));
  }
  void DrawCaption(map::ControlCaption caption, std::int32_t packed_xy,
                   std::uint32_t color) override {
    log.push_back("caption " + std::to_string(static_cast<int>(caption)) + " " +
                  std::to_string(packed_xy) + " " +
                  (color == map::kCaptionBright ? "bright" : "dim"));
  }
  void DrawBackdrop(std::int32_t x, std::int32_t y, std::int32_t width,
                    std::int32_t height) override {
    log.push_back("backdrop " + std::to_string(x) + " " + std::to_string(y) +
                  " " + std::to_string(width) + " " + std::to_string(height));
  }
  void AddTexturePage() override { log.emplace_back("tpage"); }
  void DrawCompass(std::int32_t x, std::int32_t y,
                   std::int32_t angle) override {
    log.push_back("compass " + std::to_string(x) + " " + std::to_string(y) +
                  " " + std::to_string(angle));
  }

  std::optional<bool> jammed = false;
  std::vector<std::string> log;
};

std::string Caption(int caption, int x, int y, bool bright) {
  return "caption " + std::to_string(caption) + " " +
         std::to_string(static_cast<std::int32_t>(
             (static_cast<std::uint32_t>(y) << 16U) |
             (static_cast<std::uint32_t>(x) & 0xFFFFU))) +
         (bright ? " bright" : " dim");
}
}  // namespace

TEST_CASE("The legend slides in and out with text box 0", "[map-drawing]") {
  FakeControlsHost host;
  const map::ControlsFrame hidden{.hide_ui = true};
  map::ControlsState state{.animation_state = 0, .x_offset = 99};
  CHECK(map::DrawControls(host, state, hidden));
  CHECK(state.animation_state == 1);
  CHECK(state.x_offset == -128);
  CHECK(map::DrawControls(host, state, hidden));
  CHECK(state.x_offset == -112);
  state.x_offset = 0;
  CHECK(map::DrawControls(host, state, hidden));
  CHECK(state.x_offset == 0);
  CHECK(host.log == std::vector<std::string>{"shift -8"});
}

TEST_CASE("The legend slides out and then rests", "[map-drawing]") {
  FakeControlsHost host;
  const map::ControlsFrame hidden{.hide_ui = true};
  map::ControlsState state{.animation_state = 2, .x_offset = 0};
  CHECK(map::DrawControls(host, state, hidden));
  CHECK(state.x_offset == -16);
  state.x_offset = -128;
  CHECK(map::DrawControls(host, state, hidden));
  CHECK(state.x_offset == -128);
  state.animation_state = 3;
  state.x_offset = -50;
  CHECK(map::DrawControls(host, state, hidden));
  CHECK(state.x_offset == -50);
  CHECK(host.log == std::vector<std::string>{"shift 8"});
}

TEST_CASE("A jammed map shows only the jamming caption", "[map-drawing]") {
  FakeControlsHost host;
  host.jammed = true;
  map::ControlsState state{.animation_state = 3, .x_offset = -100};
  CHECK(map::DrawControls(host, state, {}));
  CHECK(host.log ==
        std::vector<std::string>{"jammed?", Caption(5, -72, 18, true)});
}

TEST_CASE("A refused jamming state draws nothing", "[map-drawing]") {
  FakeControlsHost host;
  host.jammed = std::nullopt;
  map::ControlsState state{.animation_state = 3, .x_offset = 0};
  CHECK_FALSE(map::DrawControls(host, state, {}));
  CHECK(host.log == std::vector<std::string>{"jammed?"});
}

TEST_CASE("The commands state shades the legend's captions", "[map-drawing]") {
  FakeControlsHost host;
  map::ControlsState state{.animation_state = 3, .x_offset = -4};
  const std::int16_t commands =
      GENERATE(std::int16_t{1}, std::int16_t{6}, std::int16_t{-1});
  CHECK(map::DrawControls(
      host, state,
      {.hide_ui = false, .commands_state = commands, .compass_angle = 77}));
  std::vector<std::string> expected{"jammed?",        "button 4 4 15",
                                    "button 5 60 15", "button 6 4 33",
                                    "button 7 4 52",  "button 2 4 70"};
  if (commands == 1) {
    for (const auto& caption :
         {Caption(0, 24, 18, true), Caption(1, 80, 18, true),
          Caption(2, 24, 36, false), Caption(3, 24, 54, false),
          Caption(4, 24, 72, true)}) {
      expected.push_back(caption);
    }
  }
  for (const auto* backdrop :
       {"backdrop 12 18 114 12", "backdrop 12 36 96 12", "backdrop 12 54 78 12",
        "backdrop 12 72 60 12", "tpage", "compass 62 184 77"}) {
    expected.emplace_back(backdrop);
  }
  CHECK(host.log == expected);
}

TEST_CASE("Compass corners rotate and truncate toward zero", "[map-drawing]") {
  CHECK(map::ComputeCompassCorners(10, 20, 4, 2, 0, 4096) ==
        map::CompassCorners{{{8, 19}, {12, 19}, {8, 21}, {12, 21}}});
  CHECK(map::ComputeCompassCorners(10, 20, 3, 0, 0, 4096) ==
        map::CompassCorners{{{9, 20}, {11, 20}, {9, 20}, {11, 20}}});
  // A quarter turn swaps the axes.
  CHECK(map::ComputeCompassCorners(0, 0, 4, 2, 4096, 0) ==
        map::CompassCorners{{{1, -2}, {1, 2}, {-1, -2}, {-1, 2}}});
  // The products wrap at 32 bits and the quotients truncate.
  CHECK(map::ComputeCompassCorners(0, 0, 0x80000, 0, 0, 4096) ==
        map::CompassCorners{
            {{-262144, 0}, {-262144, 0}, {-262144, 0}, {-262144, 0}}});
}

namespace {
class FakeCompassHost final : public map::CompassHost {
 public:
  std::int32_t Sine(std::int32_t angle) override {
    angles.push_back(angle);
    return 0;
  }
  std::int32_t Cosine(std::int32_t angle) override {
    angles.push_back(-angle);
    return 4096;
  }
  void AddCompassQuad(const map::CompassQuad& quad) override {
    quads.push_back(quad);
  }
  std::vector<std::int32_t> angles;
  std::vector<map::CompassQuad> quads{};
};
}  // namespace

TEST_CASE("The compass draws its ticks letters and needle", "[map-drawing]") {
  FakeCompassHost host;
  const auto needle = map::DrawCompass(host, 62, 184, 100, 30);
  std::vector<std::int32_t> expected;
  for (const std::int32_t angle :
       {100, 356, 612, 868, 1124, 1380, 1636, 1892, 612, 582, 70}) {
    expected.push_back(angle);
    expected.push_back(-angle);
  }
  CHECK(host.angles == expected);
  REQUIRE(host.quads.size() == 17);
  const auto& drawn = host.quads.back();
  CHECK(drawn.quad.clut == ((222U << 6U) | (992U >> 4U)));
  CHECK(drawn.semi_transparent);
  CHECK(needle ==
        map::CompassCorners{{{48, 169}, {62, 169}, {48, 185}, {62, 185}}});
  CHECK(drawn.quad.corners[0] == map::ScreenPoint{48, 169});
}

TEST_CASE("The compass ticks mark every quarter as major", "[map-drawing]") {
  FakeCompassHost host;
  static_cast<void>(map::DrawCompass(host, 62, 184, 100, 30));
  REQUIRE(host.quads.size() == 17);
  for (std::size_t tick = 0; tick < 8; ++tick) {
    const auto& quad = host.quads[tick];
    CHECK(quad.semi_transparent);
    CHECK(quad.slot_bytes == 28);
    CHECK(quad.quad.texture[0].v == (tick % 4 == 0 ? 128 : 132));
    CHECK(quad.quad.clut == ((222U << 6U) | (960U >> 4U)));
    CHECK(quad.quad.texture_page == ((1U << 5U) | (448U >> 6U) | 0x10U));
  }
}

TEST_CASE("The compass letters use their own palettes", "[map-drawing]") {
  FakeCompassHost host;
  static_cast<void>(map::DrawCompass(host, 62, 184, 100, 30));
  REQUIRE(host.quads.size() == 17);
  for (std::size_t letter = 0; letter < 4; ++letter) {
    const auto& glyph = host.quads[8 + (letter * 2)];
    const auto& glow = host.quads[9 + (letter * 2)];
    CHECK_FALSE(glyph.semi_transparent);
    CHECK(glow.semi_transparent);
    CHECK(glyph.slot_bytes == 24);
    CHECK(glyph.quad.clut == (letter == 0 ? (221U << 6U) | (960U >> 4U)
                                          : (220U << 6U) | (1008U >> 4U)));
    CHECK(glow.quad.clut == (letter == 0 ? (222U << 6U) | (976U >> 4U)
                                         : (222U << 6U) | (960U >> 4U)));
  }
}

// ------------------------------------------------------------------ live --

namespace {
template <typename T>
class Restore {
 public:
  explicit Restore(T& value) : value_(value), saved_(value) {}
  Restore(const Restore&) = delete;
  Restore& operator=(const Restore&) = delete;
  Restore(Restore&&) = delete;
  Restore& operator=(Restore&&) = delete;
  ~Restore() { value_ = saved_; }

 private:
  T& value_;
  T saved_;
};

constexpr std::size_t kOrderingWords = 0x2088 / sizeof(u_long);
constexpr std::size_t kSceneHead = 4;
constexpr std::size_t kChainLimit = 0x10000;
constexpr std::uint32_t kTagLengthShift = 24;

// A packet linked into the frame: the ordering-table entry it hangs off, its
// offset in words from the primitive buffer's start and the words after its
// tag.
struct ChainedPacket {
  std::size_t entry = 0;
  std::size_t offset = 0;
  std::vector<u_long> body{};

  friend bool operator==(const ChainedPacket&, const ChainedPacket&) = default;
};

// A frame buffer, ordering table, GTE view and the map globals the drawing
// reads, restored after. The compact font tables are emptied so the legend's
// captions queue only their draw-mode packets, whatever BATTLE.PRG image an
// earlier test left loaded.
class LiveMap {
 public:
  std::vector<u_long> ordering = std::vector<u_long>(kOrderingWords);
  std::vector<u_long> primitives =
      std::vector<u_long>(0x20000 / sizeof(u_long));

  LiveMap() {
    std::memcpy(&saved_flags_, &VS::State::stateFlags, sizeof(saved_flags_));
    std::memcpy(saved_bounds_.data(), map::compassBounds,
                sizeof(saved_bounds_));
    std::memcpy(saved_trims_.data(), VS::Render::compactGlyphLeftTrim,
                sizeof(saved_trims_));
    std::memcpy(saved_widths_.data(), VS::Render::compactGlyphWidths,
                sizeof(saved_widths_));
    std::memcpy(saved_indices_.data(), VS::Render::compactGlyphIndices,
                sizeof(saved_indices_));
    std::memset(VS::Render::compactGlyphLeftTrim, 0, sizeof(saved_trims_));
    std::memset(VS::Render::compactGlyphWidths, 0, sizeof(saved_widths_));
    std::memset(VS::Render::compactGlyphIndices, 0, sizeof(saved_indices_));
    if (VS::Menu::TextBoxState* box = VS::Menu::LiveTextBox(0);
        box != nullptr) {
      saved_box_ = *box;
    }
    Core_Gte_Reset();
    Core_Gte_SetGeomScreen(256);
    Core_Gte_SetGeomOffset(160, 112);
    const VECTOR translation{0, 0, 512, 0};
    Core_Gte_SetTransVector(&translation);
    VS::Render::frameBuf = 0;
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::legacyPrimitiveBuffers[0] = primitives.data();
    Reset();
    map::currentRoomIndex = 0;
    map::isCurrentScene = 0;
    map::menuFramesCounter = 0;
    map::mapLabelCount = 0;
    map::menuCommandsState = 0;
    map::triggerHideUi = 0;
    map::currentScene = 0;
    Field_enteredDoorId = -1;
    map::ResetMapDrawingLive();
  }
  LiveMap(const LiveMap&) = delete;
  LiveMap& operator=(const LiveMap&) = delete;
  LiveMap(LiveMap&&) = delete;
  LiveMap& operator=(LiveMap&&) = delete;
  ~LiveMap() {
    std::memcpy(&VS::State::stateFlags, &saved_flags_, sizeof(saved_flags_));
    std::memcpy(map::compassBounds, saved_bounds_.data(),
                sizeof(saved_bounds_));
    std::memcpy(VS::Render::compactGlyphLeftTrim, saved_trims_.data(),
                sizeof(saved_trims_));
    std::memcpy(VS::Render::compactGlyphWidths, saved_widths_.data(),
                sizeof(saved_widths_));
    std::memcpy(VS::Render::compactGlyphIndices, saved_indices_.data(),
                sizeof(saved_indices_));
    if (VS::Menu::TextBoxState* box = VS::Menu::LiveTextBox(0);
        box != nullptr) {
      *box = saved_box_;
    }
    map::ResetMapDrawingLive();
    Core_Gte_Reset();
  }

  // Empties the frame: the cursor at the buffer's start and every ordering
  // entry ending the chain.
  void Reset() {
    std::ranges::fill(primitives, 0);
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    VS::Render::FrameScratchpad().primitive_cursor = primitives.data();
    VS::Render::FrameScratchpad().ordering_table = ordering.data() + kSceneHead;
    VS::Render::FrameScratchpad().overlay_ordering_table = ordering.data();
  }

  [[nodiscard]] std::size_t UsedWords() const {
    return static_cast<std::size_t>(
        static_cast<u_long*>(VS::Render::FrameScratchpad().primitive_cursor) -
        primitives.data());
  }

  // The packets in drawing order: walks the ordering table from its last
  // entry, recording each packet's entry, buffer offset and tagged words.
  // Empty when the chain leaves the frame.
  [[nodiscard]] std::vector<ChainedPacket> Chain() const {
    std::vector<ChainedPacket> chain;
    const auto* table = ordering.data();
    const auto* buffer = primitives.data();
    const auto* end = buffer + UsedWords();
    auto* at = const_cast<u_long*>(&ordering.back());
    std::size_t entry = ordering.size() - 1;
    for (std::size_t steps = 0; steps < kChainLimit; ++steps) {
      if (at >= table && at < table + ordering.size()) {
        entry = static_cast<std::size_t>(at - table);
      } else if (at >= buffer && at < end) {
        const std::uint32_t length = *at >> kTagLengthShift;
        chain.push_back({.entry = entry,
                         .offset = static_cast<std::size_t>(at - buffer),
                         .body = {at + 1, at + 1 + length}});
      } else {
        return {};
      }
      if (isendprim(at)) {
        return at == table ? chain : std::vector<ChainedPacket>{};
      }
      at = static_cast<u_long*>(nextPrim(at));
    }
    return {};
  }

 private:
  Restore<VS::Render::FrameScratch> scratch_{VS::Render::FrameScratchpad()};
  Restore<int> frame_{VS::Render::frameBuf};
  Restore<void*> ordering_table_{VS::Render::legacyOrderingTables[0]};
  Restore<void*> primitive_buffer_{VS::Render::legacyPrimitiveBuffers[0]};
  Restore<int> room_{map::currentRoomIndex};
  Restore<int> scene_room_{map::isCurrentScene};
  Restore<int> frames_{map::menuFramesCounter};
  Restore<int> label_count_{map::mapLabelCount};
  Restore<std::int16_t> commands_{map::menuCommandsState};
  Restore<std::int16_t> animation_{map::uiAnimationState};
  Restore<int> hide_{map::triggerHideUi};
  Restore<int> scene_{map::currentScene};
  Restore<int> geom_x_{map::geomOffsetX};
  Restore<int> geom_y_{map::geomOffsetY};
  Restore<SVECTOR> angles_{map::cameraAngles};
  Restore<std::int16_t> door_{Field_enteredDoorId};
  Restore<VS::Field::MapScene*> minimap_{Field_roomData.minimapScene};
  Restore<u_int> minimap_length_{Field_roomData.header.minimapSceneLength};
  VS::State::StateFlags saved_flags_{};
  std::array<map::CompassPoint, 4> saved_bounds_{};
  std::array<u_char, sizeof(VS::Render::compactGlyphLeftTrim)> saved_trims_{};
  std::array<u_char, sizeof(VS::Render::compactGlyphWidths)> saved_widths_{};
  std::array<u_char, sizeof(VS::Render::compactGlyphIndices)> saved_indices_{};
  VS::Menu::TextBoxState saved_box_{};
};

// A one-room minimap scene whose room has a triangle, a quad, an outline, a
// dim edge and a plain marker; the room data follows the record, 12 bytes on.
std::vector<u_long> OneRoomScene(std::int32_t visited) {
  const auto room =
      RoomBytes({.vertices = {{0, 0, 0}, {16, 0, 0}, {0, 16, 0}, {16, 16, 0}},
                 .triangles = {{0, 1, 2, 0}},
                 .quads = {{0, 1, 2, 3}},
                 .edges = {{0, 1, 0, 0}},
                 .dim_edges = {{1, 3, 0, 0}},
                 .markers = {{2, 0, 0, 0}}});
  std::vector<u_long> scene{1, static_cast<u_long>(visited), 12, 0x00020001};
  const std::size_t head = scene.size();
  scene.resize(head + (room.size() / sizeof(u_long)));
  std::memcpy(scene.data() + head, room.data(), room.size());
  return scene;
}

void LoadMinimap(std::vector<u_long>& scene) {
  Field_roomData.minimapScene =
      reinterpret_cast<VS::Field::MapScene*>(scene.data());
  Field_roomData.header.minimapSceneLength =
      static_cast<u_int>(scene.size() * sizeof(u_long));
}

// The four compass corners the drawing leaves in _compassBounds.
std::array<std::int32_t, 8> CompassBounds() {
  std::array<std::int32_t, 8> bounds{};
  static_assert(sizeof(bounds) == sizeof(map::compassBounds));
  std::memcpy(bounds.data(), map::compassBounds, sizeof(bounds));
  return bounds;
}

// Packets recorded from the original drawing of OneRoomScene's room with the
// LiveMap view: the room current, another room current and the room drawn
// through the visited-room pass with the geometry offset (-20, 8).
std::vector<ChainedPacket> CurrentRoomChain() {
  return {
      {.entry = 132,
       .offset = 7,
       .body = {0x38408000, 0x007000A0, 0x00408000, 0x007000A8, 0x00408000,
                0x007800A8, 0x00408000, 0x007800A0}},
      {.entry = 132,
       .offset = 0,
       .body = {0x30408000, 0x007000A0, 0x00408000, 0x007000A8, 0x00408000,
                0x007800A0}},
      {.entry = 131,
       .offset = 50,
       .body = {0x60C0C0C0, 0x007700A0, 0x00030001}},
      {.entry = 131,
       .offset = 46,
       .body = {0x60C0C0C0, 0x0078009F, 0x00010003}},
      {.entry = 131,
       .offset = 41,
       .body = {0x5280FF00, 0x007000A8, 0x0080FF00, 0x007800A8}},
      {.entry = 131,
       .offset = 36,
       .body = {0x5080FF00, 0x007000A0, 0x0080FF00, 0x007000A8}},
      {.entry = 131,
       .offset = 31,
       .body = {0x52FF8000, 0x007100A0, 0x00FF8000, 0x007100A8}},
      {.entry = 131,
       .offset = 26,
       .body = {0x52FF8000, 0x007000A1, 0x00FF8000, 0x007000A9}},
      {.entry = 131,
       .offset = 21,
       .body = {0x52FF8000, 0x006F00A0, 0x00FF8000, 0x006F00A8}},
      {.entry = 131,
       .offset = 16,
       .body = {0x52FF8000, 0x0070009F, 0x00FF8000, 0x007000A7}},
  };
}

std::vector<ChainedPacket> OtherRoomChain() {
  return {
      {.entry = 132,
       .offset = 7,
       .body = {0x38804000, 0x007000A0, 0x00804000, 0x007000A8, 0x00804000,
                0x007800A8, 0x00804000, 0x007800A0}},
      {.entry = 132,
       .offset = 0,
       .body = {0x30804000, 0x007000A0, 0x00804000, 0x007000A8, 0x00804000,
                0x007800A0}},
      {.entry = 131,
       .offset = 41,
       .body = {0x52804000, 0x007000A8, 0x00804000, 0x007800A8}},
      {.entry = 131,
       .offset = 36,
       .body = {0x50FF8000, 0x007000A0, 0x00FF8000, 0x007000A8}},
      {.entry = 131,
       .offset = 31,
       .body = {0x52FF8000, 0x007100A0, 0x00FF8000, 0x007100A8}},
      {.entry = 131,
       .offset = 26,
       .body = {0x52FF8000, 0x007000A1, 0x00FF8000, 0x007000A9}},
      {.entry = 131,
       .offset = 21,
       .body = {0x52FF8000, 0x006F00A0, 0x00FF8000, 0x006F00A8}},
      {.entry = 131,
       .offset = 16,
       .body = {0x52FF8000, 0x0070009F, 0x00FF8000, 0x007000A7}},
  };
}

std::vector<ChainedPacket> VisitedRoomChain() {
  return {
      {.entry = 132,
       .offset = 7,
       .body = {0x38408000, 0x0090007C, 0x00408000, 0x00900084, 0x00408000,
                0x00980084, 0x00408000, 0x0098007C}},
      {.entry = 132,
       .offset = 0,
       .body = {0x30408000, 0x0090007C, 0x00408000, 0x00900084, 0x00408000,
                0x0098007C}},
      {.entry = 131,
       .offset = 50,
       .body = {0x60C0C0C0, 0x0097007C, 0x00030001}},
      {.entry = 131,
       .offset = 46,
       .body = {0x60C0C0C0, 0x0098007B, 0x00010003}},
      {.entry = 131,
       .offset = 41,
       .body = {0x5280FF00, 0x00900084, 0x0080FF00, 0x00980084}},
      {.entry = 131,
       .offset = 36,
       .body = {0x5080FF00, 0x0090007C, 0x0080FF00, 0x00900084}},
      {.entry = 131,
       .offset = 31,
       .body = {0x52FF8000, 0x0091007C, 0x00FF8000, 0x00910084}},
      {.entry = 131,
       .offset = 26,
       .body = {0x52FF8000, 0x0090007D, 0x00FF8000, 0x00900085}},
      {.entry = 131,
       .offset = 21,
       .body = {0x52FF8000, 0x008F007C, 0x00FF8000, 0x008F0084}},
      {.entry = 131,
       .offset = 16,
       .body = {0x52FF8000, 0x0090007B, 0x00FF8000, 0x00900083}},
  };
}

// The backdrop at (-20, 10), 40 wide: its shadow alone, then 11 rows and the
// shadow, all on entry 11.
std::vector<ChainedPacket> OneRowBackdropChain() {
  return {
      {.entry = 11,
       .offset = 0,
       .body = {0x28000000, 0x000CFFEE, 0x000C0015, 0x000CFFEE, 0x000C0015}},
  };
}

std::vector<ChainedPacket> TwelveRowBackdropChain() {
  return {
      {.entry = 11,
       .offset = 55,
       .body = {0x28000000, 0x000CFFEE, 0x000C0015, 0x0017FFEE, 0x0017000A}},
      {.entry = 11,
       .offset = 50,
       .body = {0x50203840, 0x0014FFEC, 0x00081010, 0x00140009}},
      {.entry = 11,
       .offset = 45,
       .body = {0x50203840, 0x0013FFEC, 0x00081010, 0x0013000A}},
      {.entry = 11,
       .offset = 40,
       .body = {0x50203840, 0x0012FFEC, 0x00081010, 0x0012000B}},
      {.entry = 11,
       .offset = 35,
       .body = {0x50203840, 0x0011FFEC, 0x00081010, 0x0011000C}},
      {.entry = 11,
       .offset = 30,
       .body = {0x50203840, 0x0010FFEC, 0x00081010, 0x0010000D}},
      {.entry = 11,
       .offset = 25,
       .body = {0x50203840, 0x000FFFEC, 0x00081010, 0x000F000E}},
      {.entry = 11,
       .offset = 20,
       .body = {0x50203840, 0x000EFFEC, 0x00081010, 0x000E000F}},
      {.entry = 11,
       .offset = 15,
       .body = {0x50203840, 0x000DFFEC, 0x00081010, 0x000D0010}},
      {.entry = 11,
       .offset = 10,
       .body = {0x50203840, 0x000CFFEC, 0x00081010, 0x000C0011}},
      {.entry = 11,
       .offset = 5,
       .body = {0x50203840, 0x000BFFEC, 0x00081010, 0x000B0012}},
      {.entry = 11,
       .offset = 0,
       .body = {0x50203840, 0x000AFFEC, 0x00081010, 0x000A0013}},
  };
}

// The compass at (62, 184) facing angle 0 with the camera's roll 0: eight
// ticks on entry 11, then the four letters with their glows and the needle on
// entry 10.
std::vector<ChainedPacket> CompassChain() {
  return {
      {.entry = 11,
       .offset = 70,
       .body = {0x2F000000, 0x00B20052, 0x37BC8402, 0x00C1002B, 0x0037842D,
                0x00AF0051, 0x00008702, 0x00BE002A, 0x0000872D}},
      {.entry = 11,
       .offset = 60,
       .body = {0x2F000000, 0x00AA004E, 0x37BC8402, 0x00C80030, 0x0037842D,
                0x00A8004C, 0x00008702, 0x00C6002E, 0x0000872D}},
      {.entry = 11,
       .offset = 50,
       .body = {0x2F000000, 0x00A50047, 0x37BC8402, 0x00CC0038, 0x0037842D,
                0x00A40044, 0x00008702, 0x00CB0035, 0x0000872D}},
      {.entry = 11,
       .offset = 40,
       .body = {0x2F000000, 0x00A3003F, 0x37BC8002, 0x00CD003F, 0x0037802D,
                0x00A3003D, 0x00008302, 0x00CD003D, 0x0000832D}},
      {.entry = 11,
       .offset = 30,
       .body = {0x2F000000, 0x00A40038, 0x37BC8402, 0x00CB0047, 0x0037842D,
                0x00A50035, 0x00008702, 0x00CC0044, 0x0000872D}},
      {.entry = 11,
       .offset = 20,
       .body = {0x2F000000, 0x00A80030, 0x37BC8402, 0x00C6004E, 0x0037842D,
                0x00AA002E, 0x00008702, 0x00C8004C, 0x0000872D}},
      {.entry = 11,
       .offset = 10,
       .body = {0x2F000000, 0x00AF002B, 0x37BC8402, 0x00BE0052, 0x0037842D,
                0x00B2002A, 0x00008702, 0x00C10051, 0x0000872D}},
      {.entry = 11,
       .offset = 0,
       .body = {0x2F000000, 0x00B70029, 0x37BC8002, 0x00B70053, 0x0037802D,
                0x00B90029, 0x00008302, 0x00B90053, 0x0000832D}},
      {.entry = 10,
       .offset = 160,
       .body = {0x2F000000, 0x00A60037, 0x37BE8814, 0x00A60045, 0x00178823,
                0x00B60037, 0x00009914, 0x00B60045, 0x00009923}},
      {.entry = 10,
       .offset = 150,
       .body = {0x2F000000, 0x00CD0038, 0x37BC8901, 0x00CD0045, 0x0037890E,
                0x00DA0038, 0x00009601, 0x00DA0045, 0x0000960E}},
      {.entry = 10,
       .offset = 140,
       .body = {0x2D000000, 0x00D0003B, 0x373F8038, 0x00D00042, 0x0017803F,
                0x00D7003B, 0x00008738, 0x00D70042, 0x0000873F}},
      {.entry = 10,
       .offset = 130,
       .body = {0x2F000000, 0x00B2001D, 0x37BC8901, 0x00B2002A, 0x0037890E,
                0x00BF001D, 0x00009601, 0x00BF002A, 0x0000960E}},
      {.entry = 10,
       .offset = 120,
       .body = {0x2D000000, 0x00B50020, 0x373F8048, 0x00B50027, 0x0017804F,
                0x00BC0020, 0x00008748, 0x00BC0027, 0x0000874F}},
      {.entry = 10,
       .offset = 110,
       .body = {0x2F000000, 0x00B20053, 0x37BC8901, 0x00B20060, 0x0037890E,
                0x00BF0053, 0x00009601, 0x00BF0060, 0x0000960E}},
      {.entry = 10,
       .offset = 100,
       .body = {0x2D000000, 0x00B50056, 0x373F8040, 0x00B5005D, 0x00178047,
                0x00BC0056, 0x00008740, 0x00BC005D, 0x00008747}},
      {.entry = 10,
       .offset = 90,
       .body = {0x2F000000, 0x00970038, 0x37BD8901, 0x00970045, 0x0037890E,
                0x00A40038, 0x00009601, 0x00A40045, 0x0000960E}},
      {.entry = 10,
       .offset = 80,
       .body = {0x2D000000, 0x009A003B, 0x377C8030, 0x009A0042, 0x00178037,
                0x00A1003B, 0x00008730, 0x00A10042, 0x00008737}},
  };
}
}  // namespace

TEST_CASE("The live current room links glowing polygons and its marker",
          "[map-drawing-live]") {
  LiveMap frame;
  auto scene = OneRoomScene(1);
  LoadMinimap(scene);
  CHECK(map::LiveDrawRoom(0, scene.data() + 4, 1, 2) == 1);
  CHECK(frame.UsedWords() == 54);
  const auto chain = frame.Chain();
  CHECK(chain == CurrentRoomChain());
  // The quad (POLY_G4, 0x38) and triangle (POLY_G3, 0x30) sit on entry 132,
  // one deeper than the outline lines (0x50/0x52) and marker tiles (0x60).
  REQUIRE(chain.size() == 10);
  CHECK(chain[0].body[0] >> kTagLengthShift == 0x38);
  CHECK(chain[1].body[0] >> kTagLengthShift == 0x30);
  CHECK(chain[2].body[0] >> kTagLengthShift == 0x60);
}

TEST_CASE("The live room outside the current one draws no marker",
          "[map-drawing-live]") {
  LiveMap frame;
  auto scene = OneRoomScene(1);
  LoadMinimap(scene);
  map::currentRoomIndex = 1;
  CHECK(map::LiveDrawRoom(0, scene.data() + 4, 1, 2) == 1);
  CHECK(frame.UsedWords() == 46);
  CHECK(frame.Chain() == OtherRoomChain());
}

TEST_CASE("The live room refuses data outside the loaded scene",
          "[map-drawing-live]") {
  LiveMap frame;
  auto scene = OneRoomScene(1);
  LoadMinimap(scene);
  std::vector<u_long> stray = scene;
  CHECK(map::LiveDrawRoom(0, stray.data() + 4, 1, 2) == 0);
  CHECK(map::LiveDrawRoom(0, nullptr, 1, 2) == 0);
  // The marker table reaches past the scene's end.
  Field_roomData.header.minimapSceneLength -= 4;
  CHECK(map::LiveDrawRoom(0, scene.data() + 4, 1, 2) == 0);
  CHECK(frame.UsedWords() == 0);
}

TEST_CASE("Live visited rooms draw centred and clear the label queue",
          "[map-drawing-live]") {
  LiveMap frame;
  const std::int32_t visited = GENERATE(0, 1);
  auto scene = OneRoomScene(visited);
  LoadMinimap(scene);
  map::geomOffsetX = -20;
  map::geomOffsetY = 8;
  map::mapLabelCount = 5;
  const VS::Render::ScreenOffset before =
      VS::Render::GetLiveProjectionScreenOffset();
  map::LiveDrawVisitedRooms(
      reinterpret_cast<VS::Field::MapScene*>(scene.data()));
  CHECK(map::mapLabelCount == 0);
  const VS::Render::ScreenOffset after =
      VS::Render::GetLiveProjectionScreenOffset();
  CHECK(after.x == before.x);
  CHECK(after.y == before.y);
  if (visited == 0) {
    CHECK(frame.UsedWords() == 0);
  } else {
    // The room projects about (0x90 - 20, 0x88 - 8) = (124, 128).
    CHECK(frame.UsedWords() == 54);
    CHECK(frame.Chain() == VisitedRoomChain());
  }
}

TEST_CASE("Live visited rooms refuse a scene the menu did not load",
          "[map-drawing-live]") {
  LiveMap frame;
  auto scene = OneRoomScene(1);
  Field_roomData.minimapScene = nullptr;
  map::mapLabelCount = 5;
  map::LiveDrawVisitedRooms(
      reinterpret_cast<VS::Field::MapScene*>(scene.data()));
  CHECK(map::mapLabelCount == 5);
  LoadMinimap(scene);
  scene[0] = 100;
  map::LiveDrawVisitedRooms(
      reinterpret_cast<VS::Field::MapScene*>(scene.data()));
  CHECK(map::mapLabelCount == 5);
  CHECK(frame.UsedWords() == 0);
}

TEST_CASE("The live backdrop links its rows and shadow into entry 11",
          "[map-drawing-live]") {
  LiveMap frame;
  const std::int32_t height = GENERATE(1, 12);
  map::LiveDrawControlBackdrop(-20, 10, 40, height);
  if (height == 1) {
    CHECK(frame.UsedWords() == 6);
    CHECK(frame.Chain() == OneRowBackdropChain());
  } else {
    CHECK(frame.UsedWords() == 61);
    CHECK(frame.Chain() == TwelveRowBackdropChain());
  }
}

TEST_CASE("The live backdrop refuses more rows than it draws",
          "[map-drawing-live]") {
  LiveMap frame;
  map::LiveDrawControlBackdrop(0, 0, 40, 300);
  CHECK(frame.UsedWords() == 0);
}

TEST_CASE("The live compass draws its quads and leaves the needle bounds",
          "[map-drawing-live]") {
  LiveMap frame;
  map::LiveDrawCompass(62, 184, 0);
  CHECK(frame.UsedWords() == 170);
  CHECK(frame.Chain() == CompassChain());
  CHECK(CompassBounds() ==
        std::array<std::int32_t, 8>{55, 166, 69, 166, 55, 182, 69, 182});
}

TEST_CASE("The live compass needle turns with the angle and camera roll",
          "[map-drawing-live]") {
  using Bounds = std::array<std::int32_t, 8>;
  const auto [angle, roll, expected] = GENERATE(
      std::tuple{0, 300, Bounds{48, 171, 60, 165, 56, 185, 68, 179}},
      std::tuple{700, 0, Bounds{74, 169, 82, 181, 60, 177, 68, 189}},
      std::tuple{700, 300, Bounds{67, 165, 79, 174, 57, 178, 69, 187}},
      std::tuple{-1234, 0, Bounds{47, 196, 42, 183, 62, 191, 57, 178}},
      std::tuple{-1234, 300, Bounds{55, 202, 44, 191, 66, 191, 55, 180}});
  LiveMap frame;
  map::cameraAngles.vz = static_cast<std::int16_t>(roll);
  map::LiveDrawCompass(62, 184, angle);
  const auto chain = frame.Chain();
  REQUIRE(chain.size() == 17);
  CHECK(CompassBounds() == expected);
  // The needle, the last quad drawn on entry 10, uses the stored bounds.
  const auto& needle = chain[8];
  CHECK(needle.entry == 10);
  CHECK(needle.body[1] == ((static_cast<u_long>(expected[1]) << 16U) |
                           static_cast<u_long>(expected[0])));
}

TEST_CASE("The live compass corners store the rotated rectangle",
          "[map-drawing-live]") {
  using Bounds = std::array<std::int32_t, 8>;
  const auto [angle, expected] =
      GENERATE(std::tuple{0, Bounds{23, 32, 37, 32, 23, 48, 37, 48}},
               std::tuple{512, Bounds{30, 29, 41, 40, 19, 40, 30, 51}},
               std::tuple{1000, Bounds{38, 33, 38, 47, 22, 33, 22, 47}},
               std::tuple{-3000, Bounds{39, 34, 37, 48, 23, 32, 21, 46}});
  LiveMap frame;
  std::memset(map::compassBounds, 0, sizeof(map::compassBounds));
  map::LiveComputeCompassCorners(30, 40, 15, 17, angle);
  CHECK(CompassBounds() == expected);
  CHECK(frame.UsedWords() == 0);
}

namespace {
// The five button sprites (0x64) are the first packets on entry 10, each
// after its texture page word, at their places shifted by the legend's slide.
void CheckLegendButtons(const std::vector<ChainedPacket>& chain,
                        std::int32_t slide) {
  constexpr std::array<std::array<std::int32_t, 2>, 5> kButtons{
      {{-120, 15}, {-64, 15}, {-120, 33}, {-120, 52}, {-120, 70}}};
  REQUIRE(chain.size() >= kButtons.size());
  for (std::size_t button = 0; button < kButtons.size(); ++button) {
    const auto& sprite = chain[chain.size() - 1 - button];
    CHECK(sprite.entry == 10);
    REQUIRE(sprite.body.size() == 5);
    CHECK(sprite.body[1] == 0x64808080);
    const std::int32_t x = kButtons[button][0] + slide;
    CHECK(sprite.body[2] == ((static_cast<u_long>(kButtons[button][1]) << 16U) |
                             (static_cast<u_long>(x) & 0xFFFFU)));
  }
}
}  // namespace

TEST_CASE("The live legend slides in 16 pixels a frame with text box 0",
          "[map-drawing-live]") {
  LiveMap frame;
  map::menuCommandsState = static_cast<std::int16_t>(GENERATE(0, 1, 5));
  VS::State::stateFlags.map_paling[0] = 0;
  VS::Menu::TextBoxState* box = VS::Menu::LiveTextBox(0);
  REQUIRE(box != nullptr);
  box->y = 0;
  box->y_indent = 0;
  map::uiAnimationState = 0;
  for (std::int32_t step = 0; step < 3; ++step) {
    frame.Reset();
    map::LiveDrawControls();
    CHECK(map::uiAnimationState == 1);
    CHECK(box->y == -8 * step);
    CHECK(box->y_indent == -8 * step);
    CHECK(frame.UsedWords() == 457);
    const auto chain = frame.Chain();
    REQUIRE(chain.size() == 76);
    CheckLegendButtons(chain, 16 * step);
    // The legend's texture page is drawn between the backdrops and the
    // compass ticks on entry 11.
    CHECK(chain[8].body == std::vector<u_long>{0xE1000303, 0});
  }
}

TEST_CASE("The live legend refuses a scene outside the paling flags",
          "[map-drawing-live]") {
  LiveMap frame;
  map::uiAnimationState = 3;
  map::currentScene = GENERATE(32, -1);
  map::LiveDrawControls();
  CHECK(frame.UsedWords() == 0);
}

TEST_CASE("The live legend shows the jamming caption alone",
          "[map-drawing-live]") {
  LiveMap frame;
  map::uiAnimationState = 3;
  map::currentScene = 4;
  VS::State::stateFlags.map_paling[4] = 1;
  map::LiveDrawControls();
  const std::size_t jammed = frame.UsedWords();
  frame.Reset();
  VS::State::stateFlags.map_paling[4] = 0;
  map::LiveDrawControls();
  CHECK(jammed > 0);
  CHECK(jammed < frame.UsedWords());
}

TEST_CASE("Resetting the live drawing restarts the legend's slide",
          "[map-drawing-live]") {
  LiveMap frame;
  map::triggerHideUi = 1;
  VS::Menu::TextBoxState* box = VS::Menu::LiveTextBox(0);
  REQUIRE(box != nullptr);
  box->y = 50;
  map::uiAnimationState = 1;
  for (int step = 0; step < 9; ++step) map::LiveDrawControls();
  CHECK(box->y == -14);
  map::ResetMapDrawingLive();
  map::LiveDrawControls();
  CHECK(box->y == -22);
}

TEST_CASE("The live label drain draws nothing past a hidden legend",
          "[map-drawing-live]") {
  LiveMap frame;
  map::menuCommandsState = 5;
  map::mapLabelCount = 9;
  map::LiveDrawLabels();
  CHECK(frame.UsedWords() == 0);
}
