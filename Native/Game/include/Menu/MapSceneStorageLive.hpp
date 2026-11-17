#pragma once

#include <cstddef>
#include <optional>
#include <span>

namespace VS::Menu::map {
// The loaded ARM allocation's extent, independent of the selected scene
// while the next file is in flight (#426). Ownership remains with its loader.
void BindLiveMapScene(void* scene, std::size_t bytes) noexcept;
void ForgetLiveMapScene(const void* scene) noexcept;
[[nodiscard]] std::optional<std::span<std::byte>> BoundLiveMapScene(
    const void* scene) noexcept;
void ClearLiveMapTextBorrow(std::span<const std::byte> bytes);
}  // namespace VS::Menu::map
