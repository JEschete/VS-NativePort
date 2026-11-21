#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Effect timeline lookups in a P-file's timeline block (package block 3):
//   block:   i32 count, u16 section offsets[count]
//   section: i32 track count, {u16 track header, u16 data offset}[tracks]
// Every offset counts from the start of the block. The lookups bound each
// table read and each result to the block; they return a byte offset into
// it, or a PFileTimelineError saying what did not fit and where.
namespace VS::Content {

inline constexpr std::size_t kPFileTimelineHeaderBytes = 4;
inline constexpr std::size_t kPFileTimelineTrackEntryBytes = 4;
// Track keys start on a word boundary of the block: every disc package
// aligns them and the native track keeps a word pointer to them.
inline constexpr std::size_t kPFileTimelineKeyAlignment = 4;

// What a timeline lookup found outside the block.
enum class PFileTimelineFault : std::uint8_t {
  kHeader,          // the block's count word
  kSectionIndex,    // the section's entry in the offset table
  kSectionOffset,   // the section's track count word
  kTrackEntry,      // the track's {header, data offset} entry
  kTrackData,       // the track's data offset
  kTrackKeys,       // the track's keys run past the block
  kTrackAlignment,  // the track's keys do not start on a word boundary
};

// A refused lookup: the fault, the section and track it was reading (no
// track for a section fault), the block range that did not fit and the
// block's size.
struct PFileTimelineError {
  PFileTimelineFault fault{};
  std::size_t section{};
  std::optional<std::size_t> track;
  std::size_t offset{};
  std::size_t bytes{};
  std::size_t block_bytes{};

  friend bool operator==(const PFileTimelineError&,
                         const PFileTimelineError&) = default;
};

// The error as a log line, e.g. "section 3 track 2: keys at 0x30 (16 bytes)
// outside the 64-byte timeline block".
[[nodiscard]] std::string Describe(const PFileTimelineError& error);

// A track's table entry: its header (kind in the low byte, key count in the
// high byte) and the offset of its keys in the block.
struct PFileTimelineTrack {
  std::uint16_t header{};
  std::size_t data_offset{};

  friend bool operator==(const PFileTimelineTrack&,
                         const PFileTimelineTrack&) = default;
};

// A section whose every track entry and key run lies inside the block.
struct PFileTimeline {
  std::size_t section{};
  std::vector<PFileTimelineTrack> tracks;
};

// The bytes the keys of a track with `header` occupy, as their reader
// consumes them (Effects::TimelineTrackKeyBytes).
using PFileTimelineKeyBytes = std::size_t (*)(std::uint16_t header);

// The block's section count (its header word).
[[nodiscard]] std::expected<std::int32_t, PFileTimelineError>
PFileTimelineCount(std::span<const std::byte> block);

// The offset of timeline section `section` (its track count word). The
// section count is not checked, as in the original.
[[nodiscard]] std::expected<std::size_t, PFileTimelineError>
PFileTimelineSection(std::span<const std::byte> block, std::size_t section);

// Track `track`'s entry in timeline section `section`; its data offset lies
// inside the block. The track count is not checked, as in the original.
[[nodiscard]] std::expected<PFileTimelineTrack, PFileTimelineError>
PFileTimelineTrackEntry(std::span<const std::byte> block, std::size_t section,
                        std::size_t track);

// Timeline section `section` with its tracks, the count word's low byte of
// them as the original loads, each entry inside the block and each track's
// `key_bytes` of keys inside the block from a word boundary.
[[nodiscard]] std::expected<PFileTimeline, PFileTimelineError>
ReadPFileTimeline(std::span<const std::byte> block, std::size_t section,
                  PFileTimelineKeyBytes key_bytes);

}  // namespace VS::Content
