// Strict ESQ animation set decoding and encoding (EsqFile.hpp).
#include "Content/EsqFile.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/SeqFile.hpp"

namespace VS::Content {
namespace {
using Bytes = std::span<const std::byte>;

// The count word, then three tables of a word per entry: selectors and
// texture rows (each a halfword and a zero halfword), then offsets.
constexpr std::size_t kCountBytes = 4;
constexpr std::size_t kFieldBytes = 4;
constexpr std::size_t kHalfBytes = 2;
constexpr std::size_t kTables = 3;
constexpr std::size_t kRowsTable = 1;
constexpr std::size_t kOffsetsTable = 2;
constexpr std::uint16_t kNoRow = 0xFF;
// An entry's SEQ header: the map length, a zero byte, the bones, then the
// bank's size rounded up to a word.
constexpr std::size_t kSeqHeaderBytes = 8;
constexpr std::size_t kSeqSizeAt = 4;
constexpr std::size_t kWordBytes = 4;
constexpr int kBitsPerByte = 8;

template <typename T>
T ReadLe(Bytes data, std::size_t offset) {
  T value{};
  std::memcpy(&value, data.subspan(offset, sizeof value).data(), sizeof value);
  return value;
}

std::unexpected<std::string> Fail(std::size_t offset,
                                  std::string_view message) {
  return std::unexpected(std::format("byte 0x{:X}: {}", offset, message));
}

std::size_t HeaderBytes(std::size_t count) {
  return kCountBytes + (kTables * kFieldBytes * count);
}

std::size_t FieldAt(std::size_t count, std::size_t table, std::size_t entry) {
  return kCountBytes + (((table * count) + entry) * kFieldBytes);
}

std::size_t RoundUp(std::size_t size) {
  return (size + kWordBytes - 1) / kWordBytes * kWordBytes;
}

void PutLe16(std::vector<std::byte>& out, std::uint16_t value) {
  out.push_back(static_cast<std::byte>(value));
  out.push_back(static_cast<std::byte>(value >> kBitsPerByte));
}

void PutLe32(std::vector<std::byte>& out, std::uint32_t value) {
  PutLe16(out, static_cast<std::uint16_t>(value));
  PutLe16(out, static_cast<std::uint16_t>(value >> (kBitsPerByte * 2)));
}

// The halfword `entry` holds in `table`, whose second halfword must be zero.
std::expected<std::uint16_t, std::string> TableHalf(Bytes data,
                                                    std::size_t count,
                                                    std::size_t table,
                                                    std::size_t entry) {
  const std::size_t at = FieldAt(count, table, entry);
  if (ReadLe<std::uint16_t>(data, at + kHalfBytes) != 0) {
    return Fail(at + kHalfBytes,
                std::format("the halfword after entry {}'s {} is not zero",
                            entry, table == 0 ? "selector" : "texture row"));
  }
  return ReadLe<std::uint16_t>(data, at);
}

// Reads the selector and texture row of entry `i` into `entry`.
std::expected<void, std::string> DecodeFields(Bytes data, std::size_t count,
                                              std::size_t i, EsqEntry& entry) {
  auto selector = TableHalf(data, count, 0, i);
  if (!selector) {
    return std::unexpected(selector.error());
  }
  if (*selector > kEsqMaxSelector) {
    return Fail(FieldAt(count, 0, i),
                std::format("entry {}'s selector is 0x{:X}, which ends the "
                            "game's list",
                            i, *selector));
  }
  auto row = TableHalf(data, count, kRowsTable, i);
  if (!row) {
    return std::unexpected(row.error());
  }
  if (*row > kNoRow) {
    return Fail(
        FieldAt(count, kRowsTable, i),
        std::format("entry {}'s texture row {} is more than a byte", i, *row));
  }
  entry.selector = *selector;
  if (*row != kNoRow) {
    entry.texture_row = static_cast<std::uint8_t>(*row);
  }
  return {};
}

// Decodes entry `i`, which must start at `at`, and moves `at` past it.
std::expected<void, std::string> DecodeEntry(
    Bytes data, std::size_t count, std::size_t i, std::size_t& at,
    std::span<const std::uint8_t> lengths, EsqEntry& entry) {
  const std::size_t field = FieldAt(count, kOffsetsTable, i);
  const std::size_t start =
      HeaderBytes(count) + ReadLe<std::uint32_t>(data, field);
  if (start != at) {
    return Fail(field, std::format("entry {} starts at byte 0x{:X}, but the "
                                   "entry before it ends at 0x{:X}",
                                   i, start, at));
  }
  if (data.size() - at < kSeqHeaderBytes) {
    return Fail(at, std::format("entry {} is cut off in its {}-byte header", i,
                                kSeqHeaderBytes));
  }
  const std::size_t size = ReadLe<std::uint32_t>(data, at + kSeqSizeAt);
  if (size > data.size() - at - kSeqHeaderBytes) {
    return Fail(at + kSeqSizeAt,
                std::format("entry {}'s size field says {} bytes, past the end "
                            "of the file",
                            i, size));
  }
  auto animation =
      DecodeSeqFile(data.subspan(at, kSeqHeaderBytes + size), lengths);
  if (!animation) {
    return std::unexpected(
        std::format("entry {} (selector {}) at byte 0x{:X}: {}", i,
                    entry.selector, at, animation.error()));
  }
  entry.animation = std::move(*animation);
  at += kSeqHeaderBytes + size;
  return {};
}
}  // namespace

std::expected<EsqFile, std::string> DecodeEsqFile(
    std::span<const std::byte> data,
    std::span<const std::uint8_t> event_lengths) {
  if (data.size() < kCountBytes) {
    return Fail(0, std::format("the file is shorter than its {}-byte count",
                               kCountBytes));
  }
  const auto count = ReadLe<std::int32_t>(data, 0);
  if (count < 0 || static_cast<std::size_t>(count) > kEsqMaxEntries) {
    return Fail(
        0, std::format("the count {} is not 0 to {}", count, kEsqMaxEntries));
  }
  const auto entries = static_cast<std::size_t>(count);
  if (data.size() < HeaderBytes(entries)) {
    return Fail(0, std::format("the {}-entry header needs {} bytes, but the "
                               "file has {}",
                               entries, HeaderBytes(entries), data.size()));
  }
  EsqFile file;
  file.entries.resize(entries);
  std::size_t at = HeaderBytes(entries);
  for (std::size_t i = 0; i < entries; ++i) {
    if (auto fields = DecodeFields(data, entries, i, file.entries[i]);
        !fields) {
      return std::unexpected(fields.error());
    }
    if (auto entry =
            DecodeEntry(data, entries, i, at, event_lengths, file.entries[i]);
        !entry) {
      return std::unexpected(entry.error());
    }
  }
  if (at != data.size()) {
    return Fail(
        at, std::format("{} bytes follow the last entry", data.size() - at));
  }
  return file;
}

std::expected<std::vector<std::byte>, std::string> EncodeEsqFile(
    const EsqFile& file, std::span<const std::uint8_t> event_lengths) {
  const std::size_t count = file.entries.size();
  if (count > kEsqMaxEntries) {
    return std::unexpected(
        std::format("{} entries are more than {}", count, kEsqMaxEntries));
  }
  std::vector<std::byte> bodies;
  std::vector<std::uint32_t> offsets;
  for (std::size_t i = 0; i < count; ++i) {
    const EsqEntry& entry = file.entries[i];
    if (entry.selector > kEsqMaxSelector) {
      return std::unexpected(
          std::format("entry {}'s selector 0x{:X} ends the game's list", i,
                      entry.selector));
    }
    if (entry.texture_row && *entry.texture_row > kEsqMaxTextureRow) {
      return std::unexpected(
          std::format("entry {}'s texture row {} is the game's 'no row'", i,
                      *entry.texture_row));
    }
    auto animation = EncodeSeqFile(entry.animation, event_lengths);
    if (!animation) {
      return std::unexpected(std::format("entry {} (selector {}): {}", i,
                                         entry.selector, animation.error()));
    }
    if (bodies.size() > std::numeric_limits<std::uint32_t>::max()) {
      return std::unexpected(
          std::format("entry {} starts past a 32-bit offset", i));
    }
    offsets.push_back(static_cast<std::uint32_t>(bodies.size()));
    bodies.insert(bodies.end(), animation->begin(), animation->end());
    bodies.resize(RoundUp(bodies.size()), std::byte{0});
  }
  std::vector<std::byte> out;
  out.reserve(HeaderBytes(count) + bodies.size());
  PutLe32(out, static_cast<std::uint32_t>(count));
  for (const EsqEntry& entry : file.entries) {
    PutLe32(out, entry.selector);
  }
  for (const EsqEntry& entry : file.entries) {
    PutLe32(out, entry.texture_row.value_or(kNoRow));
  }
  for (const std::uint32_t offset : offsets) {
    PutLe32(out, offset);
  }
  out.insert(out.end(), bodies.begin(), bodies.end());
  if (auto decoded = DecodeEsqFile(out, event_lengths); !decoded) {
    return std::unexpected("the encoded file does not decode: " +
                           decoded.error());
  }
  return out;
}

}  // namespace VS::Content
