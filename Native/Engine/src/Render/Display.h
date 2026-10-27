// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_DISPLAY_H_TYPES
#define VS_RENDER_DISPLAY_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_RENDER_DISPLAY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_DISPLAY_H_DECLS)
#define VS_RENDER_DISPLAY_H_DECLS

extern short Render_legacyViewportLeft;
extern short Render_viewportWidth;
extern short Render_legacyViewportTop;
extern u_short Render_viewportHeight;
extern DRAWENV Render_legacyDrawEnvironments[2];
extern DISPENV Render_legacyDisplayEnvironments[2];
extern int Render_frameBuf;
extern int Render_isFrameStepPaused;
void Render_ConfigureFieldDisplay(int frame_width, int frame_height, int projection_distance, int clear_red, int clear_green, int clear_blue);
int Render_PresentFieldFrame(u_long* otag);
void Render_EnableDisplay(void);
void Render_DisableDisplay(void);
void Render_ShowDisplay(void);
void Render_HideDisplay(void);
void Render_InitializeLoadingDisplay(int w, int h, int arg2 __attribute__((unused)),
    __attribute__((unused)) int arg3, __attribute__((unused)) int arg4,
    __attribute__((unused)) int arg5);
void Render_ShowLoadingScreen(void);
void Render_UploadLoadingScreenBackBuffer(void);

#endif // VS_RENDER_DISPLAY_H_DECLS
