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
int Input_MapStickDeadZone(int arg0)
{
    if (arg0 < 64) {
        return arg0 - 64;
    }
    if (arg0 >= 192) {
        return arg0 - 192;
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
void Input_SetMotorCommand(int port, int pos, int val)
{
    if (pos != 0) {
        Input_controllerPorts[port].actuatorLevels[pos] = val;
    } else if (Input_controllerPorts[port].extendedType) {
        Input_controllerPorts[port].actuatorLevels[0] = val;
    } else {
        Input_controllerPorts[port].actuatorLevels[0] = 0x40;
    }
}

// decomp: SLUS_010.40 _padGetActData (src/SLUS_010.40/main.c:8562)
char Input_MotorCommand(int port, int pos) { return Input_controllerPorts[port].actuatorLevels[pos]; }

// decomp: SLUS_010.40 func_800433B4 (src/SLUS_010.40/main.c:8564)
int Input_StartVibrationPattern(char* arg0, u_int arg1, int arg2)
{
    int i;
    int ret = 0;

    if (arg1 == 0xFF) {
        for (i = 0; i < 2; ++i) {
            if (Input_vibrationPatternSlots[i].active == 0) {
                ret = i + 1;
                break;
            }
        }
    } else if ((arg1 - 1) < 2) {
        ret = arg1;
    }

    if (ret != 0) {
        i = ret - 1;
        Input_vibrationPatternSlots[i].pattern = arg0;
        Input_vibrationPatternSlots[i].channels[4] = 1;
        Input_vibrationPatternSlots[i].channels[10] = *arg0;
        Input_vibrationPatternSlots[i].channels[11] = 0;
        Input_vibrationPatternSlots[i].channels[5] = 0;
        Input_vibrationPatternSlots[i].channels[7] = 0;
        Input_vibrationPatternSlots[i].channels[1] = 0;
        Input_vibrationPatternSlots[i].active = 1;
        Input_vibrationPatternSlots[i].ownerTag = arg2;
        return ret;
    }
    return 0;
}

// decomp: SLUS_010.40 func_80043470 (src/SLUS_010.40/main.c:8596)
int Input_ClearVibrationSlot(int arg0)
{
    int i = arg0 - 1;
    Input_vibrationPatternSlots[i].channels[7] = 0;
    Input_vibrationPatternSlots[i].channels[1] = 0;
    Input_vibrationPatternSlots[i].pattern = 0;
    Input_vibrationPatternSlots[i].active = 0;

    return arg0;
}

// decomp: SLUS_010.40 func_800434A4 (src/SLUS_010.40/main.c:8607)
int Input_StopVibrationPatterns(u_int arg0, int arg1)
{
    int i;
    if (arg0 - 1 < 2) {
        return Input_ClearVibrationSlot(arg0);
    }
    if (arg0 != 0) {
        return 0;
    }
    if (arg1 != 0) {
        for (i = 0; i < 2; ++i) {
            if (Input_vibrationPatternSlots[i].ownerTag == arg1) {
                Input_ClearVibrationSlot(i + 1);
            }
        }
    } else {
        for (i = 0; i < 2;) {
            Input_ClearVibrationSlot(++i);
        }
    }
    return 1;
}

// decomp: SLUS_010.40 func_80043554 (src/SLUS_010.40/main.c:8630)
int Input_PauseVibrationSlot(int arg0)
{
    int v0 = arg0 - 1;
    Input_vibrationPatternSlots[v0].active = 0;
    return arg0;
}

// decomp: SLUS_010.40 func_8004357C (src/SLUS_010.40/main.c:8637)
int Input_PauseVibrationPatterns(int arg0)
{
    int i;

    if ((arg0 - 1u) < 2) {
        return Input_PauseVibrationSlot(arg0);
    }

    if (arg0 != 0) {
        return 0;
    }

    for (i = 0; i < 2;) {
        Input_PauseVibrationSlot(++i);
    }

    return 1;
}

// decomp: SLUS_010.40 func_800435DC (src/SLUS_010.40/main.c:8656)
int Input_ResumeVibrationSlot(int arg0)
{
    int v0 = arg0 - 1;
    Input_vibrationPatternSlots[v0].active = 1;
    return arg0;
}

// decomp: SLUS_010.40 func_80043608 (src/SLUS_010.40/main.c:8663)
int Input_ResumeVibrationPatterns(int arg0)
{
    int i;

    if ((arg0 - 1u) < 2) {
        return Input_ResumeVibrationSlot(arg0);
    }

    if (arg0 != 0) {
        return 0;
    }

    for (i = 0; i < 2;) {
        Input_ResumeVibrationSlot(++i);
    }

    return 1;
}

// decomp: SLUS_010.40 vs_main_resetPadAct (src/SLUS_010.40/main.c:8682)
void Input_ResetVibrationPatterns(void)
{
    int i;

    Input_InvalidateControllerConnections();

    for (i = 0; i < 2; ++i) {
        Input_vibrationPatternSlots[i].pattern = NULL;
        Input_vibrationPatternSlots[i].channels[7] = 0;
        Input_vibrationPatternSlots[i].channels[1] = 0;
        Input_vibrationPatternSlots[i].active = 0;
    }
}

// decomp: SLUS_010.40 _abs2 (src/SLUS_010.40/main.c:8696)
int Input_RoundMotorIntensityTowardZero(int arg) { return arg < 0 ? arg += 0xFF : arg; }

// decomp: SLUS_010.40 vs_main_setVibrateParams (src/SLUS_010.40/main.c:8698)
void Input_UpdateVibrationPatterns(void)
{
    int temp_a0;
    int var_a2_2;
    int i;
    int j;
    int var_v0;
    char* temp_t1;
    int new_var;

    Input_vibrationPatternSlots[0].motorLevels[1] = 0;
    Input_vibrationPatternSlots[0].motorLevels[0] = 0;

    for (j = 0; j < 2; ++j) {
        if (Input_vibrationPatternSlots[j].active == 0) {
            continue;
        }

        temp_t1 = Input_vibrationPatternSlots[j].pattern;

        if (temp_t1 != 0) {
            for (i = 0; i < 2; ++i) {
                int (*a1)[6] = (int (*)[6])Input_vibrationPatternSlots[j].channels;
                if (a1[i][5] == 0) {
                    temp_a0 = a1[i][4];
                    a1[i][5] = temp_t1[temp_a0++];
                    a1[i][1] = a1[i][3] = temp_t1[temp_a0++] << 8;
                    if (a1[i][5] == 0) {
                        a1[i][1] = 0;
                        a1[i][0] = 0;
                    } else {
                        new_var = i != 0;
                        a1[i][3] = temp_t1[temp_a0 + 1] << 8;
                        if (new_var) {
                            a1[i][2] = ((a1[i][3] - a1[i][1]) / a1[i][5]);
                        } else {
                            a1[i][2] = 0;
                        }
                        a1[i][0] = 1;
                        a1[i][4] += 2;
                    }
                }
                if (a1[i][5] != 0) {
                    a1[i][1] += a1[i][2];
                    a1[i][5] -= 1;
                } else {
                    a1[i][1] = a1[i][3];
                }

                Input_vibrationPatternSlots[0].motorLevels[i] += a1[i][1];
                if (Input_vibrationPatternSlots[0].motorLevels[i] > 0xFF00) {
                    Input_vibrationPatternSlots[0].motorLevels[i] = 0xFF00;
                } else if (Input_vibrationPatternSlots[0].motorLevels[i] < 0) {
                    Input_vibrationPatternSlots[0].motorLevels[i] = 0;
                }
            }
            Input_vibrationPatternSlots[j].active = Input_vibrationPatternSlots[j].channels[0] | Input_vibrationPatternSlots[j].channels[6];
            if (Input_vibrationPatternSlots[j].active == 0) {
                Input_vibrationPatternSlots[j].pattern = 0;
            }
            continue;
        }
        Input_vibrationPatternSlots[j].active = 0;
    }

    if (Input_settings.vibrationOn != 0) {
        Input_SetMotorCommand(0, 0, Input_RoundMotorIntensityTowardZero(Input_vibrationPatternSlots[0].motorLevels[0]) >> 8);
        Input_SetMotorCommand(0, 1, Input_RoundMotorIntensityTowardZero(Input_vibrationPatternSlots[0].motorLevels[1]) >> 8);
    }
}

// decomp: SLUS_010.40 func_800438C8 (src/SLUS_010.40/main.c:8769)
void Input_PlayVibrationPreset(int arg0)
{
    int new_var4;
    int new_var;
    int new_var3;
    int new_var2;

    if (arg0 < Input_vibrationPresets[0]) {
        new_var = 5;
        new_var3 = 4;
        new_var4 = 6;
        new_var2 = 7;
        Input_StartVibrationPattern(&Input_vibrationPresets[(&Input_vibrationPresets[arg0 * new_var3])[new_var3]
                                  + ((&Input_vibrationPresets[arg0 * new_var3])[new_var] << 8)
                                  + ((&Input_vibrationPresets[arg0 * new_var3])[new_var4] << 16)
                                  + ((&Input_vibrationPresets[arg0 * new_var3])[new_var2] << 24)],
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

