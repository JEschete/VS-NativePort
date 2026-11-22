#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "Content/TimFile.hpp"

// The native image (ASSET_PIPELINE.md §6.4, #208): pixels at their true depth
// and exact PlayStation colour words. Indexed images keep one palette index per
// pixel and every palette word, unused entries included; direct images keep
// one word per pixel. A colour word is 5-bit red (bits 0-4), green (5-9) and
// blue (10-14) and the semi-transparency (STP) bit 15. The word 0x0000 is the
// colour textures never draw; opaque black is 0x8000.
namespace VS::Content {

enum class ImageDepth : std::uint8_t {
  kIndexed4 = 4,
  kIndexed8 = 8,
  kDirect16 = 16,
};

inline constexpr std::uint16_t kTransparentWord = 0x0000;
inline constexpr std::uint16_t kStpBit = 0x8000;
inline constexpr std::uint16_t kColorBits = 0x7FFF;
inline constexpr std::uint8_t kOpaqueAlpha = 0xFF;
inline constexpr std::uint32_t kHalfwordBits = 16;
// A colour word's channels are 5 bits; an editor's are 8.
inline constexpr unsigned kChannelBits = 5;
inline constexpr unsigned kChannelMax = 31;
inline constexpr unsigned kEditorChannelBits = 8;
inline constexpr unsigned kEditorChannelMax = 255;

// Whether `depth` is one of the three depths; a cast can make any other value,
// which ValidateImage rejects.
[[nodiscard]] constexpr bool IsKnownDepth(ImageDepth depth) noexcept {
  return depth == ImageDepth::kIndexed4 || depth == ImageDepth::kIndexed8 ||
         depth == ImageDepth::kDirect16;
}
[[nodiscard]] constexpr bool IsIndexed(ImageDepth depth) noexcept {
  return depth == ImageDepth::kIndexed4 || depth == ImageDepth::kIndexed8;
}
// Bits one pixel takes: the depth's own value. An unknown depth counts as 16,
// so the helpers below never divide by zero.
[[nodiscard]] constexpr std::uint32_t BitsPerPixel(ImageDepth depth) noexcept {
  return IsKnownDepth(depth) ? static_cast<std::uint32_t>(depth)
                             : kHalfwordBits;
}
// Palette entries an index of this depth can address (0 for direct).
[[nodiscard]] constexpr std::size_t PaletteCapacity(ImageDepth depth) noexcept {
  return IsIndexed(depth) ? std::size_t{1} << BitsPerPixel(depth) : 0;
}
// Pixels one VRAM halfword holds.
[[nodiscard]] constexpr std::uint32_t PixelsPerHalfword(
    ImageDepth depth) noexcept {
  return kHalfwordBits / BitsPerPixel(depth);
}

// 5-bit channel to 8 bits by bit replication, and back to the nearest 5-bit
// value; NarrowChannel(ExpandChannel(c)) == c for every c.
[[nodiscard]] constexpr std::uint8_t ExpandChannel(std::uint8_t five) noexcept {
  return static_cast<std::uint8_t>(
      (five << (kEditorChannelBits - kChannelBits)) |
      (five >> ((2 * kChannelBits) - kEditorChannelBits)));
}
[[nodiscard]] constexpr std::uint8_t NarrowChannel(
    std::uint8_t eight) noexcept {
  return static_cast<std::uint8_t>(
      ((eight * kChannelMax) + (kEditorChannelMax / 2)) / kEditorChannelMax);
}

struct ImageColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t alpha = 0;

  bool operator==(const ImageColor&) const = default;
};

// The editor colour of a word: alpha 0 for 0x0000, else 255.
[[nodiscard]] ImageColor ColorOfWord(std::uint16_t word) noexcept;
// Whether a word's STP bit is information an image file must carry: set on an
// opaque colour other than black (0x8000 always has it).
[[nodiscard]] constexpr bool CarriesStp(std::uint16_t word) noexcept {
  return (word & kStpBit) != 0 && (word & kColorBits) != 0;
}
// The word of an editor colour: alpha 0 is 0x0000 whatever the colour, opaque
// black is 0x8000, and `stp` sets bit 15 of other opaque colours. Nothing for
// partial alpha, which the hardware cannot show.
[[nodiscard]] std::optional<std::uint16_t> WordOfColor(ImageColor color,
                                                       bool stp) noexcept;

struct NativeImage {
  ImageDepth depth = ImageDepth::kIndexed8;
  std::uint32_t width = 0;  // pixels
  std::uint32_t height = 0;
  std::vector<std::uint8_t> indices;  // indexed: width x height, row-major
  std::vector<std::uint16_t> texels;  // direct: width x height words
  // Indexed: one or more rows of the same length, at most PaletteCapacity.
  std::vector<std::vector<std::uint16_t>> palettes;

  [[nodiscard]] std::size_t PaletteEntries() const noexcept {
    return palettes.empty() ? 0 : palettes.front().size();
  }
  bool operator==(const NativeImage&) const = default;
};

// Whether a `width` x `height` image at `depth` packs into whole halfwords
// that fit VRAM (1024 x 512 halfwords).
[[nodiscard]] std::expected<void, std::string> ValidateImageSize(
    ImageDepth depth, std::uint32_t width, std::uint32_t height);
// Whether the image is complete and consistent: sizes, palette rows, every
// index within its palette, and a width that packs into whole halfwords.
[[nodiscard]] std::expected<void, std::string> ValidateImage(
    const NativeImage& image);
// Pixels packed into the halfwords consumed by the hardware upload APIs, and
// all palette rows in file order.
[[nodiscard]] std::vector<std::uint16_t> PackedWords(const NativeImage& image);
[[nodiscard]] std::vector<std::uint16_t> PaletteWords(const NativeImage& image);

// Where a TIM-sourced image came from: the words the image does not carry.
struct TimOrigin {
  std::uint32_t id = kTimMagic;
  std::uint32_t flags = 0;
  TimRect image;
  std::optional<TimRect> clut;

  bool operator==(const TimOrigin&) const = default;
};

}  // namespace VS::Content
