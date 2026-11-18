// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Content/DiscLayout.h"
#include "DiscQueue.h"
#include "Menu/Score/ScoreScreen.h"

// decomp: _0 (_0@src/MENU/MENU9.PRG/238.c)
int Menu_Score_layoutPadding1;

// decomp: _1 (_1@src/MENU/MENU9.PRG/238.c)
int Menu_Score_layoutPadding2[2];

// decomp: _bgColorStops0 (_bgColorStops0@src/MENU/MENU9.PRG/238.c)
const CVECTOR Menu_Score_kBgColorStops0[] = { { 0, 65, 107 }, { 25, 130, 108 },
    { 64, 48, 102 }, { 64, 56, 32 } };

// decomp: _bgColorStops1 (_bgColorStops1@src/MENU/MENU9.PRG/238.c)
const CVECTOR Menu_Score_kBgColorStops1[] = { { 0, 5, 51 }, { 1, 40, 38 }, { 8, 8, 32 },
    { 16, 16, 8 } };

// decomp: _buttonHeld (_buttonHeld@src/MENU/MENU9.PRG/238.c)
int Menu_Score_buttonHeld;

// decomp: _cameraAngles (_cameraAngles@src/MENU/MENU9.PRG/238.c)
SVECTOR Menu_Score_cameraAngles;

// decomp: _cameraAnglesBackup (_cameraAnglesBackup@src/MENU/MENU9.PRG/238.c)
VECTOR Menu_Score_cameraAnglesBackup;

// decomp: _cameraDistance (_cameraDistance@src/MENU/MENU9.PRG/238.c)
int Menu_Score_cameraDistance;

// decomp: _cameraHeightOffset (_cameraHeightOffset@src/MENU/MENU9.PRG/238.c)
short Menu_Score_cameraHeightOffset;

// decomp: _cameraPositionBackup (_cameraPositionBackup@src/MENU/MENU9.PRG/238.c)
VECTOR Menu_Score_cameraPositionBackup;

// decomp: _characterRank (_characterRank@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_characterRank;

// decomp: _enemyDescriptions (_enemyDescriptions@src/MENU/MENU9.PRG/238.c)
u_short* Menu_Score_enemyDescriptions;

// decomp: _enemySkipDelay (_enemySkipDelay@src/MENU/MENU9.PRG/238.c)
short Menu_Score_enemySkipDelay;

// decomp: _hideUi (_hideUi@src/MENU/MENU9.PRG/238.c)
int Menu_Score_hideUi;

// decomp: _initCameraDistance (_initCameraDistance@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_initCameraDistance;

// decomp: _mapCompletion (_mapCompletion@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_mapCompletion;

// decomp: _menuState (_menuState@src/MENU/MENU9.PRG/238.c)
int Menu_Score_menuState;

// decomp: _menuText (_menuText@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_menuText[34]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _monBinCdQueueSlot (_monBinCdQueueSlot@src/MENU/MENU9.PRG/238.c)
Content_DiscQueueSlot* Menu_Score_monBinCdQueueSlot;

// decomp: _monBinData (_monBinData@src/MENU/MENU9.PRG/238.c)
Menu_Score_EnemyEntry* Menu_Score_monBinData;

// decomp: _monBinFile (_monBinFile@src/MENU/MENU9.PRG/238.c)
const Content_DiscFile Menu_Score_kMonBinFile = { VS_MON_BIN_LBA, VS_MON_BIN_SIZE };

// decomp: _nextState (_nextState@src/MENU/MENU9.PRG/238.c)
int Menu_Score_nextState;

// decomp: _noString (_noString@src/MENU/MENU9.PRG/238.c)
char Menu_Score_unusedNoString[] = "NO\0";

// decomp: _openedChestCount (_openedChestCount@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_openedChestCount;

// decomp: _rankText (_rankText@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_rankText[192]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _rotationColor (_rotationColor@src/MENU/MENU9.PRG/238.c)
int Menu_Score_rotationColor;

// decomp: _selectedEnemy (_selectedEnemy@src/MENU/MENU9.PRG/238.c)
short Menu_Score_selectedEnemy;

// decomp: _yesString (_yesString@src/MENU/MENU9.PRG/238.c)
char Menu_Score_unusedYesString[] = "YES\0";

// decomp: _zoomColor (_zoomColor@src/MENU/MENU9.PRG/238.c)
int Menu_Score_zoomColor;

