// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Content/DiscLayout.h"
#include "OverlayFiles.h"

// decomp: BATTLE.PRG vs_battle_getMainMenuLba (src/BATTLE/BATTLE.PRG/6F014.c:804)
int Content_MainMenuOverlayDiscSector(void) { return VS_MAINMENU_PRG_LBA; }

}  // extern "C"
