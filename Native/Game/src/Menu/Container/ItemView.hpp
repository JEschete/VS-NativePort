#pragma once
// MENUD.PRG's container screen panels (src/MENU/MENUD.PRG/234.c:249-260): the
// storage being browsed, the category title, icon and controls panels and
// their slide steps, the details view phase, the two storages' records and
// the overlay's text table. The original compiled its char statics unsigned
// (-funsigned-char).
#include <cstddef>
#include <cstdint>

#include "Items/ItemRecords.hpp"

namespace VS::Menu::container {
inline constexpr std::size_t kMenuTextWords = 690;

// decomp: D_80109A7A (src/MENU/MENUD.PRG/234.c:249)
extern std::uint8_t browsingGadgetBag;
// decomp: D_80109A7B (src/MENU/MENUD.PRG/234.c:250)
extern std::uint8_t categoryTitleVisible;
// decomp: D_80109A7C (src/MENU/MENUD.PRG/234.c:251)
extern std::uint8_t categoryIconsVisible;
// decomp: D_80109A7D (src/MENU/MENUD.PRG/234.c:252)
extern std::uint8_t controlsPanelMode;
// decomp: D_80109A7E (src/MENU/MENUD.PRG/234.c:253)
extern std::uint8_t categoryTitleSlideStep;
// decomp: D_80109A7F (src/MENU/MENUD.PRG/234.c:254)
extern std::uint8_t categoryIconsSlideStep;
// decomp: D_80109A80 (src/MENU/MENUD.PRG/234.c:255)
extern std::uint8_t controlsPanelSlideStep;
// decomp: D_80109A81 (src/MENU/MENUD.PRG/234.c:256)
extern std::uint8_t detailsViewPhase;
// decomp: D_80109A84 (src/MENU/MENUD.PRG/234.c:258): the Triangle bit when
// Triangle closed the details view, otherwise 0.
extern std::int32_t detailsClosedWithTriangle;
// decomp: D_80109A88 (src/MENU/MENUD.PRG/234.c:259): the storage not browsed.
extern Items::Container* otherStorage;
// decomp: vs_menuD_containerData (src/MENU/MENUD.PRG/234.c:260): the storage
// browsed.
extern Items::Container* containerData;
// decomp: menuTextTable (src/MENU/MENUD.PRG/234.c:84)
extern std::uint16_t menuText[kMenuTextWords];
}  // namespace VS::Menu::container
