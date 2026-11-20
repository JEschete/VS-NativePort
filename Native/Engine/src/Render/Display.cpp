// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Render/Display.hpp"

#include "Legacy/Psx.hpp"

using VS::Legacy::DISPENV;
using VS::Legacy::DRAWENV;

// decomp: D_8005DFD4 (D_8005DFD4@SLUS_010.40)
std::int16_t VS::Render::legacyViewportLeft;

// decomp: D_8005DFD6 (D_8005DFD6@SLUS_010.40)
std::int16_t VS::Render::viewportWidth;

// decomp: D_8005DFD8 (D_8005DFD8@SLUS_010.40)
std::int16_t VS::Render::legacyViewportTop;

// decomp: D_8005DFDA (D_8005DFDA@SLUS_010.40)
std::uint16_t VS::Render::viewportHeight;

// decomp: vs_main_drawEnv (vs_main_drawEnv@SLUS_010.40)
DRAWENV VS::Render::legacyDrawEnvironments[VS::Render::kFrameBufferCount];

// decomp: vs_main_dispEnv (vs_main_dispEnv@SLUS_010.40)
DISPENV VS::Render::legacyDisplayEnvironments[VS::Render::kFrameBufferCount];

// decomp: vs_main_frameBuf (vs_main_frameBuf@SLUS_010.40)
int VS::Render::frameBuf;

// decomp: D_8004A52C (D_8004A52C@SLUS_010.40)
int VS::Render::isFrameStepPaused = 0;
