// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_STATUS_ACTORSTATUS_H_TYPES
#define VS_MENU_STATUS_ACTORSTATUS_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/gpu.h:95
#define vs_getXY(x, y) (((y) << 16) | ((x) & 0xFFFF))

// decomp: src/include/gpu.h:97
#define vs_getWH(w, h) vs_getXY((w), (h))

// decomp: src/include/gpu.h:99
#define vs_getTpage(x, y, tp, abr, dtd)                                                  \
    ((0xE1 << 24) | (((dtd) & 1) << 9) | getTPage((tp), (abr), (x), (y)))

// decomp: src/include/gpu.h:109
#define vs_getUV0Clut(u, v, x, y) (((u) | ((v) << 8)) | (getClut((x), (y)) << 16))

// decomp: src/include/gpu.h:110
#define vs_getUV1Tpage(u, v, x, y, tp, abr)                                              \
    (((u) | ((v) << 8)) | (getTPage((tp), (abr), (x), (y)) << 16))

// decomp: _renderLimbUiState (src/MENU/MENU4.PRG/120.c:1138)
enum Menu_Status_LimbUiMode { Menu_Status_LimbUiMode_None, Menu_Status_LimbUiMode_Init };
typedef enum Menu_Status_LimbUiMode Menu_Status_LimbUiMode;

#endif // VS_MENU_STATUS_ACTORSTATUS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_STATUS_ACTORSTATUS_H_DECLS)
#define VS_MENU_STATUS_ACTORSTATUS_H_DECLS

extern char Menu_Status_layoutPadding1[24];;
extern int Menu_Status_animationStep;
extern char Menu_Status_basicStatsAnimDelay;
extern char Menu_Status_basicStatsAnimState;
extern char Menu_Status_delayScreenUpdate;
extern int Menu_Status_drawBackgroundFirst;
extern int Menu_Status_fadeScreen;
extern char Menu_Status_initializedActor;
extern char* Menu_Status_limbStatuses[];
extern int Menu_Status_previousActor;
extern char Menu_Status_renderBasicStatsState;
extern int Menu_Status_selectedActor;
extern u_char Menu_Status_selectedElement;;
extern char Menu_Status_selectedStatRow;;
extern u_short Menu_Status_statusStrings[1760]; /* contents: disc string table (ModuleData.cpp) */;
extern int Menu_Status_xPos;
extern int Menu_Status_yPos;
void Menu_Status_DrawModelSlideFade(void);
void Menu_Status_DrawBackground(void);
void Menu_Status_DrawFrame(void);
int Menu_Status_Update(u_char* state);
void Menu_Status_ActorStatus_ResetMenu4Statics(void);

#endif // VS_MENU_STATUS_ACTORSTATUS_H_DECLS
