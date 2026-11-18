// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Input_ControllerPortState;

// decomp: portInfo_t (src/SLUS_010.40/main.h:19)
struct Input_ControllerPortState {
    char controllerType;
    char extendedType;
    char buttonBits;
    char rightStickX;
    char rightStickY;
    char leftStickX;
    char leftStickY;
    char modeLock;
    char actuatorLevels[2];
    char connected;
};
static_assert(sizeof(Input_ControllerPortState) == 0xB, "Input_ControllerPortState must keep its PS1 layout");

extern char Input_legacyPadResponseBytes[2][34];
extern char Input_legacyActuatorAlignment[];
