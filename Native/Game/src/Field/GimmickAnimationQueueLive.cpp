#include "Field/GimmickAnimationQueueLive.h"

#include <algorithm>
#include <array>
#include <iterator>

#include "Field/GimmickAnimationQueue.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/RoomSwitchesLive.h"

extern "C" {
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Items/TreasureChests.h"
}

namespace {

static_assert(std::size(Field_gimmickAnimationQueue) ==
              VS::Field::kGimmickAnimationSlots);

std::array<std::uint64_t, VS::Field::kGimmickAnimationSlots> job_serials{};
using ChestPayload = std::optional<VS::Field::ChestTarget>;
using ChestJobs = std::array<ChestPayload, VS::Field::kGimmickAnimationSlots>;
ChestJobs chest_jobs{};

void InvalidateJobSerial(std::size_t slot) {
  if (++job_serials[slot] == 0) {
    ++job_serials[slot];
  }
}

bool ValidTarget(const Field_MpdLockTrigger* target) {
  return target != nullptr && reinterpret_cast<std::uintptr_t>(target) %
                                      alignof(Field_MpdLockTrigger) ==
                                  0;
}

VS::Field::GimmickAnimationOccupancy QueueOccupancy() {
  VS::Field::GimmickAnimationOccupancy occupied{};
  for (std::size_t slot = 0; slot < occupied.size(); ++slot) {
    occupied[slot] = Field_gimmickAnimationQueue[slot].jobKind != 0;
  }
  return occupied;
}

class LiveAnimationQueueHost final
    : public VS::Field::GimmickAnimationQueueHost {
 public:
  explicit LiveAnimationQueueHost(Field_MpdLockTrigger* target)
      : target_(target) {}
  explicit LiveAnimationQueueHost(VS::Field::ChestTarget target)
      : chest_target_(target) {}

  bool HasRequestedTarget() const override {
    return chest_target_
               ? VS::Field::ResolveLiveChestTarget(*chest_target_) != nullptr
               : ValidTarget(target_);
  }
  VS::Field::GimmickAnimationOccupancy Occupancy() const override {
    return QueueOccupancy();
  }
  bool StoreRequestedJob(std::size_t slot,
                         VS::Field::GimmickAnimationKind kind) override {
    if (slot >= std::size(Field_gimmickAnimationQueue) ||
        Field_gimmickAnimationQueue[slot].jobKind != 0 ||
        !HasRequestedTarget()) {
      return false;
    }
    Field_gimmickAnimationQueue[slot] = {static_cast<std::int32_t>(kind),
                                         target_};
    chest_jobs[slot] = chest_target_;
    InvalidateJobSerial(slot);
    return true;
  }
  void SetPendingCount(std::size_t count) override {
    Field_pendingGimmickAnimationCount = static_cast<std::int16_t>(count);
  }

 private:
  Field_MpdLockTrigger* target_ = nullptr;
  std::optional<VS::Field::ChestTarget> chest_target_;
};

class LiveAnimationDispatchHost final
    : public VS::Field::GimmickAnimationDispatchHost {
 public:
  std::int32_t PendingCount() const override {
    return Field_pendingGimmickAnimationCount;
  }
  VS::Field::GimmickAnimationOccupancy Occupancy() const override {
    return QueueOccupancy();
  }
  std::optional<VS::Field::GimmickAnimationJobSnapshot> Job(
      std::size_t slot) const override {
    if (slot >= targets_.size() ||
        Field_gimmickAnimationQueue[slot].jobKind == 0) {
      return std::nullopt;
    }
    const auto& entry = Field_gimmickAnimationQueue[slot];
    targets_[slot] = entry.trigger;
    chest_targets_[slot] = chest_jobs[slot];
    const auto kind =
        static_cast<VS::Field::GimmickAnimationKind>(entry.jobKind);
    std::uint8_t countdown = 0;
    if (chest_targets_[slot]) {
      const auto* chest =
          VS::Field::ResolveLiveChestTarget(*chest_targets_[slot]);
      const bool valid_chest =
          chest != nullptr &&
          kind == VS::Field::GimmickAnimationKind::kToggleSwitch;
      if (valid_chest) {
        countdown = static_cast<std::uint8_t>(chest->reservedA);
      }
      return VS::Field::GimmickAnimationJobSnapshot{
          kind, countdown, job_serials[slot], valid_chest};
    }
    const bool valid = ValidTarget(entry.trigger);
    if (valid && kind == VS::Field::GimmickAnimationKind::kToggleSwitch) {
      countdown = static_cast<std::uint8_t>(entry.trigger->trigger.activation);
    } else if (valid && kind == VS::Field::GimmickAnimationKind::kStartDoor) {
      countdown = static_cast<std::uint8_t>(entry.trigger->delayFrames);
    }
    return VS::Field::GimmickAnimationJobSnapshot{kind, countdown,
                                                  job_serials[slot], valid};
  }
  bool StoreCountdown(std::size_t slot,
                      const VS::Field::GimmickAnimationJobSnapshot& job,
                      std::uint8_t countdown) override {
    if (!Matches(slot, job)) {
      return false;
    }
    if (chest_targets_[slot]) {
      auto* chest = VS::Field::ResolveLiveChestTarget(*chest_targets_[slot]);
      if (chest == nullptr) {
        return false;
      }
      const auto marker = static_cast<std::uint16_t>(chest->reservedA) & 0xFF00;
      chest->reservedA = static_cast<std::int16_t>(marker | countdown);
      return true;
    }
    auto& target = *targets_[slot];
    if (job.kind == VS::Field::GimmickAnimationKind::kToggleSwitch) {
      target.trigger.activation =
          static_cast<decltype(target.trigger.activation)>(countdown);
    } else {
      target.delayFrames = static_cast<decltype(target.delayFrames)>(countdown);
    }
    return true;
  }
  bool FinishDoor(std::size_t slot,
                  const VS::Field::GimmickAnimationJobSnapshot& job) override {
    return Matches(slot, job) && Field_FinishDoorAnimation(targets_[slot]) != 0;
  }
  void ToggleSwitch(
      std::size_t slot,
      const VS::Field::GimmickAnimationJobSnapshot& job) override {
    if (Matches(slot, job)) {
      if (chest_targets_[slot]) {
        static_cast<void>(
            VS::Field::ToggleLiveChestSwitch(*chest_targets_[slot]));
      } else {
        Field_ToggleRoomSwitchEntry(targets_[slot]);
      }
    }
  }
  void StartDoor(std::size_t slot,
                 const VS::Field::GimmickAnimationJobSnapshot& job) override {
    if (CapturedValid(slot, job)) {
      static_cast<void>(Field_StartDoorAnimation(targets_[slot]));
    }
  }
  bool RetireJob(std::size_t slot,
                 const VS::Field::GimmickAnimationJobSnapshot& job) override {
    if (slot >= targets_.size() || job_serials[slot] != job.serial ||
        Field_gimmickAnimationQueue[slot].jobKind !=
            static_cast<std::int32_t>(job.kind) ||
        (chest_targets_[slot]
             ? chest_jobs[slot] != chest_targets_[slot]
             : Field_gimmickAnimationQueue[slot].trigger != targets_[slot])) {
      return false;
    }
    Field_gimmickAnimationQueue[slot] = {};
    chest_jobs[slot].reset();
    const auto occupied = QueueOccupancy();
    Field_pendingGimmickAnimationCount =
        static_cast<std::int16_t>(std::ranges::count(occupied, true));
    return true;
  }

 private:
  bool CapturedValid(std::size_t slot,
                     const VS::Field::GimmickAnimationJobSnapshot& job) const {
    if (slot >= targets_.size() || job_serials[slot] != job.serial) {
      return false;
    }
    return chest_targets_[slot]
               ? VS::Field::ResolveLiveChestTarget(*chest_targets_[slot]) !=
                     nullptr
               : ValidTarget(targets_[slot]);
  }
  bool Matches(std::size_t slot,
               const VS::Field::GimmickAnimationJobSnapshot& job) const {
    return CapturedValid(slot, job) &&
           Field_gimmickAnimationQueue[slot].jobKind ==
               static_cast<std::int32_t>(job.kind) &&
           (chest_targets_[slot]
                ? chest_jobs[slot] == chest_targets_[slot]
                : Field_gimmickAnimationQueue[slot].trigger == targets_[slot]);
  }
  mutable std::array<Field_MpdLockTrigger*, VS::Field::kGimmickAnimationSlots>
      targets_{};
  mutable std::array<std::optional<VS::Field::ChestTarget>,
                     VS::Field::kGimmickAnimationSlots>
      chest_targets_{};
};

class LiveAnimationResetHost final
    : public VS::Field::GimmickAnimationResetHost {
 public:
  void ClearPendingCount() override { Field_pendingGimmickAnimationCount = 0; }
  void ClearMarkerTicks() override { Field_triggerMarkerFrameTicks = 0; }
  void ClearJob(std::size_t slot) override {
    Field_gimmickAnimationQueue[slot] = {};
    chest_jobs[slot].reset();
    InvalidateJobSerial(slot);
  }
};

}  // namespace

namespace VS::Field {
bool QueueLiveGimmickAnimation(int kind, Field_MpdLockTrigger* target) {
  LiveAnimationQueueHost host(target);
  return QueueGimmickAnimation(static_cast<GimmickAnimationKind>(kind), host);
}
bool QueueLiveGimmickAnimation(int kind, ChestTarget target) {
  if (kind != static_cast<int>(GimmickAnimationKind::kToggleSwitch)) {
    return false;
  }
  LiveAnimationQueueHost host(target);
  return QueueGimmickAnimation(static_cast<GimmickAnimationKind>(kind), host);
}
}  // namespace VS::Field

// decomp: BATTLE.PRG func_8008C070 (src/BATTLE/BATTLE.PRG/146C.c:16338).
void Field_QueueGimmickAnimationEntry(int queued_job_kind,
                                      Field_MpdLockTrigger* lock_trigger) {
  static_cast<void>(
      VS::Field::QueueLiveGimmickAnimation(queued_job_kind, lock_trigger));
}

// decomp: BATTLE.PRG func_8008C0C0 (src/BATTLE/BATTLE.PRG/146C.c:16353).
void Field_UpdateGimmickAnimationQueueEntry() {
  LiveAnimationDispatchHost host;
  static_cast<void>(VS::Field::UpdateGimmickAnimationQueue(host));
}

// decomp: BATTLE.PRG func_8008B6B4 (src/BATTLE/BATTLE.PRG/146C.c:16052).
void Field_ResetGimmickAnimationQueueEntry() {
  LiveAnimationResetHost host;
  VS::Field::ResetGimmickAnimationQueue(host);
}
