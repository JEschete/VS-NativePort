#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <expected>
#include <source_location>
#include <span>

#include "Core/Memory/Heap.hpp"

// Live binding for Heap.hpp: the game heap the copied code allocates from,
// spanning Core's arena (Core/Arena.h). An allocation the heap cannot satisfy
// returns null, as the original did; a free the heap refuses (null, outside
// the arena, not an allocated block, already freed) leaves the heap unchanged.
// Both, and a damaged block guard, are reported with the caller's site.
namespace VS::Core::Memory {
// The game heap. Uninitialized until ResetLiveGameHeap.
[[nodiscard]] Heap& GameHeap() noexcept;
// main.c:8309 and the overlay start-ups: a fresh heap over Core's arena.
void ResetLiveGameHeap();

void* AllocateLiveFromLowEnd(
    std::size_t bytes,
    std::source_location site = std::source_location::current());
void* AllocateLiveFromHighEnd(
    std::size_t bytes,
    std::source_location site = std::source_location::current());
void FreeLiveFromLowEnd(
    const void* payload,
    std::source_location site = std::source_location::current());
void FreeLiveFromHighEnd(
    const void* payload,
    std::source_location site = std::source_location::current());
// The bytes a live game heap payload can hold. It refuses the pointers the
// frees refuse, without counting or reporting them.
[[nodiscard]] std::expected<std::size_t, HeapError> LivePayloadCapacity(
    const void* payload);

// Allocations refused and frees rejected since start-up.
[[nodiscard]] std::uint64_t RefusedLiveHeapRequestCount() noexcept;
// Frees that found the block's guard unit overwritten.
[[nodiscard]] std::uint64_t DamagedLiveHeapGuardCount() noexcept;

// Swaps in a heap over `arena` for its lifetime and restores the game heap
// after it (tests and tools).
class ScopedLiveGameHeap {
 public:
  explicit ScopedLiveGameHeap(std::span<std::byte> arena);
  ~ScopedLiveGameHeap();
  ScopedLiveGameHeap(const ScopedLiveGameHeap&) = delete;
  ScopedLiveGameHeap& operator=(const ScopedLiveGameHeap&) = delete;
  ScopedLiveGameHeap(ScopedLiveGameHeap&&) = delete;
  ScopedLiveGameHeap& operator=(ScopedLiveGameHeap&&) = delete;

 private:
  Heap saved_;
};

// The module symbol map's entries for the original addresses.
void* AllocateLiveFromLowEndAt(std::uint32_t bytes);
void* AllocateLiveFromHighEndAt(std::uint32_t bytes);
void FreeLiveFromLowEndAt(void* payload);
void FreeLiveFromHighEndAt(void* payload);
// The original took any RAM range; the range is trusted for `bytes` and
// becomes the game heap.
void InitializeLiveHeapAt(void* arena, std::uint32_t bytes);
}  // namespace VS::Core::Memory
