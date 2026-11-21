#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The PlayStation TIM file, decoded strictly for the asset pipeline
// (ASSET_PIPELINE.md §6.4, #208). A TIM is an id word (byte 0 = 0x10, byte 1
// a version), a flag word (pixel mode in bits 0-2, bit 3 = a CLUT block
// follows), an optional CLUT block and the image block. Each block is a byte
// length (12 + its pixels), its VRAM rectangle (x, y, width, height; width in
// halfwords) and its halfwords. The decoder keeps every word, so EncodeTim
// rebuilds the exact bytes; it rejects what it cannot keep with a diagnostic
// naming the field and byte offset. The game's own block parse
// (Render/TimImage.hpp) keeps the original's lenient rules.
namespace VS::Content {

inline constexpr std::uint8_t kTimMagic = 0x10;
inline constexpr std::size_t kTimFileHeaderBytes = 8;
inline constexpr std::size_t kTimBlockHeaderBytes = 12;
// VRAM is 1024 x 512 halfwords; no block can cover more.
inline constexpr std::uint32_t kTimVramWidth = 1024;
inline constexpr std::uint32_t kTimVramHeight = 512;

enum class TimPixelMode : std::uint8_t {
  kIndexed4 = 0,
  kIndexed8 = 1,
  kDirect15 = 2,
  kDirect24 = 3,
};

// Width in halfwords, as the file stores it.
struct TimRect {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;

  bool operator==(const TimRect&) const = default;
};

struct TimBlock {
  TimRect rect;
  std::vector<std::uint16_t> words;  // rect.width x rect.height, row-major

  bool operator==(const TimBlock&) const = default;
};

struct TimFile {
  std::uint32_t id = kTimMagic;  // the whole word, version byte included
  std::uint32_t flags = 0;       // the whole word
  std::optional<TimBlock> clut;
  TimBlock image;

  [[nodiscard]] TimPixelMode Mode() const noexcept;
  [[nodiscard]] bool HasClut() const noexcept;
  bool operator==(const TimFile&) const = default;
};

enum class TimError : std::uint8_t {
  kTruncatedHeader,
  kBadId,
  kUnsupportedMode,
  kUnknownFlags,
  kTruncatedBlock,
  kBlockLength,
  kEmptyRect,
  kOutsideVram,
};

struct TimDiagnostic {
  TimError error = TimError::kTruncatedHeader;
  std::size_t offset = 0;  // the byte offset of the offending field
  std::string message;
};

struct DecodedTim {
  TimFile tim;
  std::size_t bytes = 0;  // the bytes the TIM occupies from the start
};

// Decodes the TIM at the start of `data`; bytes after it are the caller's
// (a container's next payload, or an error for a standalone file).
[[nodiscard]] std::expected<DecodedTim, TimDiagnostic> DecodeTim(
    std::span<const std::byte> data);
[[nodiscard]] std::string_view TimErrorName(TimError error) noexcept;

}  // namespace VS::Content
