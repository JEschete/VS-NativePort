// Hand-maintained (tools/port/owned.csv).
// Live binding for the game heap (Heap.cpp).
#include "Core/Memory/HeapLive.hpp"

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <source_location>
#include <span>
#include <utility>

#include "Core/Arena.h"
#include "Core/Memory/Heap.hpp"

extern "C" {
#include "Core/Diagnostics.h"
}

namespace VS::Core::Memory {
namespace {
std::uint64_t refused_requests = 0;
std::uint64_t damaged_guards = 0;

// Reports the first occurrence and then every power of two.
bool Reportable(std::uint64_t count) noexcept {
  return (count & (count - 1)) == 0;
}

void* Allocated(const char* operation, std::size_t bytes,
                const std::expected<std::span<std::byte>, HeapError>& payload,
                const std::source_location& site) {
  if (payload) {
    return payload->data();
  }
  ++refused_requests;
  if (Reportable(refused_requests)) {
    Core_Errorf(
        "memory",
        "%s at %s:%u: %zu bytes refused: %s (largest free block %" PRIu32
        " units, refusal %" PRIu64 ")",
        operation, site.file_name(), static_cast<unsigned>(site.line()), bytes,
        HeapErrorName(payload.error()), GameHeap().largest_free_units(),
        refused_requests);
  }
  return nullptr;
}

void Released(const char* operation, const void* payload,
              const std::expected<HeapRelease, HeapError>& release,
              const std::source_location& site) {
  if (!release) {
    ++refused_requests;
    if (Reportable(refused_requests)) {
      Core_Errorf("memory",
                  "%s at %s:%u: %p refused: %s; the heap is unchanged "
                  "(refusal %" PRIu64 ")",
                  operation, site.file_name(),
                  static_cast<unsigned>(site.line()), payload,
                  HeapErrorName(release.error()), refused_requests);
    }
    return;
  }
  if (!release->guard_intact) {
    ++damaged_guards;
    if (Reportable(damaged_guards)) {
      Core_Errorf("memory",
                  "%s at %s:%u: the header unit before %p (%" PRIu32
                  " units) was overwritten; the block was released "
                  "(damaged guard %" PRIu64 ")",
                  operation, site.file_name(),
                  static_cast<unsigned>(site.line()), payload, release->units,
                  damaged_guards);
    }
  }
}

void Initialized(const char* operation,
                 const std::expected<void, HeapError>& initialized) {
  if (!initialized) {
    Core_Errorf("memory", "%s: %s; the game heap is empty", operation,
                HeapErrorName(initialized.error()));
  }
}
}  // namespace

Heap& GameHeap() noexcept {
  static Heap heap;
  return heap;
}

void ResetLiveGameHeap() {
  Initialized(
      "ResetLiveGameHeap",
      InitializeHeap(
          GameHeap(),
          std::span<std::byte>(static_cast<std::byte*>(Core_GameHeapArena()),
                               static_cast<std::size_t>(CORE_GAME_HEAP_SIZE))));
}

void* AllocateLiveFromLowEnd(std::size_t bytes, std::source_location site) {
  return Allocated("AllocateFromLowEnd", bytes,
                   AllocateFromLowEnd(GameHeap(), bytes), site);
}

void* AllocateLiveFromHighEnd(std::size_t bytes, std::source_location site) {
  return Allocated("AllocateFromHighEnd", bytes,
                   AllocateFromHighEnd(GameHeap(), bytes), site);
}

// Generated callers hold untyped payload pointers; Locate validates them
// against the allocated block table before anything is released.
void FreeLiveFromLowEnd(const void* payload, std::source_location site) {
  Released("FreeFromLowEnd", payload,
           FreeFromLowEnd(GameHeap(), static_cast<const std::byte*>(payload)),
           site);
}

void FreeLiveFromHighEnd(const void* payload, std::source_location site) {
  Released("FreeFromHighEnd", payload,
           FreeFromHighEnd(GameHeap(), static_cast<const std::byte*>(payload)),
           site);
}

std::expected<std::size_t, HeapError> LivePayloadCapacity(const void* payload) {
  return PayloadCapacity(GameHeap(), static_cast<const std::byte*>(payload));
}

std::uint64_t RefusedLiveHeapRequestCount() noexcept {
  return refused_requests;
}

std::uint64_t DamagedLiveHeapGuardCount() noexcept { return damaged_guards; }

ScopedLiveGameHeap::ScopedLiveGameHeap(std::span<std::byte> arena)
    : saved_(std::exchange(GameHeap(), Heap{})) {
  Initialized("ScopedLiveGameHeap", InitializeHeap(GameHeap(), arena));
}

ScopedLiveGameHeap::~ScopedLiveGameHeap() { GameHeap() = std::move(saved_); }

void* AllocateLiveFromLowEndAt(std::uint32_t bytes) {
  return AllocateLiveFromLowEnd(bytes);
}

void* AllocateLiveFromHighEndAt(std::uint32_t bytes) {
  return AllocateLiveFromHighEnd(bytes);
}

void FreeLiveFromLowEndAt(void* payload) { FreeLiveFromLowEnd(payload); }

void FreeLiveFromHighEndAt(void* payload) { FreeLiveFromHighEnd(payload); }

void InitializeLiveHeapAt(void* arena, std::uint32_t bytes) {
  if (arena == nullptr) {
    Initialized("InitializeLiveHeapAt",
                std::unexpected(HeapError::kArenaTooSmall));
    GameHeap() = Heap{};
    return;
  }
  Initialized(
      "InitializeLiveHeapAt",
      InitializeHeap(GameHeap(), std::span<std::byte>(
                                     static_cast<std::byte*>(arena), bytes)));
}
}  // namespace VS::Core::Memory
