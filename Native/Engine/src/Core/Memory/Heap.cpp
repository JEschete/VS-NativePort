// Hand-maintained (tools/port/owned.csv).
// The SLUS_010.40 game heap (main.c:8884-9020) with its block tables kept out
// of the arena (Heap.hpp).
#include "Core/Memory/Heap.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>
#include <span>
#include <vector>

namespace VS::Core::Memory {
namespace {
// Every block starts with one header unit; main.c:9014 keeps the arena's last
// unit out of the first free block.
constexpr std::uint32_t kHeaderUnits = 1;
constexpr std::uint32_t kSlackUnits = 1;
constexpr std::size_t kMinimumArenaUnits = kHeaderUnits + kSlackUnits;

// The header unit of an allocated block: "VSHP", the block, "GARD".
constexpr std::uint32_t kGuardHead = 0x50485356;
constexpr std::uint32_t kGuardTail = 0x44524147;
constexpr std::size_t kGuardWords = kHeapUnitBytes / sizeof(std::uint32_t);
using GuardBytes = std::array<std::byte, kHeapUnitBytes>;

GuardBytes Guard(const HeapBlock& block) noexcept {
  const std::array<std::uint32_t, kGuardWords> words{kGuardHead, block.start,
                                                     block.units, kGuardTail};
  return std::bit_cast<GuardBytes>(words);
}

std::size_t UnitOffset(std::uint32_t unit) noexcept {
  return static_cast<std::size_t>(unit) * kHeapUnitBytes;
}
}  // namespace

struct HeapAccess {
  static std::vector<HeapBlock>& Free(Heap& heap) noexcept {
    return heap.free_;
  }
  static std::vector<HeapBlock>& Allocated(Heap& heap) noexcept {
    return heap.allocated_;
  }
  static std::span<std::byte> Arena(const Heap& heap) noexcept {
    return heap.arena_;
  }
  static void Reset(Heap& heap, std::span<std::byte> arena,
                    std::uint32_t usable_units) {
    heap.arena_ = arena;
    heap.usable_units_ = usable_units;
    heap.free_.assign({HeapBlock{.start = 0, .units = usable_units}});
    heap.allocated_.clear();
  }
};

namespace {
// The index of the allocated block whose payload starts at `payload`.
std::expected<std::size_t, HeapError> Locate(const Heap& heap,
                                             const std::byte* address) {
  const std::span<std::byte> arena = HeapAccess::Arena(heap);
  const std::less<const std::byte*> before;
  if (address == nullptr || arena.empty() || before(address, arena.data()) ||
      !before(address, std::to_address(arena.end()))) {
    return std::unexpected(HeapError::kOutsideArena);
  }
  const auto offset = static_cast<std::size_t>(address - arena.data());
  if (offset % kHeapUnitBytes != 0 || offset < kHeapUnitBytes) {
    return std::unexpected(HeapError::kNotAllocated);
  }
  const auto header =
      static_cast<std::uint32_t>(offset / kHeapUnitBytes - kHeaderUnits);
  const std::span<const HeapBlock> allocated = heap.allocated_blocks();
  const auto block =
      std::ranges::lower_bound(allocated, header, {}, &HeapBlock::start);
  if (block == allocated.end() || block->start != header) {
    return std::unexpected(HeapError::kNotAllocated);
  }
  return static_cast<std::size_t>(block - allocated.begin());
}

// Records the block, guards its header unit and returns `bytes` of its
// payload.
std::span<std::byte> Commit(Heap& heap, const HeapBlock& block,
                            std::size_t bytes) {
  auto& allocated = HeapAccess::Allocated(heap);
  allocated.insert(
      std::ranges::lower_bound(allocated, block.start, {}, &HeapBlock::start),
      block);
  const std::span<std::byte> arena = HeapAccess::Arena(heap);
  const GuardBytes guard = Guard(block);
  std::ranges::copy(guard, arena.subspan(UnitOffset(block.start)).begin());
  return arena.subspan(UnitOffset(block.start + kHeaderUnits), bytes);
}

// main.c:8884/8914: both frees insert the block in address order and
// coalesce it with the following free block, then the preceding one. They
// searched the free list from opposite ends and find the same neighbours.
std::expected<HeapRelease, HeapError> Release(Heap& heap,
                                              const std::byte* payload) {
  const auto index = Locate(heap, payload);
  if (!index) {
    return std::unexpected(index.error());
  }
  auto& allocated = HeapAccess::Allocated(heap);
  const auto position = allocated.begin() + static_cast<std::ptrdiff_t>(*index);
  const HeapBlock released = *position;
  allocated.erase(position);
  const HeapRelease result{
      .units = released.units,
      .guard_intact = std::ranges::equal(
          Guard(released), HeapAccess::Arena(heap).subspan(
                               UnitOffset(released.start), kHeapUnitBytes))};

  auto& free = HeapAccess::Free(heap);
  auto next =
      std::ranges::upper_bound(free, released.start, {}, &HeapBlock::start);
  HeapBlock merged = released;
  if (next != free.end() && merged.start + merged.units == next->start) {
    merged.units += next->units;
    next = free.erase(next);
  }
  if (next != free.begin()) {
    HeapBlock& previous = *std::prev(next);
    if (previous.start + previous.units == merged.start) {
      previous.units += merged.units;
      return result;
    }
  }
  free.insert(next, merged);
  return result;
}
}  // namespace

const char* HeapErrorName(HeapError error) noexcept {
  switch (error) {
    case HeapError::kArenaTooSmall:
      return "arena too small";
    case HeapError::kArenaTooLarge:
      return "arena too large";
    case HeapError::kOversized:
      return "oversized request";
    case HeapError::kExhausted:
      return "exhausted";
    case HeapError::kOutsideArena:
      return "outside the arena";
    case HeapError::kNotAllocated:
      return "not an allocated block";
  }
  return "unknown heap error";
}

std::expected<std::uint32_t, HeapError> AllocationUnits(
    std::size_t bytes) noexcept {
  // main.c:8950/8981 round up with ((size + 15) >> 4) + 1 in 32 bits, which
  // wraps for the largest sizes; here the count is exact or refused.
  const std::size_t payload_units =
      bytes / kHeapUnitBytes + (bytes % kHeapUnitBytes == 0 ? 0 : 1);
  if (payload_units >= std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected(HeapError::kOversized);
  }
  return static_cast<std::uint32_t>(payload_units) + kHeaderUnits;
}

std::expected<void, HeapError> InitializeHeap(Heap& heap,
                                              std::span<std::byte> arena) {
  void* start = arena.data();
  std::size_t space = arena.size();
  if (arena.empty() ||
      std::align(kHeapUnitBytes, kHeapUnitBytes, start, space) == nullptr) {
    return std::unexpected(HeapError::kArenaTooSmall);
  }
  const std::size_t units = space / kHeapUnitBytes;
  if (units < kMinimumArenaUnits) {
    return std::unexpected(HeapError::kArenaTooSmall);
  }
  if (units > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected(HeapError::kArenaTooLarge);
  }
  // main.c:9014 node->blockSz = value / 16 - 1.
  HeapAccess::Reset(heap,
                    arena.subspan(arena.size() - space, UnitOffset(units)),
                    static_cast<std::uint32_t>(units) - kSlackUnits);
  return {};
}

std::expected<std::span<std::byte>, HeapError> AllocateFromLowEnd(
    Heap& heap, std::size_t bytes) {
  const auto units = AllocationUnits(bytes);
  if (!units) {
    return std::unexpected(units.error());
  }
  // main.c:8974: the first block from the low end that fits; the allocation
  // keeps its low units and the remainder stays free after it.
  auto& free = HeapAccess::Free(heap);
  const auto fit = std::ranges::find_if(
      free, [&](const HeapBlock& block) { return block.units >= *units; });
  if (fit == free.end()) {
    return std::unexpected(HeapError::kExhausted);
  }
  const HeapBlock allocation{.start = fit->start, .units = *units};
  if (fit->units == *units) {
    free.erase(fit);
  } else {
    fit->start += *units;
    fit->units -= *units;
  }
  return Commit(heap, allocation, bytes);
}

std::expected<std::span<std::byte>, HeapError> AllocateFromHighEnd(
    Heap& heap, std::size_t bytes) {
  const auto units = AllocationUnits(bytes);
  if (!units) {
    return std::unexpected(units.error());
  }
  // main.c:8944: the first block from the high end that fits; the allocation
  // takes its high units and the free block keeps the rest.
  auto& free = HeapAccess::Free(heap);
  const auto fit = std::ranges::find_if(
      free.rbegin(), free.rend(),
      [&](const HeapBlock& block) { return block.units >= *units; });
  if (fit == free.rend()) {
    return std::unexpected(HeapError::kExhausted);
  }
  const HeapBlock allocation{.start = fit->start + fit->units - *units,
                             .units = *units};
  if (fit->units == *units) {
    free.erase(std::next(fit).base());
  } else {
    fit->units -= *units;
  }
  return Commit(heap, allocation, bytes);
}

std::expected<HeapRelease, HeapError> FreeFromLowEnd(Heap& heap,
                                                     const std::byte* payload) {
  return Release(heap, payload);
}

std::expected<HeapRelease, HeapError> FreeFromHighEnd(
    Heap& heap, const std::byte* payload) {
  return Release(heap, payload);
}

std::expected<std::size_t, HeapError> PayloadCapacity(
    const Heap& heap, const std::byte* payload) {
  const auto index = Locate(heap, payload);
  if (!index) {
    return std::unexpected(index.error());
  }
  return UnitOffset(heap.allocated_blocks()[*index].units - kHeaderUnits);
}

std::expected<std::span<const std::byte>, HeapError> PayloadContaining(
    const Heap& heap, const std::byte* address) {
  const std::span<std::byte> arena = HeapAccess::Arena(heap);
  const std::less<const std::byte*> before;
  if (address == nullptr || arena.empty() || before(address, arena.data()) ||
      !before(address, std::to_address(arena.end()))) {
    return std::unexpected(HeapError::kOutsideArena);
  }
  const auto unit = static_cast<std::uint32_t>(
      static_cast<std::size_t>(address - arena.data()) / kHeapUnitBytes);
  const std::span<const HeapBlock> allocated = heap.allocated_blocks();
  const auto after =
      std::ranges::upper_bound(allocated, unit, {}, &HeapBlock::start);
  if (after == allocated.begin()) {
    return std::unexpected(HeapError::kNotAllocated);
  }
  const HeapBlock& block = *std::prev(after);
  if (unit < block.start + kHeaderUnits || unit >= block.start + block.units) {
    return std::unexpected(HeapError::kNotAllocated);
  }
  return std::span<const std::byte>(arena).subspan(
      UnitOffset(block.start + kHeaderUnits),
      UnitOffset(block.units - kHeaderUnits));
}

std::uint32_t Heap::free_units() const noexcept {
  return std::accumulate(free_.begin(), free_.end(), std::uint32_t{0},
                         [](std::uint32_t total, const HeapBlock& block) {
                           return total + block.units;
                         });
}

std::uint32_t Heap::largest_free_units() const noexcept {
  const auto largest = std::ranges::max_element(free_, {}, &HeapBlock::units);
  return largest == free_.end() ? 0 : largest->units;
}

bool Heap::empty() const noexcept { return allocated_.empty(); }
}  // namespace VS::Core::Memory
