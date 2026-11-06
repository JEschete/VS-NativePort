// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Menu/Files.h"
#include "Save/MemoryCard/GameFiles.h"
#include "Title/SaveFiles.h"

// decomp: _fontTable (_fontTable@TITLE/TITLE.PRG)
Title_SaveMenuFontRecord Title_saveMenuFontAtlas[2]; /* contents: module image (ModuleData.cpp) */

// decomp: _debugFont (_debugFont@TITLE/TITLE.PRG)
u_long Title_debugFontAtlas[4928]; /* contents: module image (ModuleData.cpp) */

// decomp: _uiTable (_uiTable@TITLE/TITLE.PRG)
Title_SaveMenuUiRecord Title_saveMenuUiAtlas; /* contents: module image (ModuleData.cpp) */

// decomp: _backupMainSetting (_backupMainSetting@src/TITLE/TITLE.PRG/22C.c)
char Title_savedMainMenuSetting;

// decomp: _containerDataEmpty (_containerDataEmpty@src/TITLE/TITLE.PRG/22C.c)
char Title_isContainerDataEmpty;

// decomp: _dataNotSaved (_dataNotSaved@src/TITLE/TITLE.PRG/22C.c)
char Title_hasUnsavedData;

// decomp: _fileMenuElements (_fileMenuElements@src/TITLE/TITLE.PRG/22C.c)
Menu_Files_FileMenuElement Title_fileMenuElements[10];

// decomp: _fileMenuScreenFade (_fileMenuScreenFade@src/TITLE/TITLE.PRG/22C.c)
char Title_fileMenuScreenFade;

// decomp: _filePreviousProgressCounter (_filePreviousProgressCounter@src/TITLE/TITLE.PRG/22C.c)
u_short Title_previousTransferProgress;

// decomp: _fileProgressCounter (_fileProgressCounter@src/TITLE/TITLE.PRG/22C.c)
int Title_transferProgressCounter;

// decomp: _fileProgressPosition (_fileProgressPosition@src/TITLE/TITLE.PRG/22C.c)
u_short Title_transferProgressPosition;

// decomp: _fileProgressTarget (_fileProgressTarget@src/TITLE/TITLE.PRG/22C.c)
u_short Title_transferProgressTarget;

// decomp: _isSaving (_isSaving@src/TITLE/TITLE.PRG/22C.c)
char Title_isSaving;

// decomp: _memoryCardMessage (_memoryCardMessage@src/TITLE/TITLE.PRG/22C.c)
char* Title_memoryCardMessage;

// decomp: _saveFileInfo (_saveFileInfo@src/TITLE/TITLE.PRG/22C.c)
Save_CardFileInfoRecord* Title_saveFileSummaries;

// decomp: _saveMenuBg_clut (_saveMenuBg_clut@TITLE/TITLE.PRG)
Title_SaveMenuBackgroundRecord Title_saveMenuBackground; /* contents: module image (ModuleData.cpp) */

// decomp: _selectCursorColor (_selectCursorColor@src/TITLE/TITLE.PRG/22C.c)
u_char Title_selectionCursorColor;

// decomp: _selectCursorXy (_selectCursorXy@src/TITLE/TITLE.PRG/22C.c)
int Title_selectionCursorPosition;

// decomp: _spmcimg (_spmcimg@src/TITLE/TITLE.PRG/22C.c)
char* Title_memoryCardUiResource;

// decomp: _textTable (_textTable@src/TITLE/TITLE.PRG/22C.c)
u_short* Title_saveMenuText;

}  // extern "C"
