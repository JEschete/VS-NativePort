#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/StatusEffects.h"
#include "DiscQueue.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/MinimapScene.hpp"
#include "Field/Room.h"
#include "Legacy/Psx.h"
#include "Menu/Map.hpp"
#include "Menu/MapNavigation.hpp"
#include "Menu/MapNavigationLive.hpp"

namespace {
namespace nav = VS::Menu::map;
using nav::MapPoint;
using nav::MapRoom;
using nav::MapVertex;
using nav::RoomLink;
using nav::RoomVertices;

class FakeMapNavigationHost final : public nav::MapNavigationHost {
 public:
  void PlaySound(std::int32_t sound, std::int32_t volume) override {
    sounds.push_back({sound, volume});
  }
  std::vector<std::array<std::int32_t, 2>> sounds;
};

TEST_CASE("Marking every room visited sets each visited word to 1",
          "[map-navigation]") {
  std::array<MapRoom, 3> rooms{{{.visited = 0, .zone_id = 1, .map_id = 2},
                                {.visited = -5, .zone_id = 3, .map_id = 4},
                                {.visited = 1, .zone_id = 5, .map_id = 6}}};
  nav::MarkAllRoomsVisited(rooms);
  CHECK(rooms[0] == MapRoom{.visited = 1, .zone_id = 1, .map_id = 2});
  CHECK(rooms[1] == MapRoom{.visited = 1, .zone_id = 3, .map_id = 4});
  CHECK(rooms[2].visited == 1);
  nav::MarkAllRoomsVisited({});
}

TEST_CASE("A room center rounds its half sums toward zero and keeps the floor",
          "[map-navigation]") {
  const std::array<MapVertex, 3> vertices{{{.x = -3, .y = 10, .z = 4},
                                           {.x = 0, .y = -20, .z = -7},
                                           {.x = 1, .y = 30, .z = 3}}};
  // x: (-3 + 1) / 2 = -1. z: (-7 + 4) / 2 = -1.5, rounded to -1.
  CHECK(nav::RoomCenterOf(vertices) == MapPoint{.x = -1, .y = 30, .z = -1});
  const std::array<MapVertex, 2> odd{
      {{.x = 2, .y = 0, .z = 0x7FFF}, {.x = 5, .y = -1, .z = 0x7FFF}}};
  CHECK(nav::RoomCenterOf(odd) == MapPoint{.x = 3, .y = 0, .z = 0x7FFF});
  const std::array<MapVertex, 2> extremes{
      {{.x = -0x8000, .y = -0x8000, .z = -0x8000},
       {.x = 0x7FFF, .y = -0x8000, .z = -0x8000}}};
  CHECK(nav::RoomCenterOf(extremes) ==
        MapPoint{.x = 0, .y = -0x8000, .z = -0x8000});
}

TEST_CASE("A room without vertices centers on the empty box",
          "[map-navigation]") {
  CHECK(nav::RoomCenterOf({}) == MapPoint{.x = 0, .y = -0x8000, .z = 0});
}

TEST_CASE("Centering on a room moves every room and clears the center",
          "[map-navigation]") {
  std::vector<MapVertex> first{{.x = 10, .y = 4, .z = -6},
                               {.x = 20, .y = 8, .z = 6}};
  std::vector<MapVertex> second{{.x = -0x8000, .y = 0, .z = 0x7FFF}};
  const std::array<RoomVertices, 2> rooms{first, second};
  MapPoint center{.x = 9, .y = 9, .z = 9};
  REQUIRE(nav::CenterOnRoom(rooms, 0, center));
  CHECK(center == MapPoint{});
  CHECK(first[0] == MapVertex{.x = -5, .y = -4, .z = -6});
  CHECK(first[1] == MapVertex{.x = 5, .y = 0, .z = 6});
  // -0x8000 - 15 wraps to 0x7FF1.
  CHECK(second[0] == MapVertex{.x = 0x7FF1, .y = -8, .z = 0x7FFF});
}

TEST_CASE("Centering on a room outside the map changes nothing",
          "[map-navigation]") {
  std::vector<MapVertex> only{{.x = 1, .y = 2, .z = 3}};
  const std::array<RoomVertices, 1> rooms{only};
  MapPoint center{.x = 4, .y = 5, .z = 6};
  CHECK_FALSE(nav::CenterOnRoom(rooms, 1, center));
  CHECK(center == MapPoint{.x = 4, .y = 5, .z = 6});
  CHECK(only[0] == MapVertex{.x = 1, .y = 2, .z = 3});
}

TEST_CASE("The centering step takes the remainder 8 or an eighth",
          "[map-navigation]") {
  CHECK(nav::CenteringStepOf({.x = 7, .y = -7, .z = 0}) ==
        MapPoint{.x = 7, .y = -7, .z = 0});
  CHECK(nav::CenteringStepOf({.x = 8, .y = -8, .z = 128}) ==
        MapPoint{.x = 8, .y = -8, .z = 8});
  CHECK(nav::CenteringStepOf({.x = -128, .y = 129, .z = -129}) ==
        MapPoint{.x = -8, .y = 16, .z = -16});
  CHECK(nav::CenteringStepOf({.x = -0x8000, .y = 0x7FFF, .z = -135}) ==
        MapPoint{.x = -0x1000, .y = 0xFFF, .z = -16});
}

TEST_CASE("Animating the centering moves the map one step toward the center",
          "[map-navigation]") {
  std::vector<MapVertex> room{{.x = 100, .y = -0x8000, .z = 0}};
  const std::array<RoomVertices, 1> rooms{room};
  MapPoint center{.x = 200, .y = 3, .z = -20};
  nav::AnimateCentering(rooms, center);
  CHECK(center == MapPoint{.x = 175, .y = 0, .z = -12});
  CHECK(room[0] == MapVertex{.x = 75, .y = 0x7FFD, .z = 8});
  MapPoint done{};
  nav::AnimateCentering(rooms, done);
  CHECK(done == MapPoint{});
  CHECK(room[0] == MapVertex{.x = 75, .y = 0x7FFD, .z = 8});
}

TEST_CASE("Scaling keeps each product's low halfword", "[map-navigation]") {
  std::vector<MapVertex> room{{.x = 0x2000, .y = -3, .z = 0x4001},
                              {.x = -0x8000, .y = 1, .z = 0}};
  const std::array<RoomVertices, 1> rooms{room};
  nav::ScaleRoomVertices(rooms, 4);
  CHECK(room[0] == MapVertex{.x = -0x8000, .y = -12, .z = 4});
  CHECK(room[1] == MapVertex{.x = 0, .y = 4, .z = 0});
  nav::ScaleRoomVertices(rooms, -0x10001);
  CHECK(room[0] == MapVertex{.x = -0x8000, .y = 12, .z = -4});
}

TEST_CASE("The initial room prefers Ashley's room then the first visited",
          "[map-navigation]") {
  const std::array<MapRoom, 4> rooms{
      {{.visited = 0, .zone_id = 1, .map_id = 1},
       {.visited = 1, .zone_id = 1, .map_id = 2},
       {.visited = 1, .zone_id = 2, .map_id = 3},
       {.visited = 0, .zone_id = 2, .map_id = 3}}};
  using VS::Field::SceneRoomAddress;
  CHECK(nav::InitialRoomOf(rooms, SceneRoomAddress{2, 3}) ==
        nav::InitialRoom{.selected = 2, .current = 2});
  CHECK(nav::InitialRoomOf(rooms, SceneRoomAddress{1, 3}) ==
        nav::InitialRoom{.selected = 1, .current = -1});
  CHECK(nav::InitialRoomOf(rooms, std::nullopt) ==
        nav::InitialRoom{.selected = 1, .current = -1});
  const std::array<MapRoom, 1> unvisited{{{.visited = 0}}};
  CHECK(nav::InitialRoomOf(unvisited, SceneRoomAddress{5, 5}) ==
        nav::InitialRoom{.selected = 0, .current = -1});
  CHECK(nav::InitialRoomOf({}, SceneRoomAddress{0, 0}) ==
        nav::InitialRoom{.selected = 0, .current = -1});
}

TEST_CASE("Stepping rooms follows the links to the next visited room",
          "[map-navigation]") {
  const std::array<RoomLink, 4> links{{{.previous = 3, .next = 1},
                                       {.previous = 0, .next = 2},
                                       {.previous = 1, .next = 3},
                                       {.previous = 2, .next = 0}}};
  const std::array<MapRoom, 4> rooms{
      {{.visited = 1}, {.visited = 0}, {.visited = 0}, {.visited = 2}}};
  FakeMapNavigationHost host;
  const auto forward = nav::SelectAdjacentVisitedRoom(host, links, rooms, 0, 1);
  REQUIRE(forward.has_value());
  CHECK(*forward == 3);
  const auto backward =
      nav::SelectAdjacentVisitedRoom(host, links, rooms, 3, 0);
  REQUIRE(backward.has_value());
  CHECK(*backward == 0);
  CHECK(host.sounds ==
        std::vector<std::array<std::int32_t, 2>>{{0x7E, 0xB}, {0x7E, 0xB}});
}

TEST_CASE("Stepping back to the same room plays no sound", "[map-navigation]") {
  const std::array<RoomLink, 2> links{
      {{.previous = 0, .next = 1}, {.previous = 0, .next = 0}}};
  const std::array<MapRoom, 2> rooms{{{.visited = 1}, {.visited = 0}}};
  FakeMapNavigationHost host;
  const auto selected =
      nav::SelectAdjacentVisitedRoom(host, links, rooms, 0, 1);
  REQUIRE(selected.has_value());
  CHECK(*selected == 0);
  const auto self = nav::SelectAdjacentVisitedRoom(host, links, rooms, 0, -1);
  REQUIRE(self.has_value());
  CHECK(*self == 0);
  CHECK(host.sounds.empty());
}

TEST_CASE("Stepping rooms refuses broken links and unvisited cycles",
          "[map-navigation]") {
  const std::array<RoomLink, 2> links{
      {{.previous = -1, .next = 1}, {.previous = 0, .next = 0}}};
  const std::array<MapRoom, 2> unvisited{{{.visited = 0}, {.visited = 0}}};
  FakeMapNavigationHost host;
  CHECK_FALSE(
      nav::SelectAdjacentVisitedRoom(host, links, unvisited, 0, 1).has_value());
  const std::array<MapRoom, 2> visited{{{.visited = 1}, {.visited = 1}}};
  CHECK_FALSE(
      nav::SelectAdjacentVisitedRoom(host, links, visited, 0, 0).has_value());
  CHECK_FALSE(
      nav::SelectAdjacentVisitedRoom(host, links, visited, 2, 1).has_value());
  CHECK_FALSE(
      nav::SelectAdjacentVisitedRoom(host, links, visited, -1, 1).has_value());
  CHECK(host.sounds.empty());
}

// A scene image: the room count, the records, each room's vertex block and
// the room names, with record offsets resolved as the loader leaves them.
struct SceneImage {
  std::vector<std::byte> bytes;
  std::size_t names = 0;
};

template <typename T>
void Put(std::vector<std::byte>& bytes, std::size_t offset, T value) {
  std::memcpy(bytes.data() + offset, &value, sizeof(T));
}

template <typename T>
T Get(const std::vector<std::byte>& bytes, std::size_t offset) {
  T value{};
  std::memcpy(&value, bytes.data() + offset, sizeof(T));
  return value;
}

constexpr std::size_t kRecord = 12;
constexpr std::size_t kVertex = 8;
constexpr std::size_t kName = 36;

SceneImage MakeScene(const std::vector<std::vector<MapVertex>>& rooms,
                     const std::vector<MapRoom>& records,
                     const std::vector<RoomLink>& links, std::size_t size) {
  SceneImage image{std::vector<std::byte>(size)};
  Put(image.bytes, 0, static_cast<std::int32_t>(rooms.size()));
  std::size_t block = 4 + (rooms.size() * kRecord);
  for (std::size_t room = 0; room < rooms.size(); ++room) {
    const std::size_t record = 4 + (room * kRecord);
    Put(image.bytes, record, records[room].visited);
    Put(image.bytes, record + 4, static_cast<std::uint32_t>(block - record));
    Put(image.bytes, record + 8, records[room].zone_id);
    Put(image.bytes, record + 10, records[room].map_id);
    Put(image.bytes, block, static_cast<std::int32_t>(rooms[room].size()));
    for (std::size_t vertex = 0; vertex < rooms[room].size(); ++vertex) {
      const std::size_t at = block + 4 + (vertex * kVertex);
      Put(image.bytes, at, rooms[room][vertex].x);
      Put(image.bytes, at + 2, rooms[room][vertex].y);
      Put(image.bytes, at + 4, rooms[room][vertex].z);
      Put(image.bytes, at + 6, std::int16_t{0x5A5A});
    }
    block += 4 + (rooms[room].size() * kVertex);
  }
  image.names = block;
  for (std::size_t room = 0; room < links.size(); ++room) {
    Put(image.bytes, block + (room * kName) + 32, links[room].previous);
    Put(image.bytes, block + (room * kName) + 34, links[room].next);
  }
  return image;
}

MapVertex VertexAt(const SceneImage& image, std::size_t offset) {
  return {.x = Get<std::int16_t>(image.bytes, offset),
          .y = Get<std::int16_t>(image.bytes, offset + 2),
          .z = Get<std::int16_t>(image.bytes, offset + 4)};
}

// Points the minimap scene and the ARM scene buffer at test images and
// restores the map globals.
struct LiveMapState {
  LiveMapState()
      : minimap(Field_roomData.minimapScene),
        minimap_length(Field_roomData.header.minimapSceneLength),
        arm(Battle_unusedSceneBuffer),
        scene(nav::currentScene),
        center(nav::centerPoint),
        room(nav::currentRoomIndex),
        current(nav::isCurrentScene),
        names(nav::roomNamesTable) {}
  LiveMapState(const LiveMapState&) = delete;
  LiveMapState& operator=(const LiveMapState&) = delete;
  LiveMapState(LiveMapState&&) = delete;
  LiveMapState& operator=(LiveMapState&&) = delete;
  ~LiveMapState() {
    Field_roomData.minimapScene = minimap;
    Field_roomData.header.minimapSceneLength = minimap_length;
    Battle_unusedSceneBuffer = arm;
    nav::currentScene = scene;
    nav::centerPoint = center;
    nav::currentRoomIndex = room;
    nav::isCurrentScene = current;
    nav::roomNamesTable = names;
  }
  static void UseMinimap(SceneImage& image) {
    Field_roomData.minimapScene =
        reinterpret_cast<Field_MapScene*>(image.bytes.data());
    Field_roomData.header.minimapSceneLength =
        static_cast<u_int>(image.bytes.size());
  }

  Field_MapScene* minimap;
  u_int minimap_length;
  void* arm;
  int scene;
  SVECTOR center;
  int room;
  int current;
  Field_MapRoomName* names;
};

constexpr std::size_t kSceneBytes = 0x200;

SceneImage TwoRoomScene() {
  return MakeScene({{{.x = 10, .y = 4, .z = -6}, {.x = 20, .y = 8, .z = 6}},
                    {{.x = 100, .y = 0, .z = 0}}},
                   {{.visited = 0}, {.visited = 0}}, {}, kSceneBytes);
}
constexpr std::size_t kFirstVertex = 4 + (2 * kRecord) + 4;
constexpr std::size_t kSecondVertex = kFirstVertex + (2 * kVertex) + 4;

TEST_CASE("The live map centers on a minimap scene room",
          "[map-navigation][map-navigation-live]") {
  LiveMapState state;
  SceneImage image = TwoRoomScene();
  LiveMapState::UseMinimap(image);
  auto* scene = Field_roomData.minimapScene;

  nav::LiveSetRoomCenterTarget(scene, 1);
  CHECK(nav::centerPoint.vx == 100);
  CHECK(nav::centerPoint.vy == 0);

  nav::LiveCenterOnRoom(scene, 0);
  CHECK(nav::centerPoint.vx == 0);
  CHECK(VertexAt(image, kFirstVertex) == MapVertex{.x = -5, .y = -4, .z = -6});
  CHECK(VertexAt(image, kSecondVertex) == MapVertex{.x = 85, .y = -8, .z = 0});
  CHECK(Get<std::int16_t>(image.bytes, kSecondVertex + 6) == 0x5A5A);
}

TEST_CASE("The live map eases scales and marks a minimap scene",
          "[map-navigation][map-navigation-live]") {
  LiveMapState state;
  SceneImage image = TwoRoomScene();
  LiveMapState::UseMinimap(image);
  auto* scene = Field_roomData.minimapScene;

  nav::centerPoint = {.vx = 15, .vy = 0, .vz = 0, .pad = 0};
  nav::LiveAnimateCentering(scene);
  CHECK(nav::centerPoint.vx == 7);
  CHECK(VertexAt(image, kSecondVertex) == MapVertex{.x = 92, .y = 0, .z = 0});

  nav::LiveScaleRoomVertices(scene, 4);
  CHECK(VertexAt(image, kSecondVertex) == MapVertex{.x = 368, .y = 0, .z = 0});
  nav::LiveMarkAllRoomsVisited(reinterpret_cast<int*>(scene));
  CHECK(Get<std::int32_t>(image.bytes, 4) == 1);
  CHECK(Get<std::int32_t>(image.bytes, 4 + kRecord) == 1);
}

TEST_CASE("The live map refuses unknown scenes rooms and blocks",
          "[map-navigation][map-navigation-live]") {
  LiveMapState state;
  SceneImage image = MakeScene({{{.x = 10, .y = 4, .z = -6}}}, {{.visited = 0}},
                               {}, kSceneBytes);
  SceneImage other = image;
  LiveMapState::UseMinimap(image);
  nav::centerPoint = {.vx = 1, .vy = 2, .vz = 3, .pad = 0};

  nav::LiveCenterOnRoom(reinterpret_cast<Field_MapScene*>(other.bytes.data()),
                        0);
  nav::LiveCenterOnRoom(Field_roomData.minimapScene, 1);
  nav::LiveSetRoomCenterTarget(Field_roomData.minimapScene, -1);
  CHECK(nav::centerPoint.vx == 1);
  CHECK(nav::centerPoint.vz == 3);

  // The vertex block's count runs past the scene.
  Put(image.bytes, 4 + kRecord, std::int32_t{0x100});
  nav::LiveScaleRoomVertices(Field_roomData.minimapScene, 2);
  CHECK(VertexAt(image, 4 + kRecord + 4) ==
        MapVertex{.x = 10, .y = 4, .z = -6});
}

TEST_CASE("The live initial room matches the minimap scene's first room",
          "[map-navigation][map-navigation-live]") {
  LiveMapState state;
  SceneImage minimap = MakeScene(
      {{}}, {{.visited = 1, .zone_id = 7, .map_id = 9}}, {}, kSceneBytes);
  LiveMapState::UseMinimap(minimap);
  const auto size = static_cast<std::size_t>(nav::kSceneArmFiles[1].size);
  REQUIRE(size >= kSceneBytes);
  SceneImage scene = MakeScene({{}, {}, {}},
                               {{.visited = 0, .zone_id = 7, .map_id = 8},
                                {.visited = 1, .zone_id = 1, .map_id = 1},
                                {.visited = 0, .zone_id = 7, .map_id = 9}},
                               {}, size);
  Battle_unusedSceneBuffer = scene.bytes.data();
  nav::currentScene = 1;
  auto* data = static_cast<Field_MapScene*>(Battle_unusedSceneBuffer);
  CHECK(nav::LiveInitialRoomIndex(data) == 2);
  CHECK(nav::isCurrentScene == 2);

  // The minimap's first room moves to map 4, which the scene lacks.
  Put(minimap.bytes, 4 + 10, std::int16_t{4});
  CHECK(nav::LiveInitialRoomIndex(data) == 1);
  CHECK(nav::isCurrentScene == -1);

  Field_roomData.minimapScene = nullptr;
  nav::isCurrentScene = 5;
  CHECK(nav::LiveInitialRoomIndex(data) == 1);
  CHECK(nav::isCurrentScene == -1);
}

TEST_CASE("The live room step follows the ARM scene's name links",
          "[map-navigation][map-navigation-live]") {
  LiveMapState state;
  const auto size = static_cast<std::size_t>(nav::kSceneArmFiles[1].size);
  REQUIRE(size >= kSceneBytes);
  SceneImage scene =
      MakeScene({{}, {}, {}}, {{.visited = 1}, {.visited = 0}, {.visited = 1}},
                {{.previous = 2, .next = 1},
                 {.previous = 0, .next = 2},
                 {.previous = 1, .next = 0}},
                size);
  Battle_unusedSceneBuffer = scene.bytes.data();
  nav::currentScene = 1;
  nav::roomNamesTable =
      reinterpret_cast<Field_MapRoomName*>(scene.bytes.data() + scene.names);
  nav::currentRoomIndex = 0;
  nav::LiveSelectAdjacentVisitedRoom(1);
  CHECK(nav::currentRoomIndex == 2);
  nav::LiveSelectAdjacentVisitedRoom(0);
  CHECK(nav::currentRoomIndex == 0);

  // Every room unvisited: the original never returns.
  Put(scene.bytes, 4, std::int32_t{0});
  Put(scene.bytes, 4 + (2 * kRecord), std::int32_t{0});
  nav::LiveSelectAdjacentVisitedRoom(1);
  CHECK(nav::currentRoomIndex == 0);

  nav::roomNamesTable = nullptr;
  Put(scene.bytes, 4, std::int32_t{1});
  nav::LiveSelectAdjacentVisitedRoom(1);
  CHECK(nav::currentRoomIndex == 0);
}
}  // namespace
