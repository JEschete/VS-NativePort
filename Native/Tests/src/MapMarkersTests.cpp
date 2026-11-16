#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Menu/MapMarkers.hpp"
#include "Menu/MapMarkersLive.hpp"
extern "C" {
#include "Audio/Seq/Channel.h"
#include "Menu/Map.h"
#include "State/StateFlags.h"
}

namespace {
namespace map = VS::Menu::map;
using map::LabelLine;
using map::LabelText;
using map::LabelTextTable;
using map::MapLabel;
using map::MarkerQuad;
using map::MarkerRect;
using map::ScreenPoint;

struct SigilQuery {
  std::int32_t zone_id = 0;
  std::int32_t map_id = 0;
  std::uint32_t kind = 0;

  friend bool operator==(const SigilQuery&, const SigilQuery&) = default;
};

class FakeMapMarkerHost final : public map::MapMarkerHost {
 public:
  void AddMarkerQuad(const MarkerQuad& quad) override { quads.push_back(quad); }
  void AddLabelLine(const LabelLine& line) override { lines.push_back(line); }
  void AddAshleyMarker(const map::AshleyMarkerSprite& sprite) override {
    sprites.push_back(sprite);
  }
  void DrawLabelText(const LabelText& text) override { texts.push_back(text); }
  bool IsSigilDoorUnlocked(std::int32_t zone_id, std::int32_t map_id,
                           std::uint32_t kind) override {
    sigil_queries.push_back({zone_id, map_id, kind});
    return sigil_unlocked;
  }

  std::vector<MarkerQuad> quads;
  std::vector<LabelLine> lines;
  std::vector<map::AshleyMarkerSprite> sprites;
  std::vector<LabelText> texts;
  std::vector<SigilQuery> sigil_queries;
  bool sigil_unlocked = false;
};

// A self-indexed name table: entry n holds offset n.
std::array<std::uint16_t, 40> NameTable() {
  std::array<std::uint16_t, 40> names{};
  for (std::size_t entry = 0; entry < names.size(); ++entry) {
    names.at(entry) = static_cast<std::uint16_t>(entry);
  }
  return names;
}

TEST_CASE("Ashley's marker packs the icon texture and sprite colour",
          "[map-markers]") {
  FakeMapMarkerHost host;
  map::DrawAshleyMarker(host, 2, 0x00400030, 0x101010,
                        map::SpriteList::kCaller);
  map::DrawAshleyMarker(host, -4, 7, 0, map::SpriteList::kOverlay);
  REQUIRE(host.sprites.size() == 2);
  CHECK(host.sprites[0] ==
        map::AshleyMarkerSprite{.brightness_transparency = 128,
                                .packed_position = 0x00400030,
                                .packed_size = 0x00100010,
                                .texture_word = 0x37FD8060U,
                                .color_word = 0x64101010U,
                                .list = map::SpriteList::kCaller});
  CHECK(host.sprites[1].texture_word == 0x37FD8000U);
  CHECK(host.sprites[1].list == map::SpriteList::kOverlay);
}

TEST_CASE("A feature marker centres its icon and rounds half an odd width",
          "[map-markers]") {
  FakeMapMarkerHost host;
  const std::array<MarkerRect, 1> icons{{{.x = 10, .y = 20, .w = 5, .h = 99}}};
  REQUIRE(map::DrawFeatureMarker(host, icons, 0, 100, 50));
  // Half of 5 rounds to 2, the height is always 8 and the RECT's own
  // height is ignored.
  CHECK(host.quads ==
        std::vector<MarkerQuad>{
            {.corners = {{{98, 50}, {103, 50}, {98, 58}, {103, 58}}},
             .texture = {{{10, 20}, {15, 20}, {10, 28}, {15, 28}}},
             .clut = 0x37F5,
             .texture_page = 0x0C}});
}

TEST_CASE("A feature marker wraps a negative width and its texture bytes",
          "[map-markers]") {
  FakeMapMarkerHost host;
  const std::array<MarkerRect, 1> icons{{{.x = 250, .y = 252, .w = -3}}};
  REQUIRE(map::DrawFeatureMarker(host, icons, 0, 0, 0x7FFC));
  REQUIRE(host.quads.size() == 1);
  // Half of -3 rounds toward zero to -1 and the bottom wraps.
  CHECK(host.quads[0].corners ==
        std::array<ScreenPoint, 4>{
            {{1, 0x7FFC}, {-2, 0x7FFC}, {1, -0x7FFC}, {-2, -0x7FFC}}});
  CHECK(host.quads[0].texture[3] == map::TexturePoint{.u = 247, .v = 4});
}

TEST_CASE("A feature marker outside the icon table is refused",
          "[map-markers]") {
  FakeMapMarkerHost host;
  const std::array<MarkerRect, 4> icons{};
  CHECK_FALSE(map::DrawFeatureMarker(host, icons, 4, 0, 0));
  CHECK_FALSE(map::DrawFeatureMarker(host, icons, -1, 0, 0));
  CHECK(host.quads.empty());
}

TEST_CASE("Queueing a label stores each value's low halfword",
          "[map-markers]") {
  std::array<MapLabel, map::kMapLabelCapacity> labels{};
  std::int32_t count = 6;
  REQUIRE(map::QueueLabel(labels, count, 0x12345, -1, 1, 0x8000));
  CHECK(count == 7);
  CHECK(labels[6] == MapLabel{.x = 0x2345, .y = -1, .kind = 1, .id = -0x8000});
}

TEST_CASE("Queueing labels stops at capacity and refuses a negative count",
          "[map-markers]") {
  std::array<MapLabel, map::kMapLabelCapacity> labels{};
  std::int32_t count = 7;
  REQUIRE(map::QueueLabel(labels, count, 3, 4, 0, 5));
  CHECK_FALSE(map::QueueLabel(labels, count, 9, 9, 9, 9));
  CHECK(count == 8);
  std::int32_t negative = -1;
  CHECK_FALSE(map::QueueLabel(labels, negative, 9, 9, 9, 9));
  CHECK(negative == -1);
}

constexpr MarkerRect kUpperFirst{.x = 0, .y = 0, .w = 8, .h = 6};
constexpr MarkerRect kUpperSecond{.x = 8, .y = 0, .w = 10, .h = 12};
constexpr MarkerRect kLowerFirst{.x = 0, .y = 32, .w = 4, .h = 3};
constexpr MarkerRect kLowerSecond{.x = 16, .y = 32, .w = 6, .h = 5};
constexpr std::array<MarkerRect, 2> kUpperRects{kUpperFirst, kUpperSecond};
constexpr std::array<MarkerRect, 2> kLowerRects{kLowerFirst, kLowerSecond};

TEST_CASE("An odd door state flag draws the second variant", "[map-markers]") {
  FakeMapMarkerHost host;
  std::array<MapLabel, map::kMapLabelCapacity> labels{};
  std::int32_t count = 0;
  // State 0 names door (0 - 1) & 0x7F.
  REQUIRE(map::DrawDoorStateMarker(host, kUpperRects, kLowerRects, labels,
                                   count, 40, 20, 0, 3));
  REQUIRE(host.quads.size() == 2);
  CHECK(host.quads[0].corners ==
        std::array<ScreenPoint, 4>{{{35, 20}, {45, 20}, {35, 32}, {45, 32}}});
  CHECK(host.quads[1].corners ==
        std::array<ScreenPoint, 4>{{{37, 32}, {43, 32}, {37, 37}, {43, 37}}});
  CHECK(host.quads[0].clut == 0x37FD);
  CHECK(host.quads[1].clut == 0x37F6);
  CHECK(labels[0] == MapLabel{.x = 40, .y = 32, .kind = 0, .id = 0x7F});
}

TEST_CASE("An even door state flag draws the first variant", "[map-markers]") {
  FakeMapMarkerHost host;
  std::array<MapLabel, map::kMapLabelCapacity> labels{};
  std::int32_t count = 3;
  REQUIRE(map::DrawDoorStateMarker(host, kUpperRects, kLowerRects, labels,
                                   count, 0, 0, 0x82, 2));
  REQUIRE(host.quads.size() == 2);
  CHECK(host.quads[0].corners[3] == ScreenPoint{4, 6});
  CHECK(host.quads[1].corners[3] == ScreenPoint{2, 9});
  CHECK(labels[3] == MapLabel{.x = 0, .y = 6, .kind = 0, .id = 1});
  CHECK(count == 4);
}

TEST_CASE("A door state marker with a full queue draws but is refused",
          "[map-markers]") {
  FakeMapMarkerHost host;
  const std::array<MarkerRect, 2> rects{};
  std::array<MapLabel, map::kMapLabelCapacity> labels{};
  std::int32_t count = map::kMapLabelCapacity;
  CHECK_FALSE(
      map::DrawDoorStateMarker(host, rects, rects, labels, count, 0, 0, 1, 0));
  CHECK(host.quads.size() == 2);
  CHECK(count == map::kMapLabelCapacity);
}

TEST_CASE("A label line keeps each coordinate's low halfword",
          "[map-markers]") {
  FakeMapMarkerHost host;
  map::AppendLabelLine(host, 0x18000, -1, 194, 0x7FFF);
  REQUIRE(host.lines.size() == 1);
  CHECK(host.lines[0] == LabelLine{.from = {-0x8000, -1}, .to = {194, 0x7FFF}});
}

TEST_CASE("A door label bends toward its pivot and steps the row up",
          "[map-markers]") {
  FakeMapMarkerHost host;
  const auto names = NameTable();
  std::int32_t label_y = 100;
  // A quarter of 186 - 100 rounds down to 21; right of the pivot the bend
  // is the pivot.
  REQUIRE(map::DrawDoorLabel(host, names, label_y, 100, 30, 7));
  REQUIRE(map::DrawDoorLabel(host, names, label_y, 187, 31, 8));
  CHECK(host.lines == std::vector<LabelLine>{{{100, 30}, {165, 108}},
                                             {{165, 108}, {194, 108}},
                                             {{187, 31}, {186, 92}},
                                             {{186, 92}, {194, 92}}});
  REQUIRE(host.texts.size() == 2);
  CHECK(host.texts[0] == LabelText{.x = 194,
                                   .y = 100,
                                   .width = 128,
                                   .background_width = 16,
                                   .table = LabelTextTable::kDoorNames,
                                   .text_offset = 7});
  CHECK(host.texts[1].y == 84);
  CHECK(label_y == 68);
}

TEST_CASE("A door label outside its name table is refused", "[map-markers]") {
  FakeMapMarkerHost host;
  std::array<std::uint16_t, 4> names{0, 1, 2, 4};
  std::int32_t label_y = 100;
  CHECK_FALSE(map::DrawDoorLabel(host, names, label_y, 0, 0, 4));
  CHECK_FALSE(map::DrawDoorLabel(host, names, label_y, 0, 0, 3));
  CHECK(host.lines.empty());
  CHECK(host.texts.empty());
  CHECK(label_y == 100);
}

TEST_CASE("A visited connecting area label shows its own name",
          "[map-markers]") {
  FakeMapMarkerHost host;
  const auto names = NameTable();
  const std::array<std::uint32_t, 2> visited{1U << 5, 1U << 1};
  std::int32_t label_y = 50;
  REQUIRE(map::DrawConnectingAreaLabel(host, names, visited, label_y, 0, 9, 5));
  // A quarter of 170 rounds down to 42.
  CHECK(host.lines ==
        std::vector<LabelLine>{{{0, 9}, {128, 58}}, {{128, 58}, {194, 58}}});
  CHECK(host.texts ==
        std::vector<LabelText>{{.x = 178,
                                .y = 50,
                                .width = 144,
                                .background_width = 8,
                                .table = LabelTextTable::kAreaNames,
                                .text_offset = 5}});
  CHECK(label_y == 34);
}

TEST_CASE("An unvisited connecting area label shows the generic name",
          "[map-markers]") {
  FakeMapMarkerHost host;
  const auto names = NameTable();
  const std::array<std::uint32_t, 2> visited{1U << 5, 1U << 1};
  std::int32_t label_y = 50;
  REQUIRE(
      map::DrawConnectingAreaLabel(host, names, visited, label_y, 200, 9, 6));
  REQUIRE(
      map::DrawConnectingAreaLabel(host, names, visited, label_y, 0, 9, 33));
  REQUIRE(host.texts.size() == 2);
  CHECK(host.texts[0].text_offset == map::kUnvisitedAreaName);
  CHECK(host.texts[1].text_offset == 33);
  // Right of the pivot the bend is 170.
  CHECK(host.lines[0] == LabelLine{.from = {200, 9}, .to = {170, 58}});
}

TEST_CASE("A connecting area outside the visit flags is refused",
          "[map-markers]") {
  FakeMapMarkerHost host;
  const auto names = NameTable();
  const std::array<std::uint32_t, 2> visited{};
  std::int32_t label_y = 50;
  CHECK_FALSE(
      map::DrawConnectingAreaLabel(host, names, visited, label_y, 0, 0, 64));
  CHECK_FALSE(
      map::DrawConnectingAreaLabel(host, names, visited, label_y, 0, 0, -1));
  const std::array<std::uint16_t, 8> short_names{};
  CHECK_FALSE(map::DrawConnectingAreaLabel(host, short_names, visited, label_y,
                                           0, 0, 3));
  CHECK(host.lines.empty());
  CHECK(label_y == 50);
}

TEST_CASE("A door state marker shows below flag 2 unless its sigil is open",
          "[map-markers]") {
  FakeMapMarkerHost host;
  CHECK(map::ShouldDrawDoorStateMarker(host, 0x40, 1, 2, 1));
  CHECK_FALSE(map::ShouldDrawDoorStateMarker(host, 0x40, 1, 2, 2));
  CHECK(host.sigil_queries.empty());
  CHECK(map::ShouldDrawDoorStateMarker(host, 0x47, 3, 4, 0));
  CHECK(map::ShouldDrawDoorStateMarker(host, 0x48, 3, 4, 0));
  REQUIRE(host.sigil_queries.size() == 1);
  CHECK(host.sigil_queries[0] == SigilQuery{3, 4, 6});
  host.sigil_unlocked = true;
  CHECK_FALSE(map::ShouldDrawDoorStateMarker(host, 0x41, 5, 6, 0));
  CHECK(host.sigil_queries[1] == SigilQuery{5, 6, 0});
}

struct LiveLabelState {
  LiveLabelState() {
    std::memcpy(labels.data(), Menu_Map_mapLabels, sizeof(labels));
    std::memcpy(&flags, &Audio_Seq_unusedStateFlags, sizeof(flags));
  }
  LiveLabelState(const LiveLabelState&) = delete;
  LiveLabelState& operator=(const LiveLabelState&) = delete;
  LiveLabelState(LiveLabelState&&) = delete;
  LiveLabelState& operator=(LiveLabelState&&) = delete;
  ~LiveLabelState() {
    std::memcpy(Menu_Map_mapLabels, labels.data(), sizeof(labels));
    std::memcpy(&Audio_Seq_unusedStateFlags, &flags, sizeof(flags));
    Menu_Map_mapLabelCount = count;
    Menu_Map_mapLabelY = label_y;
  }

  std::array<Menu_Map_MapLabel, map::kMapLabelCapacity> labels{};
  State_StateFlags flags{};
  int count = Menu_Map_mapLabelCount;
  int label_y = Menu_Map_mapLabelY;
};

TEST_CASE("The live label queue refuses an append past its capacity",
          "[map-markers][map-markers-live]") {
  const LiveLabelState state;
  Menu_Map_mapLabelCount = 7;
  map::LiveQueueLabel(11, 22, 1, 33);
  CHECK(Menu_Map_mapLabelCount == 8);
  CHECK(Menu_Map_mapLabels[7].x == 11);
  CHECK(Menu_Map_mapLabels[7].y == 22);
  map::LiveQueueLabel(44, 55, 0, 66);
  CHECK(Menu_Map_mapLabelCount == 8);
  CHECK(Menu_Map_mapLabels[7].x == 11);
}

TEST_CASE("The live door state test reads its flag unsigned",
          "[map-markers][map-markers-live]") {
  const LiveLabelState state;
  Audio_Seq_unusedStateFlags.opaque33F[3] = 1;
  CHECK(map::LiveShouldDrawDoorStateMarker(3, 0, 0) == 1);
  // The original's lbu reads 0x80 as 128, which hides the marker.
  Audio_Seq_unusedStateFlags.opaque33F[3] = static_cast<char>(0x80);
  CHECK(map::LiveShouldDrawDoorStateMarker(3, 0, 0) == 0);
}

TEST_CASE("The live markers refuse states icons and names outside tables",
          "[map-markers][map-markers-live]") {
  const LiveLabelState state;
  Menu_Map_mapLabelCount = 0;
  Menu_Map_mapLabelY = 77;
  CHECK(map::LiveShouldDrawDoorStateMarker(0x100000, 0, 0) == 0);
  CHECK(map::LiveShouldDrawDoorStateMarker(-0x400, 0, 0) == 0);
  map::LiveDrawDoorStateMarker(0, 0, 0x100000);
  map::LiveDrawFeatureMarker(4, 0, 0);
  map::LiveDrawFeatureMarker(-1, 0, 0);
  map::LiveDrawDoorLabel(0, 0, 0x10000);
  map::LiveDrawConnectingAreaLabel(0, 0, 64);
  CHECK(Menu_Map_mapLabelCount == 0);
  CHECK(Menu_Map_mapLabelY == 77);
}
}  // namespace
