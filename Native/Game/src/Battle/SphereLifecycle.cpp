#include "Battle/SphereLifecycle.hpp"

namespace VS::Battle {
namespace {
constexpr int kTerrainGridCoordinateBits = 4;
constexpr int kEdgeTargetXShift = 8;
constexpr int kEdgeTargetYShift = 12;
constexpr int kTerrainGridDimension = 10;
constexpr int kTerrainGridMaximum = kTerrainGridDimension - 1;
constexpr std::uint8_t kVolumeActiveMask = 0x80;
constexpr std::int16_t kSphereTransitionShape = 0xF;

std::uint16_t EncodeSphereEdge(int x0, int y0, int x1, int y1) noexcept {
  return static_cast<std::uint16_t>(x0 | (y0 << kTerrainGridCoordinateBits) |
                                    (x1 << kEdgeTargetXShift) |
                                    (y1 << kEdgeTargetYShift));
}
}  // namespace

// decomp: BATTLE.PRG func_800C031C (src/BATTLE/BATTLE.PRG/573B8.c:363).
void InitializeSphereState(SphereLifecycleState& state) noexcept {
  state = {};
  state.selected_target = -1;
}

// decomp: BATTLE.PRG func_800C031C (src/BATTLE/BATTLE.PRG/573B8.c:363).
bool BuildSphereWireEdges(std::span<std::uint16_t> edges) noexcept {
  if (edges.size() < kSphereWireEdgeCount) return false;

  std::size_t index = 0;
  for (int y = 0;; ++y) {
    for (int x = 0; x < kTerrainGridMaximum; ++x) {
      edges[index++] = EncodeSphereEdge(x, y, x + 1, y);
    }
    if (y == kTerrainGridMaximum) break;
    for (int x = 0; x < kTerrainGridDimension; ++x) {
      edges[index++] = EncodeSphereEdge(x, y, x, y + 1);
    }
  }

  for (int x = 0;; ++x) {
    for (int y = kTerrainGridMaximum; y > 0; --y) {
      edges[index++] = EncodeSphereEdge(x, y, x, y - 1);
    }
    if (x == kTerrainGridMaximum) break;
    for (int y = kTerrainGridMaximum; y >= 0; --y) {
      edges[index++] = EncodeSphereEdge(x, y, x + 1, y);
    }
  }

  for (int y = kTerrainGridMaximum;; --y) {
    for (int x = kTerrainGridMaximum; x > 0; --x) {
      edges[index++] = EncodeSphereEdge(x, y, x - 1, y);
    }
    if (y == 0) break;
    for (int x = kTerrainGridMaximum; x >= 0; --x) {
      edges[index++] = EncodeSphereEdge(x, y, x, y - 1);
    }
  }

  for (int x = kTerrainGridMaximum;; --x) {
    for (int y = 0; y < kTerrainGridMaximum; ++y) {
      edges[index++] = EncodeSphereEdge(x, y, x, y + 1);
    }
    if (x == 0) break;
    for (int y = 0; y < kTerrainGridDimension; ++y) {
      edges[index++] = EncodeSphereEdge(x, y, x - 1, y);
    }
  }

  return index == kSphereWireEdgeCount;
}

// decomp: BATTLE.PRG func_800C05EC (src/BATTLE/BATTLE.PRG/573B8.c:444).
std::optional<SphereVolumeSetResult> SetSphereVolumes(
    SphereLifecycleState& state, const SphereVolume& primary,
    const SphereVolume* secondary, std::uint8_t sphere_mode,
    bool& reuse_volumes) noexcept {
  if (reuse_volumes && secondary == nullptr) return std::nullopt;

  state.primary_volume = primary;
  if (primary.kind != 1) {
    state.wire_phase_a = 0;
    state.wire_phase_c = 0;
  }

  if (reuse_volumes) {
    state.primary_volume.kind |= kVolumeActiveMask;
    state.secondary_volume.kind = secondary->kind;
    reuse_volumes = false;
    return SphereVolumeSetResult::kReused;
  }

  state.reserved_38 = 0;
  state.reserved_3c_and_input_locked = 0;
  state.wire_shape = kSphereTransitionShape;
  if (secondary != nullptr) {
    state.secondary_volume = *secondary;
    state.secondary_volume.kind |= kVolumeActiveMask;
    state.sphere_mode = sphere_mode;
    return SphereVolumeSetResult::kSecondarySet;
  }

  state.secondary_volume.kind = 0;
  return SphereVolumeSetResult::kSecondaryDisabled;
}

// decomp: BATTLE.PRG func_800C06E0 (src/BATTLE/BATTLE.PRG/573B8.c:475).
void ClearPrimarySphereVolume(SphereLifecycleState& state) noexcept {
  state.primary_volume.kind = 0;
}

// decomp: BATTLE.PRG func_800C0700 (src/BATTLE/BATTLE.PRG/573B8.c:482).
void SetSecondarySphereVolume(SphereLifecycleState& state,
                              const SphereVolume& secondary) noexcept {
  state.secondary_volume = secondary;
  state.wire_shape = 0;
}

// decomp: BATTLE.PRG func_800C0738 (src/BATTLE/BATTLE.PRG/573B8.c:488).
void ClearSecondarySphereVolume(SphereLifecycleState& state) noexcept {
  state.secondary_volume.kind = 0;
}

// decomp: BATTLE.PRG func_800C16DC (src/BATTLE/BATTLE.PRG/58578.c:470).
void ClearSphereTargetCount(SphereLifecycleState& state) noexcept {
  state.target_count = 0;
}

// decomp: BATTLE.PRG func_800C4734 (src/BATTLE/BATTLE.PRG/5BE50.c:93).
int SphereInteractionMode(const SphereLifecycleState* state,
                          bool override_button) noexcept {
  if (state == nullptr || state->wire_shape != 0) return 0;
  const auto input_locked =
      static_cast<std::uint16_t>(state->reserved_3c_and_input_locked >> 16);
  return input_locked == 0 || override_button ? 1 : 2;
}
}  // namespace VS::Battle
