#pragma once
// The viewport and the two framebuffers' drawing and display environments
// that the legacy GPU setup reads, the framebuffer being drawn and the frame
// step pause.
#include <cstdint>

#include "Legacy/Render/LibGpu.h"
#include "Render/DisplayControl.hpp"

namespace VS::Render {
// decomp: D_8005DFD4 (D_8005DFD4@SLUS_010.40)
extern std::int16_t legacyViewportLeft;
// decomp: D_8005DFD6 (D_8005DFD6@SLUS_010.40)
extern std::int16_t viewportWidth;
// decomp: D_8005DFD8 (D_8005DFD8@SLUS_010.40)
extern std::int16_t legacyViewportTop;
// decomp: D_8005DFDA (D_8005DFDA@SLUS_010.40)
extern std::uint16_t viewportHeight;
// decomp: vs_main_drawEnv (vs_main_drawEnv@SLUS_010.40)
extern DRAWENV legacyDrawEnvironments[kFrameBufferCount];
// decomp: vs_main_dispEnv (vs_main_dispEnv@SLUS_010.40)
extern DISPENV legacyDisplayEnvironments[kFrameBufferCount];
// decomp: vs_main_frameBuf (vs_main_frameBuf@SLUS_010.40)
extern int frameBuf;
// decomp: D_8004A52C (D_8004A52C@SLUS_010.40)
extern int isFrameStepPaused;
}  // namespace VS::Render
