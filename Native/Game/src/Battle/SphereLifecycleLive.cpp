#include "Battle/SphereLifecycleLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <source_location>
#include <span>

#include "Battle/Sphere.h"
#include "Battle/SphereLifecycle.hpp"
#include "Battle/TargetPages.h"
#include "Core/Log.hpp"
#include "Core/Memory/CopyLive.hpp"
#include "Input/ControllerLiveState.h"

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

static_assert(sizeof(Battle_TargetSphereSpec) == sizeof(Battle_TargetVolume));
static_assert(sizeof(Battle_TargetSphere) <=
              sizeof(Battle_TargetSphereSetupView));
static_assert(std::size(Battle_TargetSphereSetupView{}.wireEdges) ==
              kSphereWireEdgeCapacity);
static_assert(offsetof(Battle_TargetSphere, inputLocked) ==
              offsetof(Battle_TargetSphereSetupView, reserved3C) +
                  sizeof(std::int16_t));

[[noreturn]] void Refuse(const char* operation, const char* reason) {
  VS_LOG_ERROR("battle.sphere", "{} refused: {}", operation, reason);
  std::abort();
}

Battle_TargetSphereSetupView& RequiredSphere(
    Battle_TargetSphereSetupView* sphere, const char* operation) {
  if (sphere == nullptr) Refuse(operation, "missing sphere workspace");
  return *sphere;
}

const Battle_TargetSphereSpec& RequiredVolume(
    const Battle_TargetSphereSpec* volume, const char* operation) {
  if (volume == nullptr) Refuse(operation, "missing required volume");
  return *volume;
}

SphereVolume ReadVolume(const Battle_TargetSphereSpec& spec) {
  const Battle_TargetVolume volume = std::bit_cast<Battle_TargetVolume>(spec);
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

Battle_TargetSphereSpec WriteVolume(const SphereVolume& value) {
  Battle_TargetVolume volume{};
  volume.kind = static_cast<u_char>(value.kind);
  volume.opaque1 = static_cast<u_char>(value.range_kind);
  volume.angle = static_cast<std::int16_t>(value.angle);
  for (std::size_t index = 0; index < value.extents.size(); ++index) {
    volume.scale.extents[index] = value.extents[index];
  }
  volume.position[kPositionXIndex] = value.position.x;
  volume.position[kPositionYIndex] = value.position.y;
  volume.position[kPositionZIndex] = value.position.z;
  volume.position[kPositionPadIndex] = value.position.pad;
  return std::bit_cast<Battle_TargetSphereSpec>(volume);
}

SphereLifecycleState ReadState(const Battle_TargetSphereSetupView& sphere) {
  return {.primary_volume = ReadVolume(sphere.primaryVolume),
          .secondary_volume = ReadVolume(sphere.secondaryVolume),
          .wire_phase_a = sphere.wirePhaseA,
          .wire_phase_b = sphere.wirePhaseB,
          .wire_phase_c = sphere.wirePhaseC,
          .wire_shape = sphere.wireShape,
          .target_count = std::bit_cast<std::int8_t>(
              static_cast<std::uint8_t>(sphere.targetCount)),
          .selected_target = std::bit_cast<std::int8_t>(
              static_cast<std::uint8_t>(sphere.selectedTarget)),
          .sphere_mode = sphere.sphereMode,
          .reserved_38 = std::bit_cast<std::uint32_t>(sphere.reserved38),
          .reserved_3c_and_input_locked =
              std::bit_cast<std::uint32_t>(sphere.reserved3C)};
}

void WriteState(Battle_TargetSphereSetupView& sphere,
                const SphereLifecycleState& state) {
  sphere.primaryVolume = WriteVolume(state.primary_volume);
  sphere.secondaryVolume = WriteVolume(state.secondary_volume);
  sphere.wirePhaseA = state.wire_phase_a;
  sphere.wirePhaseB = state.wire_phase_b;
  sphere.wirePhaseC = state.wire_phase_c;
  sphere.wireShape = state.wire_shape;
  sphere.targetCount = std::bit_cast<char>(state.target_count);
  sphere.selectedTarget = std::bit_cast<char>(state.selected_target);
  sphere.sphereMode = state.sphere_mode;
  sphere.reserved38 = std::bit_cast<int>(state.reserved_38);
  sphere.reserved3C = std::bit_cast<int>(state.reserved_3c_and_input_locked);
}
}  // namespace

// decomp: BATTLE.PRG func_800C031C (src/BATTLE/BATTLE.PRG/573B8.c:363).
void InitializeLiveSphere(Battle_TargetSphereSetupView* sphere) {
  auto& legacy = RequiredSphere(sphere, "sphere initialization");
  SphereLifecycleState state = ReadState(legacy);
  InitializeSphereState(state);
  WriteState(legacy, state);

  std::array<std::uint16_t, kSphereWireEdgeCapacity> edges{};
  if (!BuildSphereWireEdges(edges)) {
    Refuse("sphere initialization", "wire edge capacity is too small");
  }
  for (std::size_t index = 0; index < kSphereWireEdgeCount; ++index) {
    legacy.wireEdges[index] = std::bit_cast<std::int16_t>(edges[index]);
  }
}

// decomp: BATTLE.PRG func_800C05B4 (src/BATTLE/BATTLE.PRG/573B8.c:436).
void FreeLiveSphere(Battle_TargetSphereSetupView*& sphere,
                    SphereReleaseFunction release) {
  if (sphere == nullptr) return;
  if (release == nullptr) Refuse("sphere release", "missing release callback");
  release(sphere);
  sphere = nullptr;
}

// decomp: BATTLE.PRG func_800C05EC (src/BATTLE/BATTLE.PRG/573B8.c:444).
void SetLiveSphereVolumes(Battle_TargetSphereSetupView* sphere,
                          const Battle_TargetSphereSpec* primary,
                          const Battle_TargetSphereSpec* secondary,
                          std::uint8_t sphere_mode,
                          Battle_TargetPreview* target_previews,
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
    legacy.targetList1 = target_previews;
  }
}

// decomp: BATTLE.PRG func_800C06E0 (src/BATTLE/BATTLE.PRG/573B8.c:475).
void ClearLivePrimarySphereVolume(Battle_TargetSphereSetupView* sphere) {
  if (sphere == nullptr) return;
  auto state = ReadState(*sphere);
  ClearPrimarySphereVolume(state);
  WriteState(*sphere, state);
}

// decomp: BATTLE.PRG func_800C0700 (src/BATTLE/BATTLE.PRG/573B8.c:482).
void SetLiveSecondarySphereVolume(Battle_TargetSphereSetupView* sphere,
                                  const Battle_TargetSphereSpec* secondary) {
  auto& legacy = RequiredSphere(sphere, "secondary sphere volume update");
  const auto& required_secondary =
      RequiredVolume(secondary, "secondary sphere volume update");
  auto state = ReadState(legacy);
  SetSecondarySphereVolume(state, ReadVolume(required_secondary));
  WriteState(legacy, state);
}

// decomp: BATTLE.PRG func_800C0738 (src/BATTLE/BATTLE.PRG/573B8.c:488).
void ClearLiveSecondarySphereVolume(Battle_TargetSphereSetupView* sphere) {
  if (sphere == nullptr) return;
  auto state = ReadState(*sphere);
  ClearSecondarySphereVolume(state);
  WriteState(*sphere, state);
}

// decomp: BATTLE.PRG func_800C16DC (src/BATTLE/BATTLE.PRG/58578.c:470).
void ClearLiveSphereTargets(Battle_TargetSphereSetupView* sphere) {
  if (sphere == nullptr) return;
  auto state = ReadState(*sphere);
  ClearSphereTargetCount(state);
  WriteState(*sphere, state);
}

// decomp: BATTLE.PRG func_800C4734 (src/BATTLE/BATTLE.PRG/5BE50.c:93).
int LiveSphereInteractionMode(const Battle_TargetSphereSetupView* sphere,
                              bool override_button) {
  if (sphere == nullptr) return 0;
  const auto state = ReadState(*sphere);
  return SphereInteractionMode(&state, override_button);
}

// decomp: BATTLE.PRG func_800C031C (src/BATTLE/BATTLE.PRG/573B8.c:363).
void InitializeSphereEntry() {
  Battle_ResetTargetPages();
  if (Battle_targetSphere == nullptr) {
    auto* allocated = static_cast<Battle_TargetSphereSetupView*>(
        VS::Core::Memory::AllocateLiveFromLowEnd(
            sizeof(*Battle_targetSphere), std::source_location::current()));
    if (allocated == nullptr) {
      Refuse("sphere initialization", "workspace allocation failed");
    }
    Battle_targetSphere = allocated;
  }
  VS::Core::Memory::ZeroLiveWordsBackward(
      Battle_targetSphere,
      static_cast<std::int32_t>(sizeof(*Battle_targetSphere)));
  InitializeLiveSphere(Battle_targetSphere);
}

// decomp: BATTLE.PRG func_800C05EC (src/BATTLE/BATTLE.PRG/573B8.c:444).
void SetSphereVolumesEntry(Battle_TargetSphereSpec* primary,
                           Battle_TargetSphereSpec* secondary,
                           u_char sphere_mode,
                           Battle_TargetPreview* target_previews) {
  SetLiveSphereVolumes(Battle_targetSphere, primary, secondary, sphere_mode,
                       target_previews, Battle_reuseSphereVolumes);
}

// decomp: BATTLE.PRG func_800C06E0 (src/BATTLE/BATTLE.PRG/573B8.c:475).
void ClearPrimarySphereVolumeEntry() {
  ClearLivePrimarySphereVolume(Battle_targetSphere);
}

// decomp: BATTLE.PRG func_800C0700 (src/BATTLE/BATTLE.PRG/573B8.c:482).
void SetSecondarySphereVolumeEntry(Battle_TargetSphereSpec* secondary) {
  SetLiveSecondarySphereVolume(Battle_targetSphere, secondary);
}

// decomp: BATTLE.PRG func_800C0738 (src/BATTLE/BATTLE.PRG/573B8.c:488).
void ClearSecondarySphereVolumeEntry() {
  ClearLiveSecondarySphereVolume(Battle_targetSphere);
}

// decomp: BATTLE.PRG func_800C16DC (src/BATTLE/BATTLE.PRG/58578.c:470).
void ClearSphereTargetsEntry() {
  Battle_ResetTargetPages();
  ClearLiveSphereTargets(Battle_targetSphere);
}

// decomp: BATTLE.PRG func_800C4734 (src/BATTLE/BATTLE.PRG/5BE50.c:93).
int SphereInteractionModeEntry() {
  return LiveSphereInteractionMode(
      Battle_targetSphere,
      (Input_buttonsState & kSphereOverrideButtonMask) != 0);
}
}  // namespace VS::Battle
