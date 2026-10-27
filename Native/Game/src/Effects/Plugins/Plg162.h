// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_PLUGINS_PLG162_H_TYPES
#define VS_EFFECTS_PLUGINS_PLG162_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EFFECTS_PLUGINS_PLG162_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_PLUGINS_PLG162_H_DECLS)
#define VS_EFFECTS_PLUGINS_PLG162_H_DECLS
#include "Effects/Runtime.h"

extern u_char Effects_Plg162_ringColorFactors[][4];
extern u_char Effects_Plg162_ringColorFactors2[][4];
extern CVECTOR Effects_Plg162_closedRingColorFactors[];
int Effects_Plg162_UpdateTransposedRing(Effects_RendererSlot* renderer_slot, u_int lifecycle_command, int packed_parameters);

#endif // VS_EFFECTS_PLUGINS_PLG162_H_DECLS
