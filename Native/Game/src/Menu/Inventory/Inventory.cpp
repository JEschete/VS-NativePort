#include "Menu/Inventory/Inventory.hpp"

#include <cstdint>

namespace VS::Menu::Inventory {
namespace {
// items1.h:20-21 initialize the preview slide.
constexpr std::int32_t kInitialAshleyYOffset = 0;
constexpr std::int32_t kInitialAshleyRenderState = 0;

void ResetItemMenuStatics() {
  ashleyYOffset = kInitialAshleyYOffset;
  ashleyRenderState = kInitialAshleyRenderState;
}
}  // namespace

// decomp: ashleyYOffset (src/MENU/items1.h:20)
std::int32_t ashleyYOffset = kInitialAshleyYOffset;
// decomp: ashleyRenderState (src/MENU/items1.h:21)
std::int32_t ashleyRenderState = kInitialAshleyRenderState;
// decomp: secondaryTitleAnimationStep (src/MENU/items1.h:68)
std::uint8_t secondaryTitleAnimationStep;
// decomp: itemCategoryIconsAnimationStep (src/MENU/items1.h:69)
std::uint8_t itemCategoryIconsAnimationStep;
// decomp: statusCommandAnimationStep (src/MENU/items1.h:70)
std::uint8_t statusCommandAnimationStep;
// decomp: selectedItemCategoryIconOnTop (src/MENU/items1.h:71)
std::uint8_t selectedItemCategoryIconOnTop;
// decomp: exitToBattle (src/MENU/items1.h:72)
std::int32_t exitToBattle;
// decomp: secondaryTitleEnabled (src/MENU/toggleUi.h:1)
std::uint8_t secondaryTitleEnabled;
// decomp: itemCategoryIconsEnabled (src/MENU/toggleUi.h:2)
std::uint8_t itemCategoryIconsEnabled;
// decomp: statusCommandState (src/MENU/toggleUi.h:3)
std::uint8_t statusCommandState;

// decomp: miscActions (src/MENU/MENU3.PRG/16C.c:24)
std::uint8_t miscActions[kMiscActionRows];
// decomp: itemsText (src/MENU/MENU3.PRG/16C.c:28)
Framework::ItemText* itemsText;
// decomp: selectedItemRow (src/MENU/MENU3.PRG/16C.c:29)
std::int32_t selectedItemRow;
// decomp: state (src/MENU/MENU3.PRG/16C.c:30)
std::uint8_t equipListStep;
// decomp: menuRowCount (src/MENU/MENU3.PRG/16C.c:31)
std::uint8_t menuRowCount;
// decomp: itemsFirstRow (src/MENU/MENU3.PRG/16C.c:32)
std::uint8_t itemsFirstRow;
// decomp: equippedItem (src/MENU/MENU3.PRG/16C.c:33)
std::uint8_t equippedItem;
// decomp: menuTextTable (src/MENU/items0.h:120): filled from the disc
// (ModuleData.cpp).
std::uint16_t menuText[kMenuTextWords];

void ResetMenu3Statics() { ResetItemMenuStatics(); }
void ResetMenubStatics() { ResetItemMenuStatics(); }
}  // namespace VS::Menu::Inventory
