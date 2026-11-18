// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Menu_Framework_PreviewPoseBytes;
struct Menu_Framework_PreviewPoseState;

// decomp: D_800F4538_unk58_3 (src/MENU/MAINMENU.PRG/224.c:98)
struct Menu_Framework_PreviewPoseBytes {
    u_int opaque0Bit0 : 8;
    u_int opaque1Bit0 : 8;
    u_int opaque2Bit0 : 8;
    u_int opaque3Bit0 : 8;
};
static_assert(sizeof(Menu_Framework_PreviewPoseBytes) == 0x4, "Menu_Framework_PreviewPoseBytes must keep its PS1 layout");

// decomp: D_800F4590_t (src/BATTLE/BATTLE.PRG/30DB0.h:18)
struct Menu_Framework_PreviewPoseState {
    int reserved0;
    int reserved4;
    int opaque8;
    int reservedC;
    char reserved10;
    char opaque11;
};
static_assert(sizeof(Menu_Framework_PreviewPoseState) == 0x14, "Menu_Framework_PreviewPoseState must keep its PS1 layout");
