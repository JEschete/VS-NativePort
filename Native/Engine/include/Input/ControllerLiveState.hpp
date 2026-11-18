#pragma once
// Hand-maintained (tools/port/owned.csv).
// The legacy view of the typed VS::Input controller state that the copied code
// reads and writes. ControllerLive.cpp defines it and keeps it in step with
// the core (Controller.hpp).

#include "Input/Buttons.h"
#include "Input/Controllers.h"
#include "State/Settings.h"

namespace VS::Input {
// main.c:8854-8859 _buttonHeldFrameCount: one counter per button word bit.
inline constexpr int kButtonHeldFrameCount = 32;
// main.c:8470 portInfo_t _portInfo[2]. rightStickX/Y hold the physical left
// stick and leftStickX/Y the right one, as the original named them.
extern Input_ControllerPortState controllerPorts[2];
extern int releasedButtons;
extern u_int buttonHeldFrames[kButtonHeldFrameCount];
extern Input_StickPosition stickPosBuf;
extern int buttonRepeat;
extern int previousButtons;
extern union Input_PressedButtonsView buttonsPressed;
extern int buttonsState;
extern State_Settings settings;
}  // namespace VS::Input
