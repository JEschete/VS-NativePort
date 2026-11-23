#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/SeqFile.hpp"

// The OBJ\E*.ESQ shared animation sets, decoded strictly for the asset
// pipeline (ASSET_PIPELINE.md §6.2.2, #206). The game loads one into a shared
// sequence bank (ActorAssets.cpp ParseSequenceBank) and binds each actor to
// the first entry whose selector is its model's kind or its model number;
// only an entry selected by the model number gives the actor its texture row.
//
// A file is a count, then per entry a selector halfword and a zero halfword,
// then per entry a texture-row halfword and a zero halfword, then per entry a
// word offset to the entry from the end of the offsets. Each entry is a whole
// SEQ file (SeqFile.hpp) padded to a word; the entries follow one another
// with nothing after the last.
namespace VS::Content {

inline constexpr std::string_view kEsqExtension = ".ESQ";
// The game refuses 16 or more entries (ActorAssets.hpp
// kSharedSequenceEntries keeps one slot for the end marker).
inline constexpr std::size_t kEsqMaxEntries = 15;
// The game reads selectors as signed halfwords and stops at -1.
inline constexpr std::uint16_t kEsqMaxSelector = 0xFFFE;
// The game reads the row as a byte; 0xFF is the row it skips.
inline constexpr std::uint8_t kEsqMaxTextureRow = 0xFE;

struct EsqEntry {
  // The model number or model kind the entry is for.
  std::uint16_t selector = 0;
  // The effect texture row a model selected by its number uses; none is the
  // game's 0xFF.
  std::optional<std::uint8_t> texture_row;
  SeqFile animation;

  bool operator==(const EsqEntry&) const = default;
};

struct EsqFile {
  std::vector<EsqEntry> entries;

  bool operator==(const EsqFile&) const = default;
};

// Decodes a whole ESQ file. `event_lengths` is the game's event stride table
// (kSeqEventLengthsAt in BATTLE.PRG). An entry's size field covers its
// padding, so each decoded bank holds its padding bytes as the game reads
// them: as unread bytes or as a track's terminator.
[[nodiscard]] std::expected<EsqFile, std::string> DecodeEsqFile(
    std::span<const std::byte> data,
    std::span<const std::uint8_t> event_lengths);
// The file `file` is, each entry padded with zeros to a word. A file
// DecodeEsqFile returned rebuilds byte for byte. Errors name what the file
// cannot hold.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeEsqFile(
    const EsqFile& file, std::span<const std::uint8_t> event_lengths);

}  // namespace VS::Content
