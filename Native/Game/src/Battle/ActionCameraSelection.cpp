// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionCameraSelection.hpp"

#include <algorithm>
#include <array>

namespace VS::Battle {
namespace {
using Core::Math::SquareRootInteger;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
constexpr int kSelectionDelay = 31;
constexpr int kCancelledPreview = 255;
constexpr int kProjectionLimit = 768;
constexpr int kNearDistance = 1536;
constexpr int kFarDistance = 2304;
constexpr int kDistanceStep = 192;

std::optional<std::array<ActionPoint, 2>> Endpoints(bool use_focus,
                                                    ActionCameraWorld& world) {
  auto* queue = world.Queued();
  if (queue == nullptr) return std::nullopt;
  ActionPoint source{}, target{};
  if (!world.BonePosition(queue->PrimaryActor(), 0, source)) {
    return std::nullopt;
  }
  if (queue->Count() != 0) {
    const auto hit = queue->FirstTarget();
    if (hit.position_kind != 0) {
      target = hit.Position();
    } else if (!ActionTargetLimbPosition(hit.actor,
                                         static_cast<std::int8_t>(hit.limb),
                                         target, world)) {
      return std::nullopt;
    }
  } else if (use_focus) {
    target = queue->Focus();
  } else if (!world.BonePosition(queue->PrimaryActor(), 0, target)) {
    return std::nullopt;
  }
  return std::array{source, target};
}
bool EnemyReady(ActionSelectionState& state) {
  const auto frames = static_cast<std::uint8_t>(state.Frames() + 1);
  state.SetFrames(frames);
  return frames >= kSelectionDelay;
}
bool CameraActive(std::int32_t mode) {
  return mode == 1 || mode == kAlternateActionCameraMode;
}
bool TrackAndPresent(ActionCameraWorld& world) {
  ActionPoint midpoint{};
  if (!ActionCameraMidpoint(midpoint, world)) return false;
  const auto displacement = world.TrackingDisplacement(midpoint);
  if (displacement.x == 0 && displacement.y == 0 && displacement.z == 0 &&
      world.YawDelta() == 0 && CameraActive(world.CameraMode())) {
    world.StartPresentation();
  }
  return true;
}
}  // namespace

// decomp: func_80077F14 (src/BATTLE/BATTLE.PRG/146C.c:6821)
bool ActionTargetLimbPosition(int actor, int limb, ActionPoint& output,
                              ActionCameraWorld& world) {
  const int bone = limb == -1 ? 0 : world.BoneTag(actor, limb);
  return world.BonePosition(actor, bone, output);
}

// decomp: func_80077F70 (src/BATTLE/BATTLE.PRG/146C.c:6833)
std::optional<std::int32_t> ActionTargetDistance(ActionCameraWorld& world) {
  const auto points = Endpoints(false, world);
  if (!points) return std::nullopt;
  const auto [source, target] = points.value();
  const auto dx = static_cast<std::int32_t>(target.x) - source.x;
  const auto dz = static_cast<std::int32_t>(target.z) - source.z;
  const auto horizontal_squared =
      WrappingAdd(WrappingMultiply(dx, dx), WrappingMultiply(dz, dz));
  if (horizontal_squared < 0) return std::nullopt;
  const auto horizontal = SquareRootInteger(horizontal_squared);
  const auto dy = static_cast<std::int32_t>(target.y) - source.y;
  const auto squared = WrappingAdd(WrappingMultiply(dy, dy),
                                   WrappingMultiply(horizontal, horizontal));
  if (squared < 0) return std::nullopt;
  return SquareRootInteger(squared);
}

// decomp: func_800780A8 (src/BATTLE/BATTLE.PRG/146C.c:6863)
bool ActionCameraMidpoint(ActionPoint& output, ActionCameraWorld& world) {
  const auto points = Endpoints(true, world);
  if (!points) return false;
  const auto [source, target] = points.value();
  const auto halfway = [](std::int16_t from, std::int16_t to) {
    return static_cast<std::int16_t>(from + (to - from) / 2);
  };
  output.x = halfway(source.x, target.x);
  output.z = halfway(source.z, target.z);
  output.y = halfway(source.y, target.y);
  return true;
}

// decomp: func_8007820C (src/BATTLE/BATTLE.PRG/146C.c:6886)
bool StepCameraTowardActionMidpoint(int execute, ActionCameraWorld& world) {
  ActionPoint midpoint{};
  if (!ActionCameraMidpoint(midpoint, world)) return false;
  if (execute != 0) world.StepCamera(midpoint);
  return true;
}

// decomp: func_80078248 (src/BATTLE/BATTLE.PRG/146C.c:6896)
bool UpdateInitialTargetSelection(ActionSelectionWorld& world) {
  auto* initial = world.Current();
  if (initial == nullptr) return false;
  if (initial->Selecting()) {
    if (initial->SourceActor() != 0) {
      if (!EnemyReady(*initial)) return true;
    } else {
      if (initial->UpdateMenu() == 0) return true;
      auto* current = world.Current();
      if (current == nullptr) return false;
      current->SetSelected(current->Preview());
    }
  }
  world.Commit(1);
  return true;
}

// decomp: func_800782E4 (src/BATTLE/BATTLE.PRG/146C.c:6912)
bool UpdateActionAreaSelection(ActionSelectionWorld& world) {
  auto* initial = world.Current();
  if (initial == nullptr) return false;
  if (initial->Selecting()) {
    if (initial->SourceActor() != 0) {
      if (!EnemyReady(*initial)) return true;
    } else if (initial->UpdateArea() == 0) {
      return true;
    }
  }
  world.FilterArea();
  return true;
}

// decomp: func_80078364 (src/BATTLE/BATTLE.PRG/146C.c:6927)
bool UpdateFinalTargetSelection(ActionSelectionWorld& world) {
  auto* initial = world.Current();
  if (initial == nullptr) return false;
  if (initial->Selecting() && initial->SourceActor() == 0) {
    if (initial->UpdateMenu() == 0) return true;
    auto* current = world.Current();
    if (current == nullptr) return false;
    current->SetSelected(current->Preview());
    current = world.Current();
    if (current == nullptr) return false;
    if (current->Selected() == kCancelledPreview) {
      const auto action = current->ActionId();
      const auto targeting = world.NeedsTargeting(action);
      if (!targeting) return false;
      if (targeting.value()) {
        world.InitializeTargeting(action);
      } else {
        world.ClearSphereTargets();
        world.ClearPrimaryVolume();
        world.ClearSecondaryVolume();
        world.FreeSphere();
        world.ReturnToMenu();
      }
      return true;
    }
  } else {
    if (initial->Selecting() && !EnemyReady(*initial)) return true;
    auto* current = initial->Selecting() ? world.Current() : initial;
    if (current == nullptr) return false;
    current->SetSelected(0);
  }
  world.Commit(2);
  return true;
}

// decomp: _handleCombatCameraZoom (src/BATTLE/BATTLE.PRG/146C.c:6958)
bool UpdateActionCameraZoom(ActionCameraWorld& world) {
  if (!CameraActive(world.CameraMode())) return true;
  const auto zoom = world.Zoom();
  if (!zoom) return false;
  switch (zoom.value()) {
    case 0:
      if (!StepCameraTowardActionMidpoint(1, world)) return false;
      return TrackAndPresent(world);
    case 1: {
      if (!StepCameraTowardActionMidpoint(1, world)) return false;
      constexpr int kProjectionStep = 64;
      world.SetProjection(std::min(
          WrappingAdd(world.Projection(), kProjectionStep), kProjectionLimit));
      world.ApplyProjection(world.Projection());
      const auto distance = world.Distance();
      if (distance > kNearDistance) {
        world.SetDistance(std::max(distance - kDistanceStep, kNearDistance));
      }
      return world.Projection() != kProjectionLimit || TrackAndPresent(world);
    }
    case 2: {
      if (!StepCameraTowardActionMidpoint(1, world)) return false;
      const auto distance = world.Distance();
      if (distance < kFarDistance) {
        world.SetDistance(std::min(distance + kDistanceStep, kFarDistance));
      }
      return world.Distance() != kFarDistance || TrackAndPresent(world);
    }
    default:
      if (world.YawDelta() == 0) world.StartPresentation();
      return true;
  }
}
}  // namespace VS::Battle
