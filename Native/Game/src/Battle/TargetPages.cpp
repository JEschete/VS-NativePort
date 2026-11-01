#include "Battle/TargetPages.hpp"

#include <cstdlib>
#include <vector>

#include "Core/Log.hpp"

namespace VS::Battle {
std::vector<TargetPage> PartitionTargetPages(
    std::span<const std::uint8_t> actor_ids, std::size_t page_size) {
  std::vector<TargetPage> result;
  for (std::size_t first = 0; first < actor_ids.size();) {
    std::size_t end = first;
    while (end < actor_ids.size()) {
      std::size_t group_end = end + 1;
      while (group_end < actor_ids.size() &&
             actor_ids[group_end] == actor_ids[end]) {
        ++group_end;
      }
      if (group_end - end > page_size) {
        VS_LOG_ERROR("targeting.page", "actor group exceeds visible capacity");
        std::abort();
      }
      if (group_end - first > page_size) break;
      end = group_end;
    }
    result.push_back({first, end - first});
    first = end;
  }
  return result;
}

// func_800C1664, BATTLE/BATTLE.PRG/58578.c:446. The original visible list
// remains bounded; native pages retain every candidate and whole actor groups.
void SetSphereTargets(TargetPageBook& book,
                      std::span<const std::uint8_t> actor_ids, int mode,
                      TargetSphere& sphere) {
  book = {};
  if (!sphere.Active()) {
    VS_LOG_DEBUG("targeting.page", "no active sphere for target binding");
    return;
  }
  if (actor_ids.size() > kMaxSphereTargets) {
    VS_LOG_ERROR("targeting.page", "invalid targets count={}",
                 actor_ids.size());
    std::abort();
  }
  book.pages = PartitionTargetPages(actor_ids, sphere.VisibleSlots());
  book.total = actor_ids.size();
  sphere.ShowPage(book.pages.empty() ? TargetPage{} : book.pages.front(), mode);
}
}  // namespace VS::Battle
