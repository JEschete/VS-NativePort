// Hand-maintained tests for the typed actor terrain collision rules
// (PROMOTION_PLAN rows 21 and 23, #276; the absent probe cell is #288).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Field/ActorCollision.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/CollisionMap.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
}

namespace {
using VS::Field::ActorCollisionHost;
using VS::Field::CollisionActor;
using VS::Field::CollisionCellRef;
using VS::Field::CollisionCellSource;
using VS::Field::CollisionProbe;
using VS::Field::CollisionSamples;
using VS::Field::GimmickSupport;
using VS::Field::GimmickTile;
using VS::Field::MotionVector;
using VS::Field::PlatformContact;
using VS::Field::PlatformContactHeight;
using VS::Field::PlatformProbe;
using VS::Field::SupportSample;
using VS::Field::TerrainMotion;
using VS::Field::TerrainMotionResult;

constexpr int kTileSize = 128;
constexpr int kTiles = 4;
constexpr std::uint32_t kAttributeBit = 1U << 5;
constexpr std::uint32_t kBlockedBit = 1U << 17;

// A height as the room tables hold it: 15 bits, the top bit clear.
constexpr std::int16_t Height(int value) {
  return static_cast<std::int16_t>(value & 0x7FFF);
}

constexpr std::size_t TileIndex(int tile_x, int tile_z) {
  return (static_cast<std::size_t>(tile_z) * std::size_t{kTiles}) +
         static_cast<std::size_t>(tile_x);
}

struct FakeTile {
  bool present = true;
  bool stale = false;
  std::uint32_t word = 0;
  std::int16_t floor = 0;
  std::int16_t smooth = 0;
  std::int16_t ceiling = Height(-1024);
  GimmickSupport gimmick{};
  std::int32_t gimmick_below = 0;
  PlatformContactHeight platform{};
};

// A 4x4 tile room with an actor table, gimmick tiles and visible platforms.
class FakeCollisionHost final : public ActorCollisionHost {
 public:
  std::array<FakeTile, static_cast<std::size_t>(kTiles* kTiles)> tiles{};
  std::array<std::optional<CollisionActor>, VS::Field::kCollisionActorSlots>
      actors{};
  std::map<std::uint8_t, GimmickTile> gimmick_tiles;
  std::map<std::int32_t, PlatformContact> visible;
  std::array<std::uint8_t, 32> surface_ids{};
  std::vector<CollisionActor> stored;
  std::vector<std::pair<std::int32_t, std::int32_t>> contacts;
  CollisionSamples samples;
  CollisionProbe probe;

  FakeTile& Tile(int tile_x, int tile_z) {
    return tiles.at(TileIndex(tile_x, tile_z));
  }
  void SetFloor(int tile_x, int tile_z, int height) {
    Tile(tile_x, tile_z).floor = Height(height);
    Tile(tile_x, tile_z).smooth = Height(height);
  }
  void Place(const CollisionActor& actor) {
    actors.at(static_cast<std::size_t>(actor.table_index)) = actor;
  }

  [[nodiscard]] std::optional<CollisionCellRef> CellAt(
      std::int16_t world_x, std::int16_t world_z,
      std::int32_t /*layer*/) const override {
    const std::optional<std::size_t> index = Index(world_x, world_z);
    if (!index || !tiles.at(*index).present) return std::nullopt;
    return CollisionCellRef{.source = CollisionCellSource::kRoom,
                            .index = *index};
  }
  [[nodiscard]] std::optional<std::uint32_t> CellWord(
      CollisionCellRef cell) const override {
    const FakeTile& tile = tiles.at(cell.index);
    if (tile.stale) return std::nullopt;
    return tile.word;
  }
  [[nodiscard]] std::int16_t FloorHeight(std::int32_t world_x,
                                         std::int32_t world_z) const override {
    const FakeTile* tile = At(world_x, world_z);
    return tile == nullptr ? std::int16_t{-1} : tile->floor;
  }
  [[nodiscard]] std::int16_t InterpolatedFloorHeight(
      std::int32_t world_x, std::int32_t world_z) const override {
    const FakeTile* tile = At(world_x, world_z);
    return tile == nullptr ? std::int16_t{-1} : tile->smooth;
  }
  [[nodiscard]] std::int16_t CeilingHeight(
      std::int32_t world_x, std::int32_t world_z) const override {
    const FakeTile* tile = At(world_x, world_z);
    return tile == nullptr ? Height(-1024) : tile->ceiling;
  }
  GimmickSupport SupportingGimmick(std::int32_t tile_x,
                                   std::int32_t tile_z) override {
    const FakeTile* tile = At(tile_x * kTileSize, tile_z * kTileSize);
    return tile == nullptr ? GimmickSupport{} : tile->gimmick;
  }
  [[nodiscard]] std::int32_t GimmickSupportBelow(
      std::int32_t tile_x, std::int32_t tile_z,
      std::int32_t /*reference_height*/) const override {
    const FakeTile* tile = At(tile_x * kTileSize, tile_z * kTileSize);
    return tile == nullptr ? 0 : tile->gimmick_below;
  }
  PlatformContactHeight PlatformContactAt(PlatformProbe at) override {
    const FakeTile* tile = At(at.x, at.z);
    return tile == nullptr ? PlatformContactHeight{} : tile->platform;
  }
  [[nodiscard]] std::optional<PlatformContact> VisiblePlatform(
      std::int32_t support_layer) const override {
    const auto found = visible.find(support_layer);
    if (found == visible.end()) return std::nullopt;
    return found->second;
  }
  [[nodiscard]] std::uint8_t TileSurfaceId(
      std::uint32_t surface) const override {
    return surface_ids.at(surface);
  }
  [[nodiscard]] std::optional<CollisionActor> Actor(
      std::size_t index) const override {
    return actors.at(index);
  }
  void StoreActor(const CollisionActor& actor) override {
    stored.push_back(actor);
    if (actor.table_index >= 0) {
      actors.at(static_cast<std::size_t>(actor.table_index)) = actor;
    }
  }
  [[nodiscard]] std::optional<GimmickTile> SupportGimmickTile(
      std::uint8_t slot) const override {
    const auto found = gimmick_tiles.find(slot);
    if (found == gimmick_tiles.end()) return std::nullopt;
    return found->second;
  }
  void UpdatePlatformContact(std::int32_t actor_index,
                             std::int32_t mode) override {
    contacts.emplace_back(actor_index, mode);
  }
  CollisionSamples& Samples() override { return samples; }
  CollisionProbe& Probe() override { return probe; }

 private:
  [[nodiscard]] static std::optional<std::size_t> Index(std::int32_t world_x,
                                                        std::int32_t world_z) {
    if (world_x < 0 || world_z < 0) return std::nullopt;
    const std::int32_t tile_x = world_x / kTileSize;
    const std::int32_t tile_z = world_z / kTileSize;
    if (tile_x >= kTiles || tile_z >= kTiles) return std::nullopt;
    return TileIndex(tile_x, tile_z);
  }
  [[nodiscard]] const FakeTile* At(std::int32_t world_x,
                                   std::int32_t world_z) const {
    const std::optional<std::size_t> index = Index(world_x, world_z);
    if (!index || !tiles.at(*index).present) return nullptr;
    return &tiles.at(*index);
  }
};

CollisionActor Mover(std::int16_t x, std::int16_t y, std::int16_t z) {
  CollisionActor actor;
  actor.table_index = 0;
  actor.position = {.x = x, .y = y, .z = z};
  actor.collision_radius = 32;
  actor.occupancy_radius = 32;
  actor.occupancy_height = 128;
  actor.model_id = 1;
  return actor;
}

CollisionCellRef Room(int tile_x, int tile_z) {
  return {.source = CollisionCellSource::kRoom,
          .index = TileIndex(tile_x, tile_z)};
}
}  // namespace

TEST_CASE("Collision diagonal reach is the radius at 45 degrees",
          "[field-actor-collision][field]") {
  CHECK(VS::Field::CollisionDiagonalReach(32) == 22);
  CHECK(VS::Field::CollisionDiagonalReach(100) == 70);
  CHECK(VS::Field::CollisionDiagonalReach(0) == 0);
}

TEST_CASE("Support samples keep the last cell they found",
          "[field-actor-collision][field]") {
  FakeCollisionHost host;
  host.SetFloor(1, 1, -64);
  host.Tile(1, 2).present = false;
  const MotionVector origin{.x = 192, .y = 0, .z = 192};

  CHECK(VS::Field::SampleActorSupportHeight(
            host, origin, 0, 0, SupportSample::kInterpolated) == -64);
  REQUIRE(host.probe.cell == Room(1, 1));

  CHECK(VS::Field::SampleActorSupportHeight(host, origin, 0, kTileSize,
                                            SupportSample::kInterpolated) ==
        VS::Field::kNoSupportHeight);
  CHECK(host.probe.cell == Room(1, 1));
  CHECK(VS::Field::SampleActorSupportHeight(
            host, origin, 0, kTileSize, SupportSample::kShadow) == origin.y);
  CHECK(host.probe.cell == Room(1, 1));
}

namespace {
// A support sample under the centre of tile (1, 1).
struct SupportSampleFixture {
  FakeCollisionHost host;
  MotionVector origin{.x = 192, .y = 0, .z = 192};
  FakeTile& tile = host.Tile(1, 1);

  std::int32_t Sample(SupportSample mode) {
    return VS::Field::SampleActorSupportHeight(host, origin, 0, 0, mode);
  }
};
}  // namespace

TEST_CASE(
    "A blocked support cell reads its fixed height unless "
    "shadowless",
    "[field-actor-collision][field]") {
  SupportSampleFixture fixture;
  fixture.tile.word = kBlockedBit | 3U;
  CHECK(fixture.Sample(SupportSample::kInterpolated) ==
        VS::Field::kBlockedSupportHeight);
  CHECK(fixture.Sample(SupportSample::kShadow) ==
        VS::Field::kBlockedSupportHeight);
  fixture.host.surface_ids.at(3) = 0x14;
  CHECK(fixture.Sample(SupportSample::kShadow) == 0);
  CHECK(fixture.Sample(SupportSample::kInterpolated) ==
        VS::Field::kBlockedSupportHeight);
}

TEST_CASE(
    "A resting gimmick replaces the support floor and below-reference asks "
    "lower",
    "[field-actor-collision][field]") {
  SupportSampleFixture fixture;
  fixture.tile.gimmick = {.height = -128, .gimmick_index = 4};
  fixture.tile.gimmick_below = -64;
  CHECK(fixture.Sample(SupportSample::kInterpolated) == -128);
  CHECK(fixture.Sample(SupportSample::kNoPlatforms) == -128);
  CHECK(fixture.Sample(SupportSample::kBelowReference) == -64);
}

TEST_CASE(
    "A platform above the floor supports all but the "
    "platformless samples",
    "[field-actor-collision][field]") {
  SupportSampleFixture fixture;
  fixture.tile.platform = {.height = -32, .supporting_layer = 3};
  CHECK(fixture.Sample(SupportSample::kFloor) == -32);
  CHECK(fixture.Sample(SupportSample::kInterpolated) == -32);
  CHECK(fixture.Sample(SupportSample::kShadow) == -32);
  CHECK(fixture.Sample(SupportSample::kNoPlatforms) == 0);
  CHECK(fixture.Sample(SupportSample::kBelowReference) == 0);
}

TEST_CASE("A shadow sample ignores a platform far above the origin",
          "[field-actor-collision][field]") {
  SupportSampleFixture fixture;
  fixture.tile.platform = {.height = -100, .supporting_layer = 3};
  CHECK(fixture.Sample(SupportSample::kInterpolated) == -100);
  CHECK(fixture.Sample(SupportSample::kShadow) == 0);
}

TEST_CASE("The floor support sample reads the unsmoothed height",
          "[field-actor-collision][field]") {
  SupportSampleFixture fixture;
  fixture.tile.floor = Height(-16);
  fixture.tile.smooth = Height(-24);
  CHECK(fixture.Sample(SupportSample::kFloor) == -16);
  CHECK(fixture.Sample(SupportSample::kInterpolated) == -24);
}

TEST_CASE("A floor with its top bit set has no support",
          "[field-actor-collision][field]") {
  SupportSampleFixture fixture;
  fixture.tile.floor = -1;
  fixture.tile.smooth = -1;
  CHECK(fixture.Sample(SupportSample::kFloor) == VS::Field::kNoSupportHeight);
  CHECK(fixture.Sample(SupportSample::kShadow) == fixture.origin.y);
}

namespace {
template <typename Value>
constexpr std::array<Value, 8> EverySectorHolds(Value value) {
  std::array<Value, 8> values{};
  values.fill(value);
  return values;
}

constexpr auto kLevelPerimeter = EverySectorHolds<std::int16_t>(-200);
constexpr auto kEverySector = EverySectorHolds<std::uint8_t>(1);

// A wide actor over level ground 200 below it, every cell with attribute
// bit 5 set.
struct PerimeterFixture {
  FakeCollisionHost host;
  CollisionActor actor = Mover(192, 0, 192);

  PerimeterFixture() {
    for (FakeTile& tile : host.tiles) {
      tile.word = kAttributeBit;
      tile.floor = Height(-200);
    }
    actor.collision_radius = 100;
  }
  void Sample() {
    VS::Field::SampleActorCollisionPerimeter(
        host, actor, VS::Field::CollisionDiagonalReach(100));
  }
};
}  // namespace

TEST_CASE(
    "Perimeter samples read an absent centre cell as attribute "
    "clear",
    "[field-actor-collision][field][issue-288]") {
  PerimeterFixture fixture;
  fixture.host.Tile(1, 1).present = false;
  fixture.Sample();
  CHECK(fixture.host.samples.center_attribute == 0);
  CHECK(fixture.host.samples.blocked == kEverySector);
  CHECK(fixture.actor.wall_contact);
  CHECK(fixture.host.samples.heights == kLevelPerimeter);
}

TEST_CASE(
    "Perimeter samples read a stale centre cell as attribute "
    "clear",
    "[field-actor-collision][field][issue-288]") {
  PerimeterFixture fixture;
  fixture.host.Tile(1, 1).stale = true;
  fixture.Sample();
  CHECK(fixture.host.samples.center_attribute == 0);
  CHECK(fixture.actor.wall_contact);
  CHECK(fixture.host.samples.heights == kLevelPerimeter);
}

TEST_CASE(
    "A centre cell with the attribute lets the perimeter sectors "
    "pass",
    "[field-actor-collision][field]") {
  PerimeterFixture fixture;
  fixture.Sample();
  CHECK(fixture.host.samples.center_attribute == 1);
  CHECK(fixture.host.samples.attributes == kEverySector);
  CHECK(fixture.host.samples.blocked == std::array<std::uint8_t, 8>{});
  CHECK_FALSE(fixture.actor.wall_contact);
  CHECK(fixture.host.samples.heights == kLevelPerimeter);
}

TEST_CASE("Perimeter samples skip walls for bypassing and hanging actors",
          "[field-actor-collision][field]") {
  FakeCollisionHost host;
  for (FakeTile& tile : host.tiles) tile.floor = Height(-200);
  CollisionActor actor = Mover(192, 40, 192);
  host.samples.blocked.fill(1);
  SECTION("Bypassing terrain zeroes the heights") {
    actor.bypass_terrain_collision = true;
    VS::Field::SampleActorCollisionPerimeter(host, actor, 22);
    CHECK(host.samples.heights == std::array<std::int16_t, 8>{});
  }
  SECTION("Hanging holds every sector at the actor's height") {
    actor.locomotion_state = 2;
    VS::Field::SampleActorCollisionPerimeter(host, actor, 22);
    CHECK(host.samples.heights ==
          std::array<std::int16_t, 8>{40, 40, 40, 40, 40, 40, 40, 40});
  }
  CHECK(host.samples.blocked == std::array<std::uint8_t, 8>{});
  CHECK_FALSE(actor.wall_contact);
}

TEST_CASE("Turn sectors past the eight heights land on the blocked flags",
          "[field-actor-collision][field]") {
  // A ledge 200 up along -Z (sectors 7, 0 and 1) with level ground elsewhere.
  constexpr std::int16_t kY = 0x0302;
  FakeCollisionHost host;
  for (int tile_z = 0; tile_z < kTiles; ++tile_z) {
    for (int tile_x = 0; tile_x < kTiles; ++tile_x) {
      host.SetFloor(tile_x, tile_z, tile_z == 0 ? kY - 200 : kY);
    }
  }
  CollisionActor actor = Mover(128, kY, 128);
  actor.movement_mode = 1;
  actor.turn_distance = 1;

  SECTION("A turn toward sector 0 lowers its height to the actor's") {
    actor.turn_direction = 0;
    VS::Field::SampleActorCollisionPerimeter(host, actor, 22);
    CHECK(host.samples.heights.at(0) == kY);
    CHECK(host.samples.blocked ==
          std::array<std::uint8_t, 8>{1, 1, 0, 0, 0, 0, 0, 1});
  }
  SECTION("A turn that rounds to sector 8 writes the first two flags") {
    actor.turn_direction = 4096;
    VS::Field::SampleActorCollisionPerimeter(host, actor, 22);
    CHECK(host.samples.heights.at(0) == kY - 200);
    CHECK(host.samples.blocked ==
          std::array<std::uint8_t, 8>{0x02, 0x03, 0, 0, 0, 0, 0, 1});
  }
  CHECK_FALSE(actor.wall_contact);
}

TEST_CASE("Wall repulsion pushes away from the marked sectors",
          "[field-actor-collision][field]") {
  CollisionSamples samples;
  CollisionActor actor = Mover(0, 0, 0);
  actor.platform_offset_x = 5;
  actor.platform_offset_z = -5;
  MotionVector motion{.x = -10, .y = 3, .z = 5};

  SECTION("Without wall contact the motion is kept") {
    samples.blocked.at(0) = 1;
    VS::Field::ApplyActorWallRepulsion(samples, actor, motion);
    CHECK(motion == MotionVector{.x = -10, .y = 3, .z = 5});
    CHECK(actor.platform_offset_x == 5);
  }
  SECTION("Pushes add up, clamp and stop motion into the wall") {
    actor.wall_contact = true;
    samples.blocked.at(0) = 1;
    samples.blocked.at(1) = 1;
    samples.blocked.at(2) = 1;
    VS::Field::ApplyActorWallRepulsion(samples, actor, motion);
    CHECK(motion == MotionVector{.x = 16, .y = 3, .z = 21});
    CHECK(actor.platform_offset_x == 0);
    CHECK(actor.platform_offset_z == 0);
  }
}

TEST_CASE("Terrain probes refuse what the actor cannot stand on",
          "[field-actor-collision][field]") {
  FakeCollisionHost host;
  host.samples.search_floor = -0xC0;
  host.samples.search_ceiling = -0x100;
  const CollisionActor mover = Mover(192, 0, 192);
  MotionVector probe{.x = 192, .y = 99, .z = 192};
  FakeTile& tile = host.Tile(1, 1);

  SECTION("A missing cell") {
    tile.present = false;
    CHECK(VS::Field::ClassifyActorTerrainProbe(host, mover, probe, 0) ==
          VS::Field::kRefusedProbe);
  }
  SECTION("A low ceiling") {
    tile.ceiling = Height(-150);
    CHECK(VS::Field::ClassifyActorTerrainProbe(host, mover, probe, 0) ==
          VS::Field::kRefusedProbe);
  }
  SECTION("A rise past the probe reach") {
    host.SetFloor(1, 1, -400);
    CHECK(VS::Field::ClassifyActorTerrainProbe(host, mover, probe, 0) ==
          VS::Field::kRefusedProbe);
  }
  SECTION("A rise past the search ceiling") {
    host.SetFloor(1, 1, -300);
    CHECK(VS::Field::ClassifyActorTerrainProbe(host, mover, probe, 0) ==
          VS::Field::kRefusedProbe);
  }
  SECTION("An occupied position") {
    CollisionActor other = Mover(200, 0, 200);
    other.table_index = 5;
    host.actors.at(5) = other;
    CHECK(VS::Field::ClassifyActorTerrainProbe(host, mover, probe, 0) ==
          VS::Field::kRefusedProbe);
  }
  CHECK(probe.y == 99);
}

namespace {
// A probe at the mover's own position over tile (1, 1).
struct TerrainProbeFixture {
  FakeCollisionHost host;
  CollisionActor mover = Mover(192, 0, 192);
  MotionVector probe{.x = 192, .y = 0, .z = 192};
  FakeTile& tile = host.Tile(1, 1);

  TerrainProbeFixture() {
    host.samples.search_floor = -0xC0;
    host.samples.search_ceiling = -0x100;
  }
  std::int32_t Classify() {
    return VS::Field::ClassifyActorTerrainProbe(host, mover, probe, 0);
  }
};
}  // namespace

TEST_CASE("Terrain probes on level ground and drops climb nothing",
          "[field-actor-collision][field]") {
  TerrainProbeFixture fixture;
  CHECK(fixture.Classify() == 0);
  fixture.host.SetFloor(1, 1, 64);
  CHECK(fixture.Classify() == 0);
  CHECK(fixture.probe.y == 64);
  CHECK(fixture.probe.w == 0);
}

TEST_CASE("Terrain probe rises climb in started 64-unit steps",
          "[field-actor-collision][field]") {
  TerrainProbeFixture fixture;
  fixture.host.SetFloor(1, 1, -64);
  CHECK(fixture.Classify() == 1);
  fixture.host.SetFloor(1, 1, -100);
  CHECK(fixture.Classify() == 2);
  CHECK(fixture.probe.y == -100);
}

TEST_CASE(
    "A terrain probe's support word holds the platform layer and "
    "gimmick slot",
    "[field-actor-collision][field]") {
  TerrainProbeFixture fixture;
  fixture.tile.gimmick = {.height = -120, .gimmick_index = 4};
  fixture.tile.platform = {.height = -150, .supporting_layer = 3};
  CHECK(fixture.Classify() == 3);
  CHECK(fixture.probe.y == -150);
  CHECK(fixture.probe.w == 0x0403);
}

TEST_CASE(
    "A terrain probe reads a blocked cell's fixed height and bypassing "
    "skips the probe",
    "[field-actor-collision][field]") {
  TerrainProbeFixture fixture;
  fixture.tile.word = kBlockedBit;
  CHECK(fixture.Classify() == 0);
  CHECK(fixture.probe.y == VS::Field::kBlockedSupportHeight);
  fixture.mover.bypass_terrain_collision = true;
  fixture.tile.present = false;
  CHECK(fixture.Classify() == 0);
}

TEST_CASE("Occupancy ignores intangible, excluded and non-leader movers",
          "[field-actor-collision][field]") {
  FakeCollisionHost host;
  CollisionActor mover = Mover(200, 0, 200);
  mover.table_index = 3;
  CollisionActor other = Mover(230, 0, 200);
  other.table_index = 4;
  host.actors.at(4) = other;
  const MotionVector at{.x = 230, .y = 0, .z = 200};

  CHECK_FALSE(VS::Field::IsActorPositionUnoccupied(host, mover, at));
  SECTION("Another actor slot moves through") {
    mover.slot = 1;
    CHECK(VS::Field::IsActorPositionUnoccupied(host, mover, at));
  }
  SECTION("A skipped or detached actor is no obstacle") {
    other.skipped = true;
    host.Place(other);
    CHECK(VS::Field::IsActorPositionUnoccupied(host, mover, at));
    other.skipped = false;
    other.attach_bone = 0xFC;
    host.Place(other);
    CHECK(VS::Field::IsActorPositionUnoccupied(host, mover, at));
  }
  SECTION("An actor the move already overlaps is excluded") {
    host.samples.excluded.at(4) = true;
    CHECK(VS::Field::IsActorPositionUnoccupied(host, mover, at));
  }
  SECTION("An actor out of height reach is no obstacle") {
    other.position.y = 300;
    host.Place(other);
    CHECK(VS::Field::IsActorPositionUnoccupied(host, mover, at));
  }
}

TEST_CASE("Overlapping actors may only be separated, not approached",
          "[field-actor-collision][field]") {
  FakeCollisionHost host;
  CollisionActor mover = Mover(200, 0, 200);
  mover.table_index = 3;
  CollisionActor other = Mover(230, 0, 200);
  other.table_index = 4;
  host.actors.at(4) = other;

  CHECK_FALSE(VS::Field::CanActorSeparateFromOverlaps(
      host, mover, MotionVector{.x = 8, .y = 0, .z = 0}));
  CHECK(host.samples.excluded.at(4));

  host.samples.excluded.fill(false);
  CHECK(VS::Field::CanActorSeparateFromOverlaps(
      host, mover, MotionVector{.x = -8, .y = 0, .z = 0}));
  CHECK(host.samples.excluded.at(4));
  // The mover's own slot is tested with its in-flight values.
  CHECK(host.samples.excluded.at(3));

  mover.ignored_by_collision = true;
  CHECK(VS::Field::CanActorSeparateFromOverlaps(
      host, mover, MotionVector{.x = 8, .y = 0, .z = 0}));
  CHECK_FALSE(host.samples.excluded.at(4));
}

namespace {
// Actor 5 stands on tile (3, 4) at height 0.
struct OccupiedTileFixture {
  FakeCollisionHost host;
  CollisionActor standing = Mover(0, 0, 0);

  OccupiedTileFixture() {
    standing.table_index = 5;
    standing.tile_x = 3;
    standing.tile_z = 4;
    host.Place(standing);
  }
  std::optional<std::size_t> Find(int tile_x, int tile_z, int height,
                                  int mode) {
    return VS::Field::FindActorOccupyingTile(host, {.tile_x = tile_x,
                                                    .tile_z = tile_z,
                                                    .minimum_height = height,
                                                    .mode = mode});
  }
};
}  // namespace

TEST_CASE("Occupied tile search finds an actor within height reach",
          "[field-actor-collision][field]") {
  OccupiedTileFixture fixture;
  CHECK(fixture.Find(3, 4, 100, 0) == 5U);
  CHECK(fixture.Find(3, 4, 300, 0) == std::nullopt);
  CHECK(fixture.Find(3, 4, 100, 6) == std::nullopt);
}

TEST_CASE("Hanging, held and hidden actors do not occupy their tile",
          "[field-actor-collision][field]") {
  OccupiedTileFixture fixture;
  fixture.standing.locomotion_state = 2;
  fixture.host.Place(fixture.standing);
  CHECK(fixture.Find(3, 4, 100, 0) == std::nullopt);
  fixture.standing.locomotion_state = 0;
  fixture.standing.motion_locked = true;
  fixture.host.Place(fixture.standing);
  CHECK(fixture.Find(3, 4, 100, 0) == std::nullopt);
  fixture.standing.motion_locked = false;
  fixture.standing.model_id = 0x7F;
  fixture.host.Place(fixture.standing);
  CHECK(fixture.Find(3, 4, 100, 0) == std::nullopt);
}

TEST_CASE(
    "Occupied tile search mode 1 matches a moving actor by its "
    "route target",
    "[field-actor-collision][field]") {
  OccupiedTileFixture fixture;
  fixture.standing.movement_mode = 1;
  fixture.standing.route_target = {.x = 7, .y = 8};
  fixture.host.Place(fixture.standing);
  CHECK(fixture.Find(7, 8, 0, 1) == 5U);
  CHECK(fixture.Find(3, 4, 0, 1) == std::nullopt);
  CHECK(fixture.Find(3, 4, 0, 0) == 5U);
}

TEST_CASE("The leader occupies its support gimmick's tile",
          "[field-actor-collision][field]") {
  OccupiedTileFixture fixture;
  CollisionActor leader = Mover(0, 0, 0);
  leader.support_gimmick = 2;
  leader.tile_x = 1;
  leader.tile_z = 1;
  fixture.host.Place(leader);
  fixture.host.gimmick_tiles[2] = GimmickTile{.x = 6, .z = 9};
  CHECK(fixture.Find(6, 9, 0, 0) == 0U);
  CHECK(fixture.Find(1, 1, 0, 0) == std::nullopt);
  fixture.host.gimmick_tiles.clear();
  CHECK(fixture.Find(6, 9, 0, 0) == std::nullopt);
}

TEST_CASE("Platforms reaching an actor obstruct and drop a hanging actor",
          "[field-actor-collision][field]") {
  FakeCollisionHost host;
  CollisionActor hanging = Mover(100, 0, 100);
  hanging.table_index = 2;
  hanging.locomotion_state = 2;
  hanging.support_layer = 0;
  hanging.platform_offset_x = 4;
  host.actors.at(2) = hanging;

  CHECK_FALSE(VS::Field::CheckPlatformActorObstruction(
      host, 3, MotionVector{.x = 110, .y = 10, .z = 100}));
  CHECK(host.stored.empty());

  CHECK(VS::Field::CheckPlatformActorObstruction(
      host, 3, MotionVector{.x = 110, .y = -10, .z = 100}));
  REQUIRE(host.stored.size() == 1);
  const CollisionActor& dropped = host.stored.front();
  CHECK(dropped.movement_mode == 3);
  CHECK(dropped.locomotion_state == 0);
  CHECK(dropped.root_motion == MotionVector{.x = 0, .y = 3, .z = 0});
  CHECK(dropped.platform_offset_x == 0);
  CHECK(host.contacts ==
        std::vector<std::pair<std::int32_t, std::int32_t>>{{2, 2}});
}

namespace {
// A level-floor mover at the centre of tile (1, 1).
struct TerrainMotionFixture {
  FakeCollisionHost host;
  CollisionActor actor = Mover(192, 0, 192);
  MotionVector motion{.x = 16, .y = 0, .z = 0};

  TerrainMotionResult Resolve() {
    return VS::Field::ResolveActorTerrainMotion(host, actor, motion, false);
  }
};
}  // namespace

TEST_CASE("Terrain motion takes an open step along X",
          "[field-actor-collision][field]") {
  TerrainMotionFixture fixture;
  CHECK(fixture.Resolve() ==
        TerrainMotionResult{.motion = TerrainMotion::kAlongX, .climb = 0});
  CHECK(fixture.motion == MotionVector{.x = 16, .y = 0, .z = 0});
  CHECK(fixture.host.probe.position ==
        MotionVector{.x = 240, .y = 0, .z = 192});
  CHECK(fixture.host.probe.direction == 6);
  CHECK(fixture.host.probe.cell == Room(1, 1));
  CHECK_FALSE(fixture.actor.on_ledge);
}

TEST_CASE("Terrain motion takes an open diagonal step",
          "[field-actor-collision][field]") {
  TerrainMotionFixture fixture;
  fixture.motion = {.x = 16, .y = 0, .z = 16};
  CHECK(fixture.Resolve() ==
        TerrainMotionResult{.motion = TerrainMotion::kAccepted, .climb = 0});
  CHECK(fixture.motion == MotionVector{.x = 16, .y = 0, .z = 16});
  CHECK(fixture.host.probe.direction == 5);
  CHECK_FALSE(fixture.actor.on_ledge);
}

TEST_CASE("Terrain motion into missing cells is blocked",
          "[field-actor-collision][field]") {
  FakeCollisionHost host;
  for (int tile_z = 0; tile_z < kTiles; ++tile_z) {
    host.Tile(2, tile_z).present = false;
  }
  CollisionActor actor = Mover(220, 0, 192);
  MotionVector motion{.x = 16, .y = 0, .z = 0};
  CHECK(VS::Field::ResolveActorTerrainMotion(host, actor, motion, false) ==
        TerrainMotionResult{.motion = TerrainMotion::kBlocked,
                            .climb = std::nullopt});
  CHECK(motion == MotionVector{.x = 16, .y = 0, .z = 0});
}

TEST_CASE("Terrain motion approaching an overlapped actor is blocked first",
          "[field-actor-collision][field]") {
  FakeCollisionHost host;
  CollisionActor actor = Mover(200, 0, 200);
  actor.table_index = 3;
  CollisionActor other = Mover(230, 0, 200);
  other.table_index = 4;
  host.actors.at(4) = other;
  host.probe.direction = 9;
  MotionVector motion{.x = 8, .y = 0, .z = 0};
  CHECK(VS::Field::ResolveActorTerrainMotion(host, actor, motion, false) ==
        TerrainMotionResult{.motion = TerrainMotion::kBlocked,
                            .climb = std::nullopt});
  CHECK(host.probe.direction == 9);
}

namespace {
// A mover on a ledge stepping +X.
struct LedgeSlideFixture : TerrainMotionFixture {
  LedgeSlideFixture() { actor.on_ledge = true; }
};
}  // namespace

TEST_CASE("Facing back along -X, a +X step slides along the ledge",
          "[field-actor-collision][field]") {
  LedgeSlideFixture fixture;
  fixture.actor.terrain_facing = 2;
  CHECK(fixture.Resolve() ==
        TerrainMotionResult{.motion = TerrainMotion::kAccepted, .climb = 0});
  CHECK(fixture.motion == MotionVector{.x = 8, .y = 0, .z = -8});
  CHECK(fixture.actor.terrain_facing == 7);
  CHECK(fixture.actor.on_ledge);
  CHECK(fixture.actor.was_on_ledge);
}

TEST_CASE("Facing +X on a ledge, the step is taken",
          "[field-actor-collision][field]") {
  LedgeSlideFixture fixture;
  fixture.actor.terrain_facing = 6;
  CHECK(fixture.Resolve() ==
        TerrainMotionResult{.motion = TerrainMotion::kAlongX, .climb = 0});
  CHECK(fixture.motion == MotionVector{.x = 16, .y = 0, .z = 0});
  CHECK_FALSE(fixture.actor.on_ledge);
  CHECK(fixture.actor.was_on_ledge);
}

TEST_CASE("A moving actor never slides on a ledge",
          "[field-actor-collision][field]") {
  LedgeSlideFixture fixture;
  fixture.actor.terrain_facing = 2;
  fixture.actor.movement_mode = 1;
  CHECK(fixture.Resolve().motion == TerrainMotion::kBlocked);
  CHECK(fixture.actor.was_on_ledge);
}

TEST_CASE("Probe-only motion keeps the actor's own cell",
          "[field-actor-collision][field]") {
  FakeCollisionHost host;
  CollisionActor actor = Mover(192, 0, 192);
  actor.tile_layer = 0;
  host.SetFloor(1, 1, -100);
  MotionVector motion{.x = 16, .y = 0, .z = 0};
  CHECK(VS::Field::ResolveActorTerrainMotion(host, actor, motion, true) ==
        TerrainMotionResult{.motion = TerrainMotion::kAccepted, .climb = 1});
  CHECK(host.probe.position == MotionVector{.x = 208, .y = -100, .z = 192});
  CHECK(host.probe.cell == Room(1, 1));

  // With no cell under the actor it bypasses terrain, so the probe passes
  // and no cell is kept (the original kept a NULL probe tile, #288).
  host.Tile(1, 1).present = false;
  CHECK(VS::Field::ResolveActorTerrainMotion(host, actor, motion, true) ==
        TerrainMotionResult{.motion = TerrainMotion::kAccepted, .climb = 0});
  CHECK(actor.bypass_terrain_collision);
  CHECK(host.probe.cell == std::nullopt);
}

TEST_CASE("Ledge offsets refuse an invisible platform before any change",
          "[field-actor-collision][field][issue-257]") {
  FakeCollisionHost host;
  CollisionActor actor = Mover(192, 0, 192);
  actor.support_layer = 3;
  actor.move_facing = 777;
  host.probe.direction = 4;
  MotionVector offset{.x = 1, .y = 2, .z = 3};
  CHECK_FALSE(VS::Field::CalcActorLedgeOffset(host, actor, offset));
  CHECK(actor.move_facing == 777);
  CHECK(offset == MotionVector{.x = 1, .y = 2, .z = 3});
  CHECK(actor.position == MotionVector{.x = 192, .y = 0, .z = 192});
}

namespace {
// A level-floor mover at the centre of tile (1, 1) and its ledge offset.
struct LedgeOffsetFixture {
  FakeCollisionHost host;
  CollisionActor actor = Mover(192, 0, 192);
  MotionVector offset{};

  bool Calc() { return VS::Field::CalcActorLedgeOffset(host, actor, offset); }
  void UseVisiblePlatformLedge() {
    actor.support_layer = 3;
    host.visible[3] = PlatformContact{.x = 200, .height = -64, .z = 300};
    host.probe.direction = 2;
  }
  void CheckActorStayed() const {
    CHECK(actor.position == MotionVector{.x = 192, .y = 0, .z = 192});
    CHECK_FALSE(actor.wall_contact);
  }
};
}  // namespace

TEST_CASE("Ledge offsets lift the actor onto its tile edge",
          "[field-actor-collision][field]") {
  LedgeOffsetFixture fixture;
  fixture.host.probe.direction = 4;
  CHECK(fixture.Calc());
  CHECK(fixture.offset == MotionVector{.x = 0, .y = 8, .z = 31});
  CHECK(fixture.actor.move_facing == 2048);
  fixture.CheckActorStayed();
}

TEST_CASE("Ledge offsets lift a tile edge from a gimmick further",
          "[field-actor-collision][field]") {
  LedgeOffsetFixture fixture;
  fixture.host.probe.direction = 4;
  fixture.actor.support_gimmick = 1;
  CHECK(fixture.Calc());
  CHECK(fixture.offset.y == 20);
  fixture.CheckActorStayed();
}

TEST_CASE("Ledge offsets measure a platform edge from its centre",
          "[field-actor-collision][field]") {
  LedgeOffsetFixture fixture;
  fixture.UseVisiblePlatformLedge();
  CHECK(fixture.Calc());
  CHECK(fixture.offset == MotionVector{.x = 105, .y = 0, .z = 0});
  CHECK(fixture.actor.move_facing == 1024);
  CHECK(fixture.actor.support_layer == 3);
  fixture.CheckActorStayed();
}

TEST_CASE("A wall at the edge refuses the ledge offset",
          "[field-actor-collision][field]") {
  LedgeOffsetFixture fixture;
  fixture.UseVisiblePlatformLedge();
  fixture.host.SetFloor(2, 1, -200);
  CHECK_FALSE(fixture.Calc());
  CHECK(fixture.offset.x == 105);
  CHECK(fixture.actor.move_facing == 1024);
  fixture.CheckActorStayed();
}

namespace {
// Saves the live collision state the bindings touch.
struct LiveCollisionFixture {
  CollisionProbe saved_probe = VS::Field::LiveCollisionProbe();
  CollisionSamples saved_samples = VS::Field::LiveCollisionSamples();
  SVECTOR saved_position = Field_collisionProbePosition;
  u_char saved_direction = Field_collisionProbeDirection;
  Field_RoomRecord saved_room = Field_roomData;
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);

  LiveCollisionFixture() {
    Field_roomData = {};
    std::fill(std::begin(Actors_actorRecords), std::end(Actors_actorRecords),
              nullptr);
  }
  ~LiveCollisionFixture() {
    VS::Field::LiveCollisionProbe() = saved_probe;
    VS::Field::LiveCollisionSamples() = saved_samples;
    Field_collisionProbePosition = saved_position;
    Field_collisionProbeDirection = saved_direction;
    Field_roomData = saved_room;
    std::copy(saved_records.begin(), saved_records.end(),
              std::begin(Actors_actorRecords));
  }
  LiveCollisionFixture(const LiveCollisionFixture&) = delete;
  LiveCollisionFixture& operator=(const LiveCollisionFixture&) = delete;
  LiveCollisionFixture(LiveCollisionFixture&&) = delete;
  LiveCollisionFixture& operator=(LiveCollisionFixture&&) = delete;
};
}  // namespace

TEST_CASE("Collision bindings load the probe from the movement globals",
          "[field-actor-collision][field][live-binding]") {
  const LiveCollisionFixture fixture;
  Field_collisionProbePosition = SVECTOR{11, 22, 33, 44};
  Field_collisionProbeDirection = 5;
  VS::Field::LiveCollisionProbe().position = {.x = 1, .y = 1, .z = 1};
  VS::Field::LiveCollisionProbe().cell = Room(1, 2);

  CHECK(VS::Field::LiveSampleSupportHeightAt(0, 0, 0, 0, 0, 1) ==
        VS::Field::kNoSupportHeight);
  CHECK(VS::Field::LiveCollisionProbe().position ==
        MotionVector{.x = 11, .y = 22, .z = 33, .w = 44});
  CHECK(VS::Field::LiveCollisionProbe().direction == 5);
  CHECK(VS::Field::LiveCollisionProbe().cell == Room(1, 2));
  CHECK(Field_collisionProbePosition.vx == 11);
  CHECK(Field_collisionProbePosition.pad == 44);
  CHECK(Field_collisionProbeDirection == 5);
}

TEST_CASE("The probe cell word reads a missing or stale cell as absent",
          "[field-actor-collision][field][live-binding][issue-288]") {
  const LiveCollisionFixture fixture;
  VS::Field::LiveCollisionProbe().cell = std::nullopt;
  CHECK(VS::Field::LiveCollisionProbeCellWord() == VS::Field::kAbsentCellWord);
  VS::Field::LiveCollisionProbe().cell = Room(3, 3);
  CHECK(VS::Field::LiveCollisionProbeCellWord() == VS::Field::kAbsentCellWord);
  CHECK(((VS::Field::kAbsentCellWord >> 5) & 1U) == 0);
  CHECK(((VS::Field::kAbsentCellWord >> 20) & 1U) == 0);
}

TEST_CASE("Wall repulsion of an actor's own root motion keeps the push",
          "[field-actor-collision][field][live-binding]") {
  const LiveCollisionFixture fixture;
  Actors_ActorRecord record{};
  record.wallContact = 1;
  record.platformOffsetX = 4;
  record.instance.rootMotion = SVECTOR{10, 5, 7, 9};
  VS::Field::LiveCollisionSamples().blocked = {0, 0, 0, 0, 0, 0, 1, 0};

  VS::Field::LiveApplyActorWallRepulsion(&record, &record.instance.rootMotion);
  CHECK(record.instance.rootMotion.vx == -16);
  CHECK(record.instance.rootMotion.vy == 5);
  CHECK(record.instance.rootMotion.vz == 7);
  CHECK(record.instance.rootMotion.pad == 9);
  CHECK(record.platformOffsetX == 0);
}

TEST_CASE("Terrain motion blocked by an overlap leaves the result unwritten",
          "[field-actor-collision][field][live-binding]") {
  const LiveCollisionFixture fixture;
  Actors_ActorRecord mover{};
  mover.instance.position = SVECTOR{200, 0, 200, 0};
  mover.occupancyRadius = 32;
  mover.occupancyHeight = 128;
  Actors_ActorRecord other = mover;
  other.instance.position.vx = 230;
  Actors_actorRecords[2] = &other;
  SVECTOR motion{8, 0, 0, 0};
  int result = 77;

  CHECK(VS::Field::LiveResolveActorTerrainMotion(&mover, &motion, &result, 0) ==
        static_cast<int>(TerrainMotion::kBlocked));
  CHECK(result == 77);
  CHECK(motion.vx == 8);
}

TEST_CASE("Row 21 and 23 actor collision module entries are retired",
          "[field-actor-collision][field][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  constexpr std::array<std::uint32_t, 14> kRetired{
      0x800A3C34U, 0x800A4494U, 0x800A46A4U, 0x800A6798U, 0x800A69B4U,
      0x800A6EE8U, 0x800A70DCU, 0x800A7524U, 0x800A76BCU, 0x800A8B34U,
      0x800A8E84U, 0x800A8FD4U, 0x800A9530U, 0x800F49F0U};
  for (const std::uint32_t address : kRetired) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
  }
}
