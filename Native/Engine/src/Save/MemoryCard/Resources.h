// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_SAVE_MEMORYCARD_RESOURCES_H_TYPES
#define VS_SAVE_MEMORYCARD_RESOURCES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Save_CardUiResources Save_CardUiResources;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_SAVE_MEMORYCARD_RESOURCES_H_SET_TYPES_ONLY
#endif
#include "Save/MemoryCard/GameFiles.h"
#ifdef VS_SAVE_MEMORYCARD_RESOURCES_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_SAVE_MEMORYCARD_RESOURCES_H_SET_TYPES_ONLY
#endif

// decomp: mcdata_t (src/MENU/MENU7.PRG/260.c:162)
typedef struct Save_CardUiResources {
    u_short locationPalettes[2][256];
    char fileIconPalettes[7][32];
    char fileIconBitmaps[7][128];
    char reserved860[7][128];
    char reservedBE0[8][128];
    char reservedFE0[32];
    u_short textTable[0x800];
    Save_CardFileInfoRecord fileHeaders[5];
    struct DIRENTRY directoryEntry;
} Save_CardUiResources;

#endif // VS_SAVE_MEMORYCARD_RESOURCES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_SAVE_MEMORYCARD_RESOURCES_H_DECLS)
#define VS_SAVE_MEMORYCARD_RESOURCES_H_DECLS

extern Save_CardUiResources* Save_MemoryCard_uiResources;;

#endif // VS_SAVE_MEMORYCARD_RESOURCES_H_DECLS
