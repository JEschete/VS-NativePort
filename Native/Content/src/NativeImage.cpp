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

std::expected<NativeImage, std::string> ImageFromTim(const TimFile& tim,
                                                     ImageDepth depth) {
  const TimPixelMode mode = tim.Mode();
  if (mode == TimPixelMode::kDirect24) {
    return std::unexpected(
        "24-bit TIMs have no native image form yet; none is in the "
        "inventoried corpus");
  }
  NativeImage image{.depth = depth,
                    .width = tim.image.rect.width,
                    .height = tim.image.rect.height,
                    .indices = {},
                    .texels = {},
                    .palettes = {}};
  if (!IsIndexed(depth)) {
    if (mode != TimPixelMode::kDirect15 || tim.clut) {
      return std::unexpected(
          std::format("a TIM in mode {}{} is not a 16-bit image",
                      static_cast<int>(mode), tim.clut ? " with a CLUT" : ""));
    }
    image.texels = tim.image.words;
    if (auto valid = ValidateImage(image); !valid) {
      return std::unexpected(valid.error());
    }
    return image;
  }
  const TimPixelMode matching = depth == ImageDepth::kIndexed4
                                    ? TimPixelMode::kIndexed4
                                    : TimPixelMode::kIndexed8;
  if (mode != matching && mode != TimPixelMode::kDirect15) {
    return std::unexpected(
        std::format("a TIM in mode {} cannot be drawn as a {}-bit image",
                    static_cast<int>(mode), static_cast<int>(depth)));
  }
  if (!tim.clut) {
    return std::unexpected(std::format(
        "the {}-bit image has no CLUT block; an image whose palette lives "
        "elsewhere needs that palette's source",
        static_cast<int>(depth)));
  }
  const std::vector<std::uint16_t>& clut = tim.clut->words;
  const std::size_t capacity = PaletteCapacity(depth);
  const std::size_t entries = std::min(capacity, clut.size());
  if (entries == 0 || clut.size() % entries != 0) {
    return std::unexpected(
        std::format("the CLUT's {} words do not divide into {}-entry palettes",
                    clut.size(), capacity));
  }
  for (std::size_t first = 0; first < clut.size(); first += entries) {
    image.palettes.emplace_back(
        clut.begin() + static_cast<std::ptrdiff_t>(first),
        clut.begin() + static_cast<std::ptrdiff_t>(first + entries));
  }
  const std::uint32_t per_halfword = PixelsPerHalfword(depth);
  const unsigned bits = BitsPerPixel(depth);
  const unsigned index_mask = (1U << bits) - 1;
  image.width = tim.image.rect.width * per_halfword;
  image.indices.reserve(PixelCount(image));
  for (const std::uint16_t word : tim.image.words) {
    for (std::uint32_t pixel = 0; pixel < per_halfword; ++pixel) {
      image.indices.push_back(
          static_cast<std::uint8_t>((word >> (pixel * bits)) & index_mask));
    }
  }
  if (auto valid = ValidateImage(image); !valid) {
    return std::unexpected(valid.error());
  }
  return image;
}

std::vector<std::uint16_t> PackedWords(const NativeImage& image) {
  if (!IsIndexed(image.depth)) return image.texels;
  const std::uint32_t per_halfword = PixelsPerHalfword(image.depth);
  const unsigned bits = BitsPerPixel(image.depth);
  std::vector<std::uint16_t> words(image.indices.size() / per_halfword);
  for (std::size_t i = 0; i < words.size(); ++i) {
    unsigned word = 0;
    for (std::uint32_t pixel = 0; pixel < per_halfword; ++pixel) {
      word |= unsigned{image.indices[(i * per_halfword) + pixel]}
              << (pixel * bits);
    }
    words[i] = static_cast<std::uint16_t>(word);
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

TimOrigin OriginOf(const TimFile& tim) {
  return {.id = tim.id,
          .flags = tim.flags,
          .image = tim.image.rect,
          .clut = tim.clut ? std::optional(tim.clut->rect) : std::nullopt};
}

std::expected<TimFile, std::string> TimFromImage(const NativeImage& image,
                                                 const TimOrigin& origin) {
  if (auto valid = ValidateImage(image); !valid) {
    return std::unexpected(valid.error());
  }
  TimFile tim{.id = origin.id, .flags = origin.flags, .clut = {}, .image = {}};
  tim.image.rect = origin.image;
  tim.image.rect.width =
      static_cast<std::uint16_t>(image.width / PixelsPerHalfword(image.depth));
  tim.image.rect.height = static_cast<std::uint16_t>(image.height);
  tim.image.words = PackedWords(image);
  if (IsIndexed(image.depth)) {
    std::vector<std::uint16_t> words = PaletteWords(image);
    if (!origin.clut ||
        std::size_t{origin.clut->width} * origin.clut->height != words.size()) {
      return std::unexpected(std::format(
          "the image's {} palette words do not fill its source CLUT "
          "rectangle",
          words.size()));
    }
    tim.clut = TimBlock{.rect = *origin.clut, .words = std::move(words)};
  } else if (origin.clut) {
    return std::unexpected(
        "a 16-bit image has no palettes for its source CLUT block");
  }
  if (tim.HasClut() != tim.clut.has_value()) {
    return std::unexpected(std::format(
        "the source flag word 0x{:08X} disagrees with the image's palettes",
        tim.flags));
  }
  return tim;
}

}  // namespace VS::Content
