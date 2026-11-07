#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Field {

inline constexpr std::size_t kGimmickAnimationSlots = 8;
using GimmickAnimationOccupancy = std::array<bool, kGimmickAnimationSlots>;

enum class GimmickAnimationKind : std::int32_t {
  kFinishDoor = 1,
  kToggleSwitch = 2,
  kStartDoor = 3,
};

class GimmickAnimationQueueHost {
 public:
  virtual ~GimmickAnimationQueueHost() = default;
  virtual bool HasRequestedTarget() const = 0;
  virtual GimmickAnimationOccupancy Occupancy() const = 0;
  virtual bool StoreRequestedJob(std::size_t slot,
                                 GimmickAnimationKind kind) = 0;
  virtual void SetPendingCount(std::size_t count) = 0;
};

bool QueueGimmickAnimation(GimmickAnimationKind kind,
                           GimmickAnimationQueueHost& host);

}  // namespace VS::Field
