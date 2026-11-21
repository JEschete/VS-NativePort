// Hand-maintained (tools/port/owned.csv).
#include "Content/PFile.hpp"  // NOLINT(build/include): typed API

#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace VS::Content {
namespace {
constexpr std::size_t kOffsetBytes = 2;
constexpr std::size_t kCountBytes = 4;
constexpr std::size_t kBitsPerByte = 8;
using Fault = PFileTimelineFault;

// base + index * kStride, saturated at the largest size so that a huge index
// lands outside every block.
template <std::size_t kStride>
[[nodiscard]] std::size_t At(std::size_t base, std::size_t index) {
  static_assert(kStride != 0);
  constexpr std::size_t kLargest = std::numeric_limits<std::size_t>::max();
  if (index > (kLargest - base) / kStride) return kLargest;
  return base + (index * kStride);
}

// Whether the block holds `bytes` bytes from `at`.
[[nodiscard]] bool Holds(std::span<const std::byte> block, std::size_t at,
                         std::size_t bytes) {
  return at <= block.size() && block.size() - at >= bytes;
}

// The little-endian value of `bytes` bytes at `at`, which the block holds.
[[nodiscard]] std::uint32_t ReadLittle(std::span<const std::byte> block,
                                       std::size_t at, std::size_t bytes) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < bytes; ++i) {
    value |= std::uint32_t{std::to_integer<std::uint8_t>(block[at + i])}
             << (i * kBitsPerByte);
  }
  return value;
}

[[nodiscard]] std::unexpected<PFileTimelineError> Refuse(
    Fault fault, std::span<const std::byte> block, std::size_t section,
    std::optional<std::size_t> track, std::size_t offset, std::size_t bytes) {
  return std::unexpected(PFileTimelineError{.fault = fault,
                                            .section = section,
                                            .track = track,
                                            .offset = offset,
                                            .bytes = bytes,
                                            .block_bytes = block.size()});
}

[[nodiscard]] std::string_view Subject(Fault fault) {
  switch (fault) {
    case Fault::kHeader:
      return "count word";
    case Fault::kSectionIndex:
      return "section offset entry";
    case Fault::kSectionOffset:
      return "track count word";
    case Fault::kTrackEntry:
      return "track entry";
    case Fault::kTrackData:
      return "key data offset";
    case Fault::kTrackKeys:
    case Fault::kTrackAlignment:
      return "keys";
  }
  return "data";
}

// Track `track`'s entry in the section whose table starts at `table`.
[[nodiscard]] std::expected<PFileTimelineTrack, PFileTimelineError> EntryAt(
    std::span<const std::byte> block, std::size_t section, std::size_t table,
    std::size_t track) {
  // sll 2; lhu 6(section + track*4): the entry's data offset, from the block.
  const std::size_t entry = At<kPFileTimelineTrackEntryBytes>(
      table + kPFileTimelineHeaderBytes, track);
  if (!Holds(block, entry, kPFileTimelineTrackEntryBytes)) {
    return Refuse(Fault::kTrackEntry, block, section, track, entry,
                  kPFileTimelineTrackEntryBytes);
  }
  const std::size_t data =
      ReadLittle(block, entry + kOffsetBytes, kOffsetBytes);
  if (data >= block.size()) {
    return Refuse(Fault::kTrackData, block, section, track, data, 1);
  }
  return PFileTimelineTrack{.header = static_cast<std::uint16_t>(
                                ReadLittle(block, entry, kOffsetBytes)),
                            .data_offset = data};
}
}  // namespace

std::string Describe(const PFileTimelineError& error) {
  std::string where = "timeline";
  if (error.fault != Fault::kHeader) {
    where = error.track ? std::format("section {} track {}", error.section,
                                      *error.track)
                        : std::format("section {}", error.section);
  }
  if (error.fault == Fault::kTrackAlignment) {
    return std::format("{}: keys at {:#x} off a {}-byte boundary", where,
                       error.offset, kPFileTimelineKeyAlignment);
  }
  return std::format(
      "{}: {} at {:#x} ({} bytes) outside the {}-byte "
      "timeline block",
      where, Subject(error.fault), error.offset, error.bytes,
      error.block_bytes);
}

std::expected<std::int32_t, PFileTimelineError> PFileTimelineCount(
    std::span<const std::byte> block) {
  if (!Holds(block, 0, kCountBytes)) {
    return Refuse(Fault::kHeader, block, 0, std::nullopt, 0, kCountBytes);
  }
  return static_cast<std::int32_t>(ReadLittle(block, 0, kCountBytes));
}

// decomp: BATTLE.PRG getPFileBlock3section
// (src/BATTLE/BATTLE.PRG/5BF94.c:6868)
std::expected<std::size_t, PFileTimelineError> PFileTimelineSection(
    std::span<const std::byte> block, std::size_t section) {
  // sll 1; lhu 4(block + section*2)
  const std::size_t entry =
      At<kOffsetBytes>(kPFileTimelineHeaderBytes, section);
  if (!Holds(block, entry, kOffsetBytes)) {
    return Refuse(Fault::kSectionIndex, block, section, std::nullopt, entry,
                  kOffsetBytes);
  }
  const std::size_t table = ReadLittle(block, entry, kOffsetBytes);
  if (!Holds(block, table, kCountBytes)) {
    return Refuse(Fault::kSectionOffset, block, section, std::nullopt, table,
                  kCountBytes);
  }
  return table;
}

// decomp: BATTLE.PRG func_800D5564 (src/BATTLE/BATTLE.PRG/5BF94.c:6873)
std::expected<PFileTimelineTrack, PFileTimelineError> PFileTimelineTrackEntry(
    std::span<const std::byte> block, std::size_t section, std::size_t track) {
  return PFileTimelineSection(block, section)
      .and_then([block, section, track](std::size_t table) {
        return EntryAt(block, section, table, track);
      });
}

// The reads func_800D55A4 (src/BATTLE/BATTLE.PRG/5BF94.c:6879) makes, each
// checked before the load starts a track.
std::expected<PFileTimeline, PFileTimelineError> ReadPFileTimeline(
    std::span<const std::byte> block, std::size_t section,
    PFileTimelineKeyBytes key_bytes) {
  const auto table = PFileTimelineSection(block, section);
  if (!table) return std::unexpected(table.error());
  // The load keeps the count word's low byte.
  const auto count =
      static_cast<std::uint8_t>(ReadLittle(block, *table, kCountBytes));
  PFileTimeline timeline{.section = section, .tracks = {}};
  timeline.tracks.reserve(count);
  for (std::size_t track = 0; track < count; ++track) {
    auto entry = EntryAt(block, section, *table, track);
    if (!entry) return std::unexpected(entry.error());
    const std::size_t keys =
        key_bytes != nullptr ? key_bytes(entry->header) : 0;
    if (!Holds(block, entry->data_offset, keys)) {
      return Refuse(Fault::kTrackKeys, block, section, track,
                    entry->data_offset, keys);
    }
    if (entry->data_offset % kPFileTimelineKeyAlignment != 0) {
      return Refuse(Fault::kTrackAlignment, block, section, track,
                    entry->data_offset, keys);
    }
    timeline.tracks.push_back(*entry);
  }
  return timeline;
}

}  // namespace VS::Content
