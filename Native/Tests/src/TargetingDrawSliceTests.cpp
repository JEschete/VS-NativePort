#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/TargetingDraw.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace {
namespace battle = VS::Battle;
namespace math = VS::Core::Math;

constexpr std::int16_t kUnit = 4096;
constexpr std::uint32_t kColor = 0x010401;
constexpr std::uint32_t kWireCommand = 0x44000000;
constexpr std::uint32_t kSegmentStep = 0x02000000;

math::Mat3 Identity() {
  math::Mat3 matrix;
  for (int index = 0; index < math::kMatrixOrder; ++index) {
    matrix.m[index][index] = kUnit;
  }
  return matrix;
}

struct DrawnLine {
  battle::TargetWirePoint start;
  battle::TargetWirePoint end;
  std::uint32_t command_color = 0;
  int intensity = 0;
};

// Projects a world point onto itself so the expected screen coordinates are
// the world coordinates.
class RecordingCanvas final : public battle::TargetWireCanvas {
 public:
  math::Mat3 transform = Identity();
  std::vector<std::string> calls;
  std::vector<DrawnLine> lines;
  battle::TargetVolumeExtents extents{};
  std::int16_t angle = 0;

  math::Mat3 VolumeTransform(const battle::TargetVolumeExtents& scale,
                             std::int16_t tilt) override {
    calls.emplace_back("transform");
    extents = scale;
    angle = tilt;
    return transform;
  }
  void BeginProjection() override { calls.emplace_back("view"); }
  battle::TargetWirePoint Project(
      const battle::TargetWorldPoint& world) override {
    calls.emplace_back("project");
    return {.x = world[0], .y = world[1], .depth = world[2]};
  }
  void DrawLine(const battle::TargetWirePoint& start,
                const battle::TargetWirePoint& end, std::uint32_t command_color,
                int intensity) override {
    calls.emplace_back("line");
    lines.push_back({start, end, command_color, intensity});
  }
};

battle::TargetWireVolume Volume() {
  return {.extents = {4, 4, 4, 0}, .angle = 0, .position = {10, 20, 30}};
}
}  // namespace

TEST_CASE("Typed sphere wireframe draws nothing for too few vertices",
          "[battle][targeting][typed]") {
  const std::array<battle::TargetWireVertex, 1> vertices{};
  RecordingCanvas canvas;
  battle::DrawSphereVolumeWireframe({.vertices = vertices}, Volume(), kColor,
                                    canvas);
  CHECK(canvas.calls.empty());
}

TEST_CASE("Typed sphere wireframe draws nothing without horizontal extent",
          "[battle][targeting][typed]") {
  const std::array<battle::TargetWireVertex, 2> vertices{};
  auto volume = Volume();
  volume.extents = {0, 9, 0, 0};
  RecordingCanvas canvas;
  battle::DrawSphereVolumeWireframe({.vertices = vertices}, volume, kColor,
                                    canvas);
  CHECK(canvas.calls.empty());
}

namespace {
battle::TargetWireVolume JoinVolume() {
  auto volume = Volume();
  volume.extents = {2, 7, 5, 0x30};
  volume.angle = 0x123;
  return volume;
}

void DrawJoinScene(RecordingCanvas& canvas) {
  const std::array<battle::TargetWireVertex, 4> vertices{{
      {.position = {0, 0, 0}, .segment = 3},
      {.position = {1, 0, 1}, .segment = 1},
      {.position = {0, 1, 0}, .segment = 0},
      {.position = {0, 0, 2}, .segment = 2},
  }};
  battle::DrawSphereVolumeWireframe({.depth = 8, .vertices = vertices},
                                    JoinVolume(), kColor, canvas);
}
}  // namespace

TEST_CASE("Typed sphere wireframe projects flagged vertices in order",
          "[battle][targeting][typed]") {
  RecordingCanvas canvas;
  DrawJoinScene(canvas);
  CHECK(canvas.extents == JoinVolume().extents);
  CHECK(canvas.angle == JoinVolume().angle);
  CHECK(canvas.calls == std::vector<std::string>{"transform", "view", "project",
                                                 "project", "line", "project",
                                                 "project", "line"});
}

TEST_CASE("Typed sphere wireframe fades and colours the first segment",
          "[battle][targeting][typed]") {
  RecordingCanvas canvas;
  DrawJoinScene(canvas);
  REQUIRE(canvas.lines.size() == 2);
  const auto& first = canvas.lines[0];
  CHECK(first.start.x == 11);
  CHECK(first.start.depth == 31);
  CHECK(first.end.x == 10);
  CHECK(first.end.depth == 30);
  CHECK(first.command_color == (kWireCommand | kColor) - kSegmentStep);
  CHECK(first.intensity == 32 - ((31 + 30 - 16) / 5));
}

TEST_CASE("Typed sphere wireframe lowers the command by each segment step",
          "[battle][targeting][typed]") {
  RecordingCanvas canvas;
  DrawJoinScene(canvas);
  REQUIRE(canvas.lines.size() == 2);
  const auto& second = canvas.lines[1];
  CHECK(second.start.depth == 32);
  CHECK(second.end.y == 21);
  CHECK(second.command_color == (kWireCommand | kColor) - 2 * kSegmentStep);
  CHECK(second.intensity == 32 - ((32 + 30 - 16) / 5));
}

TEST_CASE("Typed sphere wireframe pulses, tilts and saturates world points",
          "[battle][targeting][typed]") {
  const std::array<battle::TargetWireVertex, 2> vertices{{
      {.position = {1, 0, 0}},
      {.position = {INT16_MAX, INT16_MIN, 0}, .segment = 1},
  }};
  auto volume = Volume();
  volume.position = {0, 0, 0};
  RecordingCanvas canvas;
  canvas.transform.m[0][0] = 2 * kUnit;
  // A quarter-turn pulse (cos(16 * 64) is zero) collapses the wire onto the
  // volume centre.
  battle::DrawSphereVolumeWireframe({.pulse = 16, .vertices = vertices}, volume,
                                    kColor, canvas);
  REQUIRE(canvas.lines.size() == 1);
  CHECK(canvas.lines[0].end.x == 0);
  CHECK(canvas.lines[0].start.x == 0);

  RecordingCanvas full;
  full.transform.m[0][0] = 2 * kUnit;
  full.transform.m[1][1] = 2 * kUnit;
  battle::DrawSphereVolumeWireframe({.vertices = vertices}, volume, kColor,
                                    full);
  REQUIRE(full.lines.size() == 1);
  CHECK(full.lines[0].end.x == 2);
  CHECK(full.lines[0].start.x == INT16_MAX);
  CHECK(full.lines[0].start.y == INT16_MIN);
}
