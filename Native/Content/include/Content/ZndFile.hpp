#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The MAP\ZONE*.ZND zone files, decoded strictly for the asset pipeline
// (ASSET_PIPELINE.md §6.9, #199). The room loader reads a zone's file when the
// zone changes (BATTLE.PRG _loadZnd, Field/RoomLoading.hpp): its room file
// table, its enemy templates with the ZUD file each one loads, its texture
// table and its music id.
//
// A file is a 0x1C-byte header of seven little-endian words (the room table,
// enemy section and texture section, each as offset and size, then the music
// id) and a reserved word, then the three sections back to back to the end of
// the file. Files are named by sector and read size, as the game reads them.
// The enemy section is a count, that many ZUD files, then that many 0x464-byte
// templates. The texture section is either 32 zero bytes (no table) or four
// table sizes, of which only the first is used, and that table: a count and
// that many TIMs, each after its byte size.
namespace VS::Content {

inline constexpr std::string_view kZndExtension = ".ZND";
inline constexpr std::size_t kZndHeaderBytes = 0x20;
inline constexpr std::size_t kZndTemplateBytes = 0x464;
// A file the zone names: its first sector and the bytes the game reads (the
// file's size rounded up to whole sectors on the disc).
inline constexpr std::size_t kZndFileEntryBytes = 8;
inline constexpr std::size_t kZndTextureTableCount = 4;
// The texture section of a zone without a texture table.
inline constexpr std::size_t kZndEmptyTexturesBytes = 32;

struct ZndFileEntry {
  std::int32_t sector = 0;
  std::int32_t bytes = 0;

  bool operator==(const ZndFileEntry&) const = default;
};

using ZndTemplate = std::array<std::byte, kZndTemplateBytes>;

struct ZndEnemy {
  ZndFileEntry zud;
  ZndTemplate data{};

  bool operator==(const ZndEnemy&) const = default;
};

struct ZndFile {
  std::int32_t music_id = 0;
  // Every room slot in table order; an empty slot is {0, 0}.
  std::vector<ZndFileEntry> rooms;
  std::vector<ZndEnemy> enemies;
  // The texture table's TIMs, each as its bytes; nothing for a zone without
  // a table.
  std::optional<std::vector<std::vector<std::byte>>> textures;

  bool operator==(const ZndFile&) const = default;
};

// Decodes a whole zone file. It refuses what EncodeZndFile would not rebuild
// byte for byte: a nonzero reserved word, sections that do not follow each
// other from the header to the end of the file, a room table of partial
// entries, an enemy section whose size is not its count's, a texture table
// other than the first, and texture sizes that do not fill their section.
// Errors name the section at fault.
[[nodiscard]] std::expected<ZndFile, std::string> DecodeZndFile(
    std::span<const std::byte> data);
// The file `zone` is, its sections packed from the header on. A file
// DecodeZndFile returned rebuilds byte for byte.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeZndFile(
    const ZndFile& zone);

}  // namespace VS::Content
