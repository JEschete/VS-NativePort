// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_LOADING_H_TYPES
#define VS_EFFECTS_LOADING_H_TYPES
#include "Legacy/Psx.h"

struct Effects_FileLoadContext;
struct Effects_TextureLoadList;

// decomp: _loadFileContext (src/BATTLE/BATTLE.PRG/6E644.h:7)
struct Effects_FileLoadContext {
    u_short lba;
    u_short size;
};
static_assert(sizeof(Effects_FileLoadContext) == 0x4, "Effects_FileLoadContext must keep its PS1 layout");

// decomp: D_800F5638_t (src/BATTLE/BATTLE.PRG/6F014.c:56)
struct Effects_TextureLoadList {
    int assetCount;
    Effects_FileLoadContext loadContexts[8];
    u_short slots[8];
    u_short isClut[8];
};
static_assert(sizeof(Effects_TextureLoadList) == 0x44, "Effects_TextureLoadList must keep its PS1 layout");

#endif // VS_EFFECTS_LOADING_H_TYPES
