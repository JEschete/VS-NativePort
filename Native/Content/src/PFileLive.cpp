// Hand-maintained (tools/port/owned.csv).
// Live binding for the P-file timeline lookups (PFile.hpp): the bound timeline
// block is bounded by its game heap payload or by the built-in package.
#include "Content/PFileLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>

#include "Content/PFile.hpp"
#include "Core/Diagnostics.h"
#include "Core/Memory/HeapLive.hpp"

extern "C" {
#include "Effects/Package.h"
#include "Effects/Runtime.h"
}

namespace VS::Content {
namespace {
static_assert(sizeof(Effects_PackageBlock3) == kPFileTimelineHeaderBytes);
static_assert(offsetof(Effects_PackageBlock3, offsets) ==
              kPFileTimelineHeaderBytes);

std::optional<std::span<std::byte>> Within(std::byte* at, std::byte* base,
                                           std::size_t bytes) {
  const auto address = reinterpret_cast<std::uintptr_t>(at);
  const auto start = reinterpret_cast<std::uintptr_t>(base);
  if (address < start || address - start >= bytes) return std::nullopt;
  return std::span(at, bytes - (address - start));
}

// The timeline block from `block` to the end of the storage that holds it.
std::optional<std::span<std::byte>> LiveTimelineBlock(
    Effects_PackageBlock3* block) {
  if (block == nullptr) return std::nullopt;
  auto* const at = reinterpret_cast<std::byte*>(block);
  if (const auto payload = Core::Memory::LivePayloadContaining(at)) {
    // The payload is the caller's own writable heap block.
    auto* const base = const_cast<std::byte*>(payload->data());
    return Within(at, base, payload->size());
  }
  return Within(at,
                reinterpret_cast<std::byte*>(std::data(Effects_emptyPackage)),
                sizeof Effects_emptyPackage);
}

std::byte* Resolve(std::span<std::byte> block,
                   std::optional<std::size_t> offset, const char* operation,
                   int section, int track) {
  if (!offset) {
    Core_Warnf("content",
               "%s refused: section %d track %d is outside the %zu-byte "
               "timeline block",
               operation, section, track, block.size());
    return nullptr;
  }
  return block.data() + *offset;
}
}  // namespace

std::byte* PFileLiveTimelineSection(Effects_PackageBlock3* block, int section) {
  const auto bytes = LiveTimelineBlock(block);
  if (!bytes || section < 0) {
    Core_Warnf("content",
               "PFileTimelineSection refused: section %d of an unbounded "
               "timeline block %p",
               section, static_cast<void*>(block));
    return nullptr;
  }
  return Resolve(
      *bytes, PFileTimelineSection(*bytes, static_cast<std::size_t>(section)),
      "PFileTimelineSection", section, -1);
}

std::byte* PFileLiveTimelineTrackData(Effects_PackageBlock3* block, int section,
                                      int track) {
  const auto bytes = LiveTimelineBlock(block);
  if (!bytes || section < 0 || track < 0) {
    Core_Warnf("content",
               "PFileTimelineTrackData refused: section %d track %d of an "
               "unbounded timeline block %p",
               section, track, static_cast<void*>(block));
    return nullptr;
  }
  return Resolve(
      *bytes,
      PFileTimelineTrackData(*bytes, static_cast<std::size_t>(section),
                             static_cast<std::size_t>(track)),
      "PFileTimelineTrackData", section, track);
}

}  // namespace VS::Content
