// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Title/TitleScreen.hpp"

// decomp: _titleScreenBg (_titleScreenBg@TITLE/TITLE.PRG)
// contents: module image (ModuleData.cpp)
std::uint32_t
    VS::Title::titleScreenBackground[VS::Title::kTitleBackgroundWords];

// decomp: _menuCopyright (_menuCopyright@TITLE/TITLE.PRG)
// contents: module image (ModuleData.cpp)
std::uint16_t VS::Title::copyrightImage[VS::Title::kCopyrightWidth *
                                        VS::Title::kCopyrightHeight];
