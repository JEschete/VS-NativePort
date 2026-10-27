// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_PLUGINS_PLG005_H_TYPES
#define VS_EFFECTS_PLUGINS_PLG005_H_TYPES
#include "Legacy/Psx.h"

typedef struct Effects_Plg005_AnalyzeGridQuad Effects_Plg005_AnalyzeGridQuad;

// decomp: PLG005_data_t (src/EFFECT/PLG005.BIN/18.c:26)
typedef struct Effects_Plg005_AnalyzeGridQuad {
    u_char opaque0;
    u_char opaque1;
    u_char uBase;
    u_char vBase;
    u_char uScroll;
    u_char vScroll;
    short tpage;
    short clut;
    short opaqueA;
    short opaqueC;
    short opaqueE;
    short opaque10;
    short opaque12;
    short opaque14;
    int curveSampleDistance;
} Effects_Plg005_AnalyzeGridQuad;
_Static_assert(sizeof(Effects_Plg005_AnalyzeGridQuad) == 0x1C, "Effects_Plg005_AnalyzeGridQuad must keep its PS1 layout");

#endif // VS_EFFECTS_PLUGINS_PLG005_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_PLUGINS_PLG005_H_DECLS)
#define VS_EFFECTS_PLUGINS_PLG005_H_DECLS
#include "Effects/Runtime.h"

int Effects_Plg005_UpdateAnalyzeGrid(Effects_RendererSlot* renderer_slot, u_int lifecycle_command, int packed_parameters);

#endif // VS_EFFECTS_PLUGINS_PLG005_H_DECLS
