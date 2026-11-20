#include "Core/Arena.hpp"

#include <array>
#include <cstddef>
#include <span>

namespace VS::Core {

namespace {
constexpr std::size_t kArenaAlignment = 64;
alignas(kArenaAlignment) std::array<std::byte, kGameHeapSize> gameHeap;
}  // namespace

alignas(kArenaAlignment) std::array<std::byte, kOverlaySize> overlayArena;

std::span<std::byte, kGameHeapSize> GameHeapArena() noexcept {
  return gameHeap;
}

}  // namespace VS::Core
