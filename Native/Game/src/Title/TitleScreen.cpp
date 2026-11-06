// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Title/TitleScreen.h"

// decomp: _titleScreenBg (_titleScreenBg@TITLE/TITLE.PRG)
u_int Title_titleScreenBackground[222932 / sizeof(u_int)]; /* contents: module image (ModuleData.cpp) */

// decomp: _menuCopyright (_menuCopyright@TITLE/TITLE.PRG)
u_short Title_copyrightImage[8640 / sizeof(u_short)]; /* contents: module image (ModuleData.cpp) */

// decomp: _4 (_4@src/TITLE/TITLE.PRG/22C.c)
char Title_movieLocationPadding[4];

// decomp: _5 (_5@src/TITLE/TITLE.PRG/22C.c)
char Title_movieStatePadding[28];

// decomp: _6 (_6@src/TITLE/TITLE.PRG/22C.c)
char Title_movieDecoderPadding[4];

}  // extern "C"
