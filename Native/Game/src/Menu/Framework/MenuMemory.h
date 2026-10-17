// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_FRAMEWORK_MENUMEMORY_H_TYPES
#define VS_MENU_FRAMEWORK_MENUMEMORY_H_TYPES
#include "Legacy/Psx.h"

typedef struct Menu_Framework_MenuMemory Menu_Framework_MenuMemory;

// decomp: D_800F4EE8_t (src/BATTLE/BATTLE.PRG/5BF94.h:145)
typedef struct Menu_Framework_MenuMemory {
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
} Menu_Framework_MenuMemory;

#endif // VS_MENU_FRAMEWORK_MENUMEMORY_H_TYPES
