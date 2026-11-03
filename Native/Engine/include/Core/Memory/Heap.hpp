#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

// The SLUS_010.40 game heap (main.c:8884-9020): first-fit allocation from
// either end of one arena, in 16-byte units, where every block spends one unit
// on a header before its payload. Blocks are placed, split and coalesced
// exactly as the original places them.
//
// The original kept its free list inside the arena (vs_main_HeapHeader,
// main.h:142: two links, the block's units and a reserved word), so a payload
// overrun or a stray free corrupted the allocator. Natively the free and
// allocated block tables live outside the arena, a free must name a block this
// heap allocated, and the header unit holds a guard that a free checks.
namespace VS::Core::Memory {

inline constexpr std::size_t kHeapUnitBytes = 16;

// A run of units, counted from the start of the (16-byte aligned) arena. The
// first unit is the block's header; the payload follows it.
struct HeapBlock {
  std::uint32_t start = 0;
  std::uint32_t units = 0;

  friend bool operator==(const HeapBlock&, const HeapBlock&) = default;
};

enum class HeapError : std::uint8_t {
  kArenaTooSmall,  // fewer than two units once aligned
  kArenaTooLarge,  // more units than 32-bit block positions address
  kOversized,      // the request needs more units than 32 bits count
  kExhausted,      // no free block is large enough
  kOutsideArena,   // the pointer is not inside this heap's arena
  kNotAllocated,   // the pointer is not the payload of an allocated block
};

[[nodiscard]] const char* HeapErrorName(HeapError error) noexcept;

// What a free released. A damaged guard means something wrote over the
// block's header unit (the previous block ran past its end, or this one was
// written before its start); the block is still released.
struct HeapRelease {
  std::uint32_t units = 0;
  bool guard_intact = true;
};

class Heap;

// main.c:9008 vs_main_initHeap: one free block over the whole arena but its
// last unit. The arena is aligned up to a unit; the heap does not own it.
std::expected<void, HeapError> InitializeHeap(Heap& heap,
                                              std::span<std::byte> arena);
// main.c:8974 vs_main_allocHeap: the first free block from the low end that
// fits; the payload takes its low units and the remainder stays free.
std::expected<std::span<std::byte>, HeapError> AllocateFromLowEnd(
    Heap& heap, std::size_t bytes);
// main.c:8944 vs_main_allocHeapR: the first free block from the high end that
// fits; the payload takes its high units.
std::expected<std::span<std::byte>, HeapError> AllocateFromHighEnd(
    Heap& heap, std::size_t bytes);
// main.c:8914 vs_main_freeHeap and main.c:8884 vs_main_freeHeapR: release a
// payload and coalesce it with free neighbours. The originals searched the
// address-ordered free list from opposite ends; both find the same
// neighbours.
std::expected<HeapRelease, HeapError> FreeFromLowEnd(Heap& heap,
                                                     const std::byte* payload);
std::expected<HeapRelease, HeapError> FreeFromHighEnd(Heap& heap,
                                                      const std::byte* payload);

// The bytes an allocated payload can hold: its units but the header.
[[nodiscard]] std::expected<std::size_t, HeapError> PayloadCapacity(
    const Heap& heap, const std::byte* payload);
// The whole payload of the allocated block that holds `address`, which may
// point anywhere inside it. Header units and free blocks hold no payload.
[[nodiscard]] std::expected<std::span<const std::byte>, HeapError>
PayloadContaining(const Heap& heap, const std::byte* address);
// The units a request for `bytes` takes, header included.
[[nodiscard]] std::expected<std::uint32_t, HeapError> AllocationUnits(
    std::size_t bytes) noexcept;

class Heap {
 public:
  // An uninitialized heap has no arena: allocations report kExhausted and
  // frees kOutsideArena.
  Heap() = default;

  [[nodiscard]] bool initialized() const noexcept { return !arena_.empty(); }
  // The units the first free block held: the arena but its last unit.
  [[nodiscard]] std::uint32_t usable_units() const noexcept {
    return usable_units_;
  }
  // Ascending, never adjacent (adjacent free blocks are coalesced).
  [[nodiscard]] std::span<const HeapBlock> free_blocks() const noexcept {
    return free_;
  }
  // Ascending.
  [[nodiscard]] std::span<const HeapBlock> allocated_blocks() const noexcept {
    return allocated_;
  }
  [[nodiscard]] std::uint32_t free_units() const noexcept;
  [[nodiscard]] std::uint32_t largest_free_units() const noexcept;
  // No block is allocated (the free blocks have then coalesced into one).
  [[nodiscard]] bool empty() const noexcept;

 private:
  friend struct HeapAccess;

  std::span<std::byte> arena_;
  std::uint32_t usable_units_ = 0;
  std::vector<HeapBlock> free_;
  std::vector<HeapBlock> allocated_;
};
}  // namespace VS::Core::Memory
