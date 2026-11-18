// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Content/DiscLayout.h"
#include "DiscQueue.h"
#include "Field/Locations.h"
#include "Menu/Manual.h"

// decomp: _0 (_0@src/MENU/MENUE.PRG/494.c)
u_short Menu_Manual_layoutPadding1 __attribute__((unused)) = 0x0380;

// decomp: _colorStops0 (_colorStops0@src/MENU/MENUE.PRG/494.c)
CVECTOR const Menu_Manual_kColorStops0[] = { { 0, 0x20, 0x50, 0 }, { 0x19, 0x82, 0x6C, 0 },
    { 0x40, 0x30, 0x66, 0 }, { 0x40, 0x38, 0x20, 0 } };

// decomp: _colorStops1 (_colorStops1@src/MENU/MENUE.PRG/494.c)
CVECTOR const Menu_Manual_kColorStops1[] = { { 0, 0x5, 0x33, 0 }, { 0x1, 0x28, 0x26, 0 },
    { 0x8, 0x8, 0x20, 0 }, { 0x10, 0x10, 0x8, 0 } };

// decomp: _helpAssets (_helpAssets@src/MENU/MENUE.PRG/494.c)
u_long* Menu_Manual_helpAssets;

// decomp: _helpAssetsCdQueue (_helpAssetsCdQueue@src/MENU/MENUE.PRG/494.c)
Content_DiscQueueSlot* Menu_Manual_helpAssetsCdQueue;

// decomp: _helpFileCdFiles (_helpFileCdFiles@src/MENU/MENUE.PRG/494.c)
Content_DiscFile const Menu_Manual_kHelpFileCdFiles[] = { mkHfoPair(HELP01), mkHfoPair(HELP02),
    mkHfoPair(HELP03), mkHfoPair(HELP04), mkHfoPair(HELP05), mkHfoPair(HELP06),
    mkHfoPair(HELP07), mkHfoPair(HELP08), mkHfoPair(HELP09), mkHfoPair(HELP10),
    mkHfoPair(HELP11), mkHfoPair(HELP12), mkHfoPair(HELP13), mkHfoPair(HELP14) };

// decomp: _helpPageLoadCounter (_helpPageLoadCounter@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_helpPageLoadCounter;

// decomp: _helpText (_helpText@src/MENU/MENUE.PRG/494.c)
int* Menu_Manual_helpText;

// decomp: _helpTextCdQueue (_helpTextCdQueue@src/MENU/MENUE.PRG/494.c)
Content_DiscQueueSlot* Menu_Manual_helpTextCdQueue;

// decomp: _lineCount (_lineCount@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_lineCount;

// decomp: _menuBgChunkWidths (_menuBgChunkWidths@src/MENU/MENUE.PRG/494.c)
Menu_Manual_BackgroundChunkWidths const Menu_Manual_kMenuBgChunkWidths = { 128, 128, 64 };

// decomp: _menuBgTransparencies (_menuBgTransparencies@src/MENU/MENUE.PRG/494.c)
Menu_Manual_BackgroundTransparencies const Menu_Manual_kMenuBgTransparencies[] = { { 128, 115, 121, 88 },
    { 76, 64, 53, 42 }, { 33, 24, 17, 11 }, { 6, 3, 1, 0 } };

// decomp: _menuText (_menuText@src/MENU/MENUE.PRG/494.c)
u_short Menu_Manual_menuText[516]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _pageArrowAnimState (_pageArrowAnimState@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_pageArrowAnimState;

// decomp: _scrollPosition (_scrollPosition@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_scrollPosition;

// decomp: _selectedRow (_selectedRow@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_selectedRow;

// decomp: _showMenuState (_showMenuState@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_showMenuState;

// decomp: _topMenu_state (_topMenu_state@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_topMenuState;

