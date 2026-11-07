#include "Field/GimmickAnimationQueueLive.h"

#include <iterator>

#include "Field/GimmickAnimationQueue.hpp"

extern "C" {
#include "Field/Gimmicks.h"
}

namespace {

static_assert(std::size(Field_gimmickAnimationQueue) ==
              VS::Field::kGimmickAnimationSlots);

class LiveAnimationQueueHost final
    : public VS::Field::GimmickAnimationQueueHost {
 public:
  explicit LiveAnimationQueueHost(Field_MpdLockTrigger* target)
      : target_(target) {}

  bool HasRequestedTarget() const override {
    return target_ != nullptr && reinterpret_cast<std::uintptr_t>(target_) %
                                         alignof(Field_MpdLockTrigger) ==
                                     0;
  }
  VS::Field::GimmickAnimationOccupancy Occupancy() const override {
    VS::Field::GimmickAnimationOccupancy occupied{};
    for (std::size_t slot = 0; slot < occupied.size(); ++slot) {
      occupied[slot] = Field_gimmickAnimationQueue[slot].jobKind != 0;
    }
    return occupied;
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
    return true;
  }
  void SetPendingCount(std::size_t count) override {
    Field_pendingGimmickAnimationCount = static_cast<std::int16_t>(count);
  }

 private:
  Field_MpdLockTrigger* target_ = nullptr;
};

}  // namespace

// decomp: BATTLE.PRG func_8008C070 (src/BATTLE/BATTLE.PRG/146C.c:16338).
void Field_QueueGimmickAnimationEntry(int queued_job_kind,
                                      Field_MpdLockTrigger* lock_trigger) {
  LiveAnimationQueueHost host(lock_trigger);
  static_cast<void>(VS::Field::QueueGimmickAnimation(
      static_cast<VS::Field::GimmickAnimationKind>(queued_job_kind), host));
}
