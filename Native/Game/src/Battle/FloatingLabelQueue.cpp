// Hand-maintained (tools/port/owned.csv).
#include "Battle/FloatingLabelQueue.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace VS::Battle {
namespace {
std::int16_t Decrement(std::int16_t value) {
  return std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(static_cast<std::uint16_t>(value) - 1U));
}

bool Commit(FloatingLabelQueueView& queue, std::int16_t count,
            FloatingLabelSlot slot) {
  if (count < 0 || count > kFloatingLabelCapacity || slot.index < 0 ||
      slot.index >= kFloatingLabelCapacity || slot.index > count) {
    return false;
  }
  const auto end = static_cast<std::int16_t>(slot.index + 1);
  queue.SetCount(std::max(count, end));
  return true;
}
}  // namespace

// decomp: F00712 func_80093AB4 (src/BATTLE/BATTLE.PRG/2842C.c:1465).
bool ResetFloatingLabels(FloatingLabelQueueView& queue,
                         FloatingLabelResetView state) {
  state.marker_visibility_command = 0;
  state.ability_cue_frames = 0;
  queue.SetCount(0);
  state.status_label_step_timer = 0;
  state.ashley_status_label_suppression = 0;
  return queue.ClearAllLabels();
}

// decomp: F00713 func_80093B04 (src/BATTLE/BATTLE.PRG/2842C.c:1475).
bool QueueWorldLockedLabel(FloatingLabelQueueView& queue,
                           std::span<const std::int16_t> position) {
  const auto slot = ReserveFloatingLabel(queue);
  if (!slot) return false;
  auto* const label = queue.Label(slot.value());
  if (label == nullptr) return false;
  label->Timing().elapsed_frames = 0;
  label->SetWorldLocked();
  for (std::size_t axis = 0; axis + 1 < kFloatingLabelAxes; ++axis) {
    if (axis >= position.size()) return false;
    label->SetWorldCoordinate(axis, position[axis]);
  }
  const std::int16_t count = queue.Count();
  constexpr std::size_t kLastAxis = kFloatingLabelAxes - 1;
  if (kLastAxis >= position.size()) return false;
  label->SetWorldCoordinate(kLastAxis, position[kLastAxis]);
  return Commit(queue, count, slot.value());
}

// decomp: F00721 func_80094AF8 (src/BATTLE/BATTLE.PRG/2842C.c:2067).
void AdvanceFloatingLabelQueue(FloatingLabelTimingView label) {
  const auto waiting_steps = label.waiting_steps;
  label.elapsed_frames = 0;
  label.waiting_steps = Decrement(waiting_steps);
}

// decomp: F00734 func_800962E4 (src/BATTLE/BATTLE.PRG/2842C.c:2595).
std::optional<FloatingLabelSlot> ReserveFloatingLabel(
    FloatingLabelQueueView& queue) {
  const std::int16_t count = queue.Count();
  if (count < kFloatingLabelCapacity) {
    const FloatingLabelSlot slot{count};
    if (queue.Label(slot) == nullptr) return std::nullopt;
    return slot;
  }
  for (std::int32_t index = 0; index + 1 < kFloatingLabelCapacity; ++index) {
    if (!queue.CopyLabel({index}, {index + 1})) return std::nullopt;
  }
  const FloatingLabelSlot last{kFloatingLabelCapacity - 1};
  if (!queue.ClearLabel(last) || queue.Label(last) == nullptr) {
    return std::nullopt;
  }
  return last;
}

// decomp: F00735 func_8009639C (src/BATTLE/BATTLE.PRG/2842C.c:2613).
bool CompactFloatingLabels(FloatingLabelQueueView& queue,
                           std::int32_t removal_count) {
  for (std::int32_t pass = 0; pass < removal_count; ++pass) {
    for (std::int32_t index = 0; index < queue.Count(); ++index) {
      auto* const label = queue.Label({index});
      if (label == nullptr) return false;
      if (label->Inactive()) {
        if (!RemoveFloatingLabel(queue, {index})) return false;
        break;
      }
    }
  }
  return true;
}

// decomp: F00736 func_80096444 (src/BATTLE/BATTLE.PRG/2842C.c:2628).
bool RemoveFloatingLabel(FloatingLabelQueueView& queue,
                         FloatingLabelSlot slot) {
  queue.SetCount(Decrement(queue.Count()));
  for (std::int32_t index = slot.index; index < queue.Count(); ++index) {
    if (!queue.CopyLabel({index}, {index + 1})) return false;
  }
  return queue.ClearLabel({queue.Count()});
}

bool CommitFloatingLabel(FloatingLabelQueueView& queue,
                         FloatingLabelSlot slot) {
  return Commit(queue, queue.Count(), slot);
}
}  // namespace VS::Battle
