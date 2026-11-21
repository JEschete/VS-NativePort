#pragma once
// The "Now Loading" image the executable carries: its pixels and its size
// header. The original toolchain's plain char was unsigned, so the header's
// bytes are std::uint8_t.
#include <cstddef>
#include <cstdint>

namespace VS::Render {
inline constexpr std::size_t kLoadingScreenPixelWords = 1260;

// decomp: ImgHeader (src/SLUS_010.40/main.c:192-197; was imgHeader_t)
struct LegacyImageSize {
  std::uint8_t width_low;
  std::uint8_t width_high;
  std::uint8_t height_low;
  std::uint8_t height_high;
};

// decomp: _nowLoading_data (_nowLoading_data@SLUS_010.40)
extern std::uint32_t loadingScreenPixels[kLoadingScreenPixelWords];
// decomp: _nowLoading_header (_nowLoading_header@SLUS_010.40)
extern LegacyImageSize legacyLoadingImageHeader;

namespace layout {
inline constexpr std::size_t kLegacyImageSizeBytes = 0x4;
static_assert(sizeof(LegacyImageSize) == kLegacyImageSizeBytes);
}  // namespace layout
}  // namespace VS::Render
