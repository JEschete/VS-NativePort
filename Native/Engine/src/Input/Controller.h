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
int Input_MapStickDeadZone(int axis_byte);
void Input_InvalidateControllerConnections(void);
void Input_UnlockAnalogModeSwitch(void);
void Input_ConfigureControllerActuators(int portID, char padBuf[34] __attribute__((unused)));
int Input_ReadControllerPort(int portID, char padBuf[34]);
void Input_UpdateControllerConnection(int portID, char padBuf[34]);
void Input_SetMotorCommand(int port_index, int motor_index, int command);
char Input_MotorCommand(int port_index, int motor_index);
int Input_StartVibrationPattern(char* pattern, u_int slot_request, int owner_tag);
int Input_ClearVibrationSlot(int slot_id);
int Input_StopVibrationPatterns(u_int slot_selector, int owner_tag);
int Input_PauseVibrationSlot(int slot_id);
int Input_PauseVibrationPatterns(int slot_selector);
int Input_ResumeVibrationSlot(int slot_id);
int Input_ResumeVibrationPatterns(int slot_selector);
void Input_ResetVibrationPatterns(void);
int Input_RoundMotorIntensityTowardZero(int intensity_q8);
void Input_UpdateVibrationPatterns(void);
void Input_PlayVibrationPreset(int preset_index);
int Input_UpdateControllers(void);

#endif // VS_INPUT_CONTROLLER_H_DECLS
