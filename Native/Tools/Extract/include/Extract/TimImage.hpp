#pragma once

#include <expected>
#include <string>
#include <vector>

#include "Content/NativeImage.hpp"
#include "Content/TimFile.hpp"

namespace VS::Extract {

// Converts the source TIM to the consumer's native image depth. The TIM model
// and image data are read from VS.Content; import-only conversion stays here.
[[nodiscard]] std::expected<Content::NativeImage, std::string> ImageFromTim(
    const Content::TimFile& tim, Content::ImageDepth depth);

[[nodiscard]] Content::TimOrigin OriginOf(const Content::TimFile& tim);
[[nodiscard]] std::expected<Content::TimFile, std::string> TimFromImage(
    const Content::NativeImage& image, const Content::TimOrigin& origin);

}  // namespace VS::Extract
