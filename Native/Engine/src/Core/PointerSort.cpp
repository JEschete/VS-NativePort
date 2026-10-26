#include "Core/PointerSort.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>
#include <utility>

#include "Core/Log.hpp"

namespace {
constexpr int kMaximumEntries = 256;

void Require(bool valid, const char* message) {
  if (!valid) {
    VS_LOG_ERROR("sort", "{}", message);
    std::abort();
  }
}

std::int32_t Key(const void* record) {
  std::int32_t key;
  std::memcpy(&key, record, sizeof(key));
  return key;
}

void Sort(std::span<void*> entries) {
  if (entries.size() < 2) return;
  std::swap(entries.front(), entries[(entries.size() - 1) / 2]);
  std::size_t boundary = 0;
  for (std::size_t index = 1; index < entries.size(); ++index) {
    if (Key(entries[index]) > Key(entries.front())) {
      std::swap(entries[++boundary], entries[index]);
    }
  }
  std::swap(entries.front(), entries[boundary]);
  Sort(entries.first(boundary));
  Sort(entries.subspan(boundary + 1));
}
}  // namespace

// decomp: func_800E6974 / L800E6958 (src/BATTLE/BATTLE.PRG/7E05C.s:114).
// Keep midpoint partitioning: a stable sort changes equal-key identities.
void Core_SortPointersByDescendingKey(void** values, int first, int last) {
  Require(first >= 0 && first <= kMaximumEntries && last >= -1 &&
              last < kMaximumEntries,
          "Pointer-sort indices exceed the original byte-index range");
  if (first >= last) return;
  Require(values != nullptr, "Pointer sort requires an array");
  std::span<void*> entries(values + first,
                           static_cast<std::size_t>(last - first + 1));
  Require(std::ranges::all_of(
              entries, [](const void* record) { return record != nullptr; }),
          "Pointer sort contains a null record");
  Sort(entries);
}
