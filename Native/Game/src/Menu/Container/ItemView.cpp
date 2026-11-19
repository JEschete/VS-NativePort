// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Menu/Container/ItemView.h"

#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.h"

// decomp: D_80109A08 (D_80109A08@src/MENU/MENUD.PRG/234.c)
char const* Menu_Container_kStorageNames[] = { "GADGET BAG", "CONTAINER" };

// decomp: D_80109A10 (D_80109A10@src/MENU/MENUD.PRG/234.c)
char const* Menu_Container_kCategoryLabels[] = { "$WEAPON", "$BLADE", "$GRIP", "$SHIELD", "$ARMOR",
    "$GEM", "$MISC" };

// decomp: D_80109A7A (D_80109A7A@src/MENU/MENUD.PRG/234.c)
u_char Menu_Container_browsingGadgetBag;

// decomp: D_80109A7B (D_80109A7B@src/MENU/MENUD.PRG/234.c)
char Menu_Container_categoryTitleVisible;

// decomp: D_80109A7C (D_80109A7C@src/MENU/MENUD.PRG/234.c)
char Menu_Container_categoryIconsVisible;

// decomp: D_80109A7D (D_80109A7D@src/MENU/MENUD.PRG/234.c)
char Menu_Container_controlsPanelMode;

// decomp: D_80109A7E (D_80109A7E@src/MENU/MENUD.PRG/234.c)
char Menu_Container_categoryTitleSlideStep;

// decomp: D_80109A7F (D_80109A7F@src/MENU/MENUD.PRG/234.c)
char Menu_Container_categoryIconsSlideStep;

// decomp: D_80109A80 (D_80109A80@src/MENU/MENUD.PRG/234.c)
char Menu_Container_controlsPanelSlideStep;

// decomp: D_80109A81 (D_80109A81@src/MENU/MENUD.PRG/234.c)
char Menu_Container_detailsViewPhase;

// decomp: D_80109A84 (D_80109A84@src/MENU/MENUD.PRG/234.c)
int Menu_Container_detailsClosedWithTriangle;

// decomp: D_80109A88 (D_80109A88@src/MENU/MENUD.PRG/234.c)
VS::Items::Container* Menu_Container_otherStorage;

// decomp: _menuText (_menuText@src/MENU/MENUD.PRG/234.c)
u_short Menu_Container_menuText[690]; /* contents: disc string table (ModuleData.cpp) */

// decomp: vs_menuD_containerData (vs_menuD_containerData@MENU/MENUD.PRG)
VS::Items::Container* Menu_Container_containerData;
