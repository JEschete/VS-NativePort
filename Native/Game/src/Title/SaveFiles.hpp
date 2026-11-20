#pragma once
// TITLE.PRG's file screens: the font, UI and background images from the
// module image, and the screens' elements, flags and transfer progress.
#include <cstddef>
#include <cstdint>

#include "Menu/Files.hpp"

namespace VS::Save {
struct CardFileInfoRecord;
}  // namespace VS::Save

namespace VS::Title {
inline constexpr std::size_t kSaveFontRows = 220;
inline constexpr std::size_t kSaveAtlasRowBytes = 128;
inline constexpr std::size_t kSaveFontPalettes = 16;
inline constexpr std::size_t kSaveFontPaletteColors = 16;
inline constexpr std::size_t kSaveBackgroundColors = 256;
inline constexpr std::size_t kSaveBackgroundRows = 240;
inline constexpr std::size_t kSaveBackgroundRowBytes = 192;
inline constexpr std::size_t kSaveUiPageZeroRows = 224;
inline constexpr std::size_t kSaveUiPageOneRows = 256;
inline constexpr std::size_t kSaveFontPages = 2;
inline constexpr std::size_t kDebugFontWords = 4928;

// decomp: fontTable_t (src/TITLE/TITLE.PRG/22C.c:3112)
// One 4-bit font page and its palettes.
struct SaveMenuFontRecord {
  /* 0x0000 */ std::uint8_t indexed_pixels[kSaveFontRows][kSaveAtlasRowBytes];
  /* 0x6E00 */ std::uint16_t palettes[kSaveFontPalettes]
                                     [kSaveFontPaletteColors];
};

// decomp: menuBg_t (src/TITLE/TITLE.PRG/22C.c:3122)
struct SaveMenuBackgroundRecord {
  /* 0x0000 */ std::uint16_t palette[kSaveBackgroundColors];
  /* 0x0200 */ std::uint8_t indexed_pixels[kSaveBackgroundRows]
                                          [kSaveBackgroundRowBytes];
};

// decomp: uiTable_t (src/TITLE/TITLE.PRG/22C.c:3117)
// The two 4-bit UI pages.
struct SaveMenuUiRecord {
  /* 0x0000 */ std::uint8_t page_zero_pixels[kSaveUiPageZeroRows]
                                            [kSaveAtlasRowBytes];
  /* 0x7000 */ std::uint8_t page_one_pixels[kSaveUiPageOneRows]
                                           [kSaveAtlasRowBytes];
};

namespace layout {
inline constexpr std::size_t kSaveMenuFontRecordBytes = 0x7000;
inline constexpr std::size_t kSaveMenuBackgroundRecordBytes = 0xB600;
inline constexpr std::size_t kSaveMenuUiRecordBytes = 0xF000;
static_assert(sizeof(SaveMenuFontRecord) == kSaveMenuFontRecordBytes);
static_assert(sizeof(SaveMenuBackgroundRecord) ==
              kSaveMenuBackgroundRecordBytes);
static_assert(sizeof(SaveMenuUiRecord) == kSaveMenuUiRecordBytes);
}  // namespace layout

// decomp: _fontTable (_fontTable@TITLE/TITLE.PRG)
extern SaveMenuFontRecord saveMenuFontAtlas[kSaveFontPages];
// decomp: _debugFont (_debugFont@TITLE/TITLE.PRG)
extern std::uint32_t debugFontAtlas[kDebugFontWords];
// decomp: _uiTable (_uiTable@TITLE/TITLE.PRG)
extern SaveMenuUiRecord saveMenuUiAtlas;
// decomp: _backupMainSetting (_backupMainSetting@src/TITLE/TITLE.PRG/22C.c)
extern std::uint8_t savedMainMenuSetting;
// decomp: _containerDataEmpty (_containerDataEmpty@src/TITLE/TITLE.PRG/22C.c)
extern std::uint8_t isContainerDataEmpty;
// decomp: _dataNotSaved (_dataNotSaved@src/TITLE/TITLE.PRG/22C.c)
extern std::uint8_t hasUnsavedData;
// decomp: _fileMenuElements (_fileMenuElements@src/TITLE/TITLE.PRG/22C.c)
extern Menu::Files::Element fileMenuElements[Menu::Files::kElementCount];
// decomp: _fileMenuScreenFade (_fileMenuScreenFade@src/TITLE/TITLE.PRG/22C.c)
extern std::uint8_t fileMenuScreenFade;
// decomp: _filePreviousProgressCounter
// (_filePreviousProgressCounter@src/TITLE/TITLE.PRG/22C.c)
extern std::uint16_t previousTransferProgress;
// decomp: _fileProgressCounter (_fileProgressCounter@src/TITLE/TITLE.PRG/22C.c)
extern std::int32_t transferProgressCounter;
// decomp: _fileProgressPosition
// (_fileProgressPosition@src/TITLE/TITLE.PRG/22C.c)
extern std::uint16_t transferProgressPosition;
// decomp: _fileProgressTarget (_fileProgressTarget@src/TITLE/TITLE.PRG/22C.c)
extern std::uint16_t transferProgressTarget;
// decomp: _isSaving (_isSaving@src/TITLE/TITLE.PRG/22C.c)
extern std::uint8_t isSaving;
// decomp: _memoryCardMessage (_memoryCardMessage@src/TITLE/TITLE.PRG/22C.c)
extern char* memoryCardMessage;
// decomp: _saveFileInfo (_saveFileInfo@src/TITLE/TITLE.PRG/22C.c)
extern Save::CardFileInfoRecord* saveFileSummaries;
// decomp: _saveMenuBg_clut (_saveMenuBg_clut@TITLE/TITLE.PRG)
extern SaveMenuBackgroundRecord saveMenuBackground;
// decomp: _selectCursorColor (_selectCursorColor@src/TITLE/TITLE.PRG/22C.c)
extern std::uint8_t selectionCursorColor;
// decomp: _selectCursorXy (_selectCursorXy@src/TITLE/TITLE.PRG/22C.c)
extern std::int32_t selectionCursorPosition;
// decomp: _spmcimg (_spmcimg@src/TITLE/TITLE.PRG/22C.c)
extern char* memoryCardUiResource;
// decomp: _textTable (_textTable@src/TITLE/TITLE.PRG/22C.c)
extern std::uint16_t* saveMenuText;
}  // namespace VS::Title
