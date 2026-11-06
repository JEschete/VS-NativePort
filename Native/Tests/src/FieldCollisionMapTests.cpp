// Hand-maintained tests for the typed field collision map, map progress and
// trap panel rules (PROMOTION_PLAN row 23, #276).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Field/CollisionMap.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/Map.hpp"
#include "Field/MapLive.hpp"
#include "Field/Traps.hpp"
#include "Field/TrapsLive.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Field/Gimmicks.h"
#include "Field/MapProgress.h"
#include "Field/Minimap.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/Zone.h"
#include "State/Progress.h"
}

namespace {
using VS::Field::CollisionCellRef;
using VS::Field::CollisionCells;
using VS::Field::CollisionCellSource;
using VS::Field::GimmickSupportState;
using VS::Field::PlatformContact;

// Copies a global's bytes and puts them back when the test ends.
class SavedGlobal {
 public:
  template <typename T>
  explicit SavedGlobal(T& global) : where_(&global), bytes_(sizeof global) {
    std::memcpy(bytes_.data(), where_, bytes_.size());
  }
  ~SavedGlobal() { std::memcpy(where_, bytes_.data(), bytes_.size()); }
  SavedGlobal(const SavedGlobal&) = delete;
  SavedGlobal& operator=(const SavedGlobal&) = delete;
  SavedGlobal(SavedGlobal&&) = delete;
  SavedGlobal& operator=(SavedGlobal&&) = delete;

 private:
  void* where_;
  std::vector<std::byte> bytes_;
};

// A 2x1 collision map with height origin 100. Floors: tile 0 code 10
// (height 640), tile 1 code 12 (608) with an invalid sample at its far corner.
// Ceilings: tile 0 code 100 (-800), tile 1 code 20 (480, below the origin).
struct GridMap {
  static constexpr std::size_t kPatterns = 8 + 2 * 2 * 2;
  std::array<std::byte, kPatterns + std::size_t{4} * 16> bytes{};

  GridMap() {
    const std::array<std::int16_t, 4> header{2, 1, 100, 2};
    std::memcpy(bytes.data(), header.data(), sizeof header);
    const std::array<std::uint16_t, 4> descriptors{0, 1, 2, 3};
    std::memcpy(bytes.data() + 8, descriptors.data(), sizeof descriptors);
    const std::array<std::uint8_t, 4> codes{10, 12, 100, 20};
    for (std::size_t pattern = 0; pattern < codes.size(); ++pattern) {
      std::memset(bytes.data() + kPatterns + (pattern * 16), codes.at(pattern),
                  16);
    }
    bytes[kPatterns + 16 + 15] = std::byte{0x8C};
  }
};

// A 2x2 grid on two layers, and two platform cells.
struct CellTables {
  std::array<std::uint32_t, 8> room{};
  std::array<std::uint32_t, 2> platforms{};

  [[nodiscard]] CollisionCells Cells() const {
    return {
        .map =
            VS::Field::CollisionMapHeader{
                .width = 2, .height = 2, .height_origin = 0, .cell_count = 4},
        .room = room,
        .platforms = platforms};
  }
};

constexpr std::uint32_t PassabilityWord(std::uint32_t passability) {
  return passability << 10;
}

class FixedRandom final : public VS::Field::TrapRandom {
 public:
  explicit FixedRandom(std::int32_t value) : value_(value) {}
  [[nodiscard]] std::int32_t NextBounded(std::int32_t /*bound*/) override {
    return value_;
  }
  [[nodiscard]] std::int32_t NextAveraged(std::int32_t /*bound*/) override {
    return value_;
  }

 private:
  std::int32_t value_;
};

class FakeBypassTable final : public VS::Field::ActorCollisionFlags {
 public:
  std::map<std::int32_t, VS::Field::CollisionBypass> slots;

  [[nodiscard]] std::optional<VS::Field::CollisionBypass> Bypass(
      std::int32_t actor_index) const override {
    const auto slot = slots.find(actor_index);
    if (slot == slots.end()) return std::nullopt;
    return slot->second;
  }
  void SetBypass(std::int32_t actor_index,
                 VS::Field::CollisionBypass bypass) override {
    slots.at(actor_index) = bypass;
  }
};

// Locations are zone * 32 + map index; zone 9 names a location past 511.
class FakeLocations final : public VS::Field::RoomLocationIndex {
 public:
  std::vector<std::pair<std::int32_t, std::int32_t>> queries;

  std::int32_t LocationId(std::int32_t zone_id,
                          std::int32_t map_index) override {
    queries.emplace_back(zone_id, map_index);
    return zone_id == 9 ? 600 : (zone_id * 32) + map_index;
  }
};

// A target whose panel attack (stat factor 4, trap level 2) is 150 and whose
// defense is 30, so the panel's amount is 120.
VS::Field::TrapPanelTarget PanelTarget() {
  VS::Field::TrapPanelTarget target;
  target.current_hp = 120;
  target.max_hp = 300;
  target.strength = 40;
  target.limb.armor_strength = 10;
  return target;
}

using VS::Field::TrapPanelHit;
constexpr TrapPanelHit PanelHit() {
  TrapPanelHit hit;
  hit.stat_factor = 4;
  hit.affinity = 1;
  return hit;
}
constexpr TrapPanelHit kPanelHit = PanelHit();

GimmickSupportState Gimmick(std::int16_t y) {
  GimmickSupportState gimmick;
  gimmick.tile_x = 3;
  gimmick.tile_z = 4;
  gimmick.y = y;
  return gimmick;
}

// A free-standing actor at (16, 16) whose height has not been placed.
VS::Field::RoomActorSupport StandingActor() {
  VS::Field::RoomActorSupport actor;
  actor.x = 16;
  actor.y = -5;
  actor.z = 16;
  return actor;
}

CollisionCells WithRoom(CollisionCells cells,
                        std::span<const std::uint32_t> room) {
  cells.room = room;
  return cells;
}

// Whether a position on tile 0 finds its cell under a door passability.
bool OpenAt(CellTables& tables, std::uint32_t passability, std::int16_t x,
            std::int16_t z) {
  tables.room[0] = PassabilityWord(passability);
  return VS::Field::FindCollisionCellAtPosition(tables.Cells(), x, z, 0)
      .has_value();
}

std::int16_t HeightOf(const std::optional<VS::Field::RoomActorSupport>& actor) {
  return actor.value_or(VS::Field::RoomActorSupport{}).y;
}

// Gimmicks on tile (3, 4): skipped, moving and removed ones lie lower than the
// one in slot 5, which rests highest.
std::array<std::optional<GimmickSupportState>, 7> TileGimmicks() {
  auto skipped = Gimmick(-500);
  skipped.skipped = true;
  auto moving = Gimmick(-400);
  moving.motion_state = 1;
  auto removed = Gimmick(-300);
  removed.mode = VS::Field::kRemovedGimmickMode;
  return {std::nullopt, Gimmick(-200), skipped,     moving,
          removed,      Gimmick(-250), std::nullopt};
}

// Platform 1 (layer 3) at (500, 600) with its surface at height -100.
std::array<PlatformContact, 1> OnePlatform() {
  PlatformContact platform;
  platform.x = 500;
  platform.height = -100;
  platform.z = 600;
  platform.platform_index = 1;
  return {platform};
}

constexpr std::size_t kSceneRoomBytes = 12;

void SetSceneRoomCount(std::vector<std::byte>& scene, std::int32_t count) {
  std::memcpy(scene.data(), &count, sizeof count);
}

// A map scene of four rooms whose zone and map numbers name locations 34, 64,
// 0 and 600 through FakeLocations. Each visit word starts as 0x5A5A5A5A.
std::vector<std::byte> FourRoomScene() {
  const std::array<std::array<std::int16_t, 2>, 4> rooms{
      {{1, 3}, {2, 1}, {0, 1}, {9, 1}}};
  std::vector<std::byte> scene(4 + (rooms.size() * kSceneRoomBytes));
  SetSceneRoomCount(scene, static_cast<std::int32_t>(rooms.size()));
  for (std::size_t room = 0; room < rooms.size(); ++room) {
    std::byte* record = scene.data() + 4 + (room * kSceneRoomBytes);
    std::memset(record, 0x5A, 4);
    std::memcpy(record + 8, rooms.at(room).data(), 4);
  }
  return scene;
}

std::uint32_t VisitWord(const std::vector<std::byte>& scene, std::size_t room) {
  std::uint32_t word = 0;
  std::memcpy(&word, scene.data() + 4 + (room * kSceneRoomBytes), sizeof word);
  return word;
}

// The live room record pointed at a GridMap, two tile cells (tile 1 half
// closed) and camera bounds; the previous record comes back afterwards.
struct LiveRoom {
  SavedGlobal saved_room{Field_roomData};
  GridMap map;
  std::array<std::uint32_t, 2> cells{0, PassabilityWord(1)};
  std::array<std::int16_t, 4> bounds{-10, 20, 300, 400};

  LiveRoom() {
    Field_roomData.collisionMap =
        reinterpret_cast<Field_MpdCollisionHeader*>(map.bytes.data());
    Field_roomData.header.collisionMapLength = sizeof map.bytes;
    Field_roomData.tileCells =
        reinterpret_cast<Field_MpdTileCell*>(cells.data());
    Field_roomData.header.tileCellsLength = sizeof cells;
    Field_roomData.movingPlatforms = nullptr;
    Field_roomData.header.movingPlatformsLength = 0;
    Field_roomData.zoneBounds =
        reinterpret_cast<Field_ZoneBounds*>(bounds.data());
    Field_roomData.header.zoneBoundsLength = sizeof bounds;
  }
};

// Battle actor 0 with PanelTarget's stats on limb 1, a room of trap level 2
// and an action whose second hit is kPanelHit; the globals come back after.
struct LiveTrap {
  SavedGlobal saved_actors{Battle_actors};
  SavedGlobal saved_room{Field_roomData};
  std::unique_ptr<Battle_Actor> actor = std::make_unique<Battle_Actor>();
  std::unique_ptr<Battle_ActorStats> stats =
      std::make_unique<Battle_ActorStats>();
  Field_MpdEnvironment environment{};
  Battle_ActionDefinition action{};
  Battle_HitResult struck{};

  LiveTrap() {
    stats->maxHp = 300;
    stats->currentHp = 120;
    stats->strength = 40;
    stats->limbs[1].armor.currentStr = 10;
    actor->stats = stats.get();
    std::memset(static_cast<void*>(Battle_actors), 0, sizeof Battle_actors);
    Battle_actors[0] = actor.get();
    environment.trapLevel = 2;
    Field_roomData.environment = &environment;
    Field_roomData.header.environmentLength = sizeof environment;
    action.hits[1].statFactor = 4;
    action.hits[1].affinity = 1;
    struck.target.targetLimb = 1;
  }
};
}  // namespace

TEST_CASE("Collision map headers and dimensions decode bytes",
          "[field-collision-map][field]") {
  const GridMap map;
  const auto header = VS::Field::ReadCollisionMapHeader(map.bytes).value_or(
      VS::Field::CollisionMapHeader{});
  CHECK(header.width == 2);
  CHECK(header.height_origin == 100);
  CHECK_FALSE(VS::Field::ReadCollisionMapHeader(std::span(map.bytes).first(7))
                  .has_value());
  CHECK(VS::Field::CollisionMapDimensions(VS::Field::CollisionMapHeader{
            .width = 3, .height = 5, .height_origin = 0, .cell_count = 15}) ==
        0x500030);
  CHECK(VS::Field::CollisionMapDimensions(std::nullopt) == 0);
}

TEST_CASE("Camera bounds and room cell tables decode bytes",
          "[field-collision-map][field]") {
  const std::array<std::int16_t, 4> bounds{-10, 20, 300, 400};
  const auto bounds_bytes = std::as_bytes(std::span(bounds));
  CHECK(VS::Field::RoomCameraBounds(bounds_bytes) ==
        VS::Field::CameraBounds{
            .min_x = -10, .min_z = 20, .max_x = 300, .max_z = 400});
  CHECK_FALSE(VS::Field::RoomCameraBounds(bounds_bytes.first(7)).has_value());
  const CellTables tables;
  CHECK(VS::Field::RoomCollisionCells(tables.Cells()).data() ==
        tables.room.data());
}

TEST_CASE("Collision cells are found on the layer grids and platforms",
          "[field-collision-map][field]") {
  const CellTables tables;
  const CollisionCells cells = tables.Cells();
  const CollisionCellRef corner{.source = CollisionCellSource::kRoom,
                                .index = 3};
  CHECK(VS::Field::CollisionCellAt(cells, 1, 1, 0) == corner);
  // Layer 1 reads the first grid, as the original does.
  CHECK(VS::Field::CollisionCellAt(cells, 1, 1, 1) == corner);
  CHECK_FALSE(VS::Field::CollisionCellAt(cells, 2, 0, 0).has_value());
  CHECK_FALSE(VS::Field::CollisionCellAt(cells, 0, 2, 0).has_value());
  CHECK(VS::Field::CollisionCellAt(cells, 0, 0, 3) ==
        CollisionCellRef{.source = CollisionCellSource::kPlatform, .index = 1});
}

TEST_CASE("Collision cells are refused outside the authored tables",
          "[field-collision-map][field]") {
  const CellTables tables;
  const CollisionCells cells = tables.Cells();
  // Defined refusals: a platform past the authored ones, a missing room
  // table, a room table shorter than the grid and a missing map header.
  CHECK_FALSE(VS::Field::CollisionCellAt(cells, 0, 0, 4).has_value());
  CHECK_FALSE(
      VS::Field::CollisionCellAt(WithRoom(cells, {}), 0, 0, 0).has_value());
  CHECK_FALSE(
      VS::Field::CollisionCellAt(WithRoom(cells, cells.room.first(2)), 1, 1, 0)
          .has_value());
  auto no_map = cells;
  no_map.map = std::nullopt;
  CHECK_FALSE(VS::Field::CollisionCellAt(no_map, 0, 0, 0).has_value());
}

TEST_CASE("Blocking a cell addresses each layer grid and refuses other cells",
          "[field-collision-map][field]") {
  const CellTables tables;
  const CollisionCells cells = tables.Cells();
  using VS::Field::SetCollisionCellBlocked;
  CHECK(SetCollisionCellBlocked(cells, 1, 1, 0) ==
        CollisionCellRef{.source = CollisionCellSource::kRoom, .index = 3});
  CHECK(SetCollisionCellBlocked(cells, 1, 1, 1) ==
        CollisionCellRef{.source = CollisionCellSource::kRoom, .index = 7});
  CHECK(SetCollisionCellBlocked(cells, 0, 0, 2) ==
        CollisionCellRef{.source = CollisionCellSource::kPlatform, .index = 0});
  CHECK_FALSE(SetCollisionCellBlocked(cells, -1, 0, 0).has_value());
  CHECK_FALSE(
      SetCollisionCellBlocked(WithRoom(cells, cells.room.first(4)), 0, 0, 1)
          .has_value());
}

TEST_CASE("Blocked cell words change only the passability bits",
          "[field-collision-map][field]") {
  CHECK(VS::Field::BlockedCellWord(0xFFFFFFFFU, 0) == 0xFFFFE3FFU);
  CHECK(VS::Field::BlockedCellWord(0, 1) == PassabilityWord(5));
  CHECK(VS::Field::BlockedCellWord(PassabilityWord(7), 9) ==
        PassabilityWord(5));
  CHECK(VS::Field::CellPassability(PassabilityWord(5) | 0x3FFU) == 5);
}

TEST_CASE("Door passability 1 and 2 close one half of a tile",
          "[field-collision-map][field]") {
  CellTables tables;
  CHECK(OpenAt(tables, 0, 100, 100));
  CHECK_FALSE(OpenAt(tables, 1, 100, 50));
  CHECK(OpenAt(tables, 1, 50, 50));
  CHECK_FALSE(OpenAt(tables, 2, 60, 50));
  CHECK(OpenAt(tables, 2, 50, 60));
  tables.platforms[1] = PassabilityWord(0);
  CHECK(VS::Field::FindCollisionCellAtPosition(tables.Cells(), 10, 10, 3) ==
        CollisionCellRef{.source = CollisionCellSource::kPlatform, .index = 1});
}

TEST_CASE("Door passability 3 and 4 close the other halves and 5 to 7 all",
          "[field-collision-map][field]") {
  CellTables tables;
  CHECK_FALSE(OpenAt(tables, 3, 50, 50));
  CHECK(OpenAt(tables, 3, 100, 50));
  CHECK_FALSE(OpenAt(tables, 4, 50, 60));
  CHECK(OpenAt(tables, 4, 60, 50));
  // Blocked, and the unused passabilities 6 and 7 the jump table skips.
  CHECK_FALSE(OpenAt(tables, 5, 10, 10));
  CHECK_FALSE(OpenAt(tables, 6, 10, 10));
  CHECK_FALSE(OpenAt(tables, 7, 10, 10));
}

TEST_CASE("Floor heights decode and pack, keeping invalid samples' heights",
          "[field-collision-map][field]") {
  const GridMap map;
  using VS::Field::kMissingFloorHeight;
  CHECK(VS::Field::FloorHeightAt(map.bytes, 10, 10) == 640);
  CHECK(VS::Field::FloorHeightAt(map.bytes, 200, 10) == 608);
  // The invalid sample keeps its height and sets bit 15.
  CHECK(VS::Field::FloorHeightAt(map.bytes, 255, 127) == 608 - 0x8000);
  CHECK(VS::Field::FloorHeightAt(map.bytes, 256, 0) == kMissingFloorHeight);
  CHECK(VS::Field::FloorHeightAt(map.bytes, -1, 0) == kMissingFloorHeight);
  CHECK(VS::Field::FloorHeightAt({}, 10, 10) == kMissingFloorHeight);
}

TEST_CASE("Interpolated floor heights smooth toward close neighbours",
          "[field-collision-map][field]") {
  const GridMap map;
  using VS::Field::kMissingFloorHeight;
  CHECK(VS::Field::InterpolatedFloorHeight(map.bytes, 16, 16) == 640);
  // A quarter tile from tile 1's floor, weighted 480 of 1024.
  CHECK(VS::Field::InterpolatedFloorHeight(map.bytes, 127, 16) == 625);
  CHECK(VS::Field::InterpolatedFloorHeight(map.bytes, 16, 128) ==
        kMissingFloorHeight);
  CHECK(VS::Field::InterpolatedFloorHeight(std::span(map.bytes).first(7), 16,
                                           16) == kMissingFloorHeight);
}

TEST_CASE("Ceiling heights pack, unpack and read below the origin as -2032",
          "[field-collision-map][field]") {
  const GridMap map;
  CHECK(VS::Field::CeilingHeightAt(map.bytes, 10, 10) == (-800 & 0x7FFF));
  CHECK(VS::Field::CeilingHeightAt(map.bytes, 200, 10) == -2032);
  CHECK(VS::Field::CeilingHeightAt(map.bytes, 300, 10) == 0);
  CHECK(VS::Field::CeilingHeightAt({}, 10, 10) == 0);
  CHECK(VS::Field::RoomCeilingHeight(map.bytes, 10, 10) == -800);
  CHECK(VS::Field::RoomCeilingHeight(map.bytes, 200, 10) == -2032);
  CHECK(VS::Field::RoomCeilingHeight(map.bytes, 300, 10) == 0);
}

TEST_CASE("Floor and ceiling heights pack together inside the map",
          "[field-collision-map][field]") {
  const GridMap map;
  CHECK(VS::Field::FloorAndCeilingHeightsAt(map.bytes, 10, 10) ==
        VS::Field::FloorAndCeiling{.floor = 640, .ceiling = -800 & 0x7FFF});
  CHECK(VS::Field::FloorAndCeilingHeightsAt(map.bytes, 10, 200) ==
        VS::Field::FloorAndCeiling{});
}

TEST_CASE("Actor support prefers a gimmick placed exactly at the position",
          "[field-collision-map][field]") {
  const GridMap map;
  std::array<std::optional<GimmickSupportState>, 2> gimmicks{};
  CHECK(VS::Field::ActorSupportHeight(map.bytes, gimmicks, 16, 16) == 640);
  auto resting = Gimmick(300);
  resting.x = 16;
  resting.z = 16;
  gimmicks[1] = resting;
  CHECK(VS::Field::ActorSupportHeight(map.bytes, gimmicks, 16, 16) == 172);
  CHECK(VS::Field::ActorSupportHeight(map.bytes, gimmicks, 17, 16) != 172);
  resting.y = 900;
  gimmicks[1] = resting;
  CHECK(VS::Field::ActorSupportHeight(map.bytes, gimmicks, 16, 16) == 640);
}

TEST_CASE("Snapping moves only free-standing room actors to their support",
          "[field-collision-map][field]") {
  const GridMap map;
  const std::array<std::optional<GimmickSupportState>, 2> gimmicks{};
  auto locked = StandingActor();
  locked.motion_locked = true;
  auto on_platform = StandingActor();
  on_platform.tile_layer = 2;
  auto moving = StandingActor();
  moving.movement_mode = 1;
  std::array<std::optional<VS::Field::RoomActorSupport>, 5> actors{
      StandingActor(), locked, on_platform, moving, std::nullopt};
  CHECK(VS::Field::SnapRoomActorsToSupport(map.bytes, gimmicks, actors) == 1);
  CHECK(HeightOf(actors[0]) == 640);
  CHECK(HeightOf(actors[1]) == -5);
  CHECK(HeightOf(actors[2]) == -5);
  CHECK(HeightOf(actors[3]) == -5);
}

TEST_CASE("Gimmick support finds the highest resting gimmick on a tile",
          "[field-collision-map][field]") {
  auto gimmicks = TileGimmicks();
  CHECK(VS::Field::GimmickSupportHeight(gimmicks, 3, 4) ==
        VS::Field::GimmickSupport{.height = -378, .gimmick_index = 5});
  CHECK(VS::Field::GimmickSupportHeight(gimmicks, 4, 4) ==
        VS::Field::GimmickSupport{});
  // Tiles compare as unsigned bytes (the original's lbu).
  auto wide = Gimmick(-10);
  wide.tile_x = 200;
  gimmicks[6] = wide;
  CHECK(VS::Field::GimmickSupportHeight(gimmicks, 200, 4).gimmick_index == 6);
  CHECK(VS::Field::GimmickSupportHeight(gimmicks, -56, 4).gimmick_index == 0);
}

TEST_CASE("Gimmick support below a height lowers falling gimmicks",
          "[field-collision-map][field]") {
  auto gimmicks = TileGimmicks();
  CHECK(VS::Field::GimmickSupportHeightBelow(gimmicks, 3, 4, -260) == -378);
  auto falling = Gimmick(-250);
  falling.falling = true;
  gimmicks[5] = falling;
  CHECK(VS::Field::GimmickSupportHeightBelow(gimmicks, 3, 4, -260) == -328);
  CHECK(VS::Field::GimmickSupportHeightBelow(gimmicks, 3, 4, 0) == 0);
}

TEST_CASE("Moving platforms report contact in search modes 0, 1 and 7",
          "[field-collision-map][field]") {
  const auto platforms = OnePlatform();
  using VS::Field::MovingPlatformContactHeight;
  using VS::Field::PlatformContactHeight;
  const PlatformContactHeight on{.height = -100, .supporting_layer = 3};
  const PlatformContactHeight top{.height = -4, .supporting_layer = 0};
  CHECK(MovingPlatformContactHeight(platforms, {500, 0, 600}, 0, 0) == on);
  CHECK(MovingPlatformContactHeight(platforms, {500, 200, 600}, 0, 0) ==
        PlatformContactHeight{});
  CHECK(MovingPlatformContactHeight(platforms, {500, 200, 600}, 1, 0) == top);
  CHECK(MovingPlatformContactHeight(platforms, {500, 0, 600}, 1, 0) ==
        PlatformContactHeight{});
  CHECK(MovingPlatformContactHeight(platforms, {500, 0, 600}, 7, 0) == on);
  CHECK(MovingPlatformContactHeight(platforms, {564, 0, 600}, 0, 0) ==
        PlatformContactHeight{});
}

TEST_CASE("Moving platforms report contact in search modes 2 and 3",
          "[field-collision-map][field]") {
  const auto platforms = OnePlatform();
  using VS::Field::MovingPlatformContactHeight;
  using VS::Field::PlatformContactHeight;
  const PlatformContactHeight on{.height = -100, .supporting_layer = 3};
  const PlatformContactHeight top{.height = -4, .supporting_layer = 0};
  CHECK(MovingPlatformContactHeight(platforms, {500, 0, 600}, 2, 0) == on);
  CHECK(MovingPlatformContactHeight(platforms, {500, -100, 600}, 2, 0) ==
        PlatformContactHeight{});
  CHECK(MovingPlatformContactHeight(platforms, {500, 0, 600}, 3, 50) ==
        PlatformContactHeight{});
  CHECK(MovingPlatformContactHeight(platforms, {500, 0, 600}, 3, 200) == top);
}

TEST_CASE("Points lie on a moving platform within its reach",
          "[field-collision-map][field]") {
  const auto platforms = OnePlatform();
  using VS::Field::IsPointOnMovingPlatform;
  CHECK(IsPointOnMovingPlatform(platforms, 3, 564, 536));
  CHECK_FALSE(IsPointOnMovingPlatform(platforms, 3, 565, 600));
  // No visible platform on the layer: the original read through null.
  CHECK_FALSE(IsPointOnMovingPlatform(platforms, 2, 500, 600));
}

TEST_CASE("Actor collision bypass bits read and write only filled slots",
          "[field-collision-map][field]") {
  FakeBypassTable table;
  table.slots[3] = {};
  VS::Field::SetActorCollisionBypass(table, 3, 3);
  CHECK(table.slots.at(3) ==
        VS::Field::CollisionBypass{.actor_collision = true, .ignored = true});
  CHECK(VS::Field::ActorCollisionBypass(table, 3) == 3);
  VS::Field::SetActorCollisionBypass(table, 3, 6);
  CHECK(VS::Field::ActorCollisionBypass(table, 3) == 2);
  VS::Field::SetActorCollisionBypass(table, 4, 3);
  CHECK(table.slots.size() == 1);
  CHECK(VS::Field::ActorCollisionBypass(table, 4) == 0);
}

TEST_CASE("Minimap visibility keeps the low byte of the request",
          "[field-collision-map][field]") {
  std::uint8_t visibility = 0;
  CHECK(VS::Field::SetMinimapVisible(visibility, 0x1FF) == 0xFF);
  CHECK(visibility == 0xFF);
  CHECK(VS::Field::SetMinimapVisible(visibility, 0x100) == 0);
}

TEST_CASE("Minimap rotation turns the tilted map about its vertical axis",
          "[field-collision-map][field]") {
  const auto at_zero = VS::Field::BuildMinimapRotation(0);
  CHECK(at_zero[0] == std::array<std::int16_t, 3>{4096, 0, 0});
  CHECK(at_zero[1] == std::array<std::int16_t, 3>{0, 2633, -3138});
  CHECK(at_zero[2] == std::array<std::int16_t, 3>{0, 3138, 2633});
  const auto quarter = VS::Field::BuildMinimapRotation(1024);
  CHECK(quarter[0] == std::array<std::int16_t, 3>{0, 0, 4096});
  CHECK(quarter[1] == std::array<std::int16_t, 3>{3138, 2633, 0});
  CHECK(quarter[2] == std::array<std::int16_t, 3>{-2633, 3138, 0});
}

TEST_CASE("Map completion counts the visited share of countable locations",
          "[field-collision-map][field]") {
  // The first 361 locations count: eleven full words and nine bits.
  VS::Field::RoomVisitFlags countable{};
  std::fill_n(countable.begin(), 11, 0xFFFFFFFFU);
  countable[11] = 0x1FF;
  VS::Field::RoomVisitFlags visits{};
  visits[1] = 0x800000;
  CHECK(VS::Field::MapCompletionPercent(visits, countable) == 0);
  CHECK(visits[1] == 0xC00000);
  visits[0] = 0xFFFFFFFFU;
  visits[1] = 0xFFFFFFFFU;
  CHECK(VS::Field::MapCompletionPercent(visits, countable) == 6400 / 361);
  visits.fill(0xFFFFFFFFU);
  CHECK(VS::Field::MapCompletionPercent(visits, countable) == 100);
}

TEST_CASE("Scene rooms are rewritten with their location's visit bit",
          "[field-collision-map][field]") {
  auto scene = FourRoomScene();
  VS::Field::RoomVisitFlags visits{};
  visits[1] = 0x4;
  FakeLocations locations;
  CHECK(VS::Field::RestoreSceneRoomVisits(scene, visits, locations));
  CHECK(VisitWord(scene, 0) == 0x4);
  CHECK(VisitWord(scene, 1) == 0);
  CHECK(VisitWord(scene, 2) == 0);
  CHECK(VisitWord(scene, 3) == 0);
  CHECK(locations.queries.front() ==
        std::pair<std::int32_t, std::int32_t>{1, 2});
}

TEST_CASE("Scene rooms are left alone when the room count does not fit",
          "[field-collision-map][field]") {
  auto scene = FourRoomScene();
  const auto before = scene;
  const VS::Field::RoomVisitFlags visits{};
  FakeLocations locations;
  SetSceneRoomCount(scene, 5);
  CHECK_FALSE(VS::Field::RestoreSceneRoomVisits(scene, visits, locations));
  SetSceneRoomCount(scene, -1);
  CHECK_FALSE(VS::Field::RestoreSceneRoomVisits(scene, visits, locations));
  CHECK(std::equal(scene.begin() + 4, scene.end(), before.begin() + 4));
  CHECK_FALSE(VS::Field::RestoreSceneRoomVisits(std::span(scene).first(3),
                                                visits, locations));
  CHECK(locations.queries.empty());
}

TEST_CASE("Risk rolls break locks below the risk-derived percent",
          "[field-collision-map][field]") {
  // Risk 0: (150 * 100 / 256) + 20 = 78.
  FixedRandom below(77);
  CHECK(VS::Field::RollRiskLockBreak(0, below));
  FixedRandom at(78);
  CHECK_FALSE(VS::Field::RollRiskLockBreak(0, at));
  // Risk 200 caps the percent at 100, so every draw breaks the lock.
  FixedRandom top(99);
  CHECK(VS::Field::RollRiskLockBreak(200, top));
}

TEST_CASE("Trap panels leave one HP and healing panels restore the amount",
          "[field-collision-map][field]") {
  FixedRandom center(5);
  const auto target = PanelTarget();
  CHECK(VS::Field::CalculateTrapPanelDamage(kPanelHit, target, 2, false,
                                            center) == 119);
  CHECK(VS::Field::CalculateHealingPanelRecovery(kPanelHit, target, 2, false,
                                                 center) == 120);
  auto healthy = target;
  healthy.current_hp = 500;
  CHECK(VS::Field::CalculateTrapPanelDamage(kPanelHit, healthy, 2, false,
                                            center) == 120);
}

TEST_CASE("Panel amounts vary with the averaged draw and never go below 0",
          "[field-collision-map][field]") {
  auto healthy = PanelTarget();
  healthy.current_hp = 500;
  // The averaged draw less 5 varies the amount.
  FixedRandom high(10);
  CHECK(VS::Field::CalculateTrapPanelDamage(kPanelHit, healthy, 2, true,
                                            high) == 125);
  auto armored = healthy;
  armored.limb.armor_strength = 1000;
  FixedRandom low(0);
  CHECK(VS::Field::CalculateTrapPanelDamage(kPanelHit, armored, 2, true, low) ==
        0);
  CHECK(VS::Field::CalculateHealingPanelRecovery(kPanelHit, armored, 2, true,
                                                 low) == 0);
}

TEST_CASE("Panels refuse affinities outside the affinity table",
          "[field-collision-map][field]") {
  FixedRandom center(5);
  const auto target = PanelTarget();
  // Affinity 0 would index row -1 and affinity 8 past the table: refused.
  auto no_affinity = kPanelHit;
  no_affinity.affinity = 0;
  CHECK_FALSE(
      VS::Field::CalculateTrapPanelDamage(no_affinity, target, 2, false, center)
          .has_value());
  no_affinity.affinity = 8;
  CHECK_FALSE(VS::Field::CalculateHealingPanelRecovery(no_affinity, target, 2,
                                                       false, center)
                  .has_value());
}

TEST_CASE("A raised shield's type bytes add as unsigned ratings",
          "[field-collision-map][field]") {
  FixedRandom center(5);
  auto target = PanelTarget();
  target.current_hp = 500;
  target.shield.strength = 10;
  target.shield.types[2] = 200;
  auto hit = kPanelHit;
  hit.damage_type = 2;
  // Unraised: defense 30. Raised: (20 * 100 + 10 * 300 + 10 * 100) / 100.
  CHECK(VS::Field::CalculateTrapPanelDamage(hit, target, 2, false, center) ==
        120);
  target.shield_raised = true;
  CHECK(VS::Field::CalculateTrapPanelDamage(hit, target, 2, false, center) ==
        90);
}

TEST_CASE("The live collision binding reads the current room's sections",
          "[field-collision-map][field]") {
  const LiveRoom room;
  CHECK(VS::Field::LiveRoomCollisionCells() == Field_roomData.tileCells);
  CHECK(VS::Field::LiveCollisionMapDimensions(0) == ((1 << 20) | 32));
  const Field_ZoneBounds* live_bounds = VS::Field::LiveRoomCameraBounds(0);
  REQUIRE(live_bounds != nullptr);
  CHECK(live_bounds->maxZ == 400);
}

TEST_CASE("The live collision binding reads cells inside the room's table",
          "[field-collision-map][field]") {
  const LiveRoom room;
  CHECK(VS::Field::LiveCollisionCellAt(1, 0, 0) ==
        Field_roomData.tileCells + 1);
  CHECK(VS::Field::LiveCollisionCellAt(0, 1, 0) == nullptr);
  CHECK(VS::Field::LiveCollisionCellAt(0, 0, 2) == nullptr);
}

TEST_CASE("Unchecked cell readers see a missing cell as the absent word",
          "[field-collision-map][field][issue-288]") {
  const LiveRoom room;
  CHECK(VS::Field::LiveCollisionCellOrAbsent(1, 0, 0) ==
        Field_roomData.tileCells + 1);
  const Field_MpdTileCell* off_grid =
      VS::Field::LiveCollisionCellOrAbsent(0, 1, 0);
  REQUIRE(off_grid != nullptr);
  CHECK(VS::Field::LiveCollisionCellOrAbsent(0, 0, 2) == off_grid);
  std::uint32_t word = 0;
  std::memcpy(&word, off_grid, sizeof word);
  CHECK(word == VS::Field::kAbsentCellWord);
  CHECK(off_grid->surfaceKind == 3);
  const bool any_flag =
      off_grid->isPit || off_grid->hasEventTrigger ||
      off_grid->hasRoomTrigger || off_grid->blocksSphereTerrain ||
      off_grid->doorPassability != 0 || off_grid->occlusionAndFloorFlags != 0;
  CHECK_FALSE(any_flag);
}

TEST_CASE("The live collision binding blocks and finds the room's cells",
          "[field-collision-map][field]") {
  LiveRoom room;
  CHECK(VS::Field::SetLiveCollisionCellBlocked(0, 0, 0, 1) == 1);
  CHECK(room.cells[0] == PassabilityWord(5));
  CHECK(VS::Field::SetLiveCollisionCellBlocked(0, 0, 1, 1) == 0);
  const SVECTOR position{.vx = 200, .vy = 0, .vz = 10, .pad = 0};
  CHECK(VS::Field::FindLiveCollisionCellAtPosition(&position, 0) ==
        Field_roomData.tileCells + 1);
  CHECK(VS::Field::FindLiveCollisionCellAtPosition(nullptr, 0) == nullptr);
}

TEST_CASE("The live collision binding reads the room's heights",
          "[field-collision-map][field]") {
  const LiveRoom room;
  CHECK(VS::Field::LiveFloorHeightAt(10, 10) == 640);
  CHECK(VS::Field::LiveRoomCeilingHeight(10, 10) == -800);
  CHECK(VS::Field::LiveFloorAndCeilingHeightsAt(10, 10) ==
        ((0x7CE0 << 16) | 640));
}

TEST_CASE("The live bypass binding writes and reads a filled actor slot",
          "[field-collision-map][field]") {
  const SavedGlobal saved_actors(Actors_actorRecords);
  auto actor = std::make_unique<Actors_ActorRecord>();
  Actors_actorRecords[5] = actor.get();
  VS::Field::SetLiveActorCollisionBypass(5, 3);
  CHECK(actor->bypassActorCollision == 1);
  CHECK(actor->ignoredByCollision == 1);
  CHECK(VS::Field::LiveActorCollisionBypass(5) == 3);
  VS::Field::SetLiveActorCollisionBypass(5, 2);
  CHECK(VS::Field::LiveActorCollisionBypass(5) == 2);
}

TEST_CASE("The live bypass binding refuses slots outside the actor table",
          "[field-collision-map][field]") {
  const SavedGlobal saved_actors(Actors_actorRecords);
  Actors_actorRecords[6] = nullptr;
  CHECK(VS::Field::LiveActorCollisionBypass(6) == 0);
  VS::Field::SetLiveActorCollisionBypass(6, 3);
  CHECK(VS::Field::LiveActorCollisionBypass(6) == 0);
  CHECK(VS::Field::LiveActorCollisionBypass(-1) == 0);
  CHECK(VS::Field::LiveActorCollisionBypass(17) == 0);
  VS::Field::SetLiveActorCollisionBypass(-1, 3);
  VS::Field::SetLiveActorCollisionBypass(17, 3);
}

TEST_CASE("The live map binding updates the minimap",
          "[field-collision-map][field]") {
  const SavedGlobal saved_visible(Field_isMinimapVisible);
  CHECK(VS::Field::SetLiveMinimapVisible(0x101) == 1);
  CHECK(Field_isMinimapVisible == 1);
  MATRIX rotation{};
  VS::Field::BuildLiveMinimapRotation(&rotation, 0);
  CHECK(rotation.m[0][0] == 4096);
  CHECK(rotation.m[2][2] == 2633);
  VS::Field::BuildLiveMinimapRotation(nullptr, 0);
}

TEST_CASE("The live map binding marks visits while counting completion",
          "[field-collision-map][field]") {
  const SavedGlobal saved_progress(State_roomVisitProgress);
  const SavedGlobal saved_countable(Field_countableRoomMask);
  std::memset(&State_roomVisitProgress.roomFlags, 0,
              sizeof State_roomVisitProgress.roomFlags);
  std::memset(&Field_countableRoomMask, 0, sizeof Field_countableRoomMask);
  Field_countableRoomMask[1] = 0x400000;
  State_roomVisitProgress.roomFlags[1] = 0x800000;
  CHECK(VS::Field::LiveMapCompletionPercent() == 0);
  CHECK(State_roomVisitProgress.roomFlags[1] == 0xC00000);
  VS::Field::RestoreLiveSceneRoomVisits(nullptr, 0);
}

TEST_CASE("The live trap binding reads the struck actor's limb",
          "[field-collision-map][field]") {
  LiveTrap trap;
  CHECK(VS::Field::CalculateLiveTrapPanelDamage(&trap.action, nullptr,
                                                &trap.struck, 1, 0) == 119);
  CHECK(VS::Field::CalculateLiveHealingPanelRecovery(
            &trap.action, nullptr, &trap.struck, 1, 0) == 120);
}

TEST_CASE("The live trap binding refuses actions, hits, limbs and actors",
          "[field-collision-map][field]") {
  LiveTrap trap;
  // Defined refusals: no action, a third hit, a limb past six, an empty slot.
  CHECK(VS::Field::CalculateLiveTrapPanelDamage(nullptr, nullptr, &trap.struck,
                                                1, 0) == 0);
  CHECK(VS::Field::CalculateLiveTrapPanelDamage(&trap.action, nullptr,
                                                &trap.struck, 2, 0) == 0);
  trap.struck.target.targetLimb = 6;
  CHECK(VS::Field::CalculateLiveTrapPanelDamage(&trap.action, nullptr,
                                                &trap.struck, 1, 0) == 0);
  trap.struck.target.targetLimb = 1;
  trap.struck.target.targetActor = 3;
  CHECK(VS::Field::CalculateLiveHealingPanelRecovery(&trap.action, nullptr,
                                                     &trap.struck, 1, 0) == 0);
}

TEST_CASE("Row 23 module entries keep only the trap stat calculators",
          "[field-collision-map][field][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](std::uint32_t address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x8008255CU) ==
        reinterpret_cast<void*>(&VS::Field::CalculateLiveTrapPanelDamage));
  CHECK(resolve(0x80082FF4U) ==
        reinterpret_cast<void*>(&VS::Field::CalculateLiveHealingPanelRecovery));
  constexpr std::array<std::uint32_t, 24> kRetired{
      0x8007F434U, 0x8008B4C8U, 0x8008B4D8U, 0x8008B590U, 0x8008B6ECU,
      0x8008B70CU, 0x8008B744U, 0x8008B764U, 0x8008B808U, 0x8008DA24U,
      0x8008DC7CU, 0x8008DD0CU, 0x8008DDA8U, 0x8008E5F0U, 0x800A0A1CU,
      0x800A0A74U, 0x800A3500U, 0x800A35A8U, 0x800A3BC4U, 0x800A51A0U,
      0x800A8D64U, 0x800A92B8U, 0x800A9378U, 0x800B13CCU};
  for (const std::uint32_t address : kRetired) {
    CAPTURE(address);
    CHECK(resolve(address) == nullptr);
  }
}
