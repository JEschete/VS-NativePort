// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/FrameUpdate.hpp"  // NOLINT(build/include): native C++ interface.

#include <climits>

namespace VS::Battle::Ai {
namespace {
constexpr std::uint16_t kModeOneFlags = 0x30;
constexpr std::uint16_t kModeTwoFlag = 8;
constexpr std::uint32_t kSlotMask = kFrameActorSlots - 1;
constexpr std::uint32_t kActorMask = (1u << kFrameActorSlots) - 1;
}  // namespace

// decomp: BATTLE.PRG func_800E4358 (src/BATTLE/BATTLE.PRG/7B5DC.s:433).
void TickDecisionTimers(DecisionTimers& timers) {
  if (timers.decision_delay != 0) --timers.decision_delay;
  if (timers.min_action_delay != 0) --timers.min_action_delay;
}

// decomp: BATTLE.PRG func_800E6828 (src/BATTLE/BATTLE.PRG/7E028.s:8).
LocomotionSelection SelectLocomotion(std::uint16_t locomotion_flags) {
  LocomotionSelection selection;
  selection.movement_flags = static_cast<std::uint16_t>(
      locomotion_flags | (locomotion_flags >> CHAR_BIT));
  if ((selection.movement_flags & kModeOneFlags) != 0) {
    selection.mode = 1;
  } else if ((selection.movement_flags & kModeTwoFlag) != 0) {
    selection.mode = 2;
  }
  return selection;
}

// decomp: BATTLE.PRG func_800E685C (src/BATTLE/BATTLE.PRG/7E05C.s:16).
VerticalMovementSteps MakeVerticalMovementSteps(int walk_step, int run_step) {
  const auto packed = static_cast<std::uint32_t>(walk_step) |
                      (static_cast<std::uint32_t>(run_step) << CHAR_BIT);
  VerticalMovementSteps steps;
  steps.walk_step = static_cast<std::uint8_t>(packed);
  steps.run_step = static_cast<std::uint8_t>(packed >> CHAR_BIT);
  steps.max_range =
      static_cast<std::uint8_t>(walk_step > run_step ? walk_step : run_step);
  return steps;
}

// decomp: BATTLE.PRG func_800E511C (src/BATTLE/BATTLE.PRG/7C7E0.s:108).
std::uint32_t NextPerceptionTarget(std::uint32_t attribute_mask,
                                   std::uint32_t previous) {
  const std::uint32_t mask = attribute_mask & kActorMask;
  if (mask == 0) return 0;
  std::uint32_t target = previous & kSlotMask;
  for (std::uint32_t step = 0; step < kFrameActorSlots; ++step) {
    target = (target + 1) & kSlotMask;
    if ((mask & (1u << target)) != 0) break;
  }
  return target;
}

// decomp: BATTLE.PRG func_800E65DC (src/BATTLE/BATTLE.PRG/7DDDC.s:8).
std::optional<TerrainRegionWindow> LocateTerrainRegions(
    std::size_t auxiliary_bytes) {
  if (auxiliary_bytes < kTerrainRegionOffset) return std::nullopt;
  return TerrainRegionWindow{kTerrainRegionOffset,
                             auxiliary_bytes - kTerrainRegionOffset};
}

std::uint32_t PackRoomBounds(
    const std::array<std::uint8_t, kRoomExtentBytes>& extent) {
  std::uint32_t bounds = 0;
  for (std::size_t i = extent.size(); i-- > 0;) {
    bounds = (bounds << CHAR_BIT) | extent[i];
  }
  return bounds;
}

// decomp: BATTLE.PRG func_800E511C; stages at 0x800F16BC (7C7E0.s:108).
FrameUpdateResult RunActorAiUpdateStages(ActorAiFrame& frame) {
  if (!frame.HasTileFlags()) return FrameUpdateResult::kNoRoomMap;
  if (!frame.HasRoomAndActors()) {
    return FrameUpdateResult::kMissingRoomOrActors;
  }
  frame.CacheWorkingState();
  frame.SetPerceptionTarget(NextPerceptionTarget(frame.AttributeActorMask(),
                                                 frame.PerceptionTarget()));
  frame.UpdatePerception();
  frame.UpdateDecisions();
  frame.AnimateEngagement();
  frame.SaveRandomState();
  return FrameUpdateResult::kUpdated;
}
}  // namespace VS::Battle::Ai
