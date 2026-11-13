#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Battle {
inline constexpr std::int32_t kFloatingLabelCapacity = 32;
inline constexpr std::size_t kFloatingLabelAxes = 3;

struct FloatingLabelSlot {
  std::int32_t index = 0;
  bool operator==(const FloatingLabelSlot&) const = default;
};

struct FloatingLabelTimingView {
  std::int16_t& waiting_steps;
  std::uint8_t& elapsed_frames;
};

struct FloatingLabelResetView {
  std::int16_t& marker_visibility_command;
  std::int16_t& ability_cue_frames;
  std::int16_t& status_label_step_timer;
  std::int16_t& ashley_status_label_suppression;
};

class FloatingLabelView {
 public:
  virtual ~FloatingLabelView() = default;
  [[nodiscard]] virtual bool Inactive() const = 0;
  virtual FloatingLabelTimingView Timing() = 0;
  virtual void SetWorldLocked() = 0;
  virtual void SetWorldCoordinate(std::size_t axis, std::int16_t value) = 0;
};

// Slots retain the producer's entire payload. Queue operations neither decode
// nor reconstruct the actor, status, timing-result or equipment payload.
class FloatingLabelQueueView {
 public:
  virtual ~FloatingLabelQueueView() = default;
  [[nodiscard]] virtual std::int16_t Count() const = 0;
  virtual void SetCount(std::int16_t count) = 0;
  virtual FloatingLabelView* Label(FloatingLabelSlot slot) = 0;
  virtual bool CopyLabel(FloatingLabelSlot destination,
                         FloatingLabelSlot source) = 0;
  virtual bool ClearLabel(FloatingLabelSlot slot) = 0;
  virtual bool ClearAllLabels() = 0;
};

// decomp: F00712 func_80093AB4 (src/BATTLE/BATTLE.PRG/2842C.c:1465).
bool ResetFloatingLabels(FloatingLabelQueueView& queue,
                         FloatingLabelResetView state);
// The borrowed coordinates are read after reserve, individually in write order:
// they may alias a slot that reserve shifts or clears.
// decomp: F00713 func_80093B04 (src/BATTLE/BATTLE.PRG/2842C.c:1475).
bool QueueWorldLockedLabel(FloatingLabelQueueView& queue,
                           std::span<const std::int16_t> position);
// decomp: F00721 func_80094AF8 (src/BATTLE/BATTLE.PRG/2842C.c:2067).
void AdvanceFloatingLabelQueue(FloatingLabelTimingView label);
// Reserve does not publish or increment Count. At capacity it evicts the
// oldest record, clears the final record and leaves Count unchanged.
// decomp: F00734 func_800962E4 (src/BATTLE/BATTLE.PRG/2842C.c:2595).
std::optional<FloatingLabelSlot> ReserveFloatingLabel(
    FloatingLabelQueueView& queue);
// Each pass searches again from zero, including any newly shifted inactive
// record. A pass with no inactive record leaves the queue unchanged.
// decomp: F00735 func_8009639C (src/BATTLE/BATTLE.PRG/2842C.c:2613).
bool CompactFloatingLabels(FloatingLabelQueueView& queue,
                           std::int32_t removal_count);
// decomp: F00736 func_80096444 (src/BATTLE/BATTLE.PRG/2842C.c:2628).
bool RemoveFloatingLabel(FloatingLabelQueueView& queue, FloatingLabelSlot slot);

// Publish the written slot's end, preserving any already published prefix.
// A full reserve returns slot31: publication is32, not an unsafe count33.
// Count/slot corruption is refused, never clamped back into the owner.
bool CommitFloatingLabel(FloatingLabelQueueView& queue, FloatingLabelSlot slot);
}  // namespace VS::Battle
