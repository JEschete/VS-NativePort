// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_TITLE_TITLESCREEN_H_TYPES
#define VS_TITLE_TITLESCREEN_H_TYPES
#include "Legacy/Psx.h"

typedef struct Title_MenuItemAnimationState Title_MenuItemAnimationState;

// decomp: menuItemState_e (src/TITLE/TITLE.PRG/22C.c:3741)
enum Title_MenuItemMotion {
    Title_MenuItemMotion_Stationary = 0,
    Title_MenuItemMotion_MovingUp = 1,
    Title_MenuItemMotion_MovingDown = 2,
    Title_MenuItemMotion_Submenu = 3
};
typedef enum Title_MenuItemMotion Title_MenuItemMotion;

// decomp: menuItems (src/TITLE/TITLE.PRG/22C.c:4234)
enum Title_MenuItemId {
    Title_MenuItemId_Timeout = -1,
    Title_MenuItemId_NewGame = 0,
    Title_MenuItemId_Continue = 1,
    Title_MenuItemId_Vibration = 2,
    Title_MenuItemId_Sound = 3,
    Title_MenuItemId_Mono = 4,
    Title_MenuItemId_VibrationOff = 5,
    Title_MenuItemId_VibrationOn = 6,
    Title_MenuItemId_Stereo = 7
};
typedef enum Title_MenuItemId Title_MenuItemId;

// decomp: menuItemState_t (src/TITLE/TITLE.PRG/22C.c:3757)
typedef struct Title_MenuItemAnimationState {
    char isEnabled;
    char motion;
    char textSaturation;
    char outlineSaturation;
    char textPosition;
    char targetPosition;
    char outlinePosition;
    char textBlendFactor;
} Title_MenuItemAnimationState;
_Static_assert(sizeof(Title_MenuItemAnimationState) == 0x8, "Title_MenuItemAnimationState must keep its PS1 layout");

#endif // VS_TITLE_TITLESCREEN_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_TITLE_TITLESCREEN_H_DECLS)
#define VS_TITLE_TITLESCREEN_H_DECLS

extern u_int Title_titleScreenBackground[222932 / sizeof(u_int)];
extern u_short Title_copyrightImage[8640 / sizeof(u_short)];
extern char Title_movieLocationPadding[4];;
extern char Title_movieStatePadding[28];;
extern char Title_movieDecoderPadding[4];;
extern int Title_outlineTexturePageWords[];
extern int Title_outlineUvClutWords[];
extern int Title_textTexturePageWords[];
extern int Title_textUvClutWords[];

#endif // VS_TITLE_TITLESCREEN_H_DECLS
