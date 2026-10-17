// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_INPUT_VIBRATION_H_TYPES
#define VS_INPUT_VIBRATION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Input_VibrationPatternSlot Input_VibrationPatternSlot;

// decomp: padAct_t (src/SLUS_010.40/main.c:25)
typedef struct Input_VibrationPatternSlot {
    int motorLevels[2];
    u_short active;
    u_short ownerTag;
    char* pattern;
    int channels[10];
} Input_VibrationPatternSlot;

#endif // VS_INPUT_VIBRATION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_INPUT_VIBRATION_H_DECLS)
#define VS_INPUT_VIBRATION_H_DECLS

extern char Input_vibrationPresets[];

#endif // VS_INPUT_VIBRATION_H_DECLS
