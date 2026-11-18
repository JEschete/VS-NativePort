// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Menu_Framework_MenuMemory;

// decomp: D_800F4EE8_t (src/BATTLE/BATTLE.PRG/5BF94.h:145)
struct Menu_Framework_MenuMemory {
    char cursorMemories[52];
    char opaque34[6];
    struct {
        char currentCategory;
        char reserved1;
        union {
            struct {
                u_char row;
                u_char page;
            } position;
            u_short word;
        } categoryCursors[8];
    } opaque3A;
    char reserved4C[4];
    char reserved50;
    char opaque51[51];
    char selectedEquipment;
    char opaque85[25];
    u_short opaque9E;
    char opaqueA0[0x12];
    char reservedB2[0x40];
};
