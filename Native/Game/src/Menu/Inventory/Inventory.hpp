#pragma once
// The item menus' shared statics. MENU3.PRG (the item menu) and MENUB.PRG (the
// loot screen) each compile src/MENU/items1.h and toggleUi.h: Ashley's preview
// slide, the panel toggles and their animation steps. Natively the overlays
// share one copy, and an overlay reload restores the initialized ones.
// MENU3.PRG's own statics (src/MENU/MENU3.PRG/16C.c) hold the equipment list
// and the misc rows' actions. The original compiled its char statics unsigned
// (-funsigned-char).
#include <cstddef>
#include <cstdint>

#include "Menu/Framework/Framework.hpp"
#include "Menu/Inventory/ItemListRows.hpp"

namespace VS::Menu::Inventory {
inline constexpr std::size_t kMenuTextWords = 472;

// decomp: ashleyYOffset (src/MENU/items1.h:20)
extern std::int32_t ashleyYOffset;
// decomp: ashleyRenderState (src/MENU/items1.h:21)
extern std::int32_t ashleyRenderState;
// decomp: secondaryTitleAnimationStep (src/MENU/items1.h:68)
extern std::uint8_t secondaryTitleAnimationStep;
// decomp: itemCategoryIconsAnimationStep (src/MENU/items1.h:69)
extern std::uint8_t itemCategoryIconsAnimationStep;
// decomp: statusCommandAnimationStep (src/MENU/items1.h:70)
extern std::uint8_t statusCommandAnimationStep;
// decomp: selectedItemCategoryIconOnTop (src/MENU/items1.h:71)
extern std::uint8_t selectedItemCategoryIconOnTop;
// decomp: exitToBattle (src/MENU/items1.h:72)
extern std::int32_t exitToBattle;
// decomp: secondaryTitleEnabled (src/MENU/toggleUi.h:1)
extern std::uint8_t secondaryTitleEnabled;
// decomp: itemCategoryIconsEnabled (src/MENU/toggleUi.h:2)
extern std::uint8_t itemCategoryIconsEnabled;
// decomp: statusCommandState (src/MENU/toggleUi.h:3)
extern std::uint8_t statusCommandState;

// decomp: miscActions (src/MENU/MENU3.PRG/16C.c:24): each misc row's action.
extern std::uint8_t miscActions[kMiscActionRows];
// decomp: itemsText (src/MENU/MENU3.PRG/16C.c:28): the equipment list's row
// text, a game heap block of menuRowCount records; null while no list is held.
extern Framework::ItemText* itemsText;
// decomp: selectedItemRow (src/MENU/MENU3.PRG/16C.c:29)
extern std::int32_t selectedItemRow;
// decomp: state (src/MENU/MENU3.PRG/16C.c:30): the equipment list's step.
extern std::uint8_t equipListStep;
// decomp: menuRowCount (src/MENU/MENU3.PRG/16C.c:31)
extern std::uint8_t menuRowCount;
// decomp: itemsFirstRow (src/MENU/MENU3.PRG/16C.c:32)
extern std::uint8_t itemsFirstRow;
// decomp: equippedItem (src/MENU/MENU3.PRG/16C.c:33)
extern std::uint8_t equippedItem;
// decomp: menuTextTable (src/MENU/items0.h:120, MENU3.PRG's)
extern std::uint16_t menuText[kMenuTextWords];

// MENU3.PRG and MENUB.PRG reloads restore items1.h's initialized statics
// (PORTING_WATCHLIST.md §1).
void ResetMenu3Statics();
void ResetMenubStatics();
}  // namespace VS::Menu::Inventory
