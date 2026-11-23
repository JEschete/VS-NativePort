#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ZndFile.hpp"
#include "TestSupport/ModelFixtures.hpp"

namespace {

namespace fs = std::filesystem;
namespace content = VS::Content;
using Bytes = std::vector<std::byte>;
using VS::test_support::ErrorOf;
using VS::test_support::ExtractedModels;
using VS::test_support::ReadModel;

// Measured on the configured disc (ASSET_PIPELINE.md §6.9).
constexpr std::size_t kExtractedZoneCount = 256;
constexpr std::size_t kExtractedEnemyCount = 455;
constexpr std::size_t kExtractedRoomCount = 512;
constexpr std::size_t kExtractedEmptyRoomSlots = 421;
constexpr std::size_t kExtractedTexturedZones = 76;
constexpr std::size_t kExtractedTextureCount = 782;
// Header words: each section's offset and size, the music id, the reserved
// word.
constexpr std::size_t kEnemiesOffsetAt = 0x08;
constexpr std::size_t kReservedAt = 0x1C;

// Two rooms (the second slot empty), two enemies whose templates are filled
// with their index + 1, and a texture table of two payloads.
content::ZndFile SampleZone() {
  content::ZndFile zone;
  zone.music_id = 54;
  zone.rooms = {{.sector = 2947, .bytes = 61440}, {.sector = 0, .bytes = 0}};
  for (std::size_t index = 0; index < 2; ++index) {
    content::ZndEnemy enemy;
    enemy.zud = {.sector = static_cast<std::int32_t>(3464 + index),
                 .bytes = 2048};
    enemy.data.fill(static_cast<std::byte>(index + 1));
    zone.enemies.push_back(enemy);
  }
  zone.textures =
      std::vector<Bytes>{Bytes(8, std::byte{0x10}), Bytes(4, std::byte{0x20})};
  return zone;
}

Bytes Encoded(const content::ZndFile& zone) {
  auto bytes = content::EncodeZndFile(zone);
  INFO(ErrorOf(bytes));
  REQUIRE(bytes.has_value());
  return *std::move(bytes);
}

std::uint32_t Word(std::span<const std::byte> bytes, std::size_t at) {
  return std::to_integer<std::uint32_t>(bytes[at]) |
         (std::to_integer<std::uint32_t>(bytes[at + 1]) << 8U) |
         (std::to_integer<std::uint32_t>(bytes[at + 2]) << 16U) |
         (std::to_integer<std::uint32_t>(bytes[at + 3]) << 24U);
}

void PutWord(Bytes& bytes, std::size_t at, std::uint32_t word) {
  for (std::size_t index = 0; index < 4; ++index) {
    bytes.at(at + index) = static_cast<std::byte>(word >> (8U * index));
  }
}

// A zone file with these sections, its header pointing at each in turn.
Bytes Packed(const Bytes& rooms, const Bytes& enemies, const Bytes& textures) {
  Bytes bytes(content::kZndHeaderBytes);
  std::size_t at = bytes.size();
  std::size_t word = 0;
  for (const Bytes* section : {&rooms, &enemies, &textures}) {
    PutWord(bytes, word, static_cast<std::uint32_t>(at));
    PutWord(bytes, word + 4, static_cast<std::uint32_t>(section->size()));
    bytes.insert(bytes.end(), section->begin(), section->end());
    at += section->size();
    word += 8;
  }
  return bytes;
}

const Bytes kNoEnemies(4);
const Bytes kNoTextures(content::kZndEmptyTexturesBytes);

}  // namespace

TEST_CASE("ZND files round-trip their rooms, enemies and texture table",
          "[content-tables][znd]") {
  const content::ZndFile zone = SampleZone();
  const Bytes bytes = Encoded(zone);
  constexpr std::size_t kRooms = 2 * content::kZndFileEntryBytes;
  constexpr std::size_t kEnemies =
      4 + (2 * (content::kZndFileEntryBytes + content::kZndTemplateBytes));
  // Four table sizes, the count, and each payload after its size.
  constexpr std::size_t kTextures = 16 + 4 + (4 + 8) + (4 + 4);
  REQUIRE(bytes.size() ==
          content::kZndHeaderBytes + kRooms + kEnemies + kTextures);
  const std::vector<std::uint32_t> header{0x20,
                                          kRooms,
                                          0x20 + kRooms,
                                          kEnemies,
                                          0x20 + kRooms + kEnemies,
                                          kTextures,
                                          54,
                                          0};
  for (std::size_t word = 0; word < header.size(); ++word) {
    CAPTURE(word);
    CHECK(Word(bytes, 4 * word) == header.at(word));
  }
  const std::size_t textures_at = 0x20 + kRooms + kEnemies;
  CHECK(Word(bytes, textures_at) == kTextures - 16);
  CHECK(Word(bytes, textures_at + 16) == 2);
  CHECK(content::DecodeZndFile(bytes) == zone);

  // A zone with nothing is its header, a zero count and the empty texture
  // section: 68 bytes, as ZONE207 on the disc.
  const content::ZndFile empty;
  const Bytes header_only = Encoded(empty);
  CHECK(header_only.size() == 68);
  CHECK(header_only == Packed({}, kNoEnemies, kNoTextures));
  CHECK(content::DecodeZndFile(header_only) == empty);
  // An empty table is a table, not the empty section.
  content::ZndFile no_tims;
  no_tims.textures.emplace();
  CHECK(content::DecodeZndFile(Encoded(no_tims)) == no_tims);
}

TEST_CASE("ZND decoding names the bytes it rejects", "[content-tables][znd]") {
  struct Refused {
    Bytes bytes;
    std::string_view error;
  };
  const Bytes bytes = Encoded(SampleZone());
  const std::size_t textures_at = Word(bytes, 0x10);
  Bytes short_header(bytes.begin(), bytes.begin() + 31);
  Bytes reserved = bytes;
  reserved.at(kReservedAt) = std::byte{1};
  Bytes trailing = bytes;
  trailing.push_back(std::byte{0});
  Bytes gap = bytes;
  PutWord(gap, kEnemiesOffsetAt, Word(bytes, kEnemiesOffsetAt) + 4);
  Bytes second_table = bytes;
  PutWord(second_table, textures_at + 4, 1);
  Bytes table_size = bytes;
  PutWord(table_size, textures_at, Word(bytes, textures_at) - 1);
  Bytes overrun = bytes;
  PutWord(overrun, textures_at + 16 + 4 + 12, 5);
  Bytes untextured = Packed({}, kNoEnemies, kNoTextures);
  untextured.back() = std::byte{1};
  Bytes long_empty = kNoTextures;
  long_empty.push_back(std::byte{0});
  Bytes miscounted = Packed({}, kNoEnemies, kNoTextures);
  PutWord(miscounted, content::kZndHeaderBytes, 1);
  const std::vector<Refused> refusals{
      {short_header, "31 bytes are too short for the 32-byte header"},
      {reserved, "the reserved header word is 0x1, not 0"},
      {trailing, "1 bytes follow the texture section"},
      {gap,
       "the enemy section starts at 0x34, not 0x30 where the one before it "
       "ends"},
      {Packed(Bytes(7), kNoEnemies, kNoTextures),
       "the room table's 7 bytes are not whole 8-byte entries"},
      {miscounted, "the enemy section's 4 bytes do not hold its 1 enemies"},
      {second_table, "texture table 1 is not empty; only the first is read"},
      {table_size,
       "the texture table's size 23 does not fill the 24 bytes after the "
       "table sizes"},
      {overrun, "texture 1's 5 bytes run past the texture table"},
      {untextured, "a texture section without a table holds nonzero bytes"},
      {Packed({}, kNoEnemies, long_empty),
       "a texture section without a table is 33 bytes, not 32"},
  };
  for (const Refused& refused : refusals) {
    CAPTURE(refused.error);
    CHECK(ErrorOf(content::DecodeZndFile(refused.bytes)) == refused.error);
  }
}

TEST_CASE("Extracted ZND files decode and rebuild their exact bytes",
          "[content-tables][znd][extract]") {
  const auto models = ExtractedModels();
  if (!models) SKIP("Extracted Native/data is unavailable");
  std::size_t zones = 0;
  std::size_t enemies = 0;
  std::size_t rooms = 0;
  std::size_t empty_rooms = 0;
  std::size_t textured = 0;
  std::size_t textures = 0;
  for (const auto& entry :
       fs::directory_iterator(models->parent_path() / "MAP")) {
    if (entry.path().extension() != fs::path(content::kZndExtension)) {
      continue;
    }
    CAPTURE(entry.path().filename().string());
    const Bytes bytes = ReadModel(entry.path());
    const auto zone = content::DecodeZndFile(bytes);
    INFO(ErrorOf(zone));
    REQUIRE(zone.has_value());
    CHECK(Encoded(*zone) == bytes);
    ++zones;
    enemies += zone->enemies.size();
    for (const content::ZndFileEntry& room : zone->rooms) {
      const bool empty = room.sector == 0 && room.bytes == 0;
      ++(empty ? empty_rooms : rooms);
    }
    if (zone->textures) {
      ++textured;
      textures += zone->textures->size();
    }
  }
  CHECK(zones == kExtractedZoneCount);
  CHECK(enemies == kExtractedEnemyCount);
  CHECK(rooms == kExtractedRoomCount);
  CHECK(empty_rooms == kExtractedEmptyRoomSlots);
  CHECK(textured == kExtractedTexturedZones);
  CHECK(textures == kExtractedTextureCount);
}
