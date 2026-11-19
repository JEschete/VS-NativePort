// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Menu/Container/ItemView.hpp"

#include <cstdint>

#include "Items/ItemRecords.hpp"

// decomp: D_80109A7A (D_80109A7A@src/MENU/MENUD.PRG/234.c)
std::uint8_t VS::Menu::container::browsingGadgetBag;

// decomp: D_80109A7B (D_80109A7B@src/MENU/MENUD.PRG/234.c)
std::uint8_t VS::Menu::container::categoryTitleVisible;

// decomp: D_80109A7C (D_80109A7C@src/MENU/MENUD.PRG/234.c)
std::uint8_t VS::Menu::container::categoryIconsVisible;

// decomp: D_80109A7D (D_80109A7D@src/MENU/MENUD.PRG/234.c)
std::uint8_t VS::Menu::container::controlsPanelMode;

// decomp: D_80109A7E (D_80109A7E@src/MENU/MENUD.PRG/234.c)
std::uint8_t VS::Menu::container::categoryTitleSlideStep;

// decomp: D_80109A7F (D_80109A7F@src/MENU/MENUD.PRG/234.c)
std::uint8_t VS::Menu::container::categoryIconsSlideStep;

// decomp: D_80109A80 (D_80109A80@src/MENU/MENUD.PRG/234.c)
std::uint8_t VS::Menu::container::controlsPanelSlideStep;

// decomp: D_80109A81 (D_80109A81@src/MENU/MENUD.PRG/234.c)
std::uint8_t VS::Menu::container::detailsViewPhase;

// decomp: D_80109A84 (D_80109A84@src/MENU/MENUD.PRG/234.c)
std::int32_t VS::Menu::container::detailsClosedWithTriangle;

// decomp: D_80109A88 (D_80109A88@src/MENU/MENUD.PRG/234.c)
VS::Items::Container* VS::Menu::container::otherStorage;

// decomp: vs_menuD_containerData (vs_menuD_containerData@MENU/MENUD.PRG)
VS::Items::Container* VS::Menu::container::containerData;

// decomp: menuTextTable (menuTextTable@src/MENU/MENUD.PRG/234.c); contents:
// disc string table (ModuleData.cpp).
std::uint16_t
    VS::Menu::container::menuText[VS::Menu::container::kMenuTextWords];