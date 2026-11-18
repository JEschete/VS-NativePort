// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Menu/Magic.h"

// decomp: vs_battle_warlockSpellIds (vs_battle_warlockSpellIds@BATTLE/BATTLE.PRG)
char Menu_Magic_warlockSpellIds[20]; /* contents: module image (ModuleData.cpp) */

// decomp: vs_battle_shamanSpellIds (vs_battle_shamanSpellIds@BATTLE/BATTLE.PRG)
char Menu_Magic_shamanSpellIds[8]; /* contents: module image (ModuleData.cpp) */

// decomp: vs_battle_sorcererSpellIds (vs_battle_sorcererSpellIds@BATTLE/BATTLE.PRG)
char Menu_Magic_sorcererSpellIds[20]; /* contents: module image (ModuleData.cpp) */

// decomp: vs_battle_enchanterSpellIds (vs_battle_enchanterSpellIds@BATTLE/BATTLE.PRG)
char Menu_Magic_enchanterSpellIds[8]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8010694A (D_8010694A@src/MENU/MENU0.PRG/84.c)
u_char Menu_Magic_warlockLevelCursorBlink;

// decomp: _0 (_0@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_layoutPadding1[5];

// decomp: _1 (_1@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_layoutPadding2[2];

// decomp: _3 (_3@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_layoutPadding3[6];

// decomp: _ (_@src/MENU/MENU0.PRG/84.c)
const int Menu_Magic_layoutPadding4 __attribute__((unused)) = 0;

// decomp: _MPCostDirectTextBuffer (_MPCostDirectTextBuffer@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_mpCostDirectTextBuffer[16];

// decomp: _MPCostTextBuffer (_MPCostTextBuffer@src/MENU/MENU0.PRG/84.c)
signed char Menu_Magic_mpCostTextBuffer[16];

// decomp: _availableEnchanterSpells (_availableEnchanterSpells@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_availableEnchanterSpells[8];

// decomp: _availableShamanSpells (_availableShamanSpells@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_availableShamanSpells[6];

// decomp: _availableSorcererSpells (_availableSorcererSpells@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_availableSorcererSpells[18];

// decomp: _availableWarlockSpells (_availableWarlockSpells@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_availableWarlockSpells[18];

// decomp: _magicStrings (_magicStrings@MENU/MENU0.PRG)
u_short Menu_Magic_magicStrings[2462]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _savePointData (_savePointData@MENU/MENU0.PRG)
int Menu_Magic_savePointData[] = { 0x0305001B, 0x0801000E, 0x04030199, 0x0C0C001C, 0x070C0028,
    0x040200CD, 0x0F070035, 0x08060093, 0x060000CE, 0x09080106, 0x09080111, 0x0C030098,
    0x090800E0, 0x060900C8, 0x050800CA, 0x050F00CC, 0x0109009B, 0x0A07007C, 0x000400CF,
    0x080800EC, 0x020400F1, 0x0705011D, 0x06080120, 0x04080139, 0x04080147, 0x0904014F,
    0x010300A2, 0x150C00A3, 0x030200D0, 0x060600D1, 0x1007008B, 0x010400AB, 0x05020055,
    0x07060063, 0x0A0400D2, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000 };

// decomp: _teleportationStrings (_teleportationStrings@MENU/MENU0.PRG)
u_short Menu_Magic_teleportationStrings[1599]; /* contents: disc string table (ModuleData.cpp) */

