// Hand-maintained (tools/port/owned.csv).
#include "Content/PFile.hpp"  // NOLINT(build/include): typed API

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Content {
namespace {
constexpr std::size_t kOffsetBytes = 2;
constexpr std::size_t kBitsPerByte = 8;

// The little-endian u16 at `at`, when the block holds both bytes.
std::optional<std::size_t> ReadOffset(std::span<const std::byte> block,
                                      std::size_t at) {
  if (at > block.size() || block.size() - at < kOffsetBytes) {
    return std::nullopt;
  }
  return std::size_t{std::to_integer<std::uint8_t>(block[at])} |
         (std::size_t{std::to_integer<std::uint8_t>(block[at + 1])}
          << kBitsPerByte);
}

std::optional<std::size_t> InsideBlock(std::span<const std::byte> block,
                                       std::optional<std::size_t> offset) {
  if (!offset || *offset >= block.size()) {
    return std::nullopt;
  }
  return offset;
}
}  // namespace

// decomp: BATTLE.PRG _getPFileBlock3section
// (src/BATTLE/BATTLE.PRG/5BF94.c:7874)
std::optional<std::size_t> PFileTimelineSection(
    std::span<const std::byte> block, std::size_t section) {
  if (block.size() < kPFileTimelineHeaderBytes ||
      section > (block.size() - kPFileTimelineHeaderBytes) / kOffsetBytes) {
    return std::nullopt;
  }
  // sll 1; lhu 4(block + section*2)
  return InsideBlock(block, ReadOffset(block, kPFileTimelineHeaderBytes +
                                                  (section * kOffsetBytes)));
}

// decomp: BATTLE.PRG func_800D5564 (src/BATTLE/BATTLE.PRG/5BF94.c:7879)
std::optional<std::size_t> PFileTimelineTrackData(
    std::span<const std::byte> block, std::size_t section, std::size_t track) {
  const std::optional<std::size_t> table = PFileTimelineSection(block, section);
  if (!table || track > block.size() / kPFileTimelineTrackEntryBytes) {
    return std::nullopt;
  }
  // sll 2; lhu 6(section + track*4): the entry's data offset, from the block.
  const std::size_t entry = *table + kPFileTimelineHeaderBytes +
                            (track * kPFileTimelineTrackEntryBytes);
  return InsideBlock(block, ReadOffset(block, entry + kOffsetBytes));
}

}  // namespace VS::Content
