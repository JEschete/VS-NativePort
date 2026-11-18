// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

union Input_PressedButtonsView;
struct Input_StickPosition;

// decomp: vs_main_stickPos (src/SLUS_010.40/main.h:389)
struct Input_StickPosition {
    int lStickX;
    int lStickY;
    int rStickX;
    int rStickY;
};
static_assert(sizeof(Input_StickPosition) == 0x10, "Input_StickPosition must keep its PS1 layout");

// decomp: (unnamed at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\..\..\SLUS_010.40\main.h:482:8) (src/SLUS_010.40/main.h:482)
union Input_PressedButtonsView {
    u_int allButtons;
    struct {
        char lowByte;
        char highByte;
    } ports[2];
};
union Input_PressedButtonsView;
