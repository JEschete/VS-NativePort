// Strict TIM decoding and encoding (TimFile.hpp).
#include "Content/TimFile.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace VS::Content {
namespace {
constexpr std::uint32_t kIdMagicMask = 0xFF;
constexpr std::uint32_t kModeMask = 0x7;
constexpr std::uint32_t kClutFlag = 0x8;
constexpr std::uint32_t kKnownFlags = kModeMask | kClutFlag;
constexpr std::uint32_t kFirstUnsupportedMode = 4;
constexpr std::size_t kHalfwordBytes = 2;
constexpr std::size_t kFlagsOffset = 4;
constexpr std::size_t kRectOffset = 4;
constexpr std::size_t kYOffset = kRectOffset + 2;
constexpr std::size_t kWidthOffset = kRectOffset + 4;
constexpr std::size_t kHeightOffset = kRectOffset + 6;
constexpr std::size_t kByteBits = 8;
constexpr unsigned kByteMask = 0xFF;

template <typename T>
T ReadLe(std::span<const std::byte> data, std::size_t offset) {
  T value{};
  std::memcpy(&value, data.subspan(offset, sizeof value).data(), sizeof value);
  return value;
}

std::unexpected<TimDiagnostic> Fail(TimError error, std::size_t offset,
                                    std::string message) {
  return std::unexpected(TimDiagnostic{
      .error = error, .offset = offset, .message = std::move(message)});
}

struct BlockRead {
  TimBlock block;
  std::size_t end = 0;
};

std::expected<BlockRead, TimDiagnostic> ReadBlock(
    std::span<const std::byte> data, std::size_t offset,
    std::string_view name) {
  if (data.size() < offset || data.size() - offset < kTimBlockHeaderBytes) {
    return Fail(TimError::kTruncatedBlock, offset,
                std::format("the {} block header at byte {} needs {} bytes; "
                            "{} remain",
                            name, offset, kTimBlockHeaderBytes,
                            data.size() > offset ? data.size() - offset : 0));
  }
  const auto length = ReadLe<std::uint32_t>(data, offset);
  TimRect rect{.x = ReadLe<std::int16_t>(data, offset + kRectOffset),
               .y = ReadLe<std::int16_t>(data, offset + kYOffset),
               .width = ReadLe<std::uint16_t>(data, offset + kWidthOffset),
               .height = ReadLe<std::uint16_t>(data, offset + kHeightOffset)};
  if (rect.width == 0 || rect.height == 0) {
    return Fail(TimError::kEmptyRect, offset + kWidthOffset,
                std::format("the {} block's rectangle is {}x{}; a block "
                            "needs at least one halfword",
                            name, rect.width, rect.height));
  }
  if (rect.x < 0 || rect.y < 0 ||
      static_cast<std::uint32_t>(rect.x) + rect.width > kTimVramWidth ||
      static_cast<std::uint32_t>(rect.y) + rect.height > kTimVramHeight) {
    return Fail(TimError::kOutsideVram, offset + kRectOffset,
                std::format("the {} block's rectangle ({}, {}, {}x{}) extends "
                            "outside the {}x{} VRAM",
                            name, rect.x, rect.y, rect.width, rect.height,
                            kTimVramWidth, kTimVramHeight));
  }
  const std::size_t word_count = std::size_t{rect.width} * rect.height;
  const std::size_t pixel_bytes = word_count * kHalfwordBytes;
  if (length != kTimBlockHeaderBytes + pixel_bytes) {
    return Fail(TimError::kBlockLength, offset,
                std::format("the {} block declares {} bytes; its {}x{} "
                            "rectangle needs {}",
                            name, length, rect.width, rect.height,
                            kTimBlockHeaderBytes + pixel_bytes));
  }
  const std::size_t pixels_at = offset + kTimBlockHeaderBytes;
  if (data.size() - pixels_at < pixel_bytes) {
    return Fail(
        TimError::kTruncatedBlock, pixels_at,
        std::format("the {} block's pixels need {} bytes from byte "
                    "{}; {} remain",
                    name, pixel_bytes, pixels_at, data.size() - pixels_at));
  }
  BlockRead read{.block = {.rect = rect, .words = {}},
                 .end = pixels_at + pixel_bytes};
  read.block.words.resize(word_count);
  for (std::size_t i = 0; i < word_count; ++i) {
    read.block.words[i] =
        ReadLe<std::uint16_t>(data, pixels_at + (i * kHalfwordBytes));
  }
  return read;
}

template <typename T>
void AppendLe(std::vector<std::byte>& out, T value) {
  const auto bits =
      static_cast<std::uint32_t>(static_cast<std::make_unsigned_t<T>>(value));
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    out.push_back(
        static_cast<std::byte>((bits >> (kByteBits * index)) & kByteMask));
  }
}

std::expected<void, std::string> AppendBlock(std::vector<std::byte>& out,
                                             const TimBlock& block,
                                             std::string_view name) {
  const std::size_t words = std::size_t{block.rect.width} * block.rect.height;
  if (words == 0 || block.words.size() != words) {
    return std::unexpected(
        std::format("the {} block has {} words for a {}x{} rectangle", name,
                    block.words.size(), block.rect.width, block.rect.height));
  }
  AppendLe(out, static_cast<std::uint32_t>(kTimBlockHeaderBytes +
                                           (words * kHalfwordBytes)));
  AppendLe(out, block.rect.x);
  AppendLe(out, block.rect.y);
  AppendLe(out, block.rect.width);
  AppendLe(out, block.rect.height);
  for (const std::uint16_t word : block.words) AppendLe(out, word);
  return {};
}
}  // namespace

TimPixelMode TimFile::Mode() const noexcept {
  return static_cast<TimPixelMode>(flags & kModeMask);
}

bool TimFile::HasClut() const noexcept { return (flags & kClutFlag) != 0; }

std::expected<DecodedTim, TimDiagnostic> DecodeTim(
    std::span<const std::byte> data) {
  if (data.size() < kTimFileHeaderBytes) {
    return Fail(TimError::kTruncatedHeader, 0,
                std::format("a TIM header needs {} bytes; {} remain",
                            kTimFileHeaderBytes, data.size()));
  }
  DecodedTim decoded;
  TimFile& tim = decoded.tim;
  tim.id = ReadLe<std::uint32_t>(data, 0);
  tim.flags = ReadLe<std::uint32_t>(data, kFlagsOffset);
  if ((tim.id & kIdMagicMask) != kTimMagic) {
    return Fail(TimError::kBadId, 0,
                std::format("the id word 0x{:08X} does not start with the "
                            "TIM magic 0x{:02X}",
                            tim.id, kTimMagic));
  }
  if ((tim.flags & ~kKnownFlags) != 0) {
    return Fail(TimError::kUnknownFlags, kFlagsOffset,
                std::format("the flag word 0x{:08X} sets bits outside the "
                            "mode and CLUT flag",
                            tim.flags));
  }
  if ((tim.flags & kModeMask) >= kFirstUnsupportedMode) {
    return Fail(TimError::kUnsupportedMode, kFlagsOffset,
                std::format("pixel mode {} is not a TIM pixel mode (0-3)",
                            tim.flags & kModeMask));
  }
  std::size_t offset = kTimFileHeaderBytes;
  if (tim.HasClut()) {
    auto clut = ReadBlock(data, offset, "CLUT");
    if (!clut) return std::unexpected(std::move(clut.error()));
    tim.clut = std::move(clut->block);
    offset = clut->end;
  }
  auto image = ReadBlock(data, offset, "image");
  if (!image) return std::unexpected(std::move(image.error()));
  tim.image = std::move(image->block);
  decoded.bytes = image->end;
  return decoded;
}

std::string_view TimErrorName(TimError error) noexcept {
  switch (error) {
    case TimError::kTruncatedHeader:
      return "truncated-header";
    case TimError::kBadId:
      return "bad-id";
    case TimError::kUnsupportedMode:
      return "unsupported-mode";
    case TimError::kUnknownFlags:
      return "unknown-flags";
    case TimError::kTruncatedBlock:
      return "truncated-block";
    case TimError::kBlockLength:
      return "block-length";
    case TimError::kEmptyRect:
      return "empty-rect";
    case TimError::kOutsideVram:
      return "outside-vram";
  }
  return "unknown";
}

std::expected<std::vector<std::byte>, std::string> EncodeTim(
    const TimFile& tim) {
  if ((tim.id & kIdMagicMask) != kTimMagic) {
    return std::unexpected(
        std::format("the id word 0x{:08X} is not a TIM id", tim.id));
  }
  if ((tim.flags & ~kKnownFlags) != 0 ||
      (tim.flags & kModeMask) >= kFirstUnsupportedMode) {
    return std::unexpected(std::format(
        "the flag word 0x{:08X} is not a TIM flag word", tim.flags));
  }
  if (tim.HasClut() != tim.clut.has_value()) {
    return std::unexpected(std::format(
        "the flag word 0x{:08X} {} a CLUT block, but the TIM {} one", tim.flags,
        tim.HasClut() ? "declares" : "omits", tim.clut ? "has" : "has no"));
  }
  std::vector<std::byte> out;
  AppendLe(out, tim.id);
  AppendLe(out, tim.flags);
  if (tim.clut) {
    if (auto appended = AppendBlock(out, *tim.clut, "CLUT"); !appended) {
      return std::unexpected(appended.error());
    }
  }
  if (auto appended = AppendBlock(out, tim.image, "image"); !appended) {
    return std::unexpected(appended.error());
  }
  return out;
}

}  // namespace VS::Content
