#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

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

struct GimmickAnimationJobSnapshot {
  GimmickAnimationKind kind = GimmickAnimationKind::kFinishDoor;
  std::uint8_t countdown = 0;
  std::uint64_t serial = 0;
  bool has_target = true;
};

class GimmickAnimationDispatchHost {
 public:
  virtual ~GimmickAnimationDispatchHost() = default;
  virtual std::int32_t PendingCount() const = 0;
  virtual GimmickAnimationOccupancy Occupancy() const = 0;
  virtual std::optional<GimmickAnimationJobSnapshot> Job(
      std::size_t slot) const = 0;
  virtual bool StoreCountdown(std::size_t slot,
                              const GimmickAnimationJobSnapshot& job,
                              std::uint8_t countdown) = 0;
  virtual bool FinishDoor(std::size_t slot,
                          const GimmickAnimationJobSnapshot& job) = 0;
  virtual void ToggleSwitch(std::size_t slot,
                            const GimmickAnimationJobSnapshot& job) = 0;
  virtual void StartDoor(std::size_t slot,
                         const GimmickAnimationJobSnapshot& job) = 0;
  virtual bool RetireJob(std::size_t slot,
                         const GimmickAnimationJobSnapshot& job) = 0;
};

std::size_t UpdateGimmickAnimationQueue(GimmickAnimationDispatchHost& host);

}  // namespace VS::Field
