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

}  // namespace VS::Extract
