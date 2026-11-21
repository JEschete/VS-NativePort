#pragma once

#include <cstdint>

namespace VS::Content {

// An asset resolved in one opened catalog (AssetCatalog.hpp). Generation 0 is
// no asset; a handle from another catalog instance (a reopened or overridden
// store) no longer resolves, so it cannot read a different file.
struct AssetHandle {
  std::uint32_t index = 0;
  std::uint32_t generation = 0;

  [[nodiscard]] constexpr bool Valid() const noexcept {
    return generation != 0;
  }
  friend constexpr bool operator==(const AssetHandle&,
                                   const AssetHandle&) = default;
};

}  // namespace VS::Content
