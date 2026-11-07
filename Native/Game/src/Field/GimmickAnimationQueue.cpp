#include "Field/GimmickAnimationQueue.hpp"

#include <algorithm>
#include <bit>
#include <optional>

namespace VS::Field {

// decomp: BATTLE.PRG func_8008C070 (src/BATTLE/BATTLE.PRG/146C.c:16338).
// #318: original8008C08C increments before the bounded search can refuse.
bool QueueGimmickAnimation(GimmickAnimationKind kind,
                           GimmickAnimationQueueHost& host) {
  if ((kind != GimmickAnimationKind::kFinishDoor &&
       kind != GimmickAnimationKind::kToggleSwitch &&
       kind != GimmickAnimationKind::kStartDoor) ||
      !host.HasRequestedTarget()) {
    return false;
  }
  const auto occupied = host.Occupancy();
  std::optional<std::size_t> first_free;
  std::size_t active = 0;
  for (std::size_t slot = 0; slot < occupied.size(); ++slot) {
    if (occupied[slot]) {
      ++active;
    } else if (!first_free) {
      first_free = slot;
    }
  }
  if (!first_free || !host.StoreRequestedJob(*first_free, kind)) {
    return false;
  }
  host.SetPendingCount(active + 1);
  return true;
}

// decomp: BATTLE.PRG func_8008C0C0 (src/BATTLE/BATTLE.PRG/146C.c:16353).
// CPU8008C12C-8008C140 uses signed-byte switch countdown; door delay wraps8.
std::size_t UpdateGimmickAnimationQueue(GimmickAnimationDispatchHost& host) {
  if (host.PendingCount() <= 0) {
    return 0;
  }
  std::size_t retired = 0;
  for (std::size_t slot = 0; slot < kGimmickAnimationSlots; ++slot) {
    const auto job = host.Job(slot);
    if (!job) {
      continue;
    }
    if (!job->has_target) {
      retired += static_cast<std::size_t>(host.RetireJob(slot, *job));
      continue;
    }
    if (job->kind == GimmickAnimationKind::kFinishDoor) {
      if (host.FinishDoor(slot, *job)) {
        retired += static_cast<std::size_t>(host.RetireJob(slot, *job));
      }
      continue;
    }
    if (job->kind != GimmickAnimationKind::kToggleSwitch &&
        job->kind != GimmickAnimationKind::kStartDoor) {
      retired += static_cast<std::size_t>(host.RetireJob(slot, *job));
      continue;
    }
    const auto countdown = static_cast<std::uint8_t>(job->countdown - 1);
    if (!host.StoreCountdown(slot, *job, countdown)) {
      continue;
    }
    if (job->kind == GimmickAnimationKind::kToggleSwitch) {
      if (std::bit_cast<std::int8_t>(countdown) <= 0) {
        host.ToggleSwitch(slot, *job);
        retired += static_cast<std::size_t>(host.RetireJob(slot, *job));
      }
      continue;
    }
    if (countdown != 0) {
      continue;
    }
    const auto occupied = host.Occupancy();
    const bool full =
        std::ranges::all_of(occupied, [](bool active) { return active; });
    if (full) {
      if (!host.RetireJob(slot, *job)) {
        continue;
      }
      ++retired;
    }
    host.StartDoor(slot, *job);
    if (!full) {
      retired += static_cast<std::size_t>(host.RetireJob(slot, *job));
    }
  }
  return retired;
}

}  // namespace VS::Field
