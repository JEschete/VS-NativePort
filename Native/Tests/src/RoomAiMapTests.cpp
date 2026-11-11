#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/RoomAiMap.hpp"
#include "Battle/Ai/TileHeightSampling.hpp"
#include "Core/Math/Distance.hpp"

extern "C" {
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
}

namespace {
using VS::Battle::Ai::BuildRoomAiMap;
using VS::Battle::Ai::RoomAiGrid;
using VS::Battle::Ai::RoomAiSource;
using VS::Core::Math::TileCoordinate;

constexpr int kFlatFloor = 128;

// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the access below.
template <typename T>
const T& Required(const std::optional<T>& value) {
  REQUIRE(value);
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

RoomAiSource FlatRoom(int width, int height) {
  RoomAiSource source;
  source.collision_extent = VS::Battle::Ai::RoomCollisionExtent{width, height};
  source.collision_cell =
      [width, height](TileCoordinate tile) -> std::optional<std::uint32_t> {
    if (tile.x >= width || tile.z >= height) return std::nullopt;
    return 0u;
  };
  source.floor_height = [](int, int) { return kFlatFloor; };
  // std::function keeps its small target inline; NRVO or the move rebinds it.
  // NOLINTNEXTLINE(clang-analyzer-core.StackAddressEscape)
  return source;
}

void PutWord(std::span<std::byte> bytes, std::size_t offset,
             std::uint32_t value) {
  for (std::size_t index = 0; index < sizeof value; ++index) {
    bytes[offset + index] = static_cast<std::byte>(value >> (8 * index));
  }
}

class RecordingRuntime final : public VS::Battle::Ai::RoomAiRuntime {
 public:
  explicit RecordingRuntime(RoomAiSource source) : source_(std::move(source)) {}
  [[nodiscard]] RoomAiSource CurrentRoom() const override { return source_; }
  void PublishRoomAiMap(const RoomAiGrid& grid) override {
    calls.emplace_back("publish");
    published = grid;
  }
  [[nodiscard]] std::optional<VS::Battle::Ai::RoomAiSettings>
  PublishedSettings() const override {
    if (!published) return std::nullopt;
    return published->settings;
  }
  void RefreshDerivedRoomState() override { calls.emplace_back("refresh"); }
  void FreeActorAiStates() override { calls.emplace_back("free-actors"); }
  void ReleaseRoomAiMap() override {
    calls.emplace_back("release");
    published.reset();
  }

  std::vector<std::string> calls;
  std::optional<RoomAiGrid> published;

 private:
  RoomAiSource source_;
};
}  // namespace

TEST_CASE("Room AI collision flags gather the original bit fields",
          "[ai][room]") {
  CHECK(VS::Battle::Ai::DecodeCollisionFlags(0xFFFFFFFFu) == 0x387F);
  for (unsigned bit = 0; bit < 32; ++bit) {
    const std::uint32_t input = 1u << bit;
    const unsigned expected = ((input >> 23) & 0x1F) | ((input >> 12) & 0x20) |
                              ((input >> 22) & 0x40) | ((input >> 20) & 0x800) |
                              ((input >> 18) & 0x1000) |
                              ((input >> 16) & 0x2000);
    CHECK(VS::Battle::Ai::DecodeCollisionFlags(input) == expected);
  }
}

RoomAiGrid TriggeredRoom() {
  RoomAiSource source = FlatRoom(2, 3);
  source.collision_cell =
      [](TileCoordinate tile) -> std::optional<std::uint32_t> {
    if (tile.x >= 2 || tile.z >= 3) return std::nullopt;
    return tile.x == 1 && tile.z == 2 ? (1u << 17) : 0u;
  };
  source.floor_trigger = [](TileCoordinate tile)
      -> std::optional<VS::Battle::Ai::RoomFloorTrigger> {
    if (tile.x == 0 && tile.z == 1) return {{3, 1}};
    if (tile.x == 1 && tile.z == 1) return {{16, 0}};
    if (tile.x == 0 && tile.z == 2) return {{2, 2}};
    return std::nullopt;
  };
  source.environment_parameter = 3;
  return Required(BuildRoomAiMap(source));
}

TEST_CASE("Room AI maps take settings only from the source they are given",
          "[ai][room]") {
  const RoomAiGrid grid = TriggeredRoom();
  CHECK(grid.settings.max_tile_x == 2);
  CHECK(grid.settings.max_tile_z == 3);
  CHECK(grid.settings.min_tile_x == 0);
  CHECK(grid.settings.collision_extent == std::array<std::uint8_t, 2>{2, 3});
  CHECK(grid.settings.default_action_delay == 150);
  CHECK(grid.settings.flight_ceiling == -192);
}

TEST_CASE("Room AI maps take tiles only from the source they are given",
          "[ai][room]") {
  const RoomAiGrid grid = TriggeredRoom();
  CHECK(grid.flags[0][0] == 0);
  CHECK(grid.flags[2][1] == 0x20);
  CHECK(grid.flags[1][0] == 0x200);
  CHECK(grid.flags[1][1] == 0);
  CHECK(grid.flags[2][0] == 0);
  CHECK(grid.flags[0][2] == VS::Battle::Ai::kBlockedTileFlag);
  CHECK(grid.flags[31][31] == VS::Battle::Ai::kBlockedTileFlag);
  CHECK(grid.heights[0][0] == kFlatFloor >> 6);
  CHECK(grid.heights[0][2] == 0);
}

TEST_CASE("Room AI maps read settings from the auxiliary section",
          "[ai][room]") {
  std::vector<std::byte> auxiliary(VS::Battle::Ai::kRoomAuxiliaryHeaderBytes +
                                   6);
  PutWord(auxiliary, 0x450, (2u << 29) | (99u << 21) | (1u << 20));
  PutWord(auxiliary, 0x494, 0x03020A09);
  RoomAiSource source = FlatRoom(2, 3);
  source.auxiliary = std::span<const std::byte>(auxiliary);
  const std::optional<RoomAiGrid> grid = BuildRoomAiMap(source);
  const VS::Battle::Ai::RoomAiSettings& settings = Required(grid).settings;
  CHECK(settings.max_tile_x == 9);
  CHECK(settings.max_tile_z == 10);
  CHECK(settings.min_tile_x == 2);
  CHECK(settings.min_tile_z == 3);
  CHECK(settings.collision_extent == std::array<std::uint8_t, 2>{2, 3});
  CHECK(settings.default_action_delay == 99);
  CHECK(settings.allows_non_spell_actions);
  CHECK(settings.engagement_limit == 2);
}

TEST_CASE("Room AI maps reject short or oversized room headers", "[ai][room]") {
  std::vector<std::byte> auxiliary(VS::Battle::Ai::kRoomAuxiliaryHeaderBytes +
                                   6);
  RoomAiSource source = FlatRoom(2, 3);
  source.auxiliary = std::span<const std::byte>(auxiliary).first(
      VS::Battle::Ai::kRoomAuxiliaryHeaderBytes);
  CHECK(BuildRoomAiMap(source));
  source.auxiliary = std::span<const std::byte>(auxiliary).first(
      VS::Battle::Ai::kRoomAuxiliaryHeaderBytes - 1);
  CHECK_FALSE(BuildRoomAiMap(source));
  CHECK_FALSE(BuildRoomAiMap(FlatRoom(33, 1)));
  CHECK_FALSE(BuildRoomAiMap(FlatRoom(1, -1)));
}

TEST_CASE("Tile heights keep the highest sample unless floor is missing",
          "[ai][room]") {
  std::array<int, VS::Battle::Ai::kTileSampleCount> samples{};
  samples.fill(128);
  samples[5] = 192;
  CHECK(VS::Battle::Ai::SelectTileHeight(samples, 128) == 192);
  samples.fill(static_cast<int>(0x80000080u));
  samples[0] = 256;
  CHECK(VS::Battle::Ai::SelectTileHeight(samples, 128) == samples[10]);
  samples.fill(128);
  samples[15] = static_cast<int>(0x80000080u);
  CHECK(VS::Battle::Ai::SelectTileHeight(samples, 64) == 128);
}

TEST_CASE("Tile height sampling queries the tile's center and lattice",
          "[ai][room]") {
  std::vector<std::pair<int, int>> queries;
  const VS::Battle::Ai::FloorHeightQuery floor = [&queries](int x, int z) {
    queries.emplace_back(x, z);
    return x == 2 * 128 + 96 && z == 3 * 128 + 32 ? 300 : 100;
  };
  CHECK(VS::Battle::Ai::SampleAiTileGroundHeight(floor, TileCoordinate{2, 3}) ==
        300);
  REQUIRE(queries.size() == 17);
  CHECK(queries[0] == std::pair{2 * 128 + 64, 3 * 128 + 64});
  CHECK(queries[1] == std::pair{2 * 128, 3 * 128});
  CHECK(queries[2] == std::pair{2 * 128, 3 * 128 + 32});
  CHECK(queries[5] == std::pair{2 * 128 + 32, 3 * 128});
  CHECK(queries[16] == std::pair{2 * 128 + 96, 3 * 128 + 96});
}

TEST_CASE("Room AI lifecycle publishes and releases through its runtime",
          "[ai][room]") {
  RecordingRuntime runtime(FlatRoom(4, 4));
  VS::Battle::Ai::InitializeRoomAiMap(runtime);
  CHECK(runtime.calls == std::vector<std::string>{"publish", "refresh"});
  const RoomAiGrid& published = Required(runtime.published);
  CHECK(published.settings.max_tile_x == 4);
  CHECK(published.flags[3][3] == 0);
  CHECK(published.flags[4][4] == VS::Battle::Ai::kBlockedTileFlag);
  runtime.calls.clear();
  VS::Battle::Ai::FreeRoomAiState(runtime);
  CHECK(runtime.calls == std::vector<std::string>{"free-actors", "release"});
}

TEST_CASE("Room navigation entry builds and publishes the room AI map",
          "[ai][room][room-navigation]") {
  RecordingRuntime runtime(FlatRoom(3, 5));
  VS::Battle::Ai::InitializeRoomNavigation(runtime);
  CHECK(runtime.calls == std::vector<std::string>{"publish", "refresh"});
  const RoomAiGrid& published = Required(runtime.published);
  CHECK(published.settings.max_tile_x == 3);
  CHECK(published.settings.max_tile_z == 5);
  CHECK(published.flags[4][2] == 0);
  CHECK(published.flags[5][2] == VS::Battle::Ai::kBlockedTileFlag);
}

TEST_CASE("The battle runtime reads the current room as a bounded source",
          "[ai][room]") {
  const Field_RoomRecord saved = Field_roomData;
  Field_roomData = {};
  Field_MpdCollisionHeader dimensions = {2, 3, 0, 6};
  std::array<std::byte, VS::Battle::Ai::kRoomAuxiliaryHeaderBytes + 6>
      auxiliary{};
  Field_roomData.collisionMap = &dimensions;
  Field_roomData.section6 = auxiliary.data();
  Field_roomData.header.section6Length = 0x497;
  const RoomAiSource source =
      VS::Battle::Ai::BattleRoomAiRuntime().CurrentRoom();
  const VS::Battle::Ai::RoomCollisionExtent& extent =
      Required(source.collision_extent);
  CHECK(extent.width == 2);
  CHECK(extent.height == 3);
  const std::span<const std::byte> room_auxiliary = Required(source.auxiliary);
  CHECK(room_auxiliary.data() == auxiliary.data());
  CHECK(room_auxiliary.size() == 0x497);
  CHECK_FALSE(source.environment_parameter);
  CHECK_FALSE(BuildRoomAiMap(source));
  Field_roomData = saved;
}
