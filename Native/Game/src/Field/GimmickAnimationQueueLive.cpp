#include "Field/GimmickAnimationQueueLive.hpp"

#include <algorithm>
#include <array>
#include <iterator>

#include "Field/ChestInteractionsLive.hpp"
#include "Field/GimmickAnimationQueue.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/Mpd.hpp"
#include "Field/RoomSwitchesLive.hpp"
#include "Items/ItemRecords.hpp"

namespace {

static_assert(std::size(VS::Field::gimmickAnimationQueue) ==
              VS::Field::kGimmickAnimationSlots);

std::array<std::uint64_t, VS::Field::kGimmickAnimationSlots> jobSerials{};
using ChestPayload = std::optional<VS::Field::ChestTarget>;
using ChestJobs = std::array<ChestPayload, VS::Field::kGimmickAnimationSlots>;
ChestJobs chestJobs{};
using RoomPayload = std::optional<VS::Field::RoomObjectTarget>;
using RoomJobs = std::array<RoomPayload, VS::Field::kGimmickAnimationSlots>;
RoomJobs roomJobs{};

void InvalidateJobSerial(std::size_t slot) {
  if (++jobSerials[slot] == 0) {
    ++jobSerials[slot];
  }
}

bool ValidTarget(const VS::Field::MpdLockTrigger* target) {
  return target != nullptr && reinterpret_cast<std::uintptr_t>(target) %
                                      alignof(VS::Field::MpdLockTrigger) ==
                                  0;
}

VS::Field::GimmickAnimationOccupancy QueueOccupancy() {
  VS::Field::GimmickAnimationOccupancy occupied{};
  for (std::size_t slot = 0; slot < occupied.size(); ++slot) {
    occupied[slot] = VS::Field::gimmickAnimationQueue[slot].job_kind != 0;
  }
  return occupied;
}

class LiveAnimationQueueHost final
    : public VS::Field::GimmickAnimationQueueHost {
 public:
  explicit LiveAnimationQueueHost(VS::Field::MpdLockTrigger* target)
      : target_(target) {}
  explicit LiveAnimationQueueHost(VS::Field::ChestTarget target)
      : chest_target_(target) {}
  explicit LiveAnimationQueueHost(VS::Field::RoomObjectTarget target)
      : room_target_(target) {}

  bool HasRequestedTarget() const override {
    if (room_target_) {
      return VS::Field::ResolveLiveRoomObjectTarget(*room_target_) != nullptr;
    }
    return chest_target_
               ? VS::Field::ResolveLiveChestTarget(*chest_target_) != nullptr
               : ValidTarget(target_);
  }
  VS::Field::GimmickAnimationOccupancy Occupancy() const override {
    return QueueOccupancy();
  }
  bool StoreRequestedJob(std::size_t slot,
                         VS::Field::GimmickAnimationKind kind) override {
    if (slot >= std::size(VS::Field::gimmickAnimationQueue) ||
        VS::Field::gimmickAnimationQueue[slot].job_kind != 0 ||
        !HasRequestedTarget()) {
      return false;
    }
    auto* target = room_target_
                       ? VS::Field::ResolveLiveRoomObjectTarget(*room_target_)
                       : target_;
    VS::Field::gimmickAnimationQueue[slot] = {static_cast<std::int32_t>(kind),
                                              target};
    chestJobs[slot] = chest_target_;
    roomJobs[slot] = room_target_;
    InvalidateJobSerial(slot);
    return true;
  }
  void SetPendingCount(std::size_t count) override {
    VS::Field::pendingGimmickAnimationCount = static_cast<std::int16_t>(count);
  }

 private:
  VS::Field::MpdLockTrigger* target_ = nullptr;
  std::optional<VS::Field::ChestTarget> chest_target_;
  std::optional<VS::Field::RoomObjectTarget> room_target_;
};

class LiveAnimationDispatchHost final
    : public VS::Field::GimmickAnimationDispatchHost {
 public:
  std::int32_t PendingCount() const override {
    return VS::Field::pendingGimmickAnimationCount;
  }
  VS::Field::GimmickAnimationOccupancy Occupancy() const override {
    return QueueOccupancy();
  }
  std::optional<VS::Field::GimmickAnimationJobSnapshot> Job(
      std::size_t slot) const override {
    if (slot >= targets_.size() ||
        VS::Field::gimmickAnimationQueue[slot].job_kind == 0) {
      return std::nullopt;
    }
    const auto& entry = VS::Field::gimmickAnimationQueue[slot];
    targets_[slot] = entry.trigger;
    chest_targets_[slot] = chestJobs[slot];
    room_targets_[slot] = roomJobs[slot];
    const auto kind =
        static_cast<VS::Field::GimmickAnimationKind>(entry.job_kind);
    std::uint8_t countdown = 0;
    if (chest_targets_[slot]) {
      const auto* chest =
          VS::Field::ResolveLiveChestTarget(*chest_targets_[slot]);
      const bool valid_chest =
          chest != nullptr &&
          kind == VS::Field::GimmickAnimationKind::kToggleSwitch;
      if (valid_chest) {
        countdown = static_cast<std::uint8_t>(chest->toggle_state);
      }
      return VS::Field::GimmickAnimationJobSnapshot{
          kind, countdown, jobSerials[slot], valid_chest};
    }
    const auto room = room_targets_[slot];
    const bool valid =
        room
            ? entry.trigger != nullptr &&
                  VS::Field::ResolveLiveRoomObjectTarget(*room) == entry.trigger
            : ValidTarget(entry.trigger);
    if (valid && kind == VS::Field::GimmickAnimationKind::kToggleSwitch) {
      countdown = static_cast<std::uint8_t>(entry.trigger->trigger.activation);
    } else if (valid && kind == VS::Field::GimmickAnimationKind::kStartDoor) {
      countdown = static_cast<std::uint8_t>(entry.trigger->delay_frames);
    }
    return VS::Field::GimmickAnimationJobSnapshot{kind, countdown,
                                                  jobSerials[slot], valid};
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
      const auto marker =
          static_cast<std::uint16_t>(chest->toggle_state) & 0xFF00;
      chest->toggle_state = static_cast<std::int16_t>(marker | countdown);
      return true;
    }
    auto& target = *targets_[slot];
    if (job.kind == VS::Field::GimmickAnimationKind::kToggleSwitch) {
      target.trigger.activation =
          static_cast<decltype(target.trigger.activation)>(countdown);
    } else {
      target.delay_frames =
          static_cast<decltype(target.delay_frames)>(countdown);
    }
    return true;
  }
  bool FinishDoor(std::size_t slot,
                  const VS::Field::GimmickAnimationJobSnapshot& job) override {
    if (!Matches(slot, job)) return false;
    const auto room = room_targets_[slot];
    return room ? VS::Field::FinishLiveDoorAnimation(*room)
                : VS::Field::FinishDoorAnimationEntry(targets_[slot]) != 0;
  }
  void ToggleSwitch(
      std::size_t slot,
      const VS::Field::GimmickAnimationJobSnapshot& job) override {
    if (Matches(slot, job)) {
      if (chest_targets_[slot]) {
        static_cast<void>(
            VS::Field::ToggleLiveChestSwitch(*chest_targets_[slot]));
      } else {
        VS::Field::ToggleRoomSwitchEntry(targets_[slot]);
      }
    }
  }
  void StartDoor(std::size_t slot,
                 const VS::Field::GimmickAnimationJobSnapshot& job) override {
    if (CapturedValid(slot, job)) {
      const auto room = room_targets_[slot];
      if (room) {
        static_cast<void>(VS::Field::StartLiveDoorAnimation(*room));
      } else {
        static_cast<void>(VS::Field::StartDoorAnimationEntry(targets_[slot]));
      }
    }
  }
  bool RetireJob(std::size_t slot,
                 const VS::Field::GimmickAnimationJobSnapshot& job) override {
    if (slot >= targets_.size() || jobSerials[slot] != job.serial ||
        VS::Field::gimmickAnimationQueue[slot].job_kind !=
            static_cast<std::int32_t>(job.kind) ||
        (chest_targets_[slot] ? chestJobs[slot] != chest_targets_[slot]
         : room_targets_[slot]
             ? roomJobs[slot] != room_targets_[slot] ||
                   VS::Field::gimmickAnimationQueue[slot].trigger !=
                       targets_[slot]
             : VS::Field::gimmickAnimationQueue[slot].trigger !=
                   targets_[slot])) {
      return false;
    }
    VS::Field::gimmickAnimationQueue[slot] = {};
    chestJobs[slot].reset();
    roomJobs[slot].reset();
    const auto occupied = QueueOccupancy();
    VS::Field::pendingGimmickAnimationCount =
        static_cast<std::int16_t>(std::ranges::count(occupied, true));
    return true;
  }

 private:
  bool CapturedValid(std::size_t slot,
                     const VS::Field::GimmickAnimationJobSnapshot& job) const {
    if (slot >= targets_.size() || jobSerials[slot] != job.serial) {
      return false;
    }
    if (const auto room = room_targets_[slot]) {
      return targets_[slot] != nullptr &&
             VS::Field::ResolveLiveRoomObjectTarget(*room) == targets_[slot];
    }
    return chest_targets_[slot]
               ? VS::Field::ResolveLiveChestTarget(*chest_targets_[slot]) !=
                     nullptr
               : ValidTarget(targets_[slot]);
  }
  bool Matches(std::size_t slot,
               const VS::Field::GimmickAnimationJobSnapshot& job) const {
    return CapturedValid(slot, job) &&
           VS::Field::gimmickAnimationQueue[slot].job_kind ==
               static_cast<std::int32_t>(job.kind) &&
           (chest_targets_[slot] ? chestJobs[slot] == chest_targets_[slot]
            : room_targets_[slot]
                ? roomJobs[slot] == room_targets_[slot] &&
                      VS::Field::gimmickAnimationQueue[slot].trigger ==
                          targets_[slot]
                : VS::Field::gimmickAnimationQueue[slot].trigger ==
                      targets_[slot]);
  }
  mutable std::array<VS::Field::MpdLockTrigger*,
                     VS::Field::kGimmickAnimationSlots>
      targets_{};
  mutable std::array<std::optional<VS::Field::ChestTarget>,
                     VS::Field::kGimmickAnimationSlots>
      chest_targets_{};
  mutable RoomJobs room_targets_{};
};

class LiveAnimationResetHost final
    : public VS::Field::GimmickAnimationResetHost {
 public:
  void ClearPendingCount() override {
    VS::Field::pendingGimmickAnimationCount = 0;
  }
  void ClearMarkerTicks() override { VS::Field::triggerMarkerFrameTicks = 0; }
  void ClearJob(std::size_t slot) override {
    VS::Field::gimmickAnimationQueue[slot] = {};
    chestJobs[slot].reset();
    roomJobs[slot].reset();
    InvalidateJobSerial(slot);
  }
};

}  // namespace

namespace VS::Field {
bool QueueLiveGimmickAnimation(int kind, VS::Field::MpdLockTrigger* target) {
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
bool QueueLiveGimmickAnimation(int kind, RoomObjectTarget target) {
  LiveAnimationQueueHost host(target);
  return QueueGimmickAnimation(static_cast<GimmickAnimationKind>(kind), host);
}
}  // namespace VS::Field

// decomp: BATTLE.PRG func_8008C070 (src/BATTLE/BATTLE.PRG/146C.c:16209).
void VS::Field::QueueGimmickAnimationEntry(
    int queued_job_kind, VS::Field::MpdLockTrigger* lock_trigger) {
  static_cast<void>(
      VS::Field::QueueLiveGimmickAnimation(queued_job_kind, lock_trigger));
}

// decomp: BATTLE.PRG func_8008C0C0 (src/BATTLE/BATTLE.PRG/146C.c:16224).
void VS::Field::UpdateGimmickAnimationQueueEntry() {
  LiveAnimationDispatchHost host;
  static_cast<void>(VS::Field::UpdateGimmickAnimationQueue(host));
}

// decomp: BATTLE.PRG func_8008B6B4 (src/BATTLE/BATTLE.PRG/146C.c:15922).
void VS::Field::ResetGimmickAnimationQueueEntry() {
  LiveAnimationResetHost host;
  VS::Field::ResetGimmickAnimationQueue(host);
}
