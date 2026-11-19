#pragma once
// Hand-maintained (tools/port/owned.csv).
// The legacy view of the typed VS::Input controller state that the copied code
// reads and writes. ControllerLive.cpp defines it and keeps it in step with
// the core (Controller.hpp).

#include <cstdint>

#include "Input/Buttons.hpp"
#include "Input/Controller.hpp"
#include "Input/Controllers.hpp"
#include "Input/Settings.hpp"

namespace VS::Input {
// main.c:8854-8859 _buttonHeldFrameCount: one counter per button word bit.
inline constexpr int kButtonHeldFrameCount = 32;
// main.c:8470 portInfo_t _portInfo[2]; Controllers.hpp names the sticks.
extern LegacyControllerPort controllerPorts[kControllerPortCount];
extern int releasedButtons;
extern std::uint32_t buttonHeldFrames[kButtonHeldFrameCount];
extern StickPosition stickPosBuf;
extern int buttonRepeat;
extern int previousButtons;
// vs_main_buttonsPressed; LowButtonByte reads the first pad's face and
// shoulder byte.
extern std::uint32_t buttonsPressed;
extern int buttonsState;
extern VS::Input::Settings settings;
}  // namespace VS::Input
