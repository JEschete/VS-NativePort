// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Render/Display.h"

// decomp: D_8005DFD4 (D_8005DFD4@SLUS_010.40)
short Render_legacyViewportLeft;

// decomp: D_8005DFD6 (D_8005DFD6@SLUS_010.40)
short Render_viewportWidth;

// decomp: D_8005DFD8 (D_8005DFD8@SLUS_010.40)
short Render_legacyViewportTop;

// decomp: D_8005DFDA (D_8005DFDA@SLUS_010.40)
u_short Render_viewportHeight;

// decomp: vs_main_drawEnv (vs_main_drawEnv@SLUS_010.40)
DRAWENV Render_legacyDrawEnvironments[2];

// decomp: vs_main_dispEnv (vs_main_dispEnv@SLUS_010.40)
DISPENV Render_legacyDisplayEnvironments[2];

// decomp: vs_main_frameBuf (vs_main_frameBuf@SLUS_010.40)
int Render_frameBuf;

// decomp: D_8004A52C (D_8004A52C@SLUS_010.40)
int Render_isFrameStepPaused = 0;

}  // extern "C"
