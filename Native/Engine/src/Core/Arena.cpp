#include "Core/Arena.hpp"

#include <array>
#include <cstddef>
#include <span>

namespace VS::Core {

namespace {
constexpr std::size_t kArenaAlignment = 64;
alignas(kArenaAlignment) std::array<std::byte, kGameHeapSize> g_game_heap;
}  // namespace

alignas(kArenaAlignment) std::array<std::byte, kOverlaySize> overlay_arena;

std::span<std::byte, kGameHeapSize> GameHeapArena() noexcept {
  return g_game_heap;
}

}  // namespace VS::Core
