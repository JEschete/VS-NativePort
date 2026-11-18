// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


extern short Render_legacyViewportLeft;
extern short Render_viewportWidth;
extern short Render_legacyViewportTop;
extern u_short Render_viewportHeight;
extern DRAWENV Render_legacyDrawEnvironments[2];
extern DISPENV Render_legacyDisplayEnvironments[2];
extern int Render_frameBuf;
extern int Render_isFrameStepPaused;
