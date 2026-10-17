// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_INPUT_CONTROLLER_H_TYPES
#define VS_INPUT_CONTROLLER_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_INPUT_CONTROLLER_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_INPUT_CONTROLLER_H_DECLS)
#define VS_INPUT_CONTROLLER_H_DECLS
#include "Input/Buttons.h"
#include "Input/Controllers.h"
#include "Input/Vibration.h"
#include "State/Settings.h"

extern Input_VibrationPatternSlot Input_vibrationPatternSlots[2];
extern Input_ControllerPortState Input_controllerPorts[2];
extern int Input_releasedButtons;
extern u_int Input_buttonHeldFrames[32];
extern Input_StickPosition Input_stickPosBuf;
extern int Input_buttonRepeat;
extern int Input_previousButtons;
extern union Input_PressedButtonsView Input_buttonsPressed;
extern int Input_buttonsState;
extern State_Settings Input_settings;
int Input_MapStickDeadZone(int arg0);
void Input_InvalidateControllerConnections(void);
void Input_UnlockAnalogModeSwitch(void);
void Input_ConfigureControllerActuators(int portID, char padBuf[34] __attribute__((unused)));
int Input_ReadControllerPort(int portID, char padBuf[34]);
void Input_UpdateControllerConnection(int portID, char padBuf[34]);
void Input_SetMotorCommand(int port, int pos, int val);
char Input_MotorCommand(int port, int pos);
int Input_StartVibrationPattern(char* arg0, u_int arg1, int arg2);
int Input_ClearVibrationSlot(int arg0);
int Input_StopVibrationPatterns(u_int arg0, int arg1);
int Input_PauseVibrationSlot(int arg0);
int Input_PauseVibrationPatterns(int arg0);
int Input_ResumeVibrationSlot(int arg0);
int Input_ResumeVibrationPatterns(int arg0);
void Input_ResetVibrationPatterns(void);
int Input_RoundMotorIntensityTowardZero(int arg);
void Input_UpdateVibrationPatterns(void);
void Input_PlayVibrationPreset(int arg0);
int Input_UpdateControllers(void);

#endif // VS_INPUT_CONTROLLER_H_DECLS
