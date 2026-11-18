#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace VS::Core {

// The memory the game's own heap (Memory::GameHeap, Core/Memory/HeapLive.hpp)
// manages. On the PS1 it was a fixed RAM range (0x8010C000, 0xF2000 bytes);
// natively it is one aligned block owned by Core. It is larger than the
// original because the structs allocated from it hold 8-byte pointers on x64;
// the heap's block tables live outside it.
inline constexpr std::size_t kGameHeapSize = std::size_t{16} * 1024 * 1024;

[[nodiscard]] std::span<std::byte, kGameHeapSize> GameHeapArena() noexcept;

// The RAM the PS1 loaded overlay modules into (0x80068800 up to the heap at
// 0x8010C000). The modules are compiled in natively, but the game also uses the
// slots as load buffers (effect data, menu data) and some slots overlap, so the
// slots keep their original offsets inside one block.
inline constexpr std::uint32_t kOverlayBase = 0x80068800u;
inline constexpr std::uint32_t kOverlayEnd = 0x8010C000u;
inline constexpr std::size_t kOverlaySize = kOverlayEnd - kOverlayBase;

// A named object rather than an accessor so slot tables holding addresses in
// it (VS::Core::kOverlaySlots) stay constant-initialized.
extern std::array<std::byte, kOverlaySize> overlay_arena;

// The native address of a PS1 overlay-RAM address.
[[nodiscard]] constexpr void* OverlayAddress(
    std::uint32_t psx_address) noexcept {
  return overlay_arena.data() + (psx_address - kOverlayBase);
}

}  // namespace VS::Core
