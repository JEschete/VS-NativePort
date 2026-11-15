#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "Battle/SphereLifecycle.hpp"
#include "Battle/SphereLifecycleLive.hpp"
#include "Core/Memory/HeapLive.hpp"

extern "C" {
#include "Battle/Sphere.h"
#include "Battle/TargetingState.h"
}

namespace {
namespace battle = VS::Battle;

constexpr battle::SphereVolume kPrimary{
    .kind = 2,
    .range_kind = 3,
    .angle = 0x1234,
    .extents = {4, 5, 6, 7},
    .position = {11, 12, 13, 14},
};
constexpr battle::SphereVolume kSecondary{
    .kind = 1,
    .range_kind = 8,
    .angle = -0x234,
    .extents = {9, 10, 11, 12},
    .position = {-21, -22, -23, -24},
};
constexpr std::uint16_t kEdgeSentinel = 0xBEEF;

Battle_TargetSphereSpec LegacySpec(const battle::SphereVolume& value) {
  Battle_TargetVolume raw{};
  raw.kind = static_cast<u_char>(value.kind);
  raw.opaque1 = static_cast<u_char>(value.range_kind);
  raw.angle = static_cast<std::int16_t>(value.angle);
  for (std::size_t index = 0; index < value.extents.size(); ++index) {
    raw.scale.extents[index] = value.extents[index];
  }
  raw.position[0] = value.position.x;
  raw.position[1] = value.position.y;
  raw.position[2] = value.position.z;
  raw.position[3] = value.position.pad;
  return std::bit_cast<Battle_TargetSphereSpec>(raw);
}

int gReleaseCount = 0;
const void* gReleasedSphere = nullptr;

void RecordRelease(const void* sphere) {
  ++gReleaseCount;
  gReleasedSphere = sphere;
}
}  // namespace

TEST_CASE("Sphere initialization resets lifecycle state",
          "[battle][sphere][lifecycle]") {
  battle::SphereLifecycleState state{
      .primary_volume = kPrimary,
      .secondary_volume = kSecondary,
      .wire_phase_a = 1,
      .wire_phase_b = 2,
      .wire_phase_c = 3,
      .wire_shape = 4,
      .target_count = 5,
      .selected_target = 6,
      .sphere_mode = 7,
      .reserved_38 = 8,
      .reserved_3c_and_input_locked = 9,
  };
  battle::InitializeSphereState(state);
  const battle::SphereLifecycleState expected{.selected_target = -1};
  CHECK(state == expected);
}

TEST_CASE("Sphere edge lists retain all four directional orders",
          "[battle][sphere][lifecycle]") {
  std::array<std::uint16_t, battle::kSphereWireEdgeCapacity> edges{};
  CHECK(battle::BuildSphereWireEdges(edges));
  CHECK(edges[0] == 0x0100);
  CHECK(edges[9] == 0x1000);
  CHECK(edges[180] == 0x8090);
  CHECK(edges[360] == 0x9899);
  CHECK(edges[540] == 0x1909);
  CHECK(edges[battle::kSphereWireEdgeCount - 1] == 0x9080);
}

TEST_CASE("Sphere edge builder refuses a short span without partial writes",
          "[battle][sphere][lifecycle]") {
  std::array<std::uint16_t, battle::kSphereWireEdgeCount - 1> edges{};
  edges.fill(kEdgeSentinel);
  CHECK_FALSE(battle::BuildSphereWireEdges(edges));
  CHECK(edges.front() == kEdgeSentinel);
  CHECK(edges.back() == kEdgeSentinel);
}

TEST_CASE("Sphere volume setup copies and activates a secondary volume",
          "[battle][sphere][lifecycle]") {
  battle::SphereLifecycleState state{
      .primary_volume = kSecondary,
      .secondary_volume = kPrimary,
      .wire_phase_a = 21,
      .wire_phase_b = 22,
      .wire_phase_c = 23,
      .wire_shape = 24,
      .sphere_mode = 25,
      .reserved_38 = 26,
      .reserved_3c_and_input_locked = 27,
  };
  bool reuse = false;
  const auto result =
      battle::SetSphereVolumes(state, kPrimary, &kSecondary, 31, reuse);
  REQUIRE(result.has_value());
  CHECK(*result == battle::SphereVolumeSetResult::kSecondarySet);
  auto active_secondary = kSecondary;
  active_secondary.kind |= 0x80;
  const battle::SphereLifecycleState expected{
      .primary_volume = kPrimary,
      .secondary_volume = active_secondary,
      .wire_phase_b = 22,
      .wire_shape = 0xF,
      .sphere_mode = 31,
  };
  CHECK(state == expected);
  CHECK_FALSE(reuse);
}

TEST_CASE("Sphere volume setup preserves phase values for primary kind one",
          "[battle][sphere][lifecycle]") {
  auto primary = kPrimary;
  primary.kind = 1;
  battle::SphereLifecycleState state{
      .wire_phase_a = 41,
      .wire_phase_b = 42,
      .wire_phase_c = 43,
  };
  bool reuse = false;
  const auto result =
      battle::SetSphereVolumes(state, primary, nullptr, 0, reuse);
  REQUIRE(result.has_value());
  CHECK(*result == battle::SphereVolumeSetResult::kSecondaryDisabled);
  CHECK(state.primary_volume == primary);
  CHECK(state.wire_phase_a == 41);
  CHECK(state.wire_phase_b == 42);
  CHECK(state.wire_phase_c == 43);
}

TEST_CASE("Sphere volume reuse changes only the original reused fields",
          "[battle][sphere][lifecycle]") {
  battle::SphereLifecycleState state{
      .primary_volume = kSecondary,
      .secondary_volume = kPrimary,
      .wire_phase_a = 31,
      .wire_phase_b = 32,
      .wire_phase_c = 33,
      .wire_shape = 34,
      .sphere_mode = 35,
      .reserved_38 = 36,
      .reserved_3c_and_input_locked = 37,
  };
  auto expected = state;
  expected.primary_volume = kPrimary;
  expected.primary_volume.kind |= 0x80;
  expected.secondary_volume.kind = kSecondary.kind;
  expected.wire_phase_a = 0;
  expected.wire_phase_c = 0;
  bool reuse = true;
  const auto result =
      battle::SetSphereVolumes(state, kPrimary, &kSecondary, 41, reuse);
  REQUIRE(result.has_value());
  CHECK(*result == battle::SphereVolumeSetResult::kReused);
  CHECK(state == expected);
  CHECK_FALSE(reuse);
}

TEST_CASE("Sphere volume setup preserves the null-secondary asymmetry",
          "[battle][sphere][lifecycle]") {
  battle::SphereLifecycleState state{
      .primary_volume = kPrimary,
      .secondary_volume = kSecondary,
      .wire_phase_a = 51,
      .wire_phase_b = 52,
      .wire_phase_c = 53,
      .wire_shape = 54,
      .sphere_mode = 55,
      .reserved_38 = 56,
      .reserved_3c_and_input_locked = 57,
  };
  auto expected = state;
  expected.primary_volume = kPrimary;
  expected.secondary_volume.kind = 0;
  expected.wire_shape = 0xF;
  expected.reserved_38 = 0;
  expected.reserved_3c_and_input_locked = 0;
  expected.wire_phase_a = 0;
  expected.wire_phase_c = 0;
  bool reuse = false;
  const auto result =
      battle::SetSphereVolumes(state, kPrimary, nullptr, 61, reuse);
  REQUIRE(result.has_value());
  CHECK(*result == battle::SphereVolumeSetResult::kSecondaryDisabled);
  CHECK(state == expected);
}

TEST_CASE("Sphere volume reuse refuses a missing secondary before mutation",
          "[battle][sphere][lifecycle]") {
  battle::SphereLifecycleState state{
      .primary_volume = kSecondary,
      .secondary_volume = kPrimary,
      .wire_phase_a = 71,
      .wire_phase_c = 72,
      .wire_shape = 73,
      .reserved_38 = 74,
      .reserved_3c_and_input_locked = 75,
  };
  const auto original = state;
  bool reuse = true;
  const auto result =
      battle::SetSphereVolumes(state, kPrimary, nullptr, 81, reuse);
  CHECK_FALSE(result.has_value());
  CHECK(state == original);
  CHECK(reuse);
}

TEST_CASE("Clearing the primary sphere volume preserves its other fields",
          "[battle][sphere][lifecycle]") {
  battle::SphereLifecycleState state{.primary_volume = kPrimary};
  battle::ClearPrimarySphereVolume(state);
  CHECK(state.primary_volume.kind == 0);
  CHECK(state.primary_volume.range_kind == kPrimary.range_kind);
  CHECK(state.primary_volume.angle == kPrimary.angle);
  CHECK(state.primary_volume.extents == kPrimary.extents);
  CHECK(state.primary_volume.position == kPrimary.position);
}

TEST_CASE("Secondary sphere volume setup copies the volume and ends transition",
          "[battle][sphere][lifecycle]") {
  battle::SphereLifecycleState state{.wire_shape = 91};
  battle::SetSecondarySphereVolume(state, kSecondary);
  CHECK(state.secondary_volume == kSecondary);
  CHECK(state.wire_shape == 0);
}

TEST_CASE("Clearing the secondary sphere volume preserves its other fields",
          "[battle][sphere][lifecycle]") {
  battle::SphereLifecycleState state{.secondary_volume = kSecondary};
  battle::ClearSecondarySphereVolume(state);
  CHECK(state.secondary_volume.kind == 0);
  CHECK(state.secondary_volume.range_kind == kSecondary.range_kind);
  CHECK(state.secondary_volume.angle == kSecondary.angle);
  CHECK(state.secondary_volume.extents == kSecondary.extents);
  CHECK(state.secondary_volume.position == kSecondary.position);
}

TEST_CASE("Sphere target clearing changes only the target count",
          "[battle][sphere][lifecycle]") {
  battle::SphereLifecycleState state{
      .wire_shape = 1,
      .target_count = 12,
      .selected_target = 3,
  };
  battle::ClearSphereTargetCount(state);
  CHECK(state.target_count == 0);
  CHECK(state.selected_target == 3);
  CHECK(state.wire_shape == 1);
}

TEST_CASE("Sphere interaction mode handles absent, transitioning and locked",
          "[battle][sphere][lifecycle]") {
  CHECK(battle::SphereInteractionMode(nullptr, false) == 0);
  battle::SphereLifecycleState state{.wire_shape = 1};
  CHECK(battle::SphereInteractionMode(&state, false) == 0);
  state.wire_shape = 0;
  state.reserved_3c_and_input_locked = 1U << 16;
  CHECK(battle::SphereInteractionMode(&state, false) == 2);
  CHECK(battle::SphereInteractionMode(&state, true) == 1);
  state.reserved_3c_and_input_locked = 0;
  CHECK(battle::SphereInteractionMode(&state, false) == 1);
}

TEST_CASE("Live sphere initialization maps state and directional edges",
          "[battle][sphere][lifecycle]") {
  Battle_TargetSphereSetupView sphere{};
  sphere.wirePhaseA = 91;
  sphere.wirePhaseB = 92;
  sphere.wirePhaseC = 93;
  sphere.selectedTarget = 94;
  VS::Battle::InitializeLiveSphere(&sphere);
  CHECK(sphere.wirePhaseA == 0);
  CHECK(sphere.wirePhaseB == 0);
  CHECK(sphere.wirePhaseC == 0);
  CHECK(sphere.selectedTarget == -1);
  CHECK(std::bit_cast<std::uint16_t>(sphere.wireEdges[0]) == 0x0100);
  CHECK(std::bit_cast<std::uint16_t>(sphere.wireEdges[719]) == 0x9080);
}

TEST_CASE("Live sphere release frees once and clears the pointer",
          "[battle][sphere][lifecycle]") {
  Battle_TargetSphereSetupView storage{};
  auto* sphere = &storage;
  gReleaseCount = 0;
  gReleasedSphere = nullptr;
  VS::Battle::FreeLiveSphere(sphere, RecordRelease);
  CHECK(sphere == nullptr);
  CHECK(gReleaseCount == 1);
  CHECK(gReleasedSphere == &storage);
  VS::Battle::FreeLiveSphere(sphere, nullptr);
  CHECK(gReleaseCount == 1);
}

TEST_CASE("Sphere release entry frees the workspace and clears its pointer",
          "[battle][sphere][lifecycle]") {
  std::array<std::byte, 65536> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  struct SpherePointerRestorer {
    Battle_TargetSphereSetupView* saved = Battle_targetSphere;
    ~SpherePointerRestorer() { Battle_targetSphere = saved; }
  } restore;
  auto* sphere = static_cast<Battle_TargetSphereSetupView*>(
      VS::Core::Memory::AllocateLiveFromLowEnd(
          sizeof(Battle_TargetSphereSetupView)));
  REQUIRE(sphere != nullptr);
  Battle_targetSphere = sphere;
  VS::Battle::FreeSphereEntry();
  CHECK(Battle_targetSphere == nullptr);
  CHECK(VS::Core::Memory::AllocateLiveFromLowEnd(sizeof(*sphere)) == sphere);
}

TEST_CASE("Live sphere volume binding preserves pointer and reuse asymmetries",
          "[battle][sphere][lifecycle]") {
  Battle_TargetSphereSetupView sphere{};
  Battle_TargetPreview previews[2]{};
  auto primary = LegacySpec(kPrimary);
  sphere.targetList1 = previews;
  sphere.secondaryVolume = LegacySpec(kPrimary);
  sphere.reserved38 = 5;
  sphere.reserved3C = 6;
  std::uint8_t reuse = 0;
  VS::Battle::SetLiveSphereVolumes(&sphere, &primary, nullptr, 7, nullptr,
                                   reuse);
  CHECK(sphere.secondaryVolume.kind == 0);
  CHECK(sphere.secondaryVolume.reserved1 == kPrimary.range_kind);
  CHECK(sphere.targetList1 == previews);
  CHECK(sphere.sphereMode == 0);
  CHECK(sphere.reserved38 == 0);
  CHECK(sphere.reserved3C == 0);
}

TEST_CASE("Live sphere binding updates the secondary preview only on set",
          "[battle][sphere][lifecycle]") {
  Battle_TargetSphereSetupView sphere{};
  Battle_TargetPreview previews[2]{};
  auto primary = LegacySpec(kPrimary);
  auto secondary = LegacySpec(kSecondary);
  std::uint8_t reuse = 0;
  VS::Battle::SetLiveSphereVolumes(&sphere, &primary, &secondary, 9, previews,
                                   reuse);
  CHECK(sphere.secondaryVolume.kind == (secondary.kind | 0x80));
  CHECK(sphere.sphereMode == 9);
  CHECK(sphere.targetList1 == previews);
  CHECK(reuse == 0);
}

TEST_CASE("Live sphere volume reuse leaves mode and preview pointer unchanged",
          "[battle][sphere][lifecycle]") {
  Battle_TargetSphereSetupView sphere{};
  Battle_TargetPreview previews[2]{};
  auto primary = LegacySpec(kPrimary);
  auto secondary = LegacySpec(kSecondary);
  sphere.targetList1 = previews;
  std::uint8_t reuse = 1;
  VS::Battle::SetLiveSphereVolumes(&sphere, &primary, &secondary, 9, nullptr,
                                   reuse);
  CHECK(reuse == 0);
  CHECK(sphere.secondaryVolume.kind == secondary.kind);
  CHECK(sphere.targetList1 == previews);
  CHECK(sphere.sphereMode == 0);
}

TEST_CASE("Live clear and interaction adapters preserve null asymmetry",
          "[battle][sphere][lifecycle]") {
  Battle_TargetSphereSetupView sphere{};
  const auto primary = LegacySpec(kPrimary);
  sphere.primaryVolume = primary;
  sphere.secondaryVolume = primary;
  sphere.targetCount = 9;
  sphere.selectedTarget = 3;
  sphere.wireShape = 0;
  sphere.reserved3C = 1 << 16;
  VS::Battle::ClearLivePrimarySphereVolume(nullptr);
  VS::Battle::ClearLiveSecondarySphereVolume(nullptr);
  VS::Battle::ClearLiveSphereTargets(nullptr);
  CHECK(VS::Battle::LiveSphereInteractionMode(nullptr, false) == 0);
  VS::Battle::ClearLivePrimarySphereVolume(&sphere);
  CHECK(sphere.primaryVolume.kind == 0);
  VS::Battle::ClearLiveSecondarySphereVolume(&sphere);
  CHECK(sphere.secondaryVolume.kind == 0);
  VS::Battle::ClearLiveSphereTargets(&sphere);
  CHECK(sphere.targetCount == 0);
  CHECK(sphere.selectedTarget == 3);
}

TEST_CASE("Live sphere interaction query honors lock, override and transition",
          "[battle][sphere][lifecycle]") {
  Battle_TargetSphereSetupView sphere{};
  sphere.wireShape = 0;
  sphere.reserved3C = 1 << 16;
  CHECK(VS::Battle::LiveSphereInteractionMode(&sphere, false) == 2);
  CHECK(VS::Battle::LiveSphereInteractionMode(&sphere, true) == 1);
  sphere.reserved3C = 0;
  CHECK(VS::Battle::LiveSphereInteractionMode(&sphere, false) == 1);
  sphere.wireShape = 1;
  CHECK(VS::Battle::LiveSphereInteractionMode(&sphere, true) == 0);
}

TEST_CASE("Live sphere volume binding preserves null secondary bytes",
          "[battle][sphere][lifecycle]") {
  Battle_TargetSphereSetupView sphere{};
  Battle_TargetPreview previous{};
  auto primary = LegacySpec(kPrimary);
  auto secondary = LegacySpec(kSecondary);
  sphere.targetList1 = &previous;
  sphere.secondaryVolume = secondary;
  sphere.sphereMode = 5;
  std::uint8_t reuse = 0;
  VS::Battle::SetLiveSphereVolumes(&sphere, &primary, nullptr, 9, nullptr,
                                   reuse);
  CHECK(sphere.secondaryVolume.kind == 0);
  CHECK(sphere.secondaryVolume.reserved1 == secondary.reserved1);
  CHECK(sphere.sphereMode == 5);
  CHECK(sphere.targetList1 == &previous);
}

TEST_CASE("Live secondary volume adapter copies and ends transition",
          "[battle][sphere][lifecycle]") {
  Battle_TargetSphereSetupView sphere{};
  const auto secondary = LegacySpec(kSecondary);
  sphere.wireShape = 91;
  VS::Battle::SetLiveSecondarySphereVolume(&sphere, &secondary);
  CHECK(std::bit_cast<Battle_TargetVolume>(sphere.secondaryVolume).kind ==
        kSecondary.kind);
  CHECK(sphere.wireShape == 0);
  VS::Battle::ClearLiveSecondarySphereVolume(nullptr);
}
