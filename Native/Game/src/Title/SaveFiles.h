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

// decomp: menuBg_t (src/TITLE/TITLE.PRG/22C.c:3122)
typedef struct Title_SaveMenuBackgroundRecord {
    u_short palette[256];
    char indexedPixels[240][192];
} Title_SaveMenuBackgroundRecord;

// decomp: uiTable_t (src/TITLE/TITLE.PRG/22C.c:3117)
typedef struct Title_SaveMenuUiRecord {
    char pageZeroPixels[224][256 / 2];
    char pageOnePixels[256][256 / 2];
} Title_SaveMenuUiRecord;

#endif // VS_TITLE_SAVEFILES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_TITLE_SAVEFILES_H_DECLS)
#define VS_TITLE_SAVEFILES_H_DECLS
#include "Menu/Files.h"
#include "Render/LegacyPackets.h"
#include "Save/MemoryCard/GameFiles.h"

extern Title_SaveMenuFontRecord Title_saveMenuFontAtlas[1];
extern u_long Title_debugFontAtlas[4928];
extern Title_SaveMenuUiRecord Title_saveMenuUiAtlas;
extern char Title_fileMenuPrimitivePadding[8];;
extern char Title_saveScreenPadding;;
extern char Title_savedMainMenuSetting;;
extern char Title_isContainerDataEmpty;;
extern signed char Title_kFileOperationCursorSaturation[];
extern char Title_hasUnsavedData;;
extern Menu_Files_FileMenuElement Title_fileMenuElements[10];;
extern char Title_fileMenuScreenFade;;
extern u_short Title_previousTransferProgress;;
extern int Title_transferProgressCounter;;
extern u_short Title_transferProgressPosition;;
extern u_short Title_transferProgressTarget;;
extern char Title_isSaving;;
extern char* Title_memoryCardMessage;;
extern char Title_kFileMenuAnimationStops[];
extern Menu_Files_PrimitiveBuffer Title_fileMenuPrimitiveBuffer;;
extern Save_CardFileInfoRecord* Title_saveFileSummaries;;
extern Title_SaveMenuBackgroundRecord Title_saveMenuBackground;
extern u_char Title_selectionCursorColor;;
extern int Title_selectionCursorPosition;;
extern char* Title_memoryCardUiResource;;
extern u_short* Title_saveMenuText;;
void Title_SaveFiles_DrawImmediateSprite(int xy, int uvClut, int wh, int tpage);
void Title_SaveFiles_DrawSummaryLabel(int xy, enum Menu_Files_FileMenuUiId id);
void Title_SaveFiles_DrawHpMpSummary(int xy, enum Menu_Files_StatType stat, u_int currentValue, u_int maxValue);
void Title_SaveFiles_DrawTransferProgress(int x, int y);
void Title_SaveFiles_DrawTransferComplete(int colour, int y);
void Title_SaveFiles_ResetElements(void);
Menu_Files_FileMenuElement* Title_SaveFiles_InitializeElement(int id, int xy, int wh, char* text);
void Title_SaveFiles_ClearElement(int id);
int Title_SaveFiles_AreAnimationsComplete(void);
int Title_SaveFiles_DrawGlyph(u_int c, int x, int y, int clut);
int Title_SaveFiles_CheckCardStep(int portMask);
void Title_SaveFiles_DrawElement(Menu_Files_FileMenuElement* element);
void Title_SaveFiles_DrawFrame(int framebuf);
int Title_SaveFiles_UpdateLoadFiles(int initPort);
long Title_SaveFiles_UpdateLoadCard(int initPort);
int Title_SaveFiles_UpdateConfirmation(int arg0);
int Title_SaveFiles_UpdateOverwritePrompt(int init);
int Title_SaveFiles_UpdateFormatPrompt(int initPort);
int Title_SaveFiles_UpdateSaveFiles(int initPort);
int Title_SaveFiles_UpdateSaveCard(int initPort);

#endif // VS_TITLE_SAVEFILES_H_DECLS
