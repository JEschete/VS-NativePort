// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionSequence.hpp"

#include <algorithm>
#include <bit>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
constexpr std::uint32_t kQueueMask = 3;
constexpr std::int32_t kRestoredProjection = 512;

struct Readiness {
  bool ready = true;
  SequenceAction* last = nullptr;
};
Readiness CheckReadiness(ActionSequenceWorld& world) {
  Readiness result;
  for (std::uint32_t step = 0; step < world.ChainCount(); ++step) {
    auto& action = world.Step(step & kQueueMask);
    result.last = &action;
    for (std::size_t target = 0; target < action.Count(); ++target) {
      const auto hit = action.Target(target);
      if (!hit.targets_position &&
          static_cast<std::uint32_t>(world.AnimationBusy(hit.target_actor)) +
                  1U >=
              2U) {
        result.ready = false;
      }
    }
  }
  return result;
}
void RestoreCamera(ActionSequenceWorld& world) {
  using Core::Math::WrappingAdd;
  constexpr std::int32_t kProjectionStep = 64;
  constexpr std::int32_t kDistanceStep = 192;
  const auto projection = world.Projection();
  if (projection > kRestoredProjection) {
    world.ApplyProjection(std::max(WrappingAdd(projection, -kProjectionStep),
                                   kRestoredProjection));
  } else if (projection < kRestoredProjection) {
    world.ApplyProjection(std::min(WrappingAdd(projection, kProjectionStep),
                                   kRestoredProjection));
  }
  const auto distance = world.Distance();
  const std::int32_t preferred = world.PreferredDistance();
  if (distance < preferred) {
    world.SetDistance(
        std::min(WrappingAdd(distance, kDistanceStep), preferred));
  } else if (distance > preferred) {
    world.SetDistance(
        std::max(WrappingAdd(distance, -kDistanceStep), preferred));
  }
}
void ResolveTargets(ActionSequenceWorld& world) {
  for (std::uint32_t step = 0; step < world.ChainCount(); ++step) {
    auto& action = world.Step(step & kQueueMask);
    for (std::size_t target = 0; target < action.Count(); ++target) {
      const auto hit = action.Target(target);
      if (!hit.targets_position) {
        world.ResolveDefeat(action.Primary().target_actor, hit.target_actor);
      }
    }
  }
}
void ResolveSources(ActionSequenceWorld& world) {
  for (std::uint32_t step = 0; step < world.ChainCount(); ++step) {
    auto& action = world.Step(step & kQueueMask);
    const auto primary = action.Primary();
    if (!primary.targets_position) {
      world.ResolveDefeat(primary.target_actor, primary.target_actor);
    }
  }
}
}  // namespace

// decomp: func_80078748 (src/BATTLE/BATTLE.PRG/146C.c:7025).
void WaitForActionEffectReady(ActionSequenceWorld& world) {
  auto& action = world.Queued();
  constexpr std::uint32_t kEffectReady = 0x08000100;
  if (world.NotificationPending() == 0 &&
      (world.ActorFlags(action.Primary().target_actor) & kEffectReady) ==
          kEffectReady &&
      world.FreeAssetRequest() == 0 && world.CanStartEffect() != 0) {
    world.AdvanceEffectPhase();
  }
}

// decomp: func_800787F0 (src/BATTLE/BATTLE.PRG/146C.c:7035).
void StartReadyActionAnimation(ActionSequenceWorld& world) {
  constexpr std::uint32_t kAnimationBlocked = 2;
  if ((world.EffectBusyFlags() & kAnimationBlocked) == 0) {
    world.StartAnimation();
    world.StartEffectTrack();
  }
}

// decomp: func_80079050 (src/BATTLE/BATTLE.PRG/146C.c:7356).
void WaitForActionResolution(ActionSequenceWorld& world) {
  if (world.Queued().RebuildPlans() == 0) {
    world.ReleaseAttacks();
    world.FadeTrails();
  }
}

// decomp: func_800790BC (src/BATTLE/BATTLE.PRG/146C.c:7364).
TargetRecoveryStatus WaitForTargetRecovery(ActionSequenceWorld& world) {
  auto readiness = CheckReadiness(world);
  if (!readiness.ready) return TargetRecoveryStatus::kWaiting;
  if (readiness.last == nullptr) return TargetRecoveryStatus::kInvalidSequence;
  int mode = 1;
  for (std::uint32_t step = 0; step < world.ChainCount(); ++step) {
    auto& action = world.Step(step & kQueueMask);
    readiness.last = &action;
    const auto primary = action.Primary();
    if (!primary.targets_position) {
      world.BeginDefeat(primary.target_actor);
    }
    for (std::size_t target = 0; target < action.Count(); ++target) {
      const auto hit = action.Target(target);
      if (!hit.targets_position && world.BeginDefeat(hit.target_actor) != 0) {
        mode = 2;
      }
    }
  }
  if (readiness.last->Primary().target_actor != 0) mode = 0;
  world.BeginRecovery(mode);
  return TargetRecoveryStatus::kStarted;
}

// decomp: func_800793C0 (src/BATTLE/BATTLE.PRG/146C.c:7435).
void FinishActionSequence(ActionSequenceWorld& world) {
  const auto camera_mode = world.CameraMode();
  constexpr int kAlternateCamera = 3;
  if (camera_mode == 1 || camera_mode == kAlternateCamera) {
    RestoreCamera(world);
  }
  if (world.Projection() != kRestoredProjection ||
      world.Distance() != world.PreferredDistance() ||
      world.LookAtOnAshley() == 0) {
    return;
  }
  if (!CheckReadiness(world).ready) return;
  world.ResumeField();
  world.ShowHud();
  world.RestoreIdle(world.SelectedActor());
  ResolveTargets(world);
  ResolveSources(world);
  constexpr std::uint32_t kCutsceneControl = 3;
  if (world.ControlMode() == kCutsceneControl) return;
  const auto event = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(world.EvaluateRoomEvents()));
  if (event == 1) {
    world.BeginCutscene();
    return;
  }
  world.OpenLoot();
}
}  // namespace VS::Battle
