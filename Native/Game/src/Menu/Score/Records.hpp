#pragma once
// MENU9.PRG's Gazette records (src/MENU/MENU9.PRG/238.c): the list rows, the
// clear count and score, and the title, statistics and time trial string
// tables.
#include <cstddef>
#include <cstdint>

#include "Menu/Score/ScoreScreen.hpp"

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
inline constexpr std::size_t kGazetteRowCount = 32;
// The string tables' lengths in halfwords (ModuleData.cpp fills them).
inline constexpr std::size_t kMiscInfoWords = 1646;
inline constexpr std::size_t kStatDescriptionWords = 460;
inline constexpr std::size_t kStatHeaderWords = 274;
inline constexpr std::size_t kTimeAttackDescriptionWords = 232;
inline constexpr std::size_t kTimeAttackWords = 58;
inline constexpr std::size_t kTitleDescriptionWords = 784;
inline constexpr std::size_t kTitleTextWords = 364;

// decomp: _clearCount (src/MENU/MENU9.PRG/238.c:454)
extern std::uint16_t clearCount;
// decomp: _gazetteRows (src/MENU/MENU9.PRG/238.c:924)
extern ListRow gazetteRows[kGazetteRowCount];
// decomp: _miscInfo (src/MENU/MENU9.PRG/238.c:1421)
extern std::uint16_t miscInfo[kMiscInfoWords];
// decomp: _score (src/MENU/MENU9.PRG/238.c:1701)
extern std::uint32_t score;
// decomp: _statDescriptions (src/MENU/MENU9.PRG/238.c:2209)
extern std::uint16_t statDescriptions[kStatDescriptionWords];
// decomp: _statHeaders (src/MENU/MENU9.PRG/238.c:2208)
extern std::uint16_t statHeaders[kStatHeaderWords];
// decomp: _timeAttackDescriptions (src/MENU/MENU9.PRG/238.c:2477)
extern std::uint16_t timeAttackDescriptions[kTimeAttackDescriptionWords];
// decomp: _timeAttacks (src/MENU/MENU9.PRG/238.c:2475)
extern std::uint16_t timeAttacks[kTimeAttackWords];
// decomp: _titleDescriptions (src/MENU/MENU9.PRG/238.c:1739)
extern std::uint16_t titleDescriptions[kTitleDescriptionWords];
// decomp: _titleText (src/MENU/MENU9.PRG/238.c:1738)
extern std::uint16_t titleText[kTitleTextWords];
}  // namespace VS::Menu::Score
