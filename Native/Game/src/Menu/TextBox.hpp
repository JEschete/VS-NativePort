#pragma once
// BATTLE.PRG's live text boxes, their selection and the scene notification
// state (src/BATTLE/BATTLE.PRG/5BF94.c). TextBoxStateLive.cpp reads and
// writes the boxes; ModuleData.cpp fills the two tables from the module
// image.
#include <cstddef>
#include <cstdint>

#include "Legacy/Psx.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/TextBoxState.hpp"

namespace VS::Menu {
// The item names scene notifications can show, 12 bytes each.
inline constexpr std::size_t kNotificationItemNameRows = 65;
inline constexpr std::size_t kNotificationItemNameBytes = 12;
using NotificationItemName = char[kNotificationItemNameBytes];

// decomp: D_800EB9D8 (src/BATTLE/BATTLE.PRG/5BF94.c:364): the scene
// notification's heap text, null while none is shown.
extern std::int32_t* sceneNotificationBuffer;
// decomp: D_800EBF58 (src/BATTLE/BATTLE.PRG/5BF94.c:391): every second row
// names an item from 0x1CA.
extern NotificationItemName kNotificationItemNames[kNotificationItemNameRows];
// decomp: D_800EC2D8 (src/BATTLE/BATTLE.PRG/5BF94.c:396): each information
// header sprite's texture word.
extern std::uint32_t kInformationHeaderSpriteWords[kInformationHeaderCount];
// decomp: D_800F4E68 (src/BATTLE/BATTLE.PRG/5BF94.c:421): the ticks a timed
// notification stays up.
extern std::uint8_t sceneNotificationTimer;
// decomp: D_800F4E69 (src/BATTLE/BATTLE.PRG/5BF94.c:422): set when the
// notification closes by itself.
extern std::uint8_t sceneNotificationTimed;
// decomp: textBoxSelector (src/BATTLE/BATTLE.PRG/5BF94.c:423)
extern TextBoxSelectorState textBoxSelector;
// decomp: vs_battle_textBoxes (src/BATTLE/BATTLE.PRG/battleUi.h:304)
extern TextBoxState textBoxes[kTextBoxCount];
// decomp: D_800F51B8 (src/BATTLE/BATTLE.PRG/5BF94.c:426): the ordering table
// the boxes draw into this frame, four entries per box.
extern std::uint32_t* activeTextBoxOrderingTable;
}  // namespace VS::Menu
