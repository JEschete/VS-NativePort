#include "Field/GimmickAnimationQueueLive.h"

#include <algorithm>
#include <array>
#include <iterator>

#include "Field/GimmickAnimationQueue.hpp"
#include "Field/RoomSwitchesLive.h"

extern "C" {
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
}

namespace {

static_assert(std::size(Field_gimmickAnimationQueue) ==
              VS::Field::kGimmickAnimationSlots);

std::array<std::uint64_t, VS::Field::kGimmickAnimationSlots> job_serials{};

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

  bool HasRequestedTarget() const override { return ValidTarget(target_); }
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
    if (++job_serials[slot] == 0) {
      ++job_serials[slot];
    }
    return true;
  }
  void SetPendingCount(std::size_t count) override {
    Field_pendingGimmickAnimationCount = static_cast<std::int16_t>(count);
  }

 private:
  Field_MpdLockTrigger* target_ = nullptr;
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
    const auto kind =
        static_cast<VS::Field::GimmickAnimationKind>(entry.jobKind);
    std::uint8_t countdown = 0;
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
      Field_ToggleRoomSwitchEntry(targets_[slot]);
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
        Field_gimmickAnimationQueue[slot].trigger != targets_[slot]) {
      return false;
    }
    Field_gimmickAnimationQueue[slot] = {};
    const auto occupied = QueueOccupancy();
    Field_pendingGimmickAnimationCount =
        static_cast<std::int16_t>(std::ranges::count(occupied, true));
    return true;
  }

 private:
  bool CapturedValid(std::size_t slot,
                     const VS::Field::GimmickAnimationJobSnapshot& job) const {
    return slot < targets_.size() && job_serials[slot] == job.serial &&
           ValidTarget(targets_[slot]);
  }
  bool Matches(std::size_t slot,
               const VS::Field::GimmickAnimationJobSnapshot& job) const {
    return CapturedValid(slot, job) &&
           Field_gimmickAnimationQueue[slot].jobKind ==
               static_cast<std::int32_t>(job.kind) &&
           Field_gimmickAnimationQueue[slot].trigger == targets_[slot];
  }
  mutable std::array<Field_MpdLockTrigger*, VS::Field::kGimmickAnimationSlots>
      targets_{};
};

}  // namespace

// decomp: BATTLE.PRG func_8008C070 (src/BATTLE/BATTLE.PRG/146C.c:16338).
void Field_QueueGimmickAnimationEntry(int queued_job_kind,
                                      Field_MpdLockTrigger* lock_trigger) {
  LiveAnimationQueueHost host(lock_trigger);
  static_cast<void>(VS::Field::QueueGimmickAnimation(
      static_cast<VS::Field::GimmickAnimationKind>(queued_job_kind), host));
}

// decomp: BATTLE.PRG func_8008C0C0 (src/BATTLE/BATTLE.PRG/146C.c:16353).
void Field_UpdateGimmickAnimationQueueEntry() {
  LiveAnimationDispatchHost host;
  static_cast<void>(VS::Field::UpdateGimmickAnimationQueue(host));
}
