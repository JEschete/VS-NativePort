// The native image model (NativeImage.hpp).
#include "Content/NativeImage.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace VS::Content {
namespace {
constexpr unsigned kChannelMask = kChannelMax;
constexpr unsigned kGreenShift = kChannelBits;
constexpr unsigned kBlueShift = 2 * kChannelBits;

[[nodiscard]] std::size_t PixelCount(const NativeImage& image) {
  return std::size_t{image.width} * image.height;
}
}  // namespace

ImageColor ColorOfWord(std::uint16_t word) noexcept {
  if (word == kTransparentWord) return {};
  return {.red = ExpandChannel(static_cast<std::uint8_t>(word & kChannelMask)),
          .green = ExpandChannel(
              static_cast<std::uint8_t>((word >> kGreenShift) & kChannelMask)),
          .blue = ExpandChannel(
              static_cast<std::uint8_t>((word >> kBlueShift) & kChannelMask)),
          .alpha = kOpaqueAlpha};
}

std::optional<std::uint16_t> WordOfColor(ImageColor color, bool stp) noexcept {
  if (color.alpha == 0) return kTransparentWord;
  if (color.alpha != kOpaqueAlpha) return std::nullopt;
  const auto word = static_cast<std::uint16_t>(
      NarrowChannel(color.red) | (NarrowChannel(color.green) << kGreenShift) |
      (NarrowChannel(color.blue) << kBlueShift));
  if (word == 0 || stp) return static_cast<std::uint16_t>(word | kStpBit);
  return word;
}

std::expected<void, std::string> ValidateImageSize(ImageDepth depth,
                                                   std::uint32_t width,
                                                   std::uint32_t height) {
  if (!IsKnownDepth(depth)) {
    return std::unexpected(
        std::format("the image depth {} is not 4, 8 or 16 bits per pixel",
                    static_cast<int>(depth)));
  }
  const std::uint32_t per_halfword = PixelsPerHalfword(depth);
  if (width == 0 || height == 0) {
    return std::unexpected(
        std::format("the image is {}x{}; it needs pixels", width, height));
  }
  if (width % per_halfword != 0 || width / per_halfword > kTimVramWidth ||
      height > kTimVramHeight) {
    return std::unexpected(std::format(
        "a {}x{} image at {} bits per pixel does not fit whole halfwords of "
        "the {}x{} VRAM; its width must be a multiple of {}",
        width, height, static_cast<int>(depth), kTimVramWidth, kTimVramHeight,
        per_halfword));
  }
  return {};
}

std::expected<void, std::string> ValidateImage(const NativeImage& image) {
  if (auto size = ValidateImageSize(image.depth, image.width, image.height);
      !size) {
    return size;
  }
  const std::size_t pixels = PixelCount(image);
  if (!IsIndexed(image.depth)) {
    if (image.texels.size() != pixels || !image.indices.empty() ||
        !image.palettes.empty()) {
      return std::unexpected(std::format(
          "a direct {}x{} image needs {} texel words and no indices or "
          "palettes; it has {} words, {} indices and {} palettes",
          image.width, image.height, pixels, image.texels.size(),
          image.indices.size(), image.palettes.size()));
    }
    return {};
  }
  if (image.indices.size() != pixels || !image.texels.empty()) {
    return std::unexpected(std::format(
        "an indexed {}x{} image needs {} indices and no texel words; it has "
        "{} indices and {} words",
        image.width, image.height, pixels, image.indices.size(),
        image.texels.size()));
  }
  const std::size_t entries = image.PaletteEntries();
  const std::size_t capacity = PaletteCapacity(image.depth);
  if (entries == 0 || entries > capacity) {
    return std::unexpected(std::format(
        "a {}-bit image needs palettes of 1 to {} entries; it has {} "
        "palettes of {}",
        static_cast<int>(image.depth), capacity, image.palettes.size(),
        entries));
  }
  for (std::size_t row = 0; row < image.palettes.size(); ++row) {
    if (image.palettes[row].size() != entries) {
      return std::unexpected(
          std::format("palette {} has {} entries; palette 0 has {}", row,
                      image.palettes[row].size(), entries));
    }
  }
  for (std::size_t i = 0; i < pixels; ++i) {
    if (image.indices[i] >= entries) {
      return std::unexpected(std::format(
          "pixel ({}, {}) uses palette index {}; the palettes have {} entries",
          i % image.width, i / image.width, image.indices[i], entries));
    }
  }
  return {};
}

std::vector<std::uint16_t> PackedWords(const NativeImage& image) {
  if (!IsIndexed(image.depth)) return image.texels;
  const std::uint32_t per_halfword = PixelsPerHalfword(image.depth);
  const unsigned bits = BitsPerPixel(image.depth);
  std::vector<std::uint16_t> words(image.indices.size() / per_halfword);
  for (std::size_t index = 0; index < words.size(); ++index) {
    unsigned word = 0;
    for (std::uint32_t pixel = 0; pixel < per_halfword; ++pixel) {
      word |= unsigned{image.indices[(index * per_halfword) + pixel]}
              << (pixel * bits);
    }
    words[index] = static_cast<std::uint16_t>(word);
  }
  return words;
}

std::vector<std::uint16_t> PaletteWords(const NativeImage& image) {
  std::vector<std::uint16_t> words;
  words.reserve(image.palettes.size() * image.PaletteEntries());
  for (const auto& row : image.palettes) {
    words.insert(words.end(), row.begin(), row.end());
  }
  return words;
}

}  // namespace VS::Content
