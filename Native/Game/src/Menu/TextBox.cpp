// BATTLE.PRG's live text boxes, their selection and the scene notification
// state.
#include "Menu/TextBox.hpp"

#include <cstdint>

#include "Legacy/Psx.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/TextBoxState.hpp"

namespace VS::Menu {
// decomp: D_800EB9D8 (src/BATTLE/BATTLE.PRG/5BF94.c:364)
std::int32_t* sceneNotificationBuffer;

// decomp: D_800EBF58 (src/BATTLE/BATTLE.PRG/5BF94.c:391); contents: module
// image (ModuleData.cpp).
NotificationItemName kNotificationItemNames[kNotificationItemNameRows];

// decomp: D_800EC2D8 (src/BATTLE/BATTLE.PRG/5BF94.c:396); contents: module
// image (ModuleData.cpp).
std::uint32_t kInformationHeaderSpriteWords[kInformationHeaderCount];

// decomp: D_800F4E68 (src/BATTLE/BATTLE.PRG/5BF94.c:421)
std::uint8_t sceneNotificationTimer;

// decomp: D_800F4E69 (src/BATTLE/BATTLE.PRG/5BF94.c:422)
std::uint8_t sceneNotificationTimed;

// decomp: textBoxSelector (src/BATTLE/BATTLE.PRG/5BF94.c:423)
TextBoxSelectorState textBoxSelector;

// decomp: vs_battle_textBoxes (src/BATTLE/BATTLE.PRG/battleUi.h:304)
TextBoxState textBoxes[kTextBoxCount];

// decomp: D_800F51B8 (src/BATTLE/BATTLE.PRG/5BF94.c:426)
std::uint32_t* activeTextBoxOrderingTable;
}  // namespace VS::Menu
