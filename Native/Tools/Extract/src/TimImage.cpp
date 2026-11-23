#include "Extract/TimImage.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace VS::Extract {
namespace {
[[nodiscard]] std::size_t PixelCount(const Content::NativeImage& image) {
  return std::size_t{image.width} * image.height;
}
}  // namespace

std::expected<Content::NativeImage, std::string> ImageFromTim(
    const Content::TimFile& tim, Content::ImageDepth depth) {
  const Content::TimPixelMode mode = tim.Mode();
  if (mode == Content::TimPixelMode::kDirect24) {
    return std::unexpected(
        "24-bit TIMs have no native image form yet; none is in the "
        "inventoried corpus");
  }
  Content::NativeImage image{.depth = depth,
                             .width = tim.image.rect.width,
                             .height = tim.image.rect.height,
                             .indices = {},
                             .texels = {},
                             .palettes = {}};
  if (!Content::IsIndexed(depth)) {
    if (mode != Content::TimPixelMode::kDirect15 || tim.clut) {
      return std::unexpected(
          std::format("a TIM in mode {}{} is not a 16-bit image",
                      static_cast<int>(mode), tim.clut ? " with a CLUT" : ""));
    }
    image.texels = tim.image.words;
    if (auto valid = Content::ValidateImage(image); !valid) {
      return std::unexpected(valid.error());
    }
    return image;
  }
  const Content::TimPixelMode matching = depth == Content::ImageDepth::kIndexed4
                                             ? Content::TimPixelMode::kIndexed4
                                             : Content::TimPixelMode::kIndexed8;
  if (mode != matching && mode != Content::TimPixelMode::kDirect15) {
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
  const std::size_t capacity = Content::PaletteCapacity(depth);
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
  const std::uint32_t per_halfword = Content::PixelsPerHalfword(depth);
  const unsigned bits = Content::BitsPerPixel(depth);
  const unsigned index_mask = (1U << bits) - 1;
  image.width = tim.image.rect.width * per_halfword;
  image.indices.reserve(PixelCount(image));
  for (const std::uint16_t word : tim.image.words) {
    for (std::uint32_t pixel = 0; pixel < per_halfword; ++pixel) {
      image.indices.push_back(
          static_cast<std::uint8_t>((word >> (pixel * bits)) & index_mask));
    }
  }
  if (auto valid = Content::ValidateImage(image); !valid) {
    return std::unexpected(valid.error());
  }
  return image;
}

}  // namespace VS::Extract
