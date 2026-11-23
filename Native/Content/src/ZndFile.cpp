// The MAP\ZONE*.ZND zone file codec (ZndFile.hpp).
#include "Content/ZndFile.hpp"

#include <algorithm>
#include <array>
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

constexpr std::size_t kWordBytes = 4;
constexpr std::uint32_t kByteBits = 8;
// decomp: ZndHeader (src/BATTLE/BATTLE.PRG/146C.c:155): each section's offset
// and size words, then the music id; the reserved word follows.
constexpr std::size_t kSectionCount = 3;
constexpr std::size_t kSectionEntryBytes = 2 * kWordBytes;
constexpr std::size_t kMusicIdAt = kSectionCount * kSectionEntryBytes;
constexpr std::size_t kReservedAt = kMusicIdAt + kWordBytes;
static_assert(kReservedAt + kWordBytes == kZndHeaderBytes);
constexpr std::array<std::string_view, kSectionCount> kSectionNames{
    "room table",
    "enemy section",
    "texture section",
};
// The texture section: the table sizes, then the first table.
constexpr std::size_t kTextureSizesBytes = kZndTextureTableCount * kWordBytes;
// The game reads the words as signed (RoomLoading.cpp DecodeZoneFile).
constexpr std::size_t kMaxWord = std::numeric_limits<std::int32_t>::max();

std::uint32_t ReadWord(std::span<const std::byte> data, std::size_t at) {
  std::uint32_t word = 0;
  for (std::size_t index = 0; index < kWordBytes; ++index) {
    word |= std::to_integer<std::uint32_t>(data[at + index])
            << (kByteBits * index);
  }
  return word;
}

std::int32_t ReadSigned(std::span<const std::byte> data, std::size_t at) {
  return static_cast<std::int32_t>(ReadWord(data, at));
}

void AppendWord(std::vector<std::byte>& data, std::uint32_t word) {
  for (std::size_t index = 0; index < kWordBytes; ++index) {
    data.push_back(static_cast<std::byte>(word >> (kByteBits * index)));
  }
}

void WriteWord(std::vector<std::byte>& data, std::size_t at,
               std::uint32_t word) {
  for (std::size_t index = 0; index < kWordBytes; ++index) {
    data[at + index] = static_cast<std::byte>(word >> (kByteBits * index));
  }
}

ZndFileEntry ReadEntry(std::span<const std::byte> data, std::size_t at) {
  return {.sector = ReadSigned(data, at),
          .bytes = ReadSigned(data, at + kWordBytes)};
}

void AppendEntry(std::vector<std::byte>& data, const ZndFileEntry& entry) {
  AppendWord(data, static_cast<std::uint32_t>(entry.sector));
  AppendWord(data, static_cast<std::uint32_t>(entry.bytes));
}

std::expected<std::vector<ZndFileEntry>, std::string> DecodeRooms(
    std::span<const std::byte> table) {
  if (table.size() % kZndFileEntryBytes != 0) {
    return std::unexpected(
        std::format("the room table's {} bytes are not whole {}-byte entries",
                    table.size(), kZndFileEntryBytes));
  }
  std::vector<ZndFileEntry> rooms;
  for (std::size_t at = 0; at < table.size(); at += kZndFileEntryBytes) {
    rooms.push_back(ReadEntry(table, at));
  }
  return rooms;
}

std::expected<std::vector<ZndEnemy>, std::string> DecodeEnemies(
    std::span<const std::byte> section) {
  if (section.size() < kWordBytes) {
    return std::unexpected(std::format(
        "the enemy section's {} bytes hold no count", section.size()));
  }
  const std::int32_t count = ReadSigned(section, 0);
  constexpr std::size_t kPerEnemy = kZndFileEntryBytes + kZndTemplateBytes;
  if (count < 0 ||
      (section.size() - kWordBytes) / kPerEnemy !=
          static_cast<std::size_t>(count) ||
      (section.size() - kWordBytes) % kPerEnemy != 0) {
    return std::unexpected(
        std::format("the enemy section's {} bytes do not hold its {} enemies",
                    section.size(), count));
  }
  const auto enemies_count = static_cast<std::size_t>(count);
  std::vector<ZndEnemy> enemies(enemies_count);
  const std::size_t templates_at =
      kWordBytes + (enemies_count * kZndFileEntryBytes);
  for (std::size_t index = 0; index < enemies_count; ++index) {
    ZndEnemy& enemy = enemies.at(index);
    enemy.zud = ReadEntry(section, kWordBytes + (index * kZndFileEntryBytes));
    const auto bytes = section.subspan(
        templates_at + (index * kZndTemplateBytes), kZndTemplateBytes);
    std::ranges::copy(bytes, enemy.data.begin());
  }
  return enemies;
}

std::expected<std::optional<std::vector<std::vector<std::byte>>>, std::string>
DecodeTextures(std::span<const std::byte> section) {
  if (section.size() < kTextureSizesBytes + kWordBytes) {
    return std::unexpected(
        std::format("the texture section's {} bytes are shorter than its "
                    "table sizes and count",
                    section.size()));
  }
  const std::int32_t first = ReadSigned(section, 0);
  for (std::size_t table = 1; table < kZndTextureTableCount; ++table) {
    if (ReadWord(section, table * kWordBytes) != 0) {
      return std::unexpected(std::format(
          "texture table {} is not empty; only the first is read", table));
    }
  }
  if (first == 0) {
    if (section.size() != kZndEmptyTexturesBytes) {
      return std::unexpected(
          std::format("a texture section without a table is {} bytes, not {}",
                      section.size(), kZndEmptyTexturesBytes));
    }
    if (std::ranges::any_of(
            section, [](std::byte value) { return value != std::byte{}; })) {
      return std::unexpected(
          "a texture section without a table holds nonzero bytes");
    }
    return std::nullopt;
  }
  if (first < 0 ||
      static_cast<std::size_t>(first) != section.size() - kTextureSizesBytes) {
    return std::unexpected(std::format(
        "the texture table's size {} does not fill the {} bytes after the "
        "table sizes",
        first, section.size() - kTextureSizesBytes));
  }
  const auto table = section.subspan(kTextureSizesBytes);
  const std::int32_t count = ReadSigned(table, 0);
  if (count < 0) {
    return std::unexpected(
        std::format("the texture table's count is {}", count));
  }
  std::vector<std::vector<std::byte>> textures;
  std::size_t at = kWordBytes;
  for (std::int32_t index = 0; index < count; ++index) {
    if (table.size() - at < kWordBytes) {
      return std::unexpected(std::format(
          "texture {} of {} has no size before the table ends", index, count));
    }
    const std::uint32_t size = ReadWord(table, at);
    at += kWordBytes;
    if (size > table.size() - at) {
      return std::unexpected(std::format(
          "texture {}'s {} bytes run past the texture table", index, size));
    }
    const auto bytes = table.subspan(at, size);
    textures.emplace_back(bytes.begin(), bytes.end());
    at += size;
  }
  if (at != table.size()) {
    return std::unexpected(
        std::format("{} bytes follow the texture table's {} textures",
                    table.size() - at, count));
  }
  return textures;
}

}  // namespace

std::expected<ZndFile, std::string> DecodeZndFile(
    std::span<const std::byte> data) {
  if (data.size() < kZndHeaderBytes) {
    return std::unexpected(
        std::format("{} bytes are too short for the {}-byte header",
                    data.size(), kZndHeaderBytes));
  }
  if (const std::uint32_t reserved = ReadWord(data, kReservedAt);
      reserved != 0) {
    return std::unexpected(
        std::format("the reserved header word is 0x{:X}, not 0", reserved));
  }
  std::array<std::span<const std::byte>, kSectionCount> sections;
  std::size_t next = kZndHeaderBytes;
  for (std::size_t section = 0; section < kSectionCount; ++section) {
    const std::string_view name = kSectionNames.at(section);
    const std::uint32_t offset = ReadWord(data, section * kSectionEntryBytes);
    const std::uint32_t size =
        ReadWord(data, (section * kSectionEntryBytes) + kWordBytes);
    if (offset != next) {
      return std::unexpected(
          std::format("the {} starts at 0x{:X}, not 0x{:X} where the one "
                      "before it ends",
                      name, offset, next));
    }
    if (size > data.size() - next) {
      return std::unexpected(
          std::format("the {}'s {} bytes at 0x{:X} run past the {}-byte file",
                      name, size, offset, data.size()));
    }
    sections.at(section) = data.subspan(next, size);
    next += size;
  }
  if (next != data.size()) {
    return std::unexpected(
        std::format("{} bytes follow the texture section", data.size() - next));
  }
  ZndFile zone;
  zone.music_id = ReadSigned(data, kMusicIdAt);
  auto rooms = DecodeRooms(sections.at(0));
  if (!rooms) return std::unexpected(rooms.error());
  zone.rooms = std::move(*rooms);
  auto enemies = DecodeEnemies(sections.at(1));
  if (!enemies) return std::unexpected(enemies.error());
  zone.enemies = std::move(*enemies);
  auto textures = DecodeTextures(sections.at(2));
  if (!textures) return std::unexpected(textures.error());
  zone.textures = std::move(*textures);
  return zone;
}

std::expected<std::vector<std::byte>, std::string> EncodeZndFile(
    const ZndFile& zone) {
  std::vector<std::byte> data(kZndHeaderBytes);
  std::array<std::size_t, kSectionCount> starts{};
  starts.at(0) = data.size();
  for (const ZndFileEntry& room : zone.rooms) AppendEntry(data, room);
  starts.at(1) = data.size();
  AppendWord(data, static_cast<std::uint32_t>(zone.enemies.size()));
  for (const ZndEnemy& enemy : zone.enemies) AppendEntry(data, enemy.zud);
  for (const ZndEnemy& enemy : zone.enemies) {
    data.insert(data.end(), enemy.data.begin(), enemy.data.end());
  }
  starts.at(2) = data.size();
  if (!zone.textures) {
    data.resize(data.size() + kZndEmptyTexturesBytes);
  } else {
    const std::size_t sizes_at = data.size();
    data.resize(data.size() + kTextureSizesBytes);
    AppendWord(data, static_cast<std::uint32_t>(zone.textures->size()));
    for (const std::vector<std::byte>& texture : *zone.textures) {
      if (texture.size() > kMaxWord) {
        return std::unexpected(std::format(
            "a texture's {} bytes exceed the largest size the game reads",
            texture.size()));
      }
      AppendWord(data, static_cast<std::uint32_t>(texture.size()));
      data.insert(data.end(), texture.begin(), texture.end());
    }
    WriteWord(data, sizes_at,
              static_cast<std::uint32_t>(data.size() - sizes_at -
                                         kTextureSizesBytes));
  }
  if (data.size() > kMaxWord) {
    return std::unexpected(std::format(
        "the zone's {} bytes exceed the largest offset the game reads",
        data.size()));
  }
  const std::array<std::size_t, kSectionCount + 1> bounds{
      starts.at(0), starts.at(1), starts.at(2), data.size()};
  for (std::size_t section = 0; section < kSectionCount; ++section) {
    WriteWord(data, section * kSectionEntryBytes,
              static_cast<std::uint32_t>(bounds.at(section)));
    WriteWord(data, (section * kSectionEntryBytes) + kWordBytes,
              static_cast<std::uint32_t>(bounds.at(section + 1) -
                                         bounds.at(section)));
  }
  WriteWord(data, kMusicIdAt, static_cast<std::uint32_t>(zone.music_id));
  return data;
}

}  // namespace VS::Content
