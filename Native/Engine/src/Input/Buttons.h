// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_INPUT_BUTTONS_H_TYPES
#define VS_INPUT_BUTTONS_H_TYPES
#include "Legacy/Psx.h"

typedef union Input_PressedButtonsView Input_PressedButtonsView;
typedef struct Input_StickPosition Input_StickPosition;

// decomp: vs_main_stickPos (src/SLUS_010.40/main.h:389)
typedef struct Input_StickPosition {
    int lStickX;
    int lStickY;
    int rStickX;
    int rStickY;
} Input_StickPosition;
_Static_assert(sizeof(Input_StickPosition) == 0x10, "Input_StickPosition must keep its PS1 layout");

// decomp: (unnamed at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\..\..\SLUS_010.40\main.h:482:8) (src/SLUS_010.40/main.h:482)
union Input_PressedButtonsView {
    u_int allButtons;
    struct {
        char lowByte;
        char highByte;
    } ports[2];
};
typedef union Input_PressedButtonsView Input_PressedButtonsView;

#endif // VS_INPUT_BUTTONS_H_TYPES
