// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Save_CardUiResources;

#include "Save/MemoryCard/GameFiles.h"

// decomp: mcdata_t (src/MENU/MENU7.PRG/260.c:162)
struct Save_CardUiResources {
    u_short locationPalettes[2][256];
    char fileIconPalettes[7][32];
    char fileIconBitmaps[7][128];
    char reserved860[7][128];
    char reservedBE0[8][128];
    char reservedFE0[32];
    u_short textTable[0x800];
    Save_CardFileInfoRecord fileHeaders[5];
    struct DIRENTRY directoryEntry;
};

extern Save_CardUiResources* Save_MemoryCard_uiResources;;
