// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_MANUAL_H_TYPES
#define VS_MENU_MANUAL_H_TYPES
#include "Legacy/Psx.h"

typedef struct Menu_Manual_AnimationState Menu_Manual_AnimationState;
typedef struct Menu_Manual_BackgroundChunkWidths Menu_Manual_BackgroundChunkWidths;
typedef struct Menu_Manual_BackgroundTransparencies Menu_Manual_BackgroundTransparencies;

// decomp: src/include/lbaMacros.h:9
#define mkHfoPair(file) mkCdFile(file##_HF1), mkCdFile(file##_HF0)

// decomp: arrowType_e (src/MENU/MENUE.PRG/494.c:31)
enum Menu_Manual_ArrowType {
    Menu_Manual_ArrowType_Up,
    Menu_Manual_ArrowType_Down,
};
typedef enum Menu_Manual_ArrowType Menu_Manual_ArrowType;

// decomp: AnimationState_t (src/MENU/MENUE.PRG/494.c:44)
typedef struct Menu_Manual_AnimationState {
    short reserved0;
    u_short frameMask;
    short frameDuration;
    short repeat;
    short currentFrame;
    short timer;
    short isActive;
} Menu_Manual_AnimationState;
_Static_assert(sizeof(Menu_Manual_AnimationState) == 0xE, "Menu_Manual_AnimationState must keep its PS1 layout");

// decomp: _menuBgChunkWidths_t (src/MENU/MENUE.PRG/494.c:29)
typedef struct Menu_Manual_BackgroundChunkWidths {
    int reserved0;
    int reserved4;
    int reserved8;
} Menu_Manual_BackgroundChunkWidths;
_Static_assert(sizeof(Menu_Manual_BackgroundChunkWidths) == 0xC, "Menu_Manual_BackgroundChunkWidths must keep its PS1 layout");

// decomp: _menuBgTransparencies_t (src/MENU/MENUE.PRG/494.c:23)
typedef struct Menu_Manual_BackgroundTransparencies {
    int reserved0;
    int reserved4;
    int reserved8;
    int reservedC;
} Menu_Manual_BackgroundTransparencies;
_Static_assert(sizeof(Menu_Manual_BackgroundTransparencies) == 0x10, "Menu_Manual_BackgroundTransparencies must keep its PS1 layout");

#endif // VS_MENU_MANUAL_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_MANUAL_H_DECLS)
#define VS_MENU_MANUAL_H_DECLS
#include "DiscQueue.h"

extern u_short Menu_Manual_layoutPadding1 __attribute__((unused));
extern CVECTOR const Menu_Manual_kColorStops0[];
extern CVECTOR const Menu_Manual_kColorStops1[];
extern u_long* Menu_Manual_helpAssets;;
extern Content_DiscQueueSlot* Menu_Manual_helpAssetsCdQueue;;
extern Content_DiscFile const Menu_Manual_kHelpFileCdFiles[];
extern int Menu_Manual_helpPageLoadCounter;;
extern int* Menu_Manual_helpText;;
extern Content_DiscQueueSlot* Menu_Manual_helpTextCdQueue;;
extern int Menu_Manual_lineCount;;
extern Menu_Manual_BackgroundChunkWidths const Menu_Manual_kMenuBgChunkWidths;
extern Menu_Manual_BackgroundTransparencies const Menu_Manual_kMenuBgTransparencies[];
extern u_short Menu_Manual_menuText[516]; /* contents: disc string table (ModuleData.c) */;
extern int Menu_Manual_pageArrowAnimState;;
extern int Menu_Manual_scrollPosition;;
extern int Menu_Manual_selectedRow;;
extern int Menu_Manual_showMenuState;;
extern int Menu_Manual_topMenuState;;
void Menu_Manual_SetTopicRows(int rowCount, int highlightedRow, char** strings);
int Menu_Manual_GetSelectedTopicRow(void);
void Menu_Manual_ClearMenuExceptRow8(void);
void Menu_Manual_PollMenuReady(void);
int Menu_Manual_UpdateScreen(u_char* state);
int Menu_Manual_InitHelpState(void);
int Menu_Manual_UpdateHelpPages(void);
int Menu_Manual_UpdateTopicMenu(int init);
void Menu_Manual_UploadSpriteTiles(u_long const* buffer, int count);
void Menu_Manual_UploadCluts(u_long* buffer, int count);
int Menu_Manual_LastPageLineOffset(void);
void Menu_Manual_CopyPageText(void);
char* Menu_Manual_CopyEncodedLine(char* arg0, char* arg1);
void Menu_Manual_DrawPageContent(void);
int Menu_Manual_MinTileExtent(int arg0, int arg1);
int Menu_Manual_AddSpriteOffset(int arg0, int arg1);
void Menu_Manual_DrawIllustration(short* data);
void Menu_Manual_DrawPageBackground(int x, int y, int w, int h, int color);
void Menu_Manual_DrawUpperScrollFade(void);
void Menu_Manual_DrawLowerScrollFade(void);
void Menu_Manual_DrawPageArrow(enum Menu_Manual_ArrowType arrowType);
void Menu_Manual_DrawDiagramLines(void);
short Menu_Manual_AdvanceVisibilityAnimation(Menu_Manual_AnimationState* state);
void Menu_Manual_DrawControlsBackground(int x, int y, int w, int h);
void Menu_Manual_ResetMenueStatics(void);

#endif // VS_MENU_MANUAL_H_DECLS
