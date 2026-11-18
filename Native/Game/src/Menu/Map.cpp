// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Content/DiscLayout.h"
#include "DiscQueue.h"
#include "Field/Map.h"
#include "Menu/Map.h"

// decomp: D_80102C40 (D_80102C40@src/MENU/MENU5.PRG/4D8.c)
int const Menu_Map_unusedGlobal1[] = { 0x006B4100, 0x006C8219,
    0x00663040, 0x00203840, 0x00330500, 0x00262801, 0x00200808, 0x00081010 };

// decomp: D_80108D04 (D_80108D04@src/MENU/MENU5.PRG/4D8.c)
RECT Menu_Map_kDoorMarkerUpperRects[] = { { 160, 144, 16, 16 }, { 176, 144, 16, 16 } };

// decomp: D_80108D14 (D_80108D14@src/MENU/MENU5.PRG/4D8.c)
RECT Menu_Map_kDoorMarkerLowerRects[] = { { 192, 144, 26, 8 }, { 216, 160, 39, 8 } };

// decomp: _1 (_1@src/MENU/MENU5.PRG/4D8.c)
short Menu_Map_layoutPadding1[2];

// decomp: _2 (_2@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_layoutPadding2[2];

// decomp: _armFileBuff (_armFileBuff@src/MENU/MENU5.PRG/4D8.c)
void* Menu_Map_armFileBuff;

// decomp: _ashleyIconBrightness (_ashleyIconBrightness@src/MENU/MENU5.PRG/4D8.c)
int const Menu_Map_kAshleyIconBrightness[] = { 0x00404040, 0x00505050, 0x00606060,
    0x00707070, 0x00808080, 0x00808080, 0x00808080, 0x00808080, 0x00808080, 0x00808080,
    0x00808080, 0x00808080, 0x00707070, 0x00606060, 0x00505050, 0x00404040 };

// decomp: _backgroundBrightness (_backgroundBrightness@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_backgroundBrightness;

// decomp: _cameraAngles (_cameraAngles@src/MENU/MENU5.PRG/4D8.c)
SVECTOR Menu_Map_cameraAngles;

// decomp: _cameraDistance (_cameraDistance@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_cameraDistance;

// decomp: _cameraPos (_cameraPos@src/MENU/MENU5.PRG/4D8.c)
SVECTOR Menu_Map_cameraPos;

// decomp: _centerPoint (_centerPoint@src/MENU/MENU5.PRG/4D8.c)
SVECTOR Menu_Map_centerPoint;

// decomp: _compassBounds (_compassBounds@src/MENU/MENU5.PRG/4D8.c)
struct Menu_Map_CompassBound Menu_Map_compassBounds[4];

// decomp: _connectingMaps (_connectingMaps@src/MENU/MENU5.PRG/4D8.c)
u_short Menu_Map_connectingMaps[320]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _currentMapRow (_currentMapRow@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_currentMapRow = -1;

// decomp: _currentRoomIndex (_currentRoomIndex@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_currentRoomIndex;

// decomp: _currentScene (_currentScene@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_currentScene;

// decomp: _currentSceneRow (_currentSceneRow@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_currentSceneRow;

// decomp: _doors (_doors@src/MENU/MENU5.PRG/4D8.c)
u_short Menu_Map_doors[522]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _geomOffsetX (_geomOffsetX@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_geomOffsetX;

// decomp: _geomOffsetY (_geomOffsetY@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_geomOffsetY;

// decomp: _hideUi (_hideUi@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_hideUi;

// decomp: _iconRects (_iconRects@src/MENU/MENU5.PRG/4D8.c)
RECT Menu_Map_iconRects[] = { { 104, 144, 18, 8 }, { 144, 216, 44, 7 }, { 128, 56, 22, 8 },
    { 218, 144, 36, 8 } };

// decomp: _isCurrentScene (_isCurrentScene@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_isCurrentScene;

// decomp: _isPaling (_isPaling@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_isPaling;

// decomp: _mapLabelCount (_mapLabelCount@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_mapLabelCount;

// decomp: _mapLabelY (_mapLabelY@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_mapLabelY;

// decomp: _mapLabels (_mapLabels@src/MENU/MENU5.PRG/4D8.c)
struct Menu_Map_MapLabel Menu_Map_mapLabels[8];

// decomp: _mapNames (_mapNames@MENU/MENU5.PRG)
u_short Menu_Map_mapNames[282]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _menuCommandsState (_menuCommandsState@src/MENU/MENU5.PRG/4D8.c)
short Menu_Map_menuCommandsState;

// decomp: _menuFramesCounter (_menuFramesCounter@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_menuFramesCounter;

// decomp: _menuState (_menuState@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_menuState;

// decomp: _quitState (_quitState@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_quitState;

// decomp: _roomNamesTable (_roomNamesTable@src/MENU/MENU5.PRG/4D8.c)
Field_MapRoomName* Menu_Map_roomNamesTable;

// decomp: _sceneArmFiles (_sceneArmFiles@src/MENU/MENU5.PRG/4D8.c)
Content_DiscFile const Menu_Map_kSceneArmFiles[] = { { VS_DUMMY_ARM_LBA, VS_DUMMY_ARM_SIZE },
    { VS_SCEN001_ARM_LBA, VS_SCEN001_ARM_SIZE },
    { VS_SCEN002_ARM_LBA, VS_SCEN002_ARM_SIZE },
    { VS_SCEN003_ARM_LBA, VS_SCEN003_ARM_SIZE },
    { VS_SCEN004_ARM_LBA, VS_SCEN004_ARM_SIZE },
    { VS_SCEN005_ARM_LBA, VS_SCEN005_ARM_SIZE },
    { VS_SCEN006_ARM_LBA, VS_SCEN006_ARM_SIZE },
    { VS_SCEN007_ARM_LBA, VS_SCEN007_ARM_SIZE },
    { VS_SCEN008_ARM_LBA, VS_SCEN008_ARM_SIZE },
    { VS_SCEN009_ARM_LBA, VS_SCEN009_ARM_SIZE },
    { VS_SCEN010_ARM_LBA, VS_SCEN010_ARM_SIZE },
    { VS_SCEN011_ARM_LBA, VS_SCEN011_ARM_SIZE },
    { VS_SCEN012_ARM_LBA, VS_SCEN012_ARM_SIZE },
    { VS_SCEN013_ARM_LBA, VS_SCEN013_ARM_SIZE },
    { VS_SCEN014_ARM_LBA, VS_SCEN014_ARM_SIZE },
    { VS_SCEN015_ARM_LBA, VS_SCEN015_ARM_SIZE },
    { VS_SCEN016_ARM_LBA, VS_SCEN016_ARM_SIZE },
    { VS_SCEN017_ARM_LBA, VS_SCEN017_ARM_SIZE },
    { VS_SCEN018_ARM_LBA, VS_SCEN018_ARM_SIZE },
    { VS_SCEN019_ARM_LBA, VS_SCEN019_ARM_SIZE },
    { VS_SCEN020_ARM_LBA, VS_SCEN020_ARM_SIZE },
    { VS_SCEN021_ARM_LBA, VS_SCEN021_ARM_SIZE },
    { VS_SCEN022_ARM_LBA, VS_SCEN022_ARM_SIZE },
    { VS_SCEN023_ARM_LBA, VS_SCEN023_ARM_SIZE },
    { VS_SCEN024_ARM_LBA, VS_SCEN024_ARM_SIZE },
    { VS_SCEN025_ARM_LBA, VS_SCEN025_ARM_SIZE },
    { VS_SCEN026_ARM_LBA, VS_SCEN026_ARM_SIZE },
    { VS_SCEN027_ARM_LBA, VS_SCEN027_ARM_SIZE },
    { VS_SCEN028_ARM_LBA, VS_SCEN028_ARM_SIZE },
    { VS_SCEN029_ARM_LBA, VS_SCEN029_ARM_SIZE },
    { VS_SCEN030_ARM_LBA, VS_SCEN030_ARM_SIZE },
    { VS_SCEN031_ARM_LBA, VS_SCEN031_ARM_SIZE } };

// decomp: _sceneCdQueueSlot (_sceneCdQueueSlot@src/MENU/MENU5.PRG/4D8.c)
Content_DiscQueueSlot* Menu_Map_sceneCdQueueSlot;

// decomp: _sceneRowIdMap (_sceneRowIdMap@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_sceneRowIdMap[32];

// decomp: _selectedMap (_selectedMap@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_selectedMap;

// decomp: _triggerHideUi (_triggerHideUi@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_triggerHideUi;

// decomp: _uiAnimationState (_uiAnimationState@src/MENU/MENU5.PRG/4D8.c)
short Menu_Map_uiAnimationState;

// MENU5.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Map_ResetMenu5Statics(void)
{
    {
        static const int initial = -1;
        memcpy((void*)&Menu_Map_currentMapRow, &initial, sizeof Menu_Map_currentMapRow);
    }
}

