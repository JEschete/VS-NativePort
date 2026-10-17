// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_PURGETABLES_H_TYPES
#define VS_EFFECTS_PURGETABLES_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EFFECTS_PURGETABLES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_PURGETABLES_H_DECLS)
#define VS_EFFECTS_PURGETABLES_H_DECLS
#include "Effects/Loading.h"
#include "Effects/Runtime.h"

extern u_char Effects_effectUsesTextures[];
extern u_short Effects_effectEntryPointIndices[];
extern Effects_EffectRenderer Effects_effectEntryPoints[];
extern Effects_FileLoadContext Effects_fbcFiles[];
extern u_char Effects_fbcSlots[];
extern u_char Effects_fbtCounts[];
extern Effects_FileLoadContext Effects_fbtFiles[];
extern u_char Effects_fbtSlots[];
extern Effects_FileLoadContext Effects_packageFiles[];
extern Effects_FileLoadContext Effects_pluginFiles[];

#endif // VS_EFFECTS_PURGETABLES_H_DECLS
