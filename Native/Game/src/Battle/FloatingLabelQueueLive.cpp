// Hand-maintained (tools/port/owned.csv).
#include "Battle/FloatingLabelQueueLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

#include "Battle/Feedback.hpp"
#include "Battle/FloatingLabelQueue.hpp"
#include "Core/Diagnostics.hpp"

namespace VS::Battle {
namespace {
constexpr char kWorldLockedLabel = 3;
constexpr std::size_t kLegacyFloatingLabelBytes = 24;
static_assert(sizeof(VS::Battle::FloatingLabel) == kLegacyFloatingLabelBytes);
static_assert(sizeof *VS::Battle::floatingLabels ==
              kFloatingLabelCapacity * kLegacyFloatingLabelBytes);

[[noreturn]] void Refuse(const char* message) {
  VS::Core::Errorf("battle.labels", "%s", message);
  std::abort();
}

VS::Battle::FloatingLabel& CurrentLabel(FloatingLabelSlot slot) {
  if (slot.index < 0 || slot.index >= kFloatingLabelCapacity) {
    Refuse("Floating-label slot is outside its 32-record owner");
  }
  if (VS::Battle::floatingLabels == nullptr) {
    Refuse("Floating-label queue has no record owner");
  }
  return (*VS::Battle::floatingLabels)[slot.index];
}

class LiveFloatingLabelView final : public FloatingLabelView {
 public:
  void Bind(VS::Battle::FloatingLabel& label) { label_ = &label; }
  [[nodiscard]] bool Inactive() const override {
    return label_->key.bytes[0] == 0;
  }
  FloatingLabelTimingView Timing() override {
    return {label_->position.pad, label_->frame};
  }
  void SetWorldLocked() override { label_->key.bytes[0] = kWorldLockedLabel; }
  void SetWorldCoordinate(std::size_t axis, std::int16_t value) override {
    const std::array coordinates = {&label_->position.vx, &label_->position.vy,
                                    &label_->position.vz};
    if (axis >= coordinates.size()) {
      Refuse("Floating-label world coordinate is outside its position");
    }
    *coordinates[axis] = value;
  }

 private:
  VS::Battle::FloatingLabel* label_ = nullptr;
};

class LiveFloatingLabelQueue final : public FloatingLabelQueueView {
 public:
  [[nodiscard]] std::int16_t Count() const override {
    return VS::Battle::floatingLabelCount;
  }
  void SetCount(std::int16_t count) override {
    VS::Battle::floatingLabelCount = count;
  }
  FloatingLabelView* Label(FloatingLabelSlot slot) override {
    auto& label = CurrentLabel(slot);
    auto& view = views_[static_cast<std::size_t>(slot.index)];
    view.Bind(label);
    return &view;
  }
  bool CopyLabel(FloatingLabelSlot destination,
                 FloatingLabelSlot source) override {
    auto& from = CurrentLabel(source);
    auto& to = CurrentLabel(destination);
    to = from;
    return true;
  }
  bool ClearLabel(FloatingLabelSlot slot) override {
    CurrentLabel(slot) = {};
    return true;
  }
  bool ClearAllLabels() override {
    if (VS::Battle::floatingLabels == nullptr) {
      Refuse("Floating-label reset has no 32-record owner");
    }
    for (auto& label : *VS::Battle::floatingLabels) label = {};
    return true;
  }

 private:
  std::array<LiveFloatingLabelView, kFloatingLabelCapacity> views_{};
};
}  // namespace

// decomp: F00712 func_80093AB4 (src/BATTLE/BATTLE.PRG/2842C.c:1465).
void ResetLiveFloatingLabels() {
  LiveFloatingLabelQueue queue;
  if (!ResetFloatingLabels(queue, {VS::Battle::actorMarkersHidden,
                                   VS::Battle::abilityTimingCueFrames,
                                   VS::Battle::statusLabelStepTimer,
                                   VS::Battle::ashleyStatusLabelsSuppressed})) {
    Refuse("Floating-label reset requires its record owner");
  }
}

// decomp: F00713 func_80093B04 (src/BATTLE/BATTLE.PRG/2842C.c:1475).
int QueueLiveWorldLockedLabel(std::span<const std::uint16_t> position) {
  static_cast<void>(LiveFloatingLabelCount());
  LiveFloatingLabelQueue queue;
  const std::span<const std::int16_t> coordinates{
      reinterpret_cast<const std::int16_t*>(position.data()), position.size()};
  if (!QueueWorldLockedLabel(queue, coordinates)) {
    Refuse("World label requires three coordinates and a valid reserved slot");
  }
  return 0;
}

int QueueLiveWorldLockedLabelEntry(std::uint16_t* position) {
  if (position == nullptr) return QueueLiveWorldLockedLabel({});
  return QueueLiveWorldLockedLabel({position, kFloatingLabelAxes});
}

// decomp: F00721 func_80094AF8 (src/BATTLE/BATTLE.PRG/2842C.c:2067).
void AdvanceLiveFloatingLabelQueue(VS::Battle::FloatingLabel* label) {
  if (label == nullptr) {
    Refuse("Floating-label advance has no label owner");
  }
  AdvanceFloatingLabelQueue({label->position.pad, label->frame});
}

// decomp: F00734 func_800962E4 (src/BATTLE/BATTLE.PRG/2842C.c:2595).
VS::Battle::FloatingLabel* ReserveLiveFloatingLabel() {
  static_cast<void>(LiveFloatingLabelCount());
  LiveFloatingLabelQueue queue;
  const auto slot = ReserveFloatingLabel(queue);
  if (!slot) Refuse("Floating-label reservation requires a valid record slot");
  return &CurrentLabel(slot.value());
}

// decomp: F00735 func_8009639C (src/BATTLE/BATTLE.PRG/2842C.c:2613).
void CompactLiveFloatingLabels(int removal_count) {
  if (removal_count > 0) static_cast<void>(LiveFloatingLabelCount());
  LiveFloatingLabelQueue queue;
  if (!CompactFloatingLabels(queue, removal_count)) {
    Refuse("Floating-label compaction requires its used record slots");
  }
}

// decomp: F00736 func_80096444 (src/BATTLE/BATTLE.PRG/2842C.c:2628).
void RemoveLiveFloatingLabel(int label_index) {
  LiveFloatingLabelQueue queue;
  if (!RemoveFloatingLabel(queue, {label_index})) {
    Refuse("Floating-label removal requires its used record slots");
  }
}

void CommitLiveFloatingLabel(const VS::Battle::FloatingLabel* label) {
  static_cast<void>(LiveFloatingLabelCount());
  if (VS::Battle::floatingLabels == nullptr || label == nullptr) {
    Refuse("Floating-label publication has no current record owner");
  }
  LiveFloatingLabelQueue queue;
  const auto records = std::span(*VS::Battle::floatingLabels);
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (&records[index] != label) continue;
    if (!CommitFloatingLabel(queue, {static_cast<std::int32_t>(index)})) {
      Refuse("Floating-label publication has invalid count or slot metadata");
    }
    return;
  }
  Refuse("Floating-label publication record is outside the current owner");
}

std::int16_t LiveFloatingLabelCount() {
  const auto count = VS::Battle::floatingLabelCount;
  if (count > kFloatingLabelCapacity) {
    Refuse("Floating-label traversal count exceeds its 32-record owner");
  }
  if (count > 0 && VS::Battle::floatingLabels == nullptr) {
    Refuse("Floating-label traversal has no current record owner");
  }
  return count;
}
}  // namespace VS::Battle
