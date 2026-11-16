#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/SphereDraw.hpp"
#include "Battle/TargetingDraw.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace {
namespace battle = VS::Battle;
namespace math = VS::Core::Math;

constexpr std::uint32_t Pack(std::int32_t x, std::int32_t y) {
  return (static_cast<std::uint32_t>(x) & 0xFFFFU) |
         (static_cast<std::uint32_t>(y) << 16U);
}

// ---- Sphere traversal ----

class FakeDrawPort final : public battle::SphereDrawPort {
 public:
  std::vector<std::string> calls;
  std::vector<std::int16_t> sine;
  std::vector<battle::SphereDrawVolume> staged;
  std::vector<battle::TargetVolumeExtents> transformed;
  std::vector<std::pair<battle::SphereDrawVolume, std::uint32_t>> drawn;
  std::vector<std::int32_t> types;
  std::vector<std::int32_t> depths;
  math::Mat3 transform{};

  void StoreSineSamples(
      std::span<const std::int16_t, battle::kSphereSineSamples> samples)
      override {
    calls.emplace_back("sine");
    sine.assign(samples.begin(), samples.end());
  }
  void StageVolume(std::size_t slot,
                   const battle::SphereDrawVolume& volume) override {
    calls.push_back("stage" + std::to_string(slot));
    staged.push_back(volume);
  }
  void LoadFrameView() override { calls.emplace_back("view"); }
  std::int32_t ProjectCenterDepth(
      const battle::TargetWorldPoint& position) override {
    calls.emplace_back("project");
    return position[2] / 4;
  }
  void StoreCenterDepth(std::int32_t depth) override {
    calls.emplace_back("depth");
    depths.push_back(depth);
  }
  math::Mat3 VolumeTransform(const battle::TargetVolumeExtents& extents,
                             std::int16_t /*angle*/) override {
    calls.emplace_back("transform");
    transformed.push_back(extents);
    return transform;
  }
  void BuildWireframe(std::int32_t type) override {
    calls.emplace_back("build");
    types.push_back(type);
  }
  void DrawVolumeWireframe(std::size_t slot,
                           const battle::SphereDrawVolume& volume,
                           std::uint32_t color) override {
    calls.push_back("draw" + std::to_string(slot));
    drawn.emplace_back(volume, color);
  }
  void UpdateTargetLabels() override { calls.emplace_back("labels"); }
  void DrawTargetPageHint() override { calls.emplace_back("hint"); }
};

battle::SphereDrawVolume Volume(std::uint8_t kind) {
  return {.kind = kind,
          .reserved = 9,
          .angle = 0x123,
          .extents = {10, 20, 30, 0xF0},
          .position = {1000, 2000, 3000},
          .position_pad = 7};
}

// ---- Position cursor ----

battle::SphereCursorTables CursorTables() {
  return {
      .offsets = {static_cast<std::int32_t>(0xFFF80004U), 0x20004,
                  static_cast<std::int32_t>(0xFFFCFFFDU),
                  static_cast<std::int32_t>(0xFFF0FFFCU), 0x4FFFC},
      .sizes = {0x7000A, 0x7000A, 0x70007, 0xB0009, 0xB0009},
      .texcoords = {0x37FD38E8, 0x37FD40E8, 0x37FD30E8, 0x37FD30DC, 0x37FD3CDC},
      .brightness = {128, 184, 240, 232, 224, 216, 208, 200, 192, 184, 176, 168,
                     160, 152, 144, 136}};
}

// ---- Terrain cache ----

constexpr std::uint32_t kDoorShift = 10;
constexpr std::uint32_t kFloorFlag = 1U << 19U;
constexpr std::uint32_t kBlocks = 1U << 6U;

constexpr std::int32_t Sample(std::int32_t floor, std::int32_t ceiling) {
  return static_cast<std::int32_t>(
      (static_cast<std::uint32_t>(floor) & 0x7FFFU) |
      (static_cast<std::uint32_t>(ceiling) << 16U));
}

constexpr std::int32_t Packed(std::int32_t flat, std::int32_t top,
                              std::int32_t low) {
  return static_cast<std::int32_t>(
      static_cast<std::uint32_t>(flat) |
      (static_cast<std::uint32_t>(top + 0x30) << 17U) |
      ((static_cast<std::uint32_t>(low) & 0x7FFFU) << 2U));
}

class FakeTerrain final : public battle::SphereTerrainSource {
 public:
  std::int32_t dimensions = (16 << 4) | (16 << 20);
  std::map<std::pair<std::int32_t, std::int32_t>, std::uint32_t> cells;
  std::map<std::pair<std::int32_t, std::int32_t>, std::int32_t> stacks;
  std::optional<std::uint8_t> object = 3;
  // Floor of the sample at world (x, z); the ceiling is fixed.
  std::int32_t floor = -16;
  std::int32_t ceiling = 200;
  bool uneven = false;
  std::vector<std::pair<std::int32_t, std::int32_t>> samples;

  std::int32_t MapDimensions() override { return dimensions; }
  std::uint32_t CellWord(std::int32_t tile_x, std::int32_t tile_z) override {
    const auto found = cells.find({tile_x, tile_z});
    return found == cells.end() ? 0U : found->second;
  }
  std::int32_t TileStackOffset(std::int32_t tile_x,
                               std::int32_t tile_z) override {
    const auto found = stacks.find({tile_x, tile_z});
    return found == stacks.end() ? 0 : found->second;
  }
  std::optional<std::uint8_t> TopGimmickObject(
      std::int32_t /*tile_x*/, std::int32_t /*tile_z*/) override {
    return object;
  }
  std::int32_t FloorAndCeiling(std::int32_t world_x,
                               std::int32_t world_z) override {
    samples.emplace_back(world_x, world_z);
    return Sample(uneven ? floor + (world_x & 0x7F) : floor, ceiling);
  }
};
}  // namespace

TEST_CASE("Sphere wire line nudges a still point by its octant",
          "[battle][sphere-draw]") {
  const auto line = battle::DrawSphereWireLine({.x = 10, .y = 20, .depth = 100},
                                               {.x = 10, .y = 20, .depth = 100},
                                               0x30102030U, 2);
  REQUIRE(line.has_value());
  if (line.has_value()) {
    CHECK(*line == battle::SphereWireLinePacket{.slot = 100,
                                                .mode = 0xE1000200U,
                                                .color = 0x30204060U,
                                                .start_xy = Pack(11, 20),
                                                .end_xy = Pack(10, 20)});
  }
}

TEST_CASE("Sphere wire line nudges a downward line along y",
          "[battle][sphere-draw]") {
  const auto line = battle::DrawSphereWireLine(
      {.x = -5, .y = 0, .depth = 4}, {.x = -5, .y = 10, .depth = 6}, 0, 1);
  REQUIRE(line.has_value());
  if (line.has_value()) {
    CHECK(line->slot == 5);
    CHECK(line->start_xy == Pack(-5, 1));
    CHECK(line->end_xy == Pack(-5, 10));
  }
}

TEST_CASE("Sphere wire line culls midpoints outside the ordering table",
          "[battle][sphere-draw]") {
  CHECK_FALSE(
      battle::DrawSphereWireLine({.depth = 0x7FF}, {.depth = 0x801}, 0, 0)
          .has_value());
  CHECK_FALSE(battle::DrawSphereWireLine({.depth = -2}, {.depth = 0}, 0, 0)
                  .has_value());
  const auto deepest =
      battle::DrawSphereWireLine({.depth = 0x7FF}, {.depth = 0x7FF}, 0, 0);
  REQUIRE(deepest.has_value());
  if (deepest.has_value()) {
    CHECK(deepest->slot == 0x7FF);
  }
}

TEST_CASE("Sphere wire line clamps the intensity to 0 to 63",
          "[battle][sphere-draw]") {
  const auto dark = battle::DrawSphereWireLine({}, {}, 0x42010101U, -5);
  const auto bright = battle::DrawSphereWireLine({}, {}, 0x42010101U, 100);
  REQUIRE(dark.has_value());
  REQUIRE(bright.has_value());
  if (dark.has_value() && bright.has_value()) {
    CHECK(dark->color == 0x42000000U);
    CHECK(bright->color == 0x423F3F3FU);
  }
}

TEST_CASE("Sphere traversal draws nothing without a sphere",
          "[battle][sphere-draw]") {
  FakeDrawPort port;
  battle::DrawSphere(std::nullopt, port);
  CHECK(port.calls.empty());
}

TEST_CASE("Sphere traversal stretches kind 2 and tilts kind 4",
          "[battle][sphere-draw]") {
  FakeDrawPort port;
  port.transform.m[0][1] = 100;
  port.transform.m[1][1] = -7;
  port.transform.m[2][1] = 3;
  battle::DrawSphere(battle::SphereDrawVolumes{Volume(2), Volume(4)}, port);
  CHECK(port.calls ==
        std::vector<std::string>{"sine", "stage0", "view", "project", "depth",
                                 "transform", "build", "draw0", "stage1",
                                 "view", "project", "depth", "transform",
                                 "build", "draw1", "labels", "hint"});
  REQUIRE(port.transformed.size() == 2);
  CHECK(port.transformed[0] == battle::TargetVolumeExtents{10, 40, 30, 0xF0});
  CHECK(port.transformed[1] == battle::TargetVolumeExtents{10, 20, 30, 0x70});
  CHECK(port.types == std::vector<std::int32_t>{1, 2});
  CHECK(port.staged[0] == Volume(2));
  CHECK(port.depths == std::vector<std::int32_t>{750, 750});
}

TEST_CASE("Sphere traversal lifts transformed volumes by the matrix column",
          "[battle][sphere-draw]") {
  FakeDrawPort port;
  port.transform.m[0][1] = 100;
  port.transform.m[1][1] = -7;
  port.transform.m[2][1] = 3;
  battle::DrawSphere(battle::SphereDrawVolumes{Volume(2), Volume(4)}, port);
  REQUIRE(port.drawn.size() == 2);
  CHECK(port.drawn[0].first.position ==
        battle::TargetWorldPoint{1050, 1996, 3001});
  CHECK(port.drawn[0].second == battle::kSpherePrimaryVolumeColor);
  CHECK(port.drawn[1].first.position ==
        battle::TargetWorldPoint{1100, 1993, 3003});
  CHECK(port.drawn[1].first.position_pad == 7);
  CHECK(port.drawn[1].second == battle::kSphereSecondaryVolumeColor);
}

TEST_CASE("Sphere traversal maps kinds to wireframe shapes",
          "[battle][sphere-draw]") {
  const std::array<std::pair<std::uint8_t, std::int32_t>, 5> shapes{
      {{1, 0}, {3, 1}, {5, 2}, {6, 3}, {7, 4}}};
  for (const auto& [kind, type] : shapes) {
    FakeDrawPort port;
    battle::DrawSphere(battle::SphereDrawVolumes{Volume(kind), Volume(0)},
                       port);
    CHECK(port.types == std::vector<std::int32_t>{type});
    CHECK(port.transformed.empty());
    REQUIRE(port.drawn.size() == 1);
    CHECK(port.drawn[0].first == Volume(kind));
  }
}

TEST_CASE("Sphere traversal stages but skips unknown kinds",
          "[battle][sphere-draw]") {
  FakeDrawPort port;
  battle::DrawSphere(battle::SphereDrawVolumes{Volume(0), Volume(8)}, port);
  CHECK(port.calls == std::vector<std::string>{"sine", "stage0", "view",
                                               "project", "depth", "stage1",
                                               "view", "project", "depth",
                                               "labels", "hint"});
  REQUIRE(port.sine.size() == battle::kSphereSineSamples);
  CHECK(port.sine[0] == 0);
  CHECK(port.sine[8] == 4096);
  CHECK(port.sine[24] == -4096);
  CHECK(port.sine[39] == math::Sine(39 * 128));
}

TEST_CASE("Sphere camera basis rotates by the camera yaw",
          "[battle][sphere-draw]") {
  CHECK(battle::BuildSphereCameraBasis(1024) ==
        battle::SphereCameraBasis{
            .cosine = 0, .sine = 4096, .negated_sine = -4096, .yaw = 1024});
  const auto wrapped = battle::BuildSphereCameraBasis(0x10000 + 512);
  CHECK(wrapped.yaw == 512);
  CHECK(wrapped.cosine == math::Cosine(512));
  CHECK(wrapped.sine == math::Sine(512));
  CHECK(wrapped.negated_sine == -math::Sine(512));
}

TEST_CASE("Sphere position cursor draws pitch arrows while pitch is free",
          "[battle][sphere-draw]") {
  const std::uint32_t xy = Pack(80, 100);
  const auto frame = battle::DrawSpherePositionCursor(xy, 0, 1, CursorTables());
  REQUIRE(frame.has_value());
  if (!frame.has_value()) return;
  CHECK(frame->blink == 2);
  REQUIRE(frame->pitch_quads.has_value());
  if (!frame->pitch_quads.has_value()) return;
  const auto& quads = *frame->pitch_quads;
  // Word adds: a negative x offset carries into y, as in the original.
  CHECK(quads[0] == battle::SphereCursorQuad{0x2C808080U, Pack(67, 92),
                                             0x37FD38F1U, Pack(77, 92),
                                             0x000C38E7U, Pack(67, 99), 0x3FF1U,
                                             Pack(77, 99), 0x3FE7U});
  CHECK(quads[1][1] == Pack(67, 102));
  CHECK(quads[1][8] == 0x47E7U);
}

TEST_CASE("Sphere position cursor places sprites while pitch is free",
          "[battle][sphere-draw]") {
  const auto frame =
      battle::DrawSpherePositionCursor(Pack(80, 100), 0, 1, CursorTables());
  REQUIRE(frame.has_value());
  if (!frame.has_value()) return;
  CHECK(frame->sprites[0] ==
        battle::SphereCursorSprite{.brightness = 0x80,
                                   .xy = Pack(84, 92),
                                   .wh = 0x7000A,
                                   .texcoord = 0x37FD38E8});
  CHECK(frame->sprites[1].brightness == 0x80);
  CHECK(frame->sprites[2].brightness == 240);
  CHECK(frame->sprites[2].xy == Pack(77, 97));
}

TEST_CASE("Sphere position cursor highlights the locked pitch pose",
          "[battle][sphere-draw]") {
  const auto frame =
      battle::DrawSpherePositionCursor(Pack(80, 100), 1, 15, CursorTables());
  REQUIRE(frame.has_value());
  if (!frame.has_value()) return;
  CHECK(frame->blink == 0);
  CHECK_FALSE(frame->pitch_quads.has_value());
  CHECK(frame->sprites[0].brightness == 128);
  CHECK(frame->sprites[0].texcoord == 0x37FD30E8U);
  CHECK(frame->sprites[1].brightness == 0x80);
  CHECK(frame->sprites[2].xy == Pack(76, 105));
  CHECK(frame->sprites[2].wh == 0xB0009U);
}

TEST_CASE("Sphere position cursor refuses unknown pitch locks",
          "[battle][sphere-draw]") {
  CHECK_FALSE(
      battle::DrawSpherePositionCursor(0, 2, 0, CursorTables()).has_value());
  CHECK_FALSE(
      battle::DrawSpherePositionCursor(0, -1, 0, CursorTables()).has_value());
}

TEST_CASE("Sphere terrain row caches flat open tiles",
          "[battle][sphere-draw]") {
  FakeTerrain terrain;
  const auto row = battle::CacheSphereTerrainRow(640, 640, 4, terrain);
  CHECK(row.status == battle::SphereTerrainRowStatus::kCached);
  CHECK(row.heights[4] == Packed(1, 200, -16));
  CHECK(row.flags[4] == 0);
  CHECK(terrain.samples.size() == std::size_t{16} * 9);
  CHECK(terrain.samples[0] == std::pair{128, 640});
  CHECK(terrain.samples[1] == std::pair{160, 640});
  CHECK(terrain.samples[4] == std::pair{128, 672});
}

TEST_CASE("Sphere terrain row marks tiles outside the map open",
          "[battle][sphere-draw]") {
  FakeTerrain terrain;
  auto row = battle::CacheSphereTerrainRow(0, 0, 0, terrain);
  CHECK(row.status == battle::SphereTerrainRowStatus::kCached);
  for (const std::int32_t height : row.heights) {
    CHECK(height == 0x10000);
  }
  row = battle::CacheSphereTerrainRow(0, 640, 4, terrain);
  CHECK(row.heights[3] == 0x10000);
  CHECK(row.heights[4] == Packed(1, 200, -16));
  CHECK(battle::CacheSphereTerrainRow(0, 0, 9, terrain).status ==
        battle::SphereTerrainRowStatus::kRowOutOfRange);
}

TEST_CASE("Sphere terrain row blocks and flags doors",
          "[battle][sphere-draw]") {
  FakeTerrain terrain;
  terrain.uneven = true;
  terrain.cells[{1, 5}] = kBlocks;
  terrain.cells[{2, 5}] = 2U << kDoorShift;
  terrain.cells[{3, 5}] = 5U << kDoorShift;
  terrain.cells[{4, 5}] = (5U << kDoorShift) | kFloorFlag;
  const auto row = battle::CacheSphereTerrainRow(640, 640, 4, terrain);
  CHECK(row.heights[0] == static_cast<std::int32_t>(0xF0800000U));
  CHECK(row.flags[0] == 0);
  // Uneven floors: low becomes the highest floor for doors 1 to 4.
  CHECK(row.heights[1] == Packed(0, 200, -16 + 0x60));
  CHECK(row.flags[1] == 12);
  CHECK(row.heights[2] == 0x10000);
  CHECK(row.heights[3] == Packed(0, 200, -16));
  CHECK(row.flags[3] == 0);
}

TEST_CASE("Sphere terrain row lifts tiles under gimmick stacks",
          "[battle][sphere-draw]") {
  FakeTerrain terrain;
  terrain.stacks[{5, 5}] = 0x40;
  terrain.object = 0x09;
  auto row = battle::CacheSphereTerrainRow(640, 640, 4, terrain);
  CHECK(row.heights[4] == Packed(0, 200, -16 + 0x60));
  terrain.object = 0x0B;
  row = battle::CacheSphereTerrainRow(640, 640, 4, terrain);
  CHECK(row.heights[4] == Packed(1, 200, -16 + 0x50));
  terrain.object = std::nullopt;
  row = battle::CacheSphereTerrainRow(640, 640, 4, terrain);
  CHECK(row.status == battle::SphereTerrainRowStatus::kMissingGimmick);
}
