// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Plugins/Plg146.h"
#include "Effects/Plugins/Plg158.h"
#include "Effects/Plugins/Plg166.h"
#include "Effects/Plugins/Plg170.h"
#include "Effects/Plugins/Plg273.h"
#include "Effects/Plugins/Plg297.h"
#include "Effects/Plugins/Plg300.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/MirrorSurface.h"
#include "Effects/Templates/RotatingSurface.h"
#include "Effects/Templates/Surface.h"
#include "Effects/Templates/TiledRing.h"

// decomp: PLG146.BIN func_800FA8D0 (src/EFFECT/PLG146.BIN/1D8.c:6): VS_MIRROR_SURFACE_FUNCTION, instance of Effects/Templates/MirrorSurface.inl
#define VS_TILED_RING_FUNCTION Effects_Plg146_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg146_tiledRingColorFactors
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg146_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg146_mirrorSurfaceVertices
#include "Effects/Templates/MirrorSurface.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES

// decomp: PLG158.BIN func_800FA398 (src/EFFECT/PLG158.BIN/0.c:13): VS_MIRROR_SURFACE_FUNCTION, instance of Effects/Templates/MirrorSurface.inl
#define VS_SURFACE_FUNCTION Effects_Plg158_Surface
#define VS_SURFACE_VERTICES Effects_Plg158_surfaceVertices
#define VS_SURFACE_REPEAT
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg158_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg158_mirrorSurfaceVertices
#include "Effects/Templates/MirrorSurface.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES

// decomp: PLG166.BIN func_800FA6B8 (src/EFFECT/PLG166.BIN/0.c:11): VS_MIRROR_SURFACE_FUNCTION, instance of Effects/Templates/MirrorSurface.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg166_RotatingSurface
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg166_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg166_mirrorSurfaceVertices
#include "Effects/Templates/MirrorSurface.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES

// decomp: PLG170.BIN func_800F9818 (src/EFFECT/PLG170.BIN/18.c:8): VS_MIRROR_SURFACE_FUNCTION, instance of Effects/Templates/MirrorSurface.inl
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg170_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg170_mirrorSurfaceVertices
#include "Effects/Templates/MirrorSurface.inl"
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES

// decomp: PLG273.BIN func_800F9800 (src/EFFECT/PLG273.BIN/0.c:2): VS_MIRROR_SURFACE_FUNCTION, instance of Effects/Templates/MirrorSurface.inl
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg273_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg273_mirrorSurfaceVertices
#define VS_MIRROR_SURFACE_REPEAT
#include "Effects/Templates/MirrorSurface.inl"
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_MIRROR_SURFACE_REPEAT

// decomp: PLG297.BIN func_800F9818 (src/EFFECT/PLG297.BIN/18.c:8): VS_MIRROR_SURFACE_FUNCTION, instance of Effects/Templates/MirrorSurface.inl
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg297_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg297_mirrorSurfaceVertices
#include "Effects/Templates/MirrorSurface.inl"
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES

// decomp: PLG300.BIN func_800F9910 (src/EFFECT/PLG300.BIN/110.c:8): VS_MIRROR_SURFACE_FUNCTION, instance of Effects/Templates/MirrorSurface.inl
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg300_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg300_mirrorSurfaceVertices
#include "Effects/Templates/MirrorSurface.inl"
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES

}  // extern "C"
