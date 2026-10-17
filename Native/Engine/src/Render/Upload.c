// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Render/Upload.h"

// decomp: D_800EC280 (D_800EC280@BATTLE/BATTLE.PRG)
int Render_legacyUploadRectCursor; /* contents: module image (ModuleData.c) */

// decomp: D_800F51D0 (D_800F51D0@BATTLE/BATTLE.PRG)
RECT Render_legacyUploadRects[64 / sizeof(RECT)];

