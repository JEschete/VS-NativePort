#include "Battle/SphereLifecycleLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <source_location>
#include <span>

#include "Battle/Sphere.hpp"
#include "Battle/SphereLifecycle.hpp"
#include "Battle/TargetPages.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/CopyLive.hpp"
#include "Input/ControllerLiveState.hpp"

using VS::Battle::TargetVolume;

namespace VS::Core::Memory {
void* AllocateLiveFromLowEnd(std::size_t bytes, std::source_location site);
void FreeLiveFromLowEnd(const void* payload, std::source_location site);
}  // namespace VS::Core::Memory

namespace VS::Battle {
namespace {
constexpr std::size_t kPositionXIndex = 0;
constexpr std::size_t kPositionYIndex = 1;
constexpr std::size_t kPositionZIndex = 2;
constexpr std::size_t kPositionPadIndex = 3;
constexpr std::uint32_t kSphereOverrideButtonMask = 0x80;

static_assert(sizeof(VS::Battle::TargetSphereSpec) == sizeof(TargetVolume));
static_assert(sizeof(VS::Battle::TargetSphereRecord) <=
              sizeof(VS::Battle::TargetSphereSetupView));
static_assert(std::size(VS::Battle::TargetSphereSetupView{}.wire_edges) ==
              kSphereWireEdgeCapacity);
static_assert(offsetof(VS::Battle::TargetSphereRecord, input_locked) ==
              offsetof(VS::Battle::TargetSphereSetupView,
                       cursor_z_and_input_locked) +
                  sizeof(std::int16_t));

[[noreturn]] void Refuse(const char* operation, const char* reason) {
  VS::Core::LogError("battle.sphere", "{} refused: {}", operation, reason);
  std::abort();
}

VS::Battle::TargetSphereSetupView& RequiredSphere(
    VS::Battle::TargetSphereSetupView* sphere, const char* operation) {
  if (sphere == nullptr) Refuse(operation, "missing sphere workspace");
  return *sphere;
}

const VS::Battle::TargetSphereSpec& RequiredVolume(
    const VS::Battle::TargetSphereSpec* volume, const char* operation) {
  if (volume == nullptr) Refuse(operation, "missing required volume");
  return *volume;
}

SphereVolume ReadVolume(const VS::Battle::TargetSphereSpec& spec) {
  const TargetVolume volume = std::bit_cast<TargetVolume>(spec);
  TargetVolumeExtents extents{};
  for (std::size_t index = 0; index < extents.size(); ++index) {
    extents[index] = volume.scale.extents[index];
  }
  return {.kind = volume.kind,
          .range_kind = volume.opaque1,
          .angle = volume.angle,
          .extents = extents,
          .position = {volume.position[kPositionXIndex],
                       volume.position[kPositionYIndex],
                       volume.position[kPositionZIndex],
                       volume.position[kPositionPadIndex]}};
}

VS::Battle::TargetSphereSpec WriteVolume(const SphereVolume& value) {
  TargetVolume volume{};
  volume.kind = static_cast<std::uint8_t>(value.kind);
  volume.opaque1 = static_cast<std::uint8_t>(value.range_kind);
  volume.angle = static_cast<std::int16_t>(value.angle);
  for (std::size_t index = 0; index < value.extents.size(); ++index) {
    volume.scale.extents[index] = value.extents[index];
  }
  volume.position[kPositionXIndex] = value.position.x;
  volume.position[kPositionYIndex] = value.position.y;
  volume.position[kPositionZIndex] = value.position.z;
  volume.position[kPositionPadIndex] = value.position.pad;
  return std::bit_cast<VS::Battle::TargetSphereSpec>(volume);
}

SphereLifecycleState ReadState(
    const VS::Battle::TargetSphereSetupView& sphere) {
  return {.primary_volume = ReadVolume(sphere.primary_volume),
          .secondary_volume = ReadVolume(sphere.secondary_volume),
          .wire_phase_a = sphere.wire_phase_a,
          .wire_phase_b = sphere.wire_phase_b,
          .wire_phase_c = sphere.wire_phase_c,
          .wire_shape = sphere.wire_shape,
          .target_count = std::bit_cast<std::int8_t>(
              static_cast<std::uint8_t>(sphere.target_count)),
          .selected_target = std::bit_cast<std::int8_t>(
              static_cast<std::uint8_t>(sphere.selected_target)),
          .sphere_mode = sphere.sphere_mode,
          .reserved_38 = std::bit_cast<std::uint32_t>(sphere.cursor_xy),
          .reserved_3c_and_input_locked =
              std::bit_cast<std::uint32_t>(sphere.cursor_z_and_input_locked)};
}

void WriteState(VS::Battle::TargetSphereSetupView& sphere,
                const SphereLifecycleState& state) {
  sphere.primary_volume = WriteVolume(state.primary_volume);
  sphere.secondary_volume = WriteVolume(state.secondary_volume);
  sphere.wire_phase_a = state.wire_phase_a;
  sphere.wire_phase_b = state.wire_phase_b;
  sphere.wire_phase_c = state.wire_phase_c;
  sphere.wire_shape = state.wire_shape;
  sphere.target_count = std::bit_cast<char>(state.target_count);
  sphere.selected_target = std::bit_cast<char>(state.selected_target);
  sphere.sphere_mode = state.sphere_mode;
  sphere.cursor_xy = std::bit_cast<int>(state.reserved_38);
  sphere.cursor_z_and_input_locked =
      std::bit_cast<int>(state.reserved_3c_and_input_locked);
}
}  // namespace

// decomp: BATTLE.PRG func_800C031C (src/BATTLE/BATTLE.PRG/573B8.c:363).
void InitializeLiveSphere(VS::Battle::TargetSphereSetupView* sphere) {
  auto& legacy = RequiredSphere(sphere, "sphere initialization");
  SphereLifecycleState state = ReadState(legacy);
  InitializeSphereState(state);
  WriteState(legacy, state);

  std::array<std::uint16_t, kSphereWireEdgeCapacity> edges{};
  if (!BuildSphereWireEdges(edges)) {
    Refuse("sphere initialization", "wire edge capacity is too small");
  }
  for (std::size_t index = 0; index < kSphereWireEdgeCount; ++index) {
    legacy.wire_edges[index] = std::bit_cast<std::int16_t>(edges[index]);
  }
}

// decomp: BATTLE.PRG func_800C05B4 (src/BATTLE/BATTLE.PRG/573B8.c:436).
void FreeLiveSphere(VS::Battle::TargetSphereSetupView*& sphere,
                    SphereReleaseFunction release) {
  if (sphere == nullptr) return;
  if (release == nullptr) Refuse("sphere release", "missing release callback");
  release(sphere);
  sphere = nullptr;
}

// decomp: BATTLE.PRG func_800C05EC (src/BATTLE/BATTLE.PRG/573B8.c:444).
void SetLiveSphereVolumes(VS::Battle::TargetSphereSetupView* sphere,
                          const VS::Battle::TargetSphereSpec* primary,
                          const VS::Battle::TargetSphereSpec* secondary,
                          std::uint8_t sphere_mode,
                          VS::Battle::TargetPreview* target_previews,
                          std::uint8_t& reuse_volumes) {
  auto& legacy = RequiredSphere(sphere, "sphere volume update");
  const auto& required_primary =
      RequiredVolume(primary, "sphere volume update");
  const bool reuse = reuse_volumes != 0;
  if (reuse && secondary == nullptr) {
    Refuse("sphere volume reuse", "missing secondary volume");
  }

  SphereLifecycleState state = ReadState(legacy);
  const SphereVolume primary_value = ReadVolume(required_primary);
  const SphereVolume secondary_value =
      secondary == nullptr ? SphereVolume{} : ReadVolume(*secondary);
  auto reuse_state = reuse;
  const auto result = SetSphereVolumes(
      state, primary_value, secondary == nullptr ? nullptr : &secondary_value,
      sphere_mode, reuse_state);
  if (!result.has_value()) {
    Refuse("sphere volume update", "invalid lifecycle state");
  }
  WriteState(legacy, state);
  reuse_volumes = reuse_state ? 1 : 0;
  if (*result == SphereVolumeSetResult::kSecondarySet) {
    legacy.target_list1 = target_previews;
  }
}

// decomp: BATTLE.PRG func_800C06E0 (src/BATTLE/BATTLE.PRG/573B8.c:475).
void ClearLivePrimarySphereVolume(VS::Battle::TargetSphereSetupView* sphere) {
  if (sphere == nullptr) return;
  auto state = ReadState(*sphere);
  ClearPrimarySphereVolume(state);
  WriteState(*sphere, state);
}

// decomp: BATTLE.PRG func_800C0700 (src/BATTLE/BATTLE.PRG/573B8.c:482).
void SetLiveSecondarySphereVolume(
    VS::Battle::TargetSphereSetupView* sphere,
    const VS::Battle::TargetSphereSpec* secondary) {
  auto& legacy = RequiredSphere(sphere, "secondary sphere volume update");
  const auto& required_secondary =
      RequiredVolume(secondary, "secondary sphere volume update");
  auto state = ReadState(legacy);
  SetSecondarySphereVolume(state, ReadVolume(required_secondary));
  WriteState(legacy, state);
}

// decomp: BATTLE.PRG func_800C0738 (src/BATTLE/BATTLE.PRG/573B8.c:488).
void ClearLiveSecondarySphereVolume(VS::Battle::TargetSphereSetupView* sphere) {
  if (sphere == nullptr) return;
  auto state = ReadState(*sphere);
  ClearSecondarySphereVolume(state);
  WriteState(*sphere, state);
}

// decomp: BATTLE.PRG func_800C16DC (src/BATTLE/BATTLE.PRG/58578.c:470).
void ClearLiveSphereTargets(VS::Battle::TargetSphereSetupView* sphere) {
  if (sphere == nullptr) return;
  auto state = ReadState(*sphere);
  ClearSphereTargetCount(state);
  WriteState(*sphere, state);
}

// decomp: BATTLE.PRG func_800C4734 (src/BATTLE/BATTLE.PRG/5BE50.c:93).
int LiveSphereInteractionMode(const VS::Battle::TargetSphereSetupView* sphere,
                              bool override_button) {
  if (sphere == nullptr) return 0;
  const auto state = ReadState(*sphere);
  return SphereInteractionMode(&state, override_button);
}

// decomp: BATTLE.PRG func_800C031C (src/BATTLE/BATTLE.PRG/573B8.c:363).
void InitializeSphereEntry() {
  ResetTargetPages();
  if (VS::Battle::targetSphere == nullptr) {
    auto* allocated = static_cast<VS::Battle::TargetSphereSetupView*>(
        VS::Core::Memory::AllocateLiveFromLowEnd(
            sizeof(*VS::Battle::targetSphere),
            std::source_location::current()));
    if (allocated == nullptr) {
      Refuse("sphere initialization", "workspace allocation failed");
    }
    VS::Battle::targetSphere = allocated;
  }
  VS::Core::Memory::ZeroLiveWordsBackward(
      VS::Battle::targetSphere,
      static_cast<std::int32_t>(sizeof(*VS::Battle::targetSphere)));
  InitializeLiveSphere(VS::Battle::targetSphere);
}

// decomp: BATTLE.PRG func_800C05EC (src/BATTLE/BATTLE.PRG/573B8.c:444).
void SetSphereVolumesEntry(VS::Battle::TargetSphereSpec* primary,
                           VS::Battle::TargetSphereSpec* secondary,
                           std::uint8_t sphere_mode,
                           VS::Battle::TargetPreview* target_previews) {
  SetLiveSphereVolumes(VS::Battle::targetSphere, primary, secondary,
                       sphere_mode, target_previews,
                       VS::Battle::reuseSphereVolumes);
}

// decomp: BATTLE.PRG func_800C06E0 (src/BATTLE/BATTLE.PRG/573B8.c:475).
void ClearPrimarySphereVolumeEntry() {
  ClearLivePrimarySphereVolume(VS::Battle::targetSphere);
}

// decomp: BATTLE.PRG func_800C0700 (src/BATTLE/BATTLE.PRG/573B8.c:482).
void SetSecondarySphereVolumeEntry(VS::Battle::TargetSphereSpec* secondary) {
  SetLiveSecondarySphereVolume(VS::Battle::targetSphere, secondary);
}

// decomp: BATTLE.PRG func_800C0738 (src/BATTLE/BATTLE.PRG/573B8.c:488).
void ClearSecondarySphereVolumeEntry() {
  ClearLiveSecondarySphereVolume(VS::Battle::targetSphere);
}

// decomp: BATTLE.PRG func_800C16DC (src/BATTLE/BATTLE.PRG/58578.c:470).
void ClearSphereTargetsEntry() {
  ResetTargetPages();
  ClearLiveSphereTargets(VS::Battle::targetSphere);
}

// decomp: BATTLE.PRG func_800C4734 (src/BATTLE/BATTLE.PRG/5BE50.c:93).
int SphereInteractionModeEntry() {
  return LiveSphereInteractionMode(
      VS::Battle::targetSphere,
      (VS::Input::buttonsState & kSphereOverrideButtonMask) != 0);
}
}  // namespace VS::Battle
