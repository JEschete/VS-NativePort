#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Memory/Heap.hpp"
#include "Core/Memory/HeapLive.hpp"

namespace {
using VS::Core::Memory::AllocateFromHighEnd;
using VS::Core::Memory::AllocateFromLowEnd;
using VS::Core::Memory::FreeFromHighEnd;
using VS::Core::Memory::FreeFromLowEnd;
using VS::Core::Memory::Heap;
using VS::Core::Memory::HeapBlock;
using VS::Core::Memory::HeapError;
using VS::Core::Memory::InitializeHeap;
using VS::Core::Memory::kHeapUnitBytes;

constexpr std::size_t kArenaUnits = 256;

struct Arena {
  alignas(kHeapUnitBytes)
      std::array<std::byte, kArenaUnits * kHeapUnitBytes> bytes{};
  Heap heap;

  Arena() { REQUIRE(InitializeHeap(heap, bytes).has_value()); }

  [[nodiscard]] std::uint32_t UnitOf(const std::byte* payload) const {
    return static_cast<std::uint32_t>((payload - bytes.data()) /
                                      kHeapUnitBytes) -
           1;
  }
};

std::vector<HeapBlock> Blocks(std::span<const HeapBlock> blocks) {
  return {blocks.begin(), blocks.end()};
}

// main.c:8884-9020 as written: an address-ordered circular free list whose
// sentinel (heapA) sits below the arena, every node in its block's first
// unit. Units are block positions; the sentinel is -1.
class ReferenceHeap {
 public:
  explicit ReferenceHeap(int usable_units) {
    nodes_[kSentinel] = {.prev = 0, .next = 0, .units = 0};
    nodes_[0] = {.prev = kSentinel, .next = kSentinel, .units = usable_units};
  }

  // vs_main_allocHeap
  std::optional<int> AllocateLow(int units) {
    int previous = kSentinel;
    int block = nodes_[kSentinel].next;
    while (true) {
      if (nodes_[block].units >= units) {
        if (nodes_[block].units == units) {
          nodes_[previous].next = nodes_[block].next;
          nodes_[nodes_[block].next].prev = nodes_[block].prev;
        } else {
          const int remainder = block + units;
          nodes_[remainder] = {.prev = nodes_[block].prev,
                               .next = nodes_[block].next,
                               .units = nodes_[block].units - units};
          nodes_[nodes_[remainder].prev].next = remainder;
          nodes_[nodes_[remainder].next].prev = remainder;
          nodes_[block].units = units;
        }
        return block;
      }
      previous = block;
      if (block == kSentinel) {
        return std::nullopt;
      }
      block = nodes_[block].next;
    }
  }

  // vs_main_allocHeapR
  std::optional<int> AllocateHigh(int units) {
    int following = kSentinel;
    int block = nodes_[kSentinel].prev;
    while (true) {
      if (nodes_[block].units >= units) {
        if (nodes_[block].units == units) {
          nodes_[following].prev = nodes_[block].prev;
          nodes_[nodes_[block].prev].next = nodes_[block].next;
        } else {
          nodes_[block].units -= units;
          block += nodes_[block].units;
          nodes_[block].units = units;
        }
        return block;
      }
      following = block;
      if (block == kSentinel) {
        return std::nullopt;
      }
      block = nodes_[block].prev;
    }
  }

  // vs_main_freeHeap (forward) and vs_main_freeHeapR (backward)
  void Free(int target, bool backward) {
    int at = backward ? nodes_[kSentinel].prev : nodes_[kSentinel].next;
    for (; at >= target || target >= nodes_[at].next;
         at = backward ? nodes_[at].prev : nodes_[at].next) {
      if (at >= nodes_[at].next && (at < target || target < nodes_[at].next)) {
        break;
      }
    }
    if (target + nodes_[target].units == nodes_[at].next) {
      nodes_[target].units += nodes_[nodes_[at].next].units;
      nodes_[target].next = nodes_[nodes_[at].next].next;
      nodes_[nodes_[target].next].prev = target;
    } else {
      nodes_[target].next = nodes_[at].next;
      nodes_[nodes_[target].next].prev = target;
    }
    if (at + nodes_[at].units == target) {
      nodes_[at].units += nodes_[target].units;
      nodes_[at].next = nodes_[target].next;
      nodes_[nodes_[at].next].prev = at;
      return;
    }
    nodes_[at].next = target;
    nodes_[target].prev = at;
  }

  [[nodiscard]] std::vector<HeapBlock> FreeBlocks() const {
    std::vector<HeapBlock> blocks;
    for (int block = nodes_.at(kSentinel).next; block != kSentinel;
         block = nodes_.at(block).next) {
      blocks.push_back(
          {.start = static_cast<std::uint32_t>(block),
           .units = static_cast<std::uint32_t>(nodes_.at(block).units)});
    }
    return blocks;
  }

 private:
  struct Node {
    int prev = 0;
    int next = 0;
    int units = 0;
  };
  static constexpr int kSentinel = -1;
  std::map<int, Node> nodes_;
};
std::byte* MustAllocate(Heap& heap, std::size_t bytes, bool high = false) {
  const auto allocation =
      high ? AllocateFromHighEnd(heap, bytes) : AllocateFromLowEnd(heap, bytes);
  REQUIRE(allocation.has_value());
  return allocation->data();
}

// A heap with one live block and one released block next to it, for frees
// that must be refused without changing either table.
struct StrayFreeFixture {
  Arena arena;
  std::byte* kept = MustAllocate(arena.heap, 40);
  std::byte* freed = MustAllocate(arena.heap, 40);
  std::vector<HeapBlock> free_before;
  std::vector<HeapBlock> allocated_before;

  StrayFreeFixture() {
    REQUIRE(FreeFromHighEnd(arena.heap, freed).has_value());
    free_before = Blocks(arena.heap.free_blocks());
    allocated_before = Blocks(arena.heap.allocated_blocks());
  }

  [[nodiscard]] bool Unchanged() const {
    return Blocks(arena.heap.free_blocks()) == free_before &&
           Blocks(arena.heap.allocated_blocks()) == allocated_before;
  }
};

// One step of the parity run: allocate from either end or free from either
// direction, in the heap under test and in the transcribed original.
class ParityRun {
 public:
  explicit ParityRun(std::uint32_t seed)
      : reference_(static_cast<int>(arena_.heap.usable_units())),
        random_(seed) {}

  void Step() {
    constexpr std::size_t kLargestRequest = 900;
    const auto choice = random_() % 4;
    if (choice < 2 || live_.empty()) {
      Allocate(random_() % kLargestRequest, choice == 1);
    } else {
      Free(random_() % live_.size(), choice == 3);
    }
  }

  [[nodiscard]] bool Matches() const {
    return Blocks(arena_.heap.free_blocks()) == reference_.FreeBlocks();
  }

 private:
  void Allocate(std::size_t bytes, bool high) {
    const int units =
        static_cast<int>(VS::Core::Memory::AllocationUnits(bytes).value());
    const auto expected =
        high ? reference_.AllocateHigh(units) : reference_.AllocateLow(units);
    const auto allocated = high ? AllocateFromHighEnd(arena_.heap, bytes)
                                : AllocateFromLowEnd(arena_.heap, bytes);
    REQUIRE(allocated.has_value() == expected.has_value());
    if (allocated) {
      REQUIRE(static_cast<int>(arena_.UnitOf(allocated->data())) == *expected);
      live_.emplace_back(allocated->data(), *expected);
    }
  }

  void Free(std::size_t index, bool backward) {
    const auto [payload, unit] = live_.at(index);
    live_.erase(live_.begin() + static_cast<std::ptrdiff_t>(index));
    reference_.Free(unit, backward);
    REQUIRE((backward ? FreeFromHighEnd(arena_.heap, payload)
                      : FreeFromLowEnd(arena_.heap, payload))
                .has_value());
  }

  Arena arena_;
  ReferenceHeap reference_;
  std::mt19937 random_;
  std::vector<std::pair<std::byte*, int>> live_;
};
}  // namespace

TEST_CASE("Heap initialization keeps the original's slack unit",
          "[core][memory][heap]") {
  alignas(kHeapUnitBytes) static std::array<std::byte, 0xF2000> ram{};
  Heap heap;
  REQUIRE(InitializeHeap(heap, ram).has_value());
  // main.c:9014 blockSz = 0xF2000 / 16 - 1 for the PS1 heap.
  CHECK(heap.usable_units() == 0xF2000 / 16 - 1);
  CHECK(Blocks(heap.free_blocks()) ==
        std::vector<HeapBlock>{{.start = 0, .units = 0xF2000 / 16 - 1}});
  CHECK(heap.allocated_blocks().empty());
  CHECK(heap.empty());
}

TEST_CASE("An unaligned heap arena starts at its first whole unit",
          "[core][memory][heap]") {
  alignas(kHeapUnitBytes) std::array<std::byte, 128> ram{};
  Heap heap;
  REQUIRE(InitializeHeap(heap, std::span(ram).subspan(3, 64)).has_value());
  CHECK(heap.usable_units() == 2);
  CHECK(MustAllocate(heap, 1) == ram.data() + 32);
}

TEST_CASE("Heap arenas without two whole units are refused",
          "[core][memory][heap]") {
  alignas(kHeapUnitBytes) std::array<std::byte, 128> ram{};
  Heap heap;
  REQUIRE(InitializeHeap(heap, ram).has_value());
  CHECK(InitializeHeap(heap, std::span(ram).first(31)).error() ==
        HeapError::kArenaTooSmall);
  CHECK(InitializeHeap(heap, std::span(ram).subspan(1, 32)).error() ==
        HeapError::kArenaTooSmall);
  CHECK(InitializeHeap(heap, {}).error() == HeapError::kArenaTooSmall);
  CHECK(heap.usable_units() == 7);
}

TEST_CASE("An uninitialized heap refuses every request",
          "[core][memory][heap]") {
  Heap heap;
  std::array<std::byte, 16> other{};
  CHECK_FALSE(heap.initialized());
  CHECK(AllocateFromLowEnd(heap, 0).error() == HeapError::kExhausted);
  CHECK(AllocateFromHighEnd(heap, 0).error() == HeapError::kExhausted);
  CHECK(FreeFromLowEnd(heap, other.data()).error() == HeapError::kOutsideArena);
  CHECK(heap.free_units() == 0);
  CHECK(heap.largest_free_units() == 0);
}

TEST_CASE("Heap allocations take 16-byte units plus a header unit",
          "[core][memory][heap]") {
  using VS::Core::Memory::AllocationUnits;
  CHECK(AllocationUnits(0).value() == 1);
  CHECK(AllocationUnits(1).value() == 2);
  CHECK(AllocationUnits(16).value() == 2);
  CHECK(AllocationUnits(17).value() == 3);
  // The original's 32-bit rounding wrapped these to one or two units.
  CHECK(AllocationUnits(0xFFFFFFFFu).value() == 0x10000001u);
  CHECK(AllocationUnits(std::numeric_limits<std::size_t>::max()).error() ==
        HeapError::kOversized);
}

TEST_CASE("Low-end allocations split first-fit blocks upward",
          "[core][memory][heap]") {
  Arena arena;
  CHECK(MustAllocate(arena.heap, 1) == arena.bytes.data() + 16);
  CHECK(MustAllocate(arena.heap, 16) == arena.bytes.data() + 48);
  const auto third = AllocateFromLowEnd(arena.heap, 17);
  REQUIRE(third.has_value());
  CHECK(third->data() == arena.bytes.data() + 80);
  CHECK(third->size() == 17);
  CHECK(VS::Core::Memory::PayloadCapacity(arena.heap, third->data()).value() ==
        32);
}

TEST_CASE("Low-end allocations leave the remainder above them free",
          "[core][memory][heap]") {
  Arena arena;
  MustAllocate(arena.heap, 1);
  MustAllocate(arena.heap, 16);
  MustAllocate(arena.heap, 17);
  CHECK(Blocks(arena.heap.allocated_blocks()) ==
        std::vector<HeapBlock>{{.start = 0, .units = 2},
                               {.start = 2, .units = 2},
                               {.start = 4, .units = 3}});
  CHECK(Blocks(arena.heap.free_blocks()) ==
        std::vector<HeapBlock>{{.start = 7, .units = kArenaUnits - 1 - 7}});
}

TEST_CASE("High-end allocations take the tail of the highest fit",
          "[core][memory][heap]") {
  Arena arena;
  const auto usable = arena.heap.usable_units();
  const auto first = AllocateFromHighEnd(arena.heap, 32);
  const auto second = AllocateFromHighEnd(arena.heap, 0);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  CHECK(arena.UnitOf(first->data()) == usable - 3);
  CHECK(arena.UnitOf(second->data()) == usable - 4);
  CHECK(second->empty());
  CHECK(Blocks(arena.heap.free_blocks()) ==
        std::vector<HeapBlock>{{.start = 0, .units = usable - 4}});
}

TEST_CASE("Exact fits remove the free block from either end",
          "[core][memory][heap]") {
  Arena arena;
  const bool from_high = GENERATE(false, true);
  MustAllocate(arena.heap, 64);
  std::byte* const hole = MustAllocate(arena.heap, 32);
  // 5 + 3 + 247 units fill the 255 usable ones.
  MustAllocate(arena.heap, (arena.heap.usable_units() - 9) * kHeapUnitBytes);
  CHECK(arena.heap.free_blocks().empty());
  REQUIRE(FreeFromLowEnd(arena.heap, hole).has_value());
  CHECK(Blocks(arena.heap.free_blocks()) ==
        std::vector<HeapBlock>{{.start = 5, .units = 3}});
  CHECK(MustAllocate(arena.heap, 32, from_high) == hole);
  CHECK(arena.heap.free_blocks().empty());
}

TEST_CASE("A full heap refuses even empty requests", "[core][memory][heap]") {
  Arena arena;
  MustAllocate(arena.heap, (arena.heap.usable_units() - 1) * kHeapUnitBytes);
  CHECK(arena.heap.free_blocks().empty());
  CHECK(AllocateFromLowEnd(arena.heap, 0).error() == HeapError::kExhausted);
  CHECK(AllocateFromHighEnd(arena.heap, 0).error() == HeapError::kExhausted);
}

TEST_CASE("Frees coalesce with both neighbours in any order",
          "[core][memory][heap]") {
  const bool from_high = GENERATE(false, true);
  std::array<std::size_t, 3> order{0, 1, 2};
  do {
    Arena arena;
    std::array<std::byte*, 3> payloads{};
    for (auto& payload : payloads) {
      payload = MustAllocate(arena.heap, 40);
    }
    for (const auto index : order) {
      const auto released =
          from_high ? FreeFromHighEnd(arena.heap, payloads.at(index))
                    : FreeFromLowEnd(arena.heap, payloads.at(index));
      REQUIRE(released.has_value());
      CHECK(released->units == 4);
      CHECK(released->guard_intact);
    }
    CHECK(arena.heap.empty());
    CHECK(Blocks(arena.heap.free_blocks()) ==
          std::vector<HeapBlock>{{.start = 0, .units = kArenaUnits - 1}});
  } while (std::ranges::next_permutation(order).found);
}

TEST_CASE("Heap placement matches the original free list",
          "[core][memory][heap]") {
  constexpr int kOperations = 4000;
  ParityRun run(GENERATE(1u, 0x5EEDu, 0xC0FFEEu));
  for (int operation = 0; operation < kOperations; ++operation) {
    run.Step();
    REQUIRE(run.Matches());
  }
}

TEST_CASE("Stray and repeated frees inside the arena are refused",
          "[core][memory][heap]") {
  StrayFreeFixture fixture;
  Heap& heap = fixture.arena.heap;
  CHECK(FreeFromLowEnd(heap, fixture.freed).error() ==
        HeapError::kNotAllocated);
  CHECK(FreeFromLowEnd(heap, fixture.kept + 1).error() ==
        HeapError::kNotAllocated);
  CHECK(FreeFromLowEnd(heap, fixture.kept + 16).error() ==
        HeapError::kNotAllocated);
  CHECK(FreeFromHighEnd(heap, fixture.kept - 16).error() ==
        HeapError::kNotAllocated);
  CHECK(fixture.Unchanged());
}

TEST_CASE("Frees outside the arena are refused", "[core][memory][heap]") {
  StrayFreeFixture fixture;
  Heap& heap = fixture.arena.heap;
  std::array<std::byte, 16> outside{};
  CHECK(FreeFromHighEnd(heap, nullptr).error() == HeapError::kOutsideArena);
  CHECK(FreeFromHighEnd(heap, outside.data()).error() ==
        HeapError::kOutsideArena);
  CHECK(FreeFromLowEnd(heap,
                       fixture.arena.bytes.data() + fixture.arena.bytes.size())
            .error() == HeapError::kOutsideArena);
  CHECK(fixture.Unchanged());
}

TEST_CASE("Heap payloads are not initialized", "[core][memory][heap]") {
  Arena arena;
  std::ranges::fill(arena.bytes, std::byte{0xCD});
  const auto payload = AllocateFromLowEnd(arena.heap, 20);
  REQUIRE(payload.has_value());
  CHECK(std::ranges::all_of(
      *payload, [](std::byte value) { return value == std::byte{0xCD}; }));
}

TEST_CASE("Heap overruns into the next header are detected",
          "[core][memory][heap]") {
  Arena arena;
  std::byte* const first = MustAllocate(arena.heap, 20);
  std::byte* const second = MustAllocate(arena.heap, 20);
  // Writing past the first payload's capacity reaches the second's header.
  const auto capacity =
      VS::Core::Memory::PayloadCapacity(arena.heap, first).value();
  CHECK(capacity == 32);
  arena.bytes.at(kHeapUnitBytes + capacity) = std::byte{0};
  const auto damaged = FreeFromLowEnd(arena.heap, second);
  REQUIRE(damaged.has_value());
  CHECK_FALSE(damaged->guard_intact);
  const auto intact = FreeFromLowEnd(arena.heap, first);
  REQUIRE(intact.has_value());
  CHECK(intact->guard_intact);
}

TEST_CASE("The live game heap scope nests and restores",
          "[core][memory][heap]") {
  namespace memory = VS::Core::Memory;
  alignas(kHeapUnitBytes) std::array<std::byte, 64> outer_bytes{};
  const memory::ScopedLiveGameHeap outer(outer_bytes);
  REQUIRE(memory::GameHeap().usable_units() == 3);
  {
    alignas(kHeapUnitBytes) std::array<std::byte, 256> inner_bytes{};
    const memory::ScopedLiveGameHeap inner(inner_bytes);
    CHECK(memory::GameHeap().usable_units() == 15);
    CHECK(memory::AllocateLiveFromLowEnd(8) == inner_bytes.data() + 16);
    CHECK(memory::AllocateLiveFromHighEndAt(8) ==
          inner_bytes.data() + (14 * kHeapUnitBytes));
  }
  CHECK(memory::GameHeap().usable_units() == 3);
}

TEST_CASE("The live game heap reports refused requests",
          "[core][memory][heap]") {
  namespace memory = VS::Core::Memory;
  alignas(kHeapUnitBytes) std::array<std::byte, 256> bytes{};
  alignas(kHeapUnitBytes) std::array<std::byte, 64> elsewhere{};
  const memory::ScopedLiveGameHeap scope(bytes);
  memory::AllocateLiveFromLowEnd(8);
  const auto refused = memory::RefusedLiveHeapRequestCount();
  CHECK(memory::AllocateLiveFromLowEnd(1024) == nullptr);
  memory::FreeLiveFromLowEnd(nullptr);
  memory::FreeLiveFromHighEnd(elsewhere.data() + 16);
  CHECK(memory::RefusedLiveHeapRequestCount() == refused + 3);
  CHECK(memory::GameHeap().allocated_blocks().size() == 1);
}

TEST_CASE("The live game heap counts damaged guards and still frees",
          "[core][memory][heap]") {
  namespace memory = VS::Core::Memory;
  alignas(kHeapUnitBytes) std::array<std::byte, 256> bytes{};
  const memory::ScopedLiveGameHeap scope(bytes);
  void* const low = memory::AllocateLiveFromLowEnd(8);
  void* const high = memory::AllocateLiveFromHighEndAt(8);
  const auto damaged = memory::DamagedLiveHeapGuardCount();
  bytes.at((14 * kHeapUnitBytes) - 1) = std::byte{0xFF};
  memory::FreeLiveFromHighEndAt(high);
  memory::FreeLiveFromLowEnd(low);
  CHECK(memory::DamagedLiveHeapGuardCount() == damaged + 1);
  CHECK(memory::GameHeap().empty());
  CHECK(memory::GameHeap().largest_free_units() == 15);
}
