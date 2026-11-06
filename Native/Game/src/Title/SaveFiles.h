// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_TITLE_SAVEFILES_H_TYPES
#define VS_TITLE_SAVEFILES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Title_SaveMenuBackgroundRecord Title_SaveMenuBackgroundRecord;
typedef struct Title_SaveMenuFontRecord Title_SaveMenuFontRecord;
typedef struct Title_SaveMenuUiRecord Title_SaveMenuUiRecord;

// decomp: fontTable_t (src/TITLE/TITLE.PRG/22C.c:3112)
typedef struct Title_SaveMenuFontRecord {
    char indexedPixels[220][256 / 2];
    u_short palettes[16][16];
} Title_SaveMenuFontRecord;
_Static_assert(sizeof(Title_SaveMenuFontRecord) == 0x7000, "Title_SaveMenuFontRecord must keep its PS1 layout");

// decomp: menuBg_t (src/TITLE/TITLE.PRG/22C.c:3122)
typedef struct Title_SaveMenuBackgroundRecord {
    u_short palette[256];
    char indexedPixels[240][192];
} Title_SaveMenuBackgroundRecord;
_Static_assert(sizeof(Title_SaveMenuBackgroundRecord) == 0xB600, "Title_SaveMenuBackgroundRecord must keep its PS1 layout");

// decomp: uiTable_t (src/TITLE/TITLE.PRG/22C.c:3117)
typedef struct Title_SaveMenuUiRecord {
    char pageZeroPixels[224][256 / 2];
    char pageOnePixels[256][256 / 2];
} Title_SaveMenuUiRecord;
_Static_assert(sizeof(Title_SaveMenuUiRecord) == 0xF000, "Title_SaveMenuUiRecord must keep its PS1 layout");

#endif // VS_TITLE_SAVEFILES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_TITLE_SAVEFILES_H_DECLS)
#define VS_TITLE_SAVEFILES_H_DECLS
#include "Menu/Files.h"
#include "Save/MemoryCard/GameFiles.h"

extern Title_SaveMenuFontRecord Title_saveMenuFontAtlas[2];
extern u_long Title_debugFontAtlas[4928];
extern Title_SaveMenuUiRecord Title_saveMenuUiAtlas;
extern char Title_savedMainMenuSetting;;
extern char Title_isContainerDataEmpty;;
extern char Title_hasUnsavedData;;
extern Menu_Files_FileMenuElement Title_fileMenuElements[10];;
extern char Title_fileMenuScreenFade;;
extern u_short Title_previousTransferProgress;;
extern int Title_transferProgressCounter;;
extern u_short Title_transferProgressPosition;;
extern u_short Title_transferProgressTarget;;
extern char Title_isSaving;;
extern char* Title_memoryCardMessage;;
extern Save_CardFileInfoRecord* Title_saveFileSummaries;;
extern Title_SaveMenuBackgroundRecord Title_saveMenuBackground;
extern u_char Title_selectionCursorColor;;
extern int Title_selectionCursorPosition;;
extern char* Title_memoryCardUiResource;;
extern u_short* Title_saveMenuText;;

#endif // VS_TITLE_SAVEFILES_H_DECLS
