#pragma once
// Hand-maintained (tools/port/owned.csv).
// The legacy view of the typed VS::Input controller state that the copied code
// reads and writes. ControllerLive.cpp defines it and keeps it in step with
// the core (Controller.hpp).

#include "Input/Buttons.h"
#include "Input/Controllers.h"
#include "State/Settings.h"

#ifdef __cplusplus
extern "C" {
#endif

// main.c:8854-8859 _buttonHeldFrameCount: one counter per button word bit.
enum { kInputButtonHeldFrameCount = 32 };

// main.c:8470 portInfo_t _portInfo[2]. rightStickX/Y hold the physical left
// stick and leftStickX/Y the right one, as the original named them.
extern Input_ControllerPortState Input_controllerPorts[2];
extern int Input_releasedButtons;
extern u_int Input_buttonHeldFrames[kInputButtonHeldFrameCount];
extern Input_StickPosition Input_stickPosBuf;
extern int Input_buttonRepeat;
extern int Input_previousButtons;
extern union Input_PressedButtonsView Input_buttonsPressed;
extern int Input_buttonsState;
extern State_Settings Input_settings;

#ifdef __cplusplus
}
#endif
