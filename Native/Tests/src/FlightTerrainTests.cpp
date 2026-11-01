#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/FlightTerrain.hpp"
#include "Battle/Ai/RoomAiMap.hpp"
#include "Field/TerrainHeight.hpp"

namespace {
using VS::Battle::Ai::FlightNeighborhood;
using VS::Field::kTerrainGridSide;
using VS::Field::TerrainGrid;
using VS::Field::TerrainRow;

// Cell words: the signed 10-bit height in 64-unit steps.
constexpr std::uint16_t kOneStepDown = 0x3FF;   // -64
constexpr std::uint16_t kTwoStepsDown = 0x3FE;  // -128
constexpr std::uint16_t kOneStepUp = 0x001;     // +64

struct Terrain {
  std::array<TerrainRow, kTerrainGridSide> rows{};
  [[nodiscard]] TerrainGrid Grid() const {
    return TerrainGrid{std::span<const TerrainRow, kTerrainGridSide>(rows)};
  }
};

VS::Battle::Ai::FlightDirectionSteps CompassSteps() {
  return {
      {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}}};
}

FlightNeighborhood Around(const Terrain& terrain, std::vector<int>& asked,
                          std::uint8_t flyable_mask) {
  FlightNeighborhood neighborhood;
  neighborhood.terrain = terrain.Grid();
  neighborhood.origin = {10, 10};
  neighborhood.steps = CompassSteps();
  neighborhood.canFly = [&asked, flyable_mask](int direction) {
    asked.push_back(direction);
    return ((flyable_mask >> direction) & 1) != 0;
  };
  return neighborhood;
}

class SettingsRuntime final : public VS::Battle::Ai::RoomAiRuntime {
 public:
  explicit SettingsRuntime(std::optional<VS::Battle::Ai::RoomAiSettings> s)
      : settings_(s) {}
  [[nodiscard]] VS::Battle::Ai::RoomAiSource CurrentRoom() const override {
    return {};
  }
  void PublishRoomAiMap(const VS::Battle::Ai::RoomAiGrid&) override {}
  [[nodiscard]] std::optional<VS::Battle::Ai::RoomAiSettings>
  PublishedSettings() const override {
    return settings_;
  }
  void RefreshDerivedRoomState() override {}
  void FreeActorAiStates() override {}
  void ReleaseRoomAiMap() override {}

 private:
  std::optional<VS::Battle::Ai::RoomAiSettings> settings_;
};
}  // namespace

TEST_CASE("Lowest reachable ground checks one cardinal direction",
          "[ai][flight][terrain]") {
  Terrain terrain;
  terrain.rows[10][9] = kOneStepDown;  // direction 6
  terrain.rows[9][9] = kTwoStepsDown;  // direction 7, not examined
  std::vector<int> asked;
  CHECK(VS::Battle::Ai::LowestReachableGroundHeight(
            Around(terrain, asked, 0xFF), 6 << 9) == -64);
  CHECK(asked == std::vector<int>{6});
}

TEST_CASE("Lowest reachable ground spans three directions between cardinals",
          "[ai][flight][terrain]") {
  Terrain terrain;
  terrain.rows[10][9] = kOneStepDown;   // direction 6
  terrain.rows[9][9] = kTwoStepsDown;   // direction 7
  terrain.rows[9][10] = kTwoStepsDown;  // direction 0 (wrapped)
  std::vector<int> asked;
  // Direction 7 is not flyable: its lower ground is ignored.
  CHECK(VS::Battle::Ai::LowestReachableGroundHeight(
            Around(terrain, asked, 0x41), (7u << 9) | 1u) == -128);
  CHECK(asked == std::vector<int>{6, 7, 0});

  asked.clear();
  CHECK(VS::Battle::Ai::LowestReachableGroundHeight(
            Around(terrain, asked, 0x40), (7u << 9) | 1u) == -64);
}

TEST_CASE("Lowest reachable ground never rises above zero",
          "[ai][flight][terrain]") {
  Terrain terrain;
  terrain.rows[10][9] = kOneStepUp;
  std::vector<int> asked;
  CHECK(VS::Battle::Ai::LowestReachableGroundHeight(
            Around(terrain, asked, 0xFF), 6 << 9) == 0);
  CHECK(VS::Battle::Ai::LowestReachableGroundHeight(
            Around(terrain, asked, 0x00), 6 << 9) == 0);
  FlightNeighborhood untested;
  untested.terrain = terrain.Grid();
  CHECK(VS::Battle::Ai::LowestReachableGroundHeight(untested, 6 << 9) == 0);
}

TEST_CASE("Segment ground samples sixteen steps past the start",
          "[ai][flight][terrain]") {
  Terrain terrain;
  const int start_x = 8 * 128 + 64;
  const int start_z = 4 * 128 + 64;
  const int end_x = start_x + 16 * 128;
  terrain.rows[4][8] = kTwoStepsDown;  // the start tile is not sampled
  CHECK(VS::Battle::Ai::MinimumAiGroundHeightAlongSegment(
            terrain.Grid(), start_x, start_z, end_x, start_z) == 0);
  terrain.rows[4][24] = kOneStepDown;  // the last sample
  CHECK(VS::Battle::Ai::MinimumAiGroundHeightAlongSegment(
            terrain.Grid(), start_x, start_z, end_x, start_z) == -64);
  terrain.rows[4][12] = kOneStepUp;
  CHECK(VS::Battle::Ai::MinimumAiGroundHeightAlongSegment(
            terrain.Grid(), start_x, start_z, end_x, start_z) == -64);
}

TEST_CASE("Room navigation ceiling comes from the published settings",
          "[ai][flight][room]") {
  VS::Battle::Ai::RoomAiSettings settings;
  settings.flightCeiling = 640;
  CHECK(VS::Battle::Ai::RoomNavigationCeilingHeight(
            SettingsRuntime(settings)) == 640);
  CHECK(VS::Battle::Ai::RoomNavigationCeilingHeight(
            SettingsRuntime(std::nullopt)) == 0);
}
