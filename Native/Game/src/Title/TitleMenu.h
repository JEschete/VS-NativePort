// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_TITLE_TITLEMENU_H_TYPES
#define VS_TITLE_TITLEMENU_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_TITLE_TITLEMENU_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_TITLE_TITLEMENU_H_DECLS)
#define VS_TITLE_TITLEMENU_H_DECLS
#include "Title/TitleScreen.h"

extern int Title_titleScreenVisitCount;
extern u_short Title_kMenuItemOutlinePalettes[][16];
extern Title_MenuItemAnimationState Title_menuItemStates[10];;
extern u_short Title_kMenuItemTextPalettes[][16];
extern int Title_kMenuOutlineSizes[];
extern int Title_kMenuOutlinePositions[];
void Title_PlayNewGameSounds(void);
void Title_PlayMenuChangeSound(void);
void Title_PlayMenuSelectSound(void);
void Title_PlayMenuLeaveSound(void);
int Title_ShowContinueScreen(void);
void Title_ShowClearSaveScreen(void);
void Title_ShowPublisherAndDeveloper();
void Title_InitializeMenuItem(int menuItem, char pos);
void Title_LoadBackgroundTextures(void);
void Title_InitializeTitleDisplay(void);
void* Title_InitializeMenu(int menuItem);
void Title_ReleaseMenuCapture(void* arg0);
void Title_BlendMenuBackground(u_short* bgData, int textBlendFactor);
void Title_BlendCopyright(u_short* arg0, int textBlendFactor);
void Title_BlendMenuItemClut(int menuItem, int textBlendFactor, int clut0, int clut1);
void Title_UpdateMenuAnimation(void);
void Title_DrawMenuItems(void);
void Title_DrawMenu(void);
void Title_DrawMenuAndPollInput(void);
void Title_ShowVibrationSettings(void);
void Title_ShowSoundSettings(void);
int Title_NewGameChoice(void);
void Title_InitializeSettings(void);
int Title_Run(void);
void Title_InitializeGameDisplay(int w, int h, int distance, int r, int g, int b);
int Title_PresentGameFrame(u_long* ot);
void Title_InitializeNewGameData(void);
void Title_PrepareExit(int cause);

#endif // VS_TITLE_TITLEMENU_H_DECLS
