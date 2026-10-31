#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace VS::Core {
// A signed sort key and the caller's position for the element it describes.
struct SortKey {
  std::int32_t key;
  std::size_t position;
};

// Orders keys descending with the original midpoint partition, whose tie order
// callers depend on. Refuses more than 256 keys, the original byte index range,
// and leaves them unchanged.
[[nodiscard]] bool SortPointersByDescendingKey(std::span<SortKey> keys);
}  // namespace VS::Core
