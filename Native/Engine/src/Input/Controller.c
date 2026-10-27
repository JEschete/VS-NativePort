// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Boot/GameLoop.h"
#include "Boot/Lifecycle.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Input/Controllers.h"
#include "Input/Vibration.h"
#include "State/Settings.h"
#include "State/Timers.h"

// decomp: _padAct (_padAct@SLUS_010.40)
Input_VibrationPatternSlot Input_vibrationPatternSlots[2];

// decomp: _portInfo (_portInfo@SLUS_010.40)
Input_ControllerPortState Input_controllerPorts[2];

// decomp: vs_main_buttonsReleased (vs_main_buttonsReleased@SLUS_010.40)
int Input_releasedButtons;

// decomp: _buttonHeldFrameCount (_buttonHeldFrameCount@SLUS_010.40)
u_int Input_buttonHeldFrames[32];

// decomp: vs_main_stickPosBuf (vs_main_stickPosBuf@SLUS_010.40)
Input_StickPosition Input_stickPosBuf;

// decomp: vs_main_buttonRepeat (vs_main_buttonRepeat@SLUS_010.40)
int Input_buttonRepeat;

// decomp: vs_main_buttonsPreviousState (vs_main_buttonsPreviousState@SLUS_010.40)
int Input_previousButtons;

// decomp: vs_main_buttonsPressed (vs_main_buttonsPressed@SLUS_010.40)
union Input_PressedButtonsView Input_buttonsPressed;

// decomp: vs_main_buttonsState (vs_main_buttonsState@SLUS_010.40)
int Input_buttonsState;

// decomp: vs_main_settings (vs_main_settings@SLUS_010.40)
State_Settings Input_settings;

// decomp: BATTLE.PRG vs_battle_mapStickDeadZone (src/BATTLE/BATTLE.PRG/5BE50.c:82)
int Input_MapStickDeadZone(int axis_byte)
{
    if (axis_byte < 64) {
        return axis_byte - 64;
    }
    if (axis_byte >= 192) {
        return axis_byte - 192;
    }
    return 0;
}

// decomp: SLUS_010.40 vs_main_padDisconnectAll (src/SLUS_010.40/main.c:8454)
void Input_InvalidateControllerConnections(void)
{
    Input_controllerPorts[0].connected = 0;
    Input_controllerPorts[1].connected = 0;
}

// decomp: SLUS_010.40 _unlockPadModeSwitch (src/SLUS_010.40/main.c:8460)
void Input_UnlockAnalogModeSwitch(void)
{
    Input_InvalidateControllerConnections();
    if (PadInfoMode(0, InfoModeCurExID, 0) != 0) {
        PadSetMainMode(0, 1, 2);
    }
}

// decomp: SLUS_010.40 _padResetDefaults (src/SLUS_010.40/main.c:8468)
void Input_ConfigureControllerActuators(int portID, char padBuf[34] __attribute__((unused)))
{
    Input_ControllerPortState* port = &Input_controllerPorts[portID >> 4];
    port->extendedType = PadInfoMode(portID, InfoModeCurExID, 0);

    if (port->extendedType) {
        port->actuatorLevels[0] = 0;
        port->actuatorLevels[1] = 0;
        port->modeLock = 2;
        PadSetMainMode(portID, PadInfoMode(portID, InfoModeCurExOffs, 0), port->modeLock);
    } else {
        port->actuatorLevels[0] = 0x40;
        port->actuatorLevels[1] = 0;
    }
    PadSetAct(portID, port->actuatorLevels, sizeof port->actuatorLevels);
    PadSetActAlign(portID, Input_legacyActuatorAlignment);
}

// decomp: SLUS_010.40 vs_main_updatePadState (src/SLUS_010.40/main.c:8486)
int Input_ReadControllerPort(int portID, char padBuf[34])
{
    Input_ControllerPortState* port;
    char mode;
    int btnStates;

    if (padBuf[0] != 0) {
        port = &Input_controllerPorts[portID >> 4];
        port->leftStickY = 128;
        port->leftStickX = 128;
        port->rightStickY = 128;
        port->rightStickX = 128;
        return 0;
    }
    port = &Input_controllerPorts[portID >> 4];
    mode = PadInfoMode(portID, InfoModeCurID, 0);
    port->controllerType = mode;
    switch (mode) {
    case 1:
    case 2:
    case 3:
    case 6:
        btnStates = 0;
        break;
    default:
        btnStates = ~((padBuf[2] << 8) | padBuf[3]);
        break;
    }

    port->controllerType = PadInfoMode(portID, InfoModeCurID, 0);
    if ((port->controllerType == 5) || (port->controllerType == 7)) {
        port->rightStickX = padBuf[6];
        port->rightStickY = padBuf[7];
        port->leftStickX = padBuf[4];
        port->leftStickY = padBuf[5];
    } else {
        port->leftStickY = 128;
        port->leftStickX = 128;
        port->rightStickY = 128;
        port->rightStickX = 128;
    }
    port->buttonBits = btnStates;
    return btnStates;
}

// decomp: SLUS_010.40 vs_main_padConnect (src/SLUS_010.40/main.c:8531)
void Input_UpdateControllerConnection(int portID, char padBuf[34])
{
    int dummy[5] __attribute__((unused));

    Input_ControllerPortState* port = &Input_controllerPorts[portID >> 4];
    int state = PadGetState(portID);

    if (state == PadStateFindPad) {
        port->connected = 0;
    }
    if (port->connected == 0) {
        PadSetAct(portID, port->actuatorLevels, sizeof port->actuatorLevels);
        if ((state == PadStateFindCTP1)
            || ((state == PadStateStable) && (PadSetActAlign(portID, Input_legacyActuatorAlignment) != 0))) {
            Input_ConfigureControllerActuators(portID, padBuf);
            port->connected = 1;
        }
    }
}

// decomp: SLUS_010.40 _padSetActData (src/SLUS_010.40/main.c:8551)
void Input_SetMotorCommand(int port_index, int motor_index, int command)
{
    if (motor_index != 0) {
        Input_controllerPorts[port_index].actuatorLevels[motor_index] = command;
    } else if (Input_controllerPorts[port_index].extendedType) {
        Input_controllerPorts[port_index].actuatorLevels[0] = command;
    } else {
        Input_controllerPorts[port_index].actuatorLevels[0] = 0x40;
    }
}

// decomp: SLUS_010.40 _padGetActData (src/SLUS_010.40/main.c:8562)
char Input_MotorCommand(int port_index, int motor_index) { return Input_controllerPorts[port_index].actuatorLevels[motor_index]; }

// decomp: SLUS_010.40 func_800433B4 (src/SLUS_010.40/main.c:8564)
int Input_StartVibrationPattern(char* pattern, u_int slot_request, int owner_tag)
{
    int slot_index;
    int slot_id = 0;

    if (slot_request == 0xFF) {
        for (slot_index = 0; slot_index < 2; ++slot_index) {
            if (Input_vibrationPatternSlots[slot_index].active == 0) {
                slot_id = slot_index + 1;
                break;
            }
        }
    } else if ((slot_request - 1) < 2) {
        slot_id = slot_request;
    }

    if (slot_id != 0) {
        slot_index = slot_id - 1;
        Input_vibrationPatternSlots[slot_index].pattern = pattern;
        Input_vibrationPatternSlots[slot_index].channels[4] = 1;
        Input_vibrationPatternSlots[slot_index].channels[10] = *pattern;
        Input_vibrationPatternSlots[slot_index].channels[11] = 0;
        Input_vibrationPatternSlots[slot_index].channels[5] = 0;
        Input_vibrationPatternSlots[slot_index].channels[7] = 0;
        Input_vibrationPatternSlots[slot_index].channels[1] = 0;
        Input_vibrationPatternSlots[slot_index].active = 1;
        Input_vibrationPatternSlots[slot_index].ownerTag = owner_tag;
        return slot_id;
    }
    return 0;
}

// decomp: SLUS_010.40 func_80043470 (src/SLUS_010.40/main.c:8596)
int Input_ClearVibrationSlot(int slot_id)
{
    int slot_index = slot_id - 1;
    Input_vibrationPatternSlots[slot_index].channels[7] = 0;
    Input_vibrationPatternSlots[slot_index].channels[1] = 0;
    Input_vibrationPatternSlots[slot_index].pattern = 0;
    Input_vibrationPatternSlots[slot_index].active = 0;

    return slot_id;
}

// decomp: SLUS_010.40 func_800434A4 (src/SLUS_010.40/main.c:8607)
int Input_StopVibrationPatterns(u_int slot_selector, int owner_tag)
{
    int slot_cursor;
    if (slot_selector - 1 < 2) {
        return Input_ClearVibrationSlot(slot_selector);
    }
    if (slot_selector != 0) {
        return 0;
    }
    if (owner_tag != 0) {
        for (slot_cursor = 0; slot_cursor < 2; ++slot_cursor) {
            if (Input_vibrationPatternSlots[slot_cursor].ownerTag == owner_tag) {
                Input_ClearVibrationSlot(slot_cursor + 1);
            }
        }
    } else {
        for (slot_cursor = 0; slot_cursor < 2;) {
            Input_ClearVibrationSlot(++slot_cursor);
        }
    }
    return 1;
}

// decomp: SLUS_010.40 func_80043554 (src/SLUS_010.40/main.c:8630)
int Input_PauseVibrationSlot(int slot_id)
{
    int slot_index = slot_id - 1;
    Input_vibrationPatternSlots[slot_index].active = 0;
    return slot_id;
}

// decomp: SLUS_010.40 func_8004357C (src/SLUS_010.40/main.c:8637)
int Input_PauseVibrationPatterns(int slot_selector)
{
    int slot_id;

    if ((slot_selector - 1u) < 2) {
        return Input_PauseVibrationSlot(slot_selector);
    }

    if (slot_selector != 0) {
        return 0;
    }

    for (slot_id = 0; slot_id < 2;) {
        Input_PauseVibrationSlot(++slot_id);
    }

    return 1;
}

// decomp: SLUS_010.40 func_800435DC (src/SLUS_010.40/main.c:8656)
int Input_ResumeVibrationSlot(int slot_id)
{
    int slot_index = slot_id - 1;
    Input_vibrationPatternSlots[slot_index].active = 1;
    return slot_id;
}

// decomp: SLUS_010.40 func_80043608 (src/SLUS_010.40/main.c:8663)
int Input_ResumeVibrationPatterns(int slot_selector)
{
    int slot_id;

    if ((slot_selector - 1u) < 2) {
        return Input_ResumeVibrationSlot(slot_selector);
    }

    if (slot_selector != 0) {
        return 0;
    }

    for (slot_id = 0; slot_id < 2;) {
        Input_ResumeVibrationSlot(++slot_id);
    }

    return 1;
}

// decomp: SLUS_010.40 vs_main_resetPadAct (src/SLUS_010.40/main.c:8682)
void Input_ResetVibrationPatterns(void)
{
    int slot_index;

    Input_InvalidateControllerConnections();

    for (slot_index = 0; slot_index < 2; ++slot_index) {
        Input_vibrationPatternSlots[slot_index].pattern = NULL;
        Input_vibrationPatternSlots[slot_index].channels[7] = 0;
        Input_vibrationPatternSlots[slot_index].channels[1] = 0;
        Input_vibrationPatternSlots[slot_index].active = 0;
    }
}

// decomp: SLUS_010.40 _abs2 (src/SLUS_010.40/main.c:8696)
int Input_RoundMotorIntensityTowardZero(int intensity_q8) { return intensity_q8 < 0 ? intensity_q8 += 0xFF : intensity_q8; }

// decomp: SLUS_010.40 vs_main_setVibrateParams (src/SLUS_010.40/main.c:8698)
void Input_UpdateVibrationPatterns(void)
{
    int pattern_offset;
    int var_a2_2;
    int motor_index;
    int slot_index;
    int var_v0;
    char* pattern;
    int interpolate_intensity;

    Input_vibrationPatternSlots[0].motorLevels[1] = 0;
    Input_vibrationPatternSlots[0].motorLevels[0] = 0;

    for (slot_index = 0; slot_index < 2; ++slot_index) {
        if (Input_vibrationPatternSlots[slot_index].active == 0) {
            continue;
        }

        pattern = Input_vibrationPatternSlots[slot_index].pattern;

        if (pattern != 0) {
            for (motor_index = 0; motor_index < 2; ++motor_index) {
                int (*motor_envelopes)[6] = (int (*)[6])Input_vibrationPatternSlots[slot_index].channels;
                if (motor_envelopes[motor_index][5] == 0) {
                    pattern_offset = motor_envelopes[motor_index][4];
                    motor_envelopes[motor_index][5] = pattern[pattern_offset++];
                    motor_envelopes[motor_index][1] = motor_envelopes[motor_index][3] = pattern[pattern_offset++] << 8;
                    if (motor_envelopes[motor_index][5] == 0) {
                        motor_envelopes[motor_index][1] = 0;
                        motor_envelopes[motor_index][0] = 0;
                    } else {
                        interpolate_intensity = motor_index != 0;
                        motor_envelopes[motor_index][3] = pattern[pattern_offset + 1] << 8;
                        if (interpolate_intensity) {
                            motor_envelopes[motor_index][2] = ((motor_envelopes[motor_index][3] - motor_envelopes[motor_index][1]) / motor_envelopes[motor_index][5]);
                        } else {
                            motor_envelopes[motor_index][2] = 0;
                        }
                        motor_envelopes[motor_index][0] = 1;
                        motor_envelopes[motor_index][4] += 2;
                    }
                }
                if (motor_envelopes[motor_index][5] != 0) {
                    motor_envelopes[motor_index][1] += motor_envelopes[motor_index][2];
                    motor_envelopes[motor_index][5] -= 1;
                } else {
                    motor_envelopes[motor_index][1] = motor_envelopes[motor_index][3];
                }

                Input_vibrationPatternSlots[0].motorLevels[motor_index] += motor_envelopes[motor_index][1];
                if (Input_vibrationPatternSlots[0].motorLevels[motor_index] > 0xFF00) {
                    Input_vibrationPatternSlots[0].motorLevels[motor_index] = 0xFF00;
                } else if (Input_vibrationPatternSlots[0].motorLevels[motor_index] < 0) {
                    Input_vibrationPatternSlots[0].motorLevels[motor_index] = 0;
                }
            }
            Input_vibrationPatternSlots[slot_index].active = Input_vibrationPatternSlots[slot_index].channels[0] | Input_vibrationPatternSlots[slot_index].channels[6];
            if (Input_vibrationPatternSlots[slot_index].active == 0) {
                Input_vibrationPatternSlots[slot_index].pattern = 0;
            }
            continue;
        }
        Input_vibrationPatternSlots[slot_index].active = 0;
    }

    if (Input_settings.vibrationOn != 0) {
        Input_SetMotorCommand(0, 0, Input_RoundMotorIntensityTowardZero(Input_vibrationPatternSlots[0].motorLevels[0]) >> 8);
        Input_SetMotorCommand(0, 1, Input_RoundMotorIntensityTowardZero(Input_vibrationPatternSlots[0].motorLevels[1]) >> 8);
    }
}

// decomp: SLUS_010.40 func_800438C8 (src/SLUS_010.40/main.c:8769)
void Input_PlayVibrationPreset(int preset_index)
{
    int offset_byte2_index;
    int offset_byte1_index;
    int directory_stride;
    int offset_byte3_index;

    if (preset_index < Input_vibrationPresets[0]) {
        offset_byte1_index = 5;
        directory_stride = 4;
        offset_byte2_index = 6;
        offset_byte3_index = 7;
        Input_StartVibrationPattern(&Input_vibrationPresets[(&Input_vibrationPresets[preset_index * directory_stride])[directory_stride]
                                  + ((&Input_vibrationPresets[preset_index * directory_stride])[offset_byte1_index] << 8)
                                  + ((&Input_vibrationPresets[preset_index * directory_stride])[offset_byte2_index] << 16)
                                  + ((&Input_vibrationPresets[preset_index * directory_stride])[offset_byte3_index] << 24)],
            0xFF, 0x80);
    }
}

// decomp: SLUS_010.40 vs_main_processPadState (src/SLUS_010.40/main.c:8789)
int Input_UpdateControllers(void)
{
    int dummy[2] __attribute__((unused));
    int i;
    u_int btnState;

    Input_buttonsState = Input_ReadControllerPort(0, Input_legacyPadResponseBytes[0]) & 0xFFFF;
    Input_buttonsState |= Input_ReadControllerPort(16, Input_legacyPadResponseBytes[1]) << 16;
    Input_UpdateControllerConnection(0, Input_legacyPadResponseBytes[0]);
    Input_UpdateControllerConnection(16, Input_legacyPadResponseBytes[1]);

    switch (Input_controllerPorts[0].controllerType) {
    case 4:
        Input_stickPosBuf.lStickY = 128;
        Input_stickPosBuf.lStickX = 128;
        Input_stickPosBuf.rStickY = 128;
        Input_stickPosBuf.rStickX = 128;
        break;
    case 7:
        Input_stickPosBuf.rStickX = Input_controllerPorts[0].rightStickX;
        Input_stickPosBuf.rStickY = Input_controllerPorts[0].rightStickY;
        Input_stickPosBuf.lStickX = Input_controllerPorts[0].leftStickX;
        Input_stickPosBuf.lStickY = Input_controllerPorts[0].leftStickY;

        if (Input_controllerPorts[0].rightStickX < 16) {
            Input_buttonsState |= PADLleft;
        }
        if (Input_controllerPorts[0].rightStickX >= 241) {
            Input_buttonsState |= PADLright;
        }

        if (Input_controllerPorts[0].rightStickY < 16) {
            Input_buttonsState |= PADLup;
        }
        if (Input_controllerPorts[0].rightStickY >= 241) {
            Input_buttonsState |= PADLdown;
        }

        if (Input_controllerPorts[0].leftStickX < 32) {
            Input_buttonsState |= PADj;
        }
        if (Input_controllerPorts[0].leftStickX >= 225) {
            Input_buttonsState |= PADj;
        }

        if (Input_controllerPorts[0].leftStickY < 32) {
            Input_buttonsState |= PADj;
        }
        if (Input_controllerPorts[0].leftStickY >= 225) {
            Input_buttonsState |= PADj;
        }
        break;
    default:
        Input_stickPosBuf.lStickY = 128;
        Input_stickPosBuf.lStickX = 128;
        Input_stickPosBuf.rStickY = 128;
        Input_stickPosBuf.rStickX = 128;
        break;
    }

    Input_buttonsPressed.allButtons = ~Input_previousButtons & Input_buttonsState;
    Input_releasedButtons = Input_previousButtons & ~Input_buttonsState;
    Input_previousButtons = Input_buttonsState;

    btnState = Input_buttonsState;
    for (i = 0; i < 32; ++i, btnState >>= 1) {
        if (btnState & 1) {
            Input_buttonHeldFrames[i] += State_tickSpeed;
        } else {
            Input_buttonHeldFrames[i] = 0;
        }
    }

    Input_buttonRepeat = Input_buttonsPressed.allButtons;

    for (i = 0; i < 32; ++i) {
        if ((Input_buttonHeldFrames[i] >= 20) && !((Input_buttonHeldFrames[i] - 20) & 3)) {
            Input_buttonRepeat |= 1 << i;
        }
    }

#define VS_RESET (PADstart | PADselect | PADR1 | PADR2 | PADL1 | PADL2)

    if ((Boot_isSoftResetEnabled != 0) && ((Input_previousButtons & VS_RESET) == VS_RESET)
        && (Input_buttonsPressed.allButtons & VS_RESET)) {
        Input_settings.monoSound = 0;
        Input_settings.vibrationOn = 1;
        Boot_ResetToTitle();
    }

#undef VS_RESET

    return 1;
}

