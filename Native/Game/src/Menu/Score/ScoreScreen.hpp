#pragma once
// MENU9.PRG's score screen (src/MENU/MENU9.PRG/238.c): the Gazette's list
// rows, MON.BIN's enemy records, the remembered list cursors and the screen's
// module state.
#include <array>
#include <cstddef>
#include <cstdint>

#include "Legacy/Psx.h"

namespace VS::Content {
struct DiscFile;
struct DiscQueueSlot;
}  // namespace VS::Content

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
// decomp: _gazetteRow (src/MENU/MENU9.PRG/238.c:363): one row of the Gazette's
// title, statistics and time trial lists. The original compiled its char
// fields unsigned (-funsigned-char).
struct ListRow {
  std::uint8_t selected = 0;
  // Read per list: the title list's unlocked flag, the time trials' style.
  std::uint8_t opaque1 = 0;
  std::uint8_t animation_state = 0;
  std::uint8_t disabled = 0;
  std::int16_t x = 0;
  std::int16_t row_index = 0;
  // Text in the module's string tables (Records.hpp).
  std::uint8_t* title = nullptr;
  std::uint8_t* description = nullptr;
};

namespace screen_layout {
inline constexpr std::size_t kEnemyNameBytes = 28;
inline constexpr std::size_t kEnemyUnlockedOffset = 0x0C;
inline constexpr std::size_t kEnemyNameOffset = 0x10;
inline constexpr std::size_t kEnemyEntryBytes = 0x2C;
inline constexpr std::size_t kCursorMemoryBytes = 8;
}  // namespace screen_layout

// decomp: _monBinData_t (src/MENU/MENU9.PRG/238.c:377): one enemy's record in
// MON.BIN, read in place from the file's bytes.
struct EnemyEntry {
  std::int16_t zud_id = 0;
  std::int16_t class_id = 0;
  std::int16_t kill_flags_offset = 0;
  std::int16_t kill_flags_count = 0;
  std::int16_t selected = 0;
  std::int16_t animation_state = 0;
  std::uint8_t unlocked = 0;
  std::uint8_t prev = 0;
  std::uint8_t next = 0;
  std::uint8_t reserved_f = 0;
  std::array<std::uint8_t, screen_layout::kEnemyNameBytes> name{};
};
static_assert(sizeof(EnemyEntry) == screen_layout::kEnemyEntryBytes);
static_assert(offsetof(EnemyEntry, unlocked) ==
              screen_layout::kEnemyUnlockedOffset);
static_assert(offsetof(EnemyEntry, name) == screen_layout::kEnemyNameOffset);

// decomp: vs_battle_menu9CursorMemory_t (src/BATTLE/BATTLE.PRG/146C.h:791):
// the Gazette lists' remembered pages and rows, which BATTLE.PRG keeps while
// MENU9.PRG is unloaded.
struct CursorMemory {
  std::uint8_t title_page = 0;
  std::uint8_t title_row = 0;
  std::uint8_t encyclopaedia_page = 0;
  std::uint8_t encyclopaedia_row = 0;
  std::uint8_t record_time_page = 0;
  std::uint8_t record_time_row = 0;
  std::uint8_t gazette_page = 0;
  std::uint8_t gazette_row = 0;
};
static_assert(sizeof(CursorMemory) == screen_layout::kCursorMemoryBytes);

// The string tables' lengths in halfwords (ModuleData.cpp fills them).
inline constexpr std::size_t kMenuTextWords = 34;
inline constexpr std::size_t kRankTextWords = 192;

// decomp: _buttonHeld (src/MENU/MENU9.PRG/238.c:1088)
extern std::int32_t buttonHeld;
// decomp: _cameraAngles (src/MENU/MENU9.PRG/238.c:579)
extern SVECTOR cameraAngles;
// decomp: _cameraAnglesBackup (src/MENU/MENU9.PRG/238.c:466)
extern VECTOR cameraAnglesBackup;
// decomp: _cameraDistance (src/MENU/MENU9.PRG/238.c:576)
extern std::int32_t cameraDistance;
// decomp: _cameraHeightOffset (src/MENU/MENU9.PRG/238.c:577)
extern std::int16_t cameraHeightOffset;
// decomp: _cameraPositionBackup (src/MENU/MENU9.PRG/238.c:463)
extern VECTOR cameraPositionBackup;
// decomp: _characterRank (src/MENU/MENU9.PRG/238.c:584)
extern std::uint16_t characterRank;
// decomp: _enemyDescriptions (src/MENU/MENU9.PRG/238.c:946)
extern std::uint16_t* enemyDescriptions;
// decomp: _enemySkipDelay (src/MENU/MENU9.PRG/238.c:1248)
extern std::int16_t enemySkipDelay;
// decomp: _hideUi (src/MENU/MENU9.PRG/238.c:428)
extern std::int32_t hideUi;
// decomp: _initCameraDistance (src/MENU/MENU9.PRG/238.c:578)
extern std::uint16_t initCameraDistance;
// decomp: _mapCompletion (src/MENU/MENU9.PRG/238.c:434)
extern std::uint16_t mapCompletion;
// decomp: _menuState (src/MENU/MENU9.PRG/238.c:427)
extern std::int32_t menuState;
// decomp: _menuText (src/MENU/MENU9.PRG/238.c:1446)
extern std::uint16_t menuText[kMenuTextWords];
// decomp: _monBinCdQueueSlot (src/MENU/MENU9.PRG/238.c:613)
extern VS::Content::DiscQueueSlot* monBinCdQueueSlot;
// decomp: _monBinData (src/MENU/MENU9.PRG/238.c:612)
extern EnemyEntry* monBinData;
// decomp: _monBinFile (src/MENU/MENU9.PRG/238.c:612)
extern const VS::Content::DiscFile kMonBinFile;
// decomp: _nextState (src/MENU/MENU9.PRG/238.c:608)
extern std::int32_t nextState;
// decomp: _openedChestCount (src/MENU/MENU9.PRG/238.c:446)
extern std::uint16_t openedChestCount;
// decomp: _rankText (src/MENU/MENU9.PRG/238.c:584)
extern std::uint16_t rankText[kRankTextWords];
// decomp: _rotationColor (src/MENU/MENU9.PRG/238.c:1127)
extern std::int32_t rotationColor;
// decomp: _selectedEnemy (src/MENU/MENU9.PRG/238.c:1091)
extern std::int16_t selectedEnemy;
// decomp: _zoomColor (src/MENU/MENU9.PRG/238.c:1128)
extern std::int32_t zoomColor;
}  // namespace VS::Menu::Score
