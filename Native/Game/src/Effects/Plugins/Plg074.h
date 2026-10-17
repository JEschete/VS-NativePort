// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_PLUGINS_PLG074_H_TYPES
#define VS_EFFECTS_PLUGINS_PLG074_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EFFECTS_PLUGINS_PLG074_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_PLUGINS_PLG074_H_DECLS)
#define VS_EFFECTS_PLUGINS_PLG074_H_DECLS

int Effects_Plg074_UpdateTiledRing();
int Effects_Plg074_UpdateRingArc();
int Effects_Plg074_GenerateTrailOrigins();
int Effects_Plg074_GenerateTrailTargets();
int Effects_Plg074_GenerateTrailControlPoints();
int Effects_Plg074_InitializeTrails();
int Effects_Plg074_UpdateTrails();
int Effects_Plg074_RenderTrails();
int Effects_Plg074_UpdateTrailEmitter();
int Effects_Plg074_UpdateRotatingSurface();

#endif // VS_EFFECTS_PLUGINS_PLG074_H_DECLS
