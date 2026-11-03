#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <optional>
#include <span>

// Effect timeline lookups in a P-file's timeline block (package block 3):
//   block:   i32 count, u16 section offsets[count]
//   section: i32 track count, {u16 track header, u16 data offset}[tracks]
// Every offset counts from the start of the block. The lookups bound each
// table read and each result to the block; they return a byte offset into
// it, or nullopt when the block does not hold it.
namespace VS::Content {

inline constexpr std::size_t kPFileTimelineHeaderBytes = 4;
inline constexpr std::size_t kPFileTimelineTrackEntryBytes = 4;

// The offset of timeline section `section` (its track table).
[[nodiscard]] std::optional<std::size_t> PFileTimelineSection(
    std::span<const std::byte> block, std::size_t section);

// The offset of track `track`'s key data in timeline section `section`.
[[nodiscard]] std::optional<std::size_t> PFileTimelineTrackData(
    std::span<const std::byte> block, std::size_t section, std::size_t track);

}  // namespace VS::Content
