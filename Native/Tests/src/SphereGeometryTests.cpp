#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "Battle/SphereGeometry.hpp"
#include "Battle/SphereGeometryLive.hpp"
#include "Battle/TargetVolume.hpp"

using VS::Battle::TargetVolume;

namespace {
namespace battle = VS::Battle;

constexpr std::int16_t kFixedOne = 4096;
constexpr std::size_t kCircleVertices = 33;
constexpr std::size_t kMaximumWireVertices = 297;
constexpr std::int16_t kSentinel = -1234;

template <typename T>
const T& Required(const std::optional<T>& result) {
  if (!result.has_value()) {
    throw std::runtime_error("expected sphere geometry operation to succeed");
  }
  return *result;
}

battle::SphereArcBasis Basis() {
  return {{{10, 20, 30}, {100, 0, 0}, {0, 0, 50}}};
}

std::array<std::int16_t, 32> SineSamples() {
  std::array<std::int16_t, 32> samples{};
  samples[1] = kFixedOne;
  samples[8] = kFixedOne;
  samples[10] = -kFixedOne;
  return samples;
}

std::array<battle::TargetWireVertex, kMaximumWireVertices> SentinelVertices() {
  std::array<battle::TargetWireVertex, kMaximumWireVertices> vertices{};
  for (auto& vertex : vertices) {
    vertex.position = {kSentinel, kSentinel, kSentinel};
    vertex.segment = kSentinel;
  }
  return vertices;
}

battle::SphereGeometryTables Tables(
    std::span<const std::int16_t> arc_bases,
    std::span<const std::int16_t> mirrored_vertices,
    std::span<const std::uint8_t> starts,
    std::span<const std::uint16_t> commands,
    std::span<const std::int16_t> sine_samples) {
  return {.arc_bases = arc_bases,
          .mirrored_vertices = mirrored_vertices,
          .shape_command_starts = starts,
          .commands = commands,
          .sine_samples = sine_samples};
}

struct ArcMirrorFixture {
  std::array<std::int16_t, 4> arc_bases{10, 20, 30, 40};
  std::array<std::int16_t, 6> mirrored_vertices{100, 200, 300, -400, 500, -600};
  std::array<std::uint8_t, 2> starts{0, 4};
  std::array<std::uint16_t, 4> commands{0, 0x4000, 0x8000, 0xC000};
  std::array<std::int16_t, 32> sine_samples = SineSamples();

  battle::SphereGeometryTables BuildTables() const {
    return Tables(arc_bases, mirrored_vertices, starts, commands, sine_samples);
  }
};
}  // namespace

TEST_CASE("F01285 appends sampled arc vertices with exact segment flags",
          "[battle][sphere-geometry][F01285]") {
  const auto samples = SineSamples();
  auto vertices = SentinelVertices();
  const auto next =
      battle::AppendSphereArcVertices(Basis(), samples, 0, 2, vertices, 1);
  REQUIRE(next);
  CHECK(*next == 4);
  CHECK(vertices[0].position ==
        battle::TargetWorldPoint{kSentinel, kSentinel, kSentinel});
  CHECK(vertices[1].position == battle::TargetWorldPoint{110, 20, 30});
  CHECK(vertices[2].position == battle::TargetWorldPoint{10, 20, 80});
  CHECK(vertices[3].position == battle::TargetWorldPoint{-90, 20, 30});
  CHECK(vertices[1].segment == 0);
  CHECK(vertices[2].segment == 2);
  CHECK(vertices[3].segment == 2);
}

TEST_CASE("F01285 closes full arcs and refuses unbounded inputs",
          "[battle][sphere-geometry][F01285]") {
  const auto samples = SineSamples();
  auto vertices = SentinelVertices();
  const auto next =
      battle::AppendSphereArcVertices(Basis(), samples, 0, 32, vertices, 0);
  REQUIRE(next);
  CHECK(*next == 33);
  CHECK(vertices[0].segment == 0);
  CHECK(vertices[1].segment == 1);
  CHECK(vertices[32].segment == 1);
  CHECK(vertices[32].position == vertices[0].position);
  CHECK_FALSE(battle::AppendSphereArcVertices(
      Basis(), std::span(samples).first(31), 0, 2, vertices, 0));
  CHECK_FALSE(
      battle::AppendSphereArcVertices(Basis(), samples, 0, 33, vertices, 0));
  CHECK_FALSE(battle::AppendSphereArcVertices(Basis(), samples, 0, 2,
                                              std::span(vertices).first(2), 0));
}

TEST_CASE("F01286 returns a typed scaled yaw and pitch transform",
          "[battle][sphere-geometry][F01286]") {
  const auto identity = battle::BuildSphereVolumeTransform({8, 4, 2, 0}, 0);
  CHECK(identity.m[0] == std::array<std::int16_t, 3>{256, 0, 0});
  CHECK(identity.m[1] == std::array<std::int16_t, 3>{0, 128, 0});
  CHECK(identity.m[2] == std::array<std::int16_t, 3>{0, 0, 64});
  const auto yaw =
      battle::BuildSphereVolumeTransform({8, 4, 2, 0}, kFixedOne / 4);
  CHECK(yaw.m[0] == std::array<std::int16_t, 3>{0, 0, 64});
  CHECK(yaw.m[1] == std::array<std::int16_t, 3>{0, 128, 0});
  CHECK(yaw.m[2] == std::array<std::int16_t, 3>{-256, 0, 0});
  const auto pitch = battle::BuildSphereVolumeTransform({8, 4, 2, 64}, 0);
  CHECK(pitch.m[0] == std::array<std::int16_t, 3>{256, 0, 0});
  CHECK(pitch.m[1] == std::array<std::int16_t, 3>{0, 0, 64});
  CHECK(pitch.m[2] == std::array<std::int16_t, 3>{0, -128, 0});
}

TEST_CASE("F01286 live mapping rejects a missing volume",
          "[battle][sphere-geometry][F01286][live]") {
  CHECK_FALSE(battle::BuildLiveSphereVolumeTransform(nullptr));
  TargetVolume volume{};
  volume.scale.extents[0] = 8;
  volume.scale.extents[1] = 4;
  volume.scale.extents[2] = 2;
  const auto transform = battle::BuildLiveSphereVolumeTransform(&volume);
  const auto& value = Required(transform);
  CHECK(value.m[0][0] == 256);
  CHECK(value.m[1][1] == 128);
  CHECK(value.m[2][2] == 64);
}

TEST_CASE("F01303 appends circle positions from the current basis",
          "[battle][sphere-geometry][F01303]") {
  auto vertices = SentinelVertices();
  const auto next = battle::AppendSphereCircleVertices(Basis(), 0, vertices, 2);
  CHECK(Required(next) == 2 + kCircleVertices);
  CHECK(vertices[0].position ==
        battle::TargetWorldPoint{kSentinel, kSentinel, kSentinel});
  CHECK(vertices[2].position == battle::TargetWorldPoint{110, 20, 30});
  CHECK(vertices[10].position == battle::TargetWorldPoint{10, 20, 80});
  CHECK(vertices[18].position == battle::TargetWorldPoint{-90, 20, 30});
}

TEST_CASE("F01303 closes the circle with the expected segment flags",
          "[battle][sphere-geometry][F01303]") {
  auto vertices = SentinelVertices();
  const auto next = battle::AppendSphereCircleVertices(Basis(), 0, vertices, 2);
  CHECK(Required(next) == 2 + kCircleVertices);
  CHECK(vertices[34].position == vertices[2].position);
  CHECK(vertices[2].segment == 0);
  CHECK(vertices[3].segment == 1);
  CHECK(vertices[34].segment == 1);
}

TEST_CASE("F01303 refuses a circle that exceeds its output span",
          "[battle][sphere-geometry][F01303]") {
  auto vertices = SentinelVertices();
  CHECK_FALSE(battle::AppendSphereCircleVertices(
      Basis(), 0, std::span(vertices).first(kCircleVertices - 1), 0));
  CHECK_FALSE(battle::AppendSphereCircleVertices(Basis(), 0, vertices,
                                                 kMaximumWireVertices));
}

TEST_CASE("F01304 preserves rib order and omits the second small circle",
          "[battle][sphere-geometry][F01304]") {
  auto vertices = SentinelVertices();
  const auto appended = battle::AppendSphereConeVertices(0, 0, vertices, 0);
  const auto& result = Required(appended);
  CHECK(result.next_index == 4 + kCircleVertices);
  CHECK(vertices[0].position == battle::TargetWorldPoint{0, 0, 0});
  CHECK(vertices[3].position == battle::TargetWorldPoint{0, 0, 0});
  CHECK(vertices[4].segment == 0);
  CHECK(vertices[5].segment == 1);
  CHECK(result.basis == battle::SphereArcBasis{});
}

TEST_CASE("F01304 appends four spirals in rib order",
          "[battle][sphere-geometry][F01304]") {
  auto vertices = SentinelVertices();
  const auto appended = battle::AppendSphereConeVertices(512, 1, vertices, 0);
  const auto& result = Required(appended);
  CHECK(result.next_index == 8 + 2 * kCircleVertices);
  CHECK(vertices[0].position == battle::TargetWorldPoint{0, 0, 0});
  CHECK(vertices[1].position == battle::TargetWorldPoint{-362, -512, -362});
  CHECK(vertices[1].segment == 1);
}

TEST_CASE("F01304 appends both closing circles and reports the final basis",
          "[battle][sphere-geometry][F01304]") {
  auto vertices = SentinelVertices();
  const auto appended = battle::AppendSphereConeVertices(512, 1, vertices, 0);
  const auto& result = Required(appended);
  CHECK(result.next_index == 8 + 2 * kCircleVertices);
  CHECK(vertices[8].segment == 0);
  CHECK(vertices[40].segment == 1);
  CHECK(vertices[41].segment == 0);
  CHECK(vertices[73].segment == 1);
  CHECK(result.basis[0][1] == -kFixedOne / 2);
  CHECK(result.basis[1][0] == kFixedOne / 2);
  CHECK(result.basis[2][2] == kFixedOne / 2);
}

TEST_CASE("F01304 refuses cone radii beyond the supported range",
          "[battle][sphere-geometry][F01304]") {
  auto vertices = SentinelVertices();
  CHECK_FALSE(battle::AppendSphereConeVertices(0, 9, vertices, 0));
}

TEST_CASE("F01305 dispatches arc and advances the animation state",
          "[battle][sphere-geometry][F01305]") {
  const ArcMirrorFixture fixture{};
  const auto tables = fixture.BuildTables();
  auto vertices = SentinelVertices();
  const auto built = battle::BuildSphereWireframe(
      0, {.phase_a = 10, .phase_b = 20, .phase_c = 30, .shape = 0}, tables,
      vertices);
  const auto& result = Required(built);
  CHECK(result.state.phase_a == 13);
  CHECK(result.state.phase_b == 27);
  CHECK(result.state.phase_c == 41);
  CHECK(result.state.shape == 0);
  CHECK(result.vertex_count == 111);
  CHECK(vertices[0].position == battle::TargetWorldPoint{10, 60, 30});
  CHECK(vertices[0].segment == 0);
  CHECK(vertices[1].segment == 1);
}

TEST_CASE("F01305 dispatches mirrored vertices after earlier commands",
          "[battle][sphere-geometry][F01305]") {
  const ArcMirrorFixture fixture{};
  const auto tables = fixture.BuildTables();
  auto vertices = SentinelVertices();
  const auto built = battle::BuildSphereWireframe(
      0, {.phase_a = 10, .phase_b = 20, .phase_c = 30, .shape = 0}, tables,
      vertices);
  CHECK(Required(built).vertex_count == 111);
  CHECK(vertices[107].position == battle::TargetWorldPoint{100, 200, 300});
  CHECK(vertices[108].position == battle::TargetWorldPoint{-400, 500, -600});
  CHECK(vertices[109].position == battle::TargetWorldPoint{-100, 200, -300});
  CHECK(vertices[110].position == battle::TargetWorldPoint{400, 500, 600});
  CHECK(vertices[110].segment == 1);
}

TEST_CASE("F01305 routes static shapes without advancing animated phases",
          "[battle][sphere-geometry][F01305]") {
  const std::array<std::uint8_t, 51> starts{};
  const std::array<std::uint16_t, 1> ignored{0xC000};
  const auto tables = Tables({}, {}, starts, ignored, {});
  auto vertices = SentinelVertices();
  const auto built = battle::BuildSphereWireframe(
      4, {.phase_a = 1, .phase_b = 2, .phase_c = 3, .shape = 2}, tables,
      vertices);
  const auto& result = Required(built);
  CHECK(result.state.shape == 1);
  CHECK(result.state.phase_a == 1);
  CHECK(result.state.phase_b == 2);
  CHECK(result.state.phase_c == 3);
  CHECK(result.vertex_count == 0);
}

TEST_CASE("F01305 rejects invalid types and command start spans",
          "[battle][sphere-geometry][F01305]") {
  const std::array<std::uint8_t, 51> starts{};
  const std::array<std::uint16_t, 1> ignored{0xC000};
  const auto tables = Tables({}, {}, starts, ignored, {});
  auto vertices = SentinelVertices();
  CHECK_FALSE(battle::BuildSphereWireframe(5, {}, tables, vertices));
  CHECK_FALSE(battle::BuildSphereWireframe(
      0, {}, Tables({}, {}, std::span(starts).first(1), ignored, {}),
      vertices));
}

TEST_CASE("F01305 rejects incomplete mirrored tables and output",
          "[battle][sphere-geometry][F01305]") {
  const std::array<std::int16_t, 6> mirrored{1, 2, 3, 4, 5, 6};
  const std::array<std::uint8_t, 2> mirrored_starts{0, 1};
  const std::array<std::uint16_t, 1> mirror_command{0x8000};
  const auto mirror_tables = Tables({}, std::span(mirrored).first(5),
                                    mirrored_starts, mirror_command, {});
  auto vertices = SentinelVertices();
  CHECK_FALSE(battle::BuildSphereWireframe(0, {}, mirror_tables, vertices));
  CHECK_FALSE(battle::BuildSphereWireframe(
      0, {}, Tables({}, {}, mirrored_starts, mirror_command, {}),
      std::span(vertices).first(3)));
}

TEST_CASE("F01305 live mapping rejects a missing sphere",
          "[battle][sphere-geometry][F01305][live]") {
  const std::array<std::int16_t, 6> mirrored{1, 2, 3, 4, 5, 6};
  const std::array<std::uint8_t, 2> starts{0, 1};
  const std::array<std::uint16_t, 1> commands{0x8000};
  const auto tables = Tables({}, mirrored, starts, commands, {});
  CHECK_FALSE(battle::BuildLiveSphereWireframe(nullptr, 0, tables));
}

TEST_CASE("F01305 live mapping commits animation state and vertex count",
          "[battle][sphere-geometry][F01305][live]") {
  const std::array<std::int16_t, 6> mirrored{1, 2, 3, 4, 5, 6};
  const std::array<std::uint8_t, 2> starts{0, 1};
  const std::array<std::uint16_t, 1> commands{0x8000};
  const auto tables = Tables({}, mirrored, starts, commands, {});

  auto sphere = std::make_unique<VS::Battle::TargetSphereSetupView>();
  sphere->wire_phase_a = 10;
  sphere->wire_phase_b = 20;
  sphere->wire_phase_c = 30;
  sphere->wire_shape = 0;
  sphere->pulse_mode_and_vertex_count = 0x12345678;
  sphere->wire_vertices[4].xyz[0] = -99;
  const auto built = battle::BuildLiveSphereWireframe(sphere.get(), 0, tables);
  CHECK(Required(built).vertex_count == 4);
  CHECK(sphere->wire_phase_a == 13);
  CHECK(sphere->wire_phase_b == 27);
  CHECK(sphere->wire_phase_c == 41);
  CHECK(sphere->pulse_mode_and_vertex_count == 0x00045678);
}

TEST_CASE("F01305 live mapping writes vertices and preserves the suffix",
          "[battle][sphere-geometry][F01305][live]") {
  const std::array<std::int16_t, 6> mirrored{1, 2, 3, 4, 5, 6};
  const std::array<std::uint8_t, 2> starts{0, 1};
  const std::array<std::uint16_t, 1> commands{0x8000};
  const auto tables = Tables({}, mirrored, starts, commands, {});
  auto sphere = std::make_unique<VS::Battle::TargetSphereSetupView>();
  sphere->wire_shape = 0;
  sphere->pulse_mode_and_vertex_count = 0x12345678;
  sphere->wire_vertices[4].xyz[0] = -99;
  const auto built = battle::BuildLiveSphereWireframe(sphere.get(), 0, tables);
  CHECK(Required(built).vertex_count == 4);
  CHECK(sphere->wire_vertices[0].xyz[0] == 1);
  CHECK(sphere->wire_vertices[0].segment == 0);
  CHECK(sphere->wire_vertices[1].xyz[0] == 4);
  CHECK(sphere->wire_vertices[2].xyz[0] == -1);
  CHECK(sphere->wire_vertices[3].xyz[2] == -6);
  CHECK(sphere->wire_vertices[4].xyz[0] == -99);
}
