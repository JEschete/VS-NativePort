#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Legacy/Psx.h"
#include "Render/DisplayControl.hpp"

// Live binding for the display policies (DisplayControl.cpp): the libgpu
// display and draw environments, the frame-buffer index, the viewport globals
// and the module-loaded loading image.
namespace VS::Render {
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): original call shape.
void ConfigureLiveFieldDisplay(int frame_width, int frame_height,
                               int projection_distance, int clear_red,
                               int clear_green, int clear_blue);
int PresentLiveFieldFrame(u_long* ordering_table);
void EnableLiveDisplay();
void DisableLiveDisplay();
void ShowLiveDisplay();
void HideLiveDisplay();
void ShowLiveLoadingScreen();
}  // namespace VS::Render
