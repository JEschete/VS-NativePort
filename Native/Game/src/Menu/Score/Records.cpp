// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Menu/Score/Records.hpp"

#include <cstdint>

// decomp: _clearCount (_clearCount@src/MENU/MENU9.PRG/238.c)
std::uint16_t VS::Menu::Score::clearCount;

// decomp: _gazetteRows (_gazetteRows@src/MENU/MENU9.PRG/238.c)
VS::Menu::Score::ListRow
    VS::Menu::Score::gazetteRows[VS::Menu::Score::kGazetteRowCount];

// decomp: _miscInfo (_miscInfo@src/MENU/MENU9.PRG/238.c)
// contents: disc string table (ModuleData.cpp)
std::uint16_t VS::Menu::Score::miscInfo[VS::Menu::Score::kMiscInfoWords];

// decomp: _score (_score@src/MENU/MENU9.PRG/238.c)
std::uint32_t VS::Menu::Score::score;

// decomp: _statDescriptions (_statDescriptions@src/MENU/MENU9.PRG/238.c)
// contents: disc string table (ModuleData.cpp)
std::uint16_t
    VS::Menu::Score::statDescriptions[VS::Menu::Score::kStatDescriptionWords];

// decomp: _statHeaders (_statHeaders@src/MENU/MENU9.PRG/238.c)
// contents: disc string table (ModuleData.cpp)
std::uint16_t VS::Menu::Score::statHeaders[VS::Menu::Score::kStatHeaderWords];

// decomp: _timeAttackDescriptions
// (_timeAttackDescriptions@src/MENU/MENU9.PRG/238.c)
// contents: disc string table (ModuleData.cpp)
std::uint16_t VS::Menu::Score::timeAttackDescriptions
    [VS::Menu::Score::kTimeAttackDescriptionWords];

// decomp: _timeAttacks (_timeAttacks@src/MENU/MENU9.PRG/238.c)
// contents: disc string table (ModuleData.cpp)
std::uint16_t VS::Menu::Score::timeAttacks[VS::Menu::Score::kTimeAttackWords];

// decomp: _titleDescriptions (_titleDescriptions@src/MENU/MENU9.PRG/238.c)
// contents: disc string table (ModuleData.cpp)
std::uint16_t
    VS::Menu::Score::titleDescriptions[VS::Menu::Score::kTitleDescriptionWords];

// decomp: _titleText (_titleText@src/MENU/MENU9.PRG/238.c)
// contents: disc string table (ModuleData.cpp)
std::uint16_t VS::Menu::Score::titleText[VS::Menu::Score::kTitleTextWords];
