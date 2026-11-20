// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Title/SaveFiles.hpp"

// decomp: _fontTable (_fontTable@TITLE/TITLE.PRG)
VS::Title::SaveMenuFontRecord VS::Title::saveMenuFontAtlas[VS::Title::kSaveFontPages]; /* contents: module image (ModuleData.cpp) */

// decomp: _debugFont (_debugFont@TITLE/TITLE.PRG)
std::uint32_t VS::Title::debugFontAtlas[VS::Title::kDebugFontWords]; /* contents: module image (ModuleData.cpp) */

// decomp: _uiTable (_uiTable@TITLE/TITLE.PRG)
VS::Title::SaveMenuUiRecord VS::Title::saveMenuUiAtlas; /* contents: module image (ModuleData.cpp) */

// decomp: _backupMainSetting (_backupMainSetting@src/TITLE/TITLE.PRG/22C.c)
std::uint8_t VS::Title::savedMainMenuSetting;

// decomp: _containerDataEmpty (_containerDataEmpty@src/TITLE/TITLE.PRG/22C.c)
std::uint8_t VS::Title::isContainerDataEmpty;

// decomp: _dataNotSaved (_dataNotSaved@src/TITLE/TITLE.PRG/22C.c)
std::uint8_t VS::Title::hasUnsavedData;

// decomp: _fileMenuElements (_fileMenuElements@src/TITLE/TITLE.PRG/22C.c)
VS::Menu::Files::Element VS::Title::fileMenuElements[VS::Menu::Files::kElementCount];

// decomp: _fileMenuScreenFade (_fileMenuScreenFade@src/TITLE/TITLE.PRG/22C.c)
std::uint8_t VS::Title::fileMenuScreenFade;

// decomp: _filePreviousProgressCounter (_filePreviousProgressCounter@src/TITLE/TITLE.PRG/22C.c)
std::uint16_t VS::Title::previousTransferProgress;

// decomp: _fileProgressCounter (_fileProgressCounter@src/TITLE/TITLE.PRG/22C.c)
std::int32_t VS::Title::transferProgressCounter;

// decomp: _fileProgressPosition (_fileProgressPosition@src/TITLE/TITLE.PRG/22C.c)
std::uint16_t VS::Title::transferProgressPosition;

// decomp: _fileProgressTarget (_fileProgressTarget@src/TITLE/TITLE.PRG/22C.c)
std::uint16_t VS::Title::transferProgressTarget;

// decomp: _isSaving (_isSaving@src/TITLE/TITLE.PRG/22C.c)
std::uint8_t VS::Title::isSaving;

// decomp: _memoryCardMessage (_memoryCardMessage@src/TITLE/TITLE.PRG/22C.c)
char* VS::Title::memoryCardMessage;

// decomp: _saveFileInfo (_saveFileInfo@src/TITLE/TITLE.PRG/22C.c)
VS::Save::CardFileInfoRecord* VS::Title::saveFileSummaries;

// decomp: _saveMenuBg_clut (_saveMenuBg_clut@TITLE/TITLE.PRG)
VS::Title::SaveMenuBackgroundRecord VS::Title::saveMenuBackground; /* contents: module image (ModuleData.cpp) */

// decomp: _selectCursorColor (_selectCursorColor@src/TITLE/TITLE.PRG/22C.c)
std::uint8_t VS::Title::selectionCursorColor;

// decomp: _selectCursorXy (_selectCursorXy@src/TITLE/TITLE.PRG/22C.c)
std::int32_t VS::Title::selectionCursorPosition;

// decomp: _spmcimg (_spmcimg@src/TITLE/TITLE.PRG/22C.c)
char* VS::Title::memoryCardUiResource;

// decomp: _textTable (_textTable@src/TITLE/TITLE.PRG/22C.c)
std::uint16_t* VS::Title::saveMenuText;

