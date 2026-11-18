// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


#include "Items/Inventory.h"

extern char const* Menu_Container_kStorageNames[];
extern char const* Menu_Container_kCategoryLabels[];
extern u_char Menu_Container_browsingGadgetBag;;
extern char Menu_Container_categoryTitleVisible;;
extern char Menu_Container_categoryIconsVisible;;
extern char Menu_Container_controlsPanelMode;;
extern char Menu_Container_categoryTitleSlideStep;;
extern char Menu_Container_categoryIconsSlideStep;;
extern char Menu_Container_controlsPanelSlideStep;;
extern char Menu_Container_detailsViewPhase;;
extern int Menu_Container_detailsClosedWithTriangle;;
extern Items_Container* Menu_Container_otherStorage;;
extern u_short Menu_Container_menuText[690]; /* contents: disc string table (ModuleData.cpp) */;
extern Items_Container* Menu_Container_containerData;;
