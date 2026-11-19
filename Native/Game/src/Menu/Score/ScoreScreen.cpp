// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Menu/Score/ScoreScreen.hpp"

#include <cstdint>

#include "Content/DiscLayout.h"
#include "DiscQueue.h"

// decomp: _buttonHeld (_buttonHeld@src/MENU/MENU9.PRG/238.c)
std::int32_t VS::Menu::Score::buttonHeld;

// decomp: _cameraAngles (_cameraAngles@src/MENU/MENU9.PRG/238.c)
SVECTOR VS::Menu::Score::cameraAngles;

// decomp: _cameraAnglesBackup (_cameraAnglesBackup@src/MENU/MENU9.PRG/238.c)
VECTOR VS::Menu::Score::cameraAnglesBackup;

// decomp: _cameraDistance (_cameraDistance@src/MENU/MENU9.PRG/238.c)
std::int32_t VS::Menu::Score::cameraDistance;

// decomp: _cameraHeightOffset (_cameraHeightOffset@src/MENU/MENU9.PRG/238.c)
std::int16_t VS::Menu::Score::cameraHeightOffset;

// decomp: _cameraPositionBackup (_cameraPositionBackup@src/MENU/MENU9.PRG/238.c)
VECTOR VS::Menu::Score::cameraPositionBackup;

// decomp: _characterRank (_characterRank@src/MENU/MENU9.PRG/238.c)
std::uint16_t VS::Menu::Score::characterRank;

// decomp: _enemyDescriptions (_enemyDescriptions@src/MENU/MENU9.PRG/238.c)
std::uint16_t* VS::Menu::Score::enemyDescriptions;

// decomp: _enemySkipDelay (_enemySkipDelay@src/MENU/MENU9.PRG/238.c)
std::int16_t VS::Menu::Score::enemySkipDelay;

// decomp: _hideUi (_hideUi@src/MENU/MENU9.PRG/238.c)
std::int32_t VS::Menu::Score::hideUi;

// decomp: _initCameraDistance (_initCameraDistance@src/MENU/MENU9.PRG/238.c)
std::uint16_t VS::Menu::Score::initCameraDistance;

// decomp: _mapCompletion (_mapCompletion@src/MENU/MENU9.PRG/238.c)
std::uint16_t VS::Menu::Score::mapCompletion;

// decomp: _menuState (_menuState@src/MENU/MENU9.PRG/238.c)
std::int32_t VS::Menu::Score::menuState;

// decomp: _menuText (_menuText@src/MENU/MENU9.PRG/238.c)
std::uint16_t VS::Menu::Score::menuText
    [VS::Menu::Score::kMenuTextWords]; /* contents: disc string table
                                          (ModuleData.cpp) */

// decomp: _monBinCdQueueSlot (_monBinCdQueueSlot@src/MENU/MENU9.PRG/238.c)
Content_DiscQueueSlot* VS::Menu::Score::monBinCdQueueSlot;

// decomp: _monBinData (_monBinData@src/MENU/MENU9.PRG/238.c)
VS::Menu::Score::EnemyEntry* VS::Menu::Score::monBinData;

// decomp: _monBinFile (_monBinFile@src/MENU/MENU9.PRG/238.c)
const Content_DiscFile VS::Menu::Score::kMonBinFile = {VS_MON_BIN_LBA,
                                                       VS_MON_BIN_SIZE};

// decomp: _nextState (_nextState@src/MENU/MENU9.PRG/238.c)
std::int32_t VS::Menu::Score::nextState;

// decomp: _openedChestCount (_openedChestCount@src/MENU/MENU9.PRG/238.c)
std::uint16_t VS::Menu::Score::openedChestCount;

// decomp: _rankText (_rankText@src/MENU/MENU9.PRG/238.c)
std::uint16_t VS::Menu::Score::rankText
    [VS::Menu::Score::kRankTextWords]; /* contents: disc string table
                                          (ModuleData.cpp) */

// decomp: _rotationColor (_rotationColor@src/MENU/MENU9.PRG/238.c)
std::int32_t VS::Menu::Score::rotationColor;

// decomp: _selectedEnemy (_selectedEnemy@src/MENU/MENU9.PRG/238.c)
std::int16_t VS::Menu::Score::selectedEnemy;

// decomp: _zoomColor (_zoomColor@src/MENU/MENU9.PRG/238.c)
std::int32_t VS::Menu::Score::zoomColor;
