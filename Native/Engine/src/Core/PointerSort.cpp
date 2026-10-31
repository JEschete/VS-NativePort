#include "Core/PointerSort.hpp"

#include <cstddef>
#include <span>
#include <utility>

namespace VS::Core {
namespace {
constexpr std::size_t kMaximumEntries = 256;

void Sort(std::span<SortKey> keys) {
  if (keys.size() < 2) return;
  std::swap(keys.front(), keys[(keys.size() - 1) / 2]);
  std::size_t boundary = 0;
  for (std::size_t index = 1; index < keys.size(); ++index) {
    if (keys[index].key > keys.front().key) {
      std::swap(keys[++boundary], keys[index]);
    }
  }
  std::swap(keys.front(), keys[boundary]);
  Sort(keys.first(boundary));
  Sort(keys.subspan(boundary + 1));
}
}  // namespace

// decomp: func_800E6974 / L800E6958 (src/BATTLE/BATTLE.PRG/7E05C.s:114).
// Keep midpoint partitioning: a stable sort changes equal-key identities.
bool SortPointersByDescendingKey(std::span<SortKey> keys) {
  if (keys.size() > kMaximumEntries) return false;
  Sort(keys);
  return true;
}
}  // namespace VS::Core
