// The MAP\*.ZUD enemy bundle codec (ZudFile.hpp).
#include "Content/ZudFile.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace VS::Content {
namespace {

constexpr std::size_t kWordBytes = 4;
constexpr std::uint32_t kByteBits = 8;
// Each section's offset and size words, from the end of the identity bytes.
// decomp: ZudHeader (src/BATTLE/BATTLE.PRG/146C.c:128)
constexpr std::size_t kSectionWordsAt = kZudIdentityBytes;
constexpr std::size_t kSectionEntryBytes = 2 * kWordBytes;
static_assert(kSectionWordsAt + (kZudSectionCount * kSectionEntryBytes) ==
              kZudHeaderBytes);
// The game reads the words as signed (ActorSpawning.cpp ZudSectionFits).
constexpr std::size_t kMaxWord = std::numeric_limits<std::int32_t>::max();

constexpr std::array<std::string_view, kZudSectionCount> kSectionNames{
    "body", "weapon", "shield", "common animation", "battle animation",
};

std::uint32_t ReadWord(std::span<const std::byte> data, std::size_t at) {
  std::uint32_t word = 0;
  for (std::size_t index = 0; index < kWordBytes; ++index) {
    word |= std::to_integer<std::uint32_t>(data[at + index])
            << (kByteBits * index);
  }
  return word;
}

void WriteWord(std::vector<std::byte>& data, std::size_t at,
               std::uint32_t word) {
  for (std::size_t index = 0; index < kWordBytes; ++index) {
    data[at + index] = static_cast<std::byte>(word >> (kByteBits * index));
  }
}

std::size_t EntryAt(std::size_t section) {
  return kSectionWordsAt + (section * kSectionEntryBytes);
}

}  // namespace

std::string_view ZudSectionName(ZudSection section) noexcept {
  const auto index = static_cast<std::size_t>(section);
  return index < kSectionNames.size() ? kSectionNames.at(index) : "section";
}

std::expected<ZudFile, std::string> DecodeZudFile(
    std::span<const std::byte> data) {
  if (data.size() < kZudHeaderBytes) {
    return std::unexpected(
        std::format("{} bytes are too short for the {}-byte header",
                    data.size(), kZudHeaderBytes));
  }
  ZudFile zud;
  for (std::size_t index = 0; index < kZudIdentityBytes; ++index) {
    zud.identity.at(index) = std::to_integer<std::uint8_t>(data[index]);
  }
  std::size_t next = kZudHeaderBytes;
  for (std::size_t section = 0; section < kZudSectionCount; ++section) {
    const std::string_view name = kSectionNames.at(section);
    const std::size_t at = EntryAt(section);
    const std::uint32_t offset = ReadWord(data, at);
    const std::uint32_t size = ReadWord(data, at + kWordBytes);
    if (offset != next) {
      return std::unexpected(
          std::format("the {} section starts at 0x{:X}, not 0x{:X} where the "
                      "one before it ends",
                      name, offset, next));
    }
    if (size > data.size() - next) {
      return std::unexpected(
          std::format("the {} section's {} bytes at 0x{:X} run past the "
                      "{}-byte file",
                      name, size, offset, data.size()));
    }
    const auto bytes = data.subspan(next, size);
    zud.sections.at(section).assign(bytes.begin(), bytes.end());
    next += size;
  }
  if (next != data.size()) {
    return std::unexpected(std::format(
        "{} bytes follow the battle animation section", data.size() - next));
  }
  return zud;
}

std::expected<std::vector<std::byte>, std::string> EncodeZudFile(
    const ZudFile& zud) {
  std::size_t total = kZudHeaderBytes;
  for (std::size_t section = 0; section < kZudSectionCount; ++section) {
    total += zud.sections.at(section).size();
    if (total > kMaxWord) {
      return std::unexpected(
          std::format("the {} section ends past the largest offset the game "
                      "reads",
                      kSectionNames.at(section)));
    }
  }
  std::vector<std::byte> data(kZudHeaderBytes);
  data.reserve(total);
  std::ranges::transform(zud.identity, data.begin(), [](std::uint8_t value) {
    return static_cast<std::byte>(value);
  });
  for (std::size_t section = 0; section < kZudSectionCount; ++section) {
    const std::vector<std::byte>& bytes = zud.sections.at(section);
    const std::size_t at = EntryAt(section);
    WriteWord(data, at, static_cast<std::uint32_t>(data.size()));
    WriteWord(data, at + kWordBytes, static_cast<std::uint32_t>(bytes.size()));
    data.insert(data.end(), bytes.begin(), bytes.end());
  }
  return data;
}

}  // namespace VS::Content
