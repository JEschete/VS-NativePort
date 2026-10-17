// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_INPUT_CONTROLLERS_H_TYPES
#define VS_INPUT_CONTROLLERS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Input_ControllerPortState Input_ControllerPortState;

// decomp: portInfo_t (src/SLUS_010.40/main.h:19)
typedef struct Input_ControllerPortState {
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
} Input_ControllerPortState;
_Static_assert(sizeof(Input_ControllerPortState) == 0xB, "Input_ControllerPortState must keep its PS1 layout");

#endif // VS_INPUT_CONTROLLERS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_INPUT_CONTROLLERS_H_DECLS)
#define VS_INPUT_CONTROLLERS_H_DECLS

extern char Input_legacyPadResponseBytes[2][34];
extern char Input_legacyActuatorAlignment[];

#endif // VS_INPUT_CONTROLLERS_H_DECLS
