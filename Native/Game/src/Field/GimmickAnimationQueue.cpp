#include "Field/GimmickAnimationQueue.hpp"

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

}  // namespace VS::Field
