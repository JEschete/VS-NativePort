#pragma once
// TITLE.PRG's title screen images, loaded from the module image.
#include <cstddef>
#include <cstdint>

namespace VS::Title {
// The encoded title background: 222,932 bytes.
inline constexpr std::size_t kTitleBackgroundWords = 55733;
inline constexpr std::size_t kCopyrightWidth = 180;
inline constexpr std::size_t kCopyrightHeight = 24;

// decomp: _titleScreenBg (_titleScreenBg@TITLE/TITLE.PRG)
extern std::uint32_t titleScreenBackground[kTitleBackgroundWords];
// decomp: _menuCopyright (_menuCopyright@TITLE/TITLE.PRG)
// 16-bit pixels.
extern std::uint16_t copyrightImage[kCopyrightWidth * kCopyrightHeight];
}  // namespace VS::Title
