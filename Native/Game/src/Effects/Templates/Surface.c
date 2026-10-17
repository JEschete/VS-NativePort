// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Effects/Plugins/Plg061.h"
#include "Effects/Plugins/Plg077.h"
#include "Effects/Plugins/Plg087.h"
#include "Effects/Plugins/Plg128.h"
#include "Effects/Plugins/Plg129.h"
#include "Effects/Plugins/Plg150.h"
#include "Effects/Plugins/Plg158.h"
#include "Effects/Plugins/Plg170.h"
#include "Effects/Plugins/Plg263.h"
#include "Effects/Plugins/Plg272.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Templates/Grid.h"
#include "Effects/Templates/Mesh10.h"
#include "Effects/Templates/MirrorSurface.h"
#include "Effects/Templates/Ring.h"
#include "Effects/Templates/RotatingSurface.h"
#include "Effects/Templates/Surface.h"
#include "Effects/Templates/TiledRing.h"

// decomp: PLG061.BIN func_800F9818 (src/EFFECT/PLG061.BIN/18.c:8): VS_SURFACE_FUNCTION, instance of Effects/Templates/Surface.inl
#define VS_SURFACE_FUNCTION Effects_Plg061_Surface
#define VS_SURFACE_VERTICES Effects_Plg061_surfaceVertices
#include "Effects/Templates/Surface.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES

// decomp: PLG077.BIN func_800FBDE4 (src/EFFECT/PLG077.BIN/18.c:344): VS_SURFACE_FUNCTION, instance of Effects/Templates/Surface.inl
#define VS_MESH10_FUNCTION Effects_Plg077_Mesh10
#define VS_MESH10_VERTICES Effects_Plg077_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg077_mesh10Faces
#define VS_GRID_FUNCTION Effects_Plg077_Grid
#define VS_SURFACE_FUNCTION Effects_Plg077_Surface
#define VS_SURFACE_VERTICES Effects_Plg077_surfaceVertices
#define VS_SURFACE_REPEAT
#include "Effects/Templates/Surface.inl"
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES
#undef VS_GRID_FUNCTION
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT

// decomp: PLG087.BIN func_800F99F0 (src/EFFECT/PLG087.BIN/1F0.c:8): VS_SURFACE_FUNCTION, instance of Effects/Templates/Surface.inl
#define VS_SURFACE_FUNCTION Effects_Plg087_Surface
#define VS_SURFACE_VERTICES Effects_Plg087_surfaceVertices
#define VS_SURFACE_REPEAT
#include "Effects/Templates/Surface.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT

// decomp: PLG128.BIN func_800F98F8 (src/EFFECT/PLG128.BIN/F8.c:44): VS_SURFACE_FUNCTION, instance of Effects/Templates/Surface.inl
#define TRAIL_SAMPLE_COUNT 5
#define VS_SURFACE_FUNCTION Effects_Plg128_Surface
#define VS_SURFACE_VERTICES Effects_Plg128_surfaceVertices
#define VS_SURFACE_REPEAT
#include "Effects/Templates/Surface.inl"
#undef TRAIL_SAMPLE_COUNT
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT

// decomp: PLG128.BIN func_800FCE0C (src/EFFECT/PLG128.BIN/F8.c:61): VS_SURFACE_FUNCTION, instance of Effects/Templates/Surface.inl
#define TRAIL_SAMPLE_COUNT 5
#define VS_SURFACE_FUNCTION Effects_Plg128_Surface2
#define VS_SURFACE_VERTICES Effects_Plg128_surfaceVertices2
#define VS_SURFACE_REPEAT
#define VS_CLOSED_RING_FUNCTION Effects_Plg128_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg128_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg128_RotatingSurface
#define VS_GRID_FUNCTION Effects_Plg128_Grid
#include "Effects/Templates/Surface.inl"
#undef TRAIL_SAMPLE_COUNT
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_ROTATING_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_GRID_FUNCTION

// decomp: PLG129.BIN func_800FAF30 (src/EFFECT/PLG129.BIN/0.c:12): VS_SURFACE_FUNCTION, instance of Effects/Templates/Surface.inl
#define VS_RING_FUNCTION Effects_Plg129_Ring
#define VS_RING_COLORS Effects_Plg129_ringColorFactors
#define VS_RING_TRANSPOSED
#define VS_MESH10_FUNCTION Effects_Plg129_Mesh10
#define VS_MESH10_VERTICES Effects_Plg129_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg129_mesh10Faces
#define VS_SURFACE_FUNCTION Effects_Plg129_Surface
#define VS_SURFACE_VERTICES Effects_Plg129_surfaceVertices
#define VS_SURFACE_REPEAT
#include "Effects/Templates/Surface.inl"
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT

// decomp: PLG150.BIN func_800FB260 (src/EFFECT/PLG150.BIN/18.c:9): VS_SURFACE_FUNCTION, instance of Effects/Templates/Surface.inl
#define VS_TILED_RING_FUNCTION Effects_Plg150_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg150_tiledRingColorFactors
#define VS_GRID_FUNCTION Effects_Plg150_Grid
#define VS_SURFACE_FUNCTION Effects_Plg150_Surface
#define VS_SURFACE_VERTICES Effects_Plg150_surfaceVertices
#include "Effects/Templates/Surface.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_GRID_FUNCTION
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES

// decomp: PLG158.BIN func_800F9800 (src/EFFECT/PLG158.BIN/0.c:8): VS_SURFACE_FUNCTION, instance of Effects/Templates/Surface.inl
#define VS_SURFACE_FUNCTION Effects_Plg158_Surface
#define VS_SURFACE_VERTICES Effects_Plg158_surfaceVertices
#define VS_SURFACE_REPEAT
#include "Effects/Templates/Surface.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT

// decomp: PLG170.BIN func_800FCABC (src/EFFECT/PLG170.BIN/18.c:24): VS_SURFACE_FUNCTION, instance of Effects/Templates/Surface.inl
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg170_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg170_mirrorSurfaceVertices
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg170_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg170_Ring
#define VS_RING_COLORS Effects_Plg170_ringColorFactors
#define VS_RING_TRANSPOSED
#define VS_GRID_FUNCTION Effects_Plg170_Grid
#define VS_SURFACE_FUNCTION Effects_Plg170_Surface
#define VS_SURFACE_VERTICES Effects_Plg170_surfaceVertices
#define VS_SURFACE_REPEAT
#include "Effects/Templates/Surface.inl"
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_ROTATING_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED
#undef VS_GRID_FUNCTION
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT

// decomp: PLG263.BIN func_800F9910 (src/EFFECT/PLG263.BIN/110.c:2): VS_SURFACE_FUNCTION, instance of Effects/Templates/Surface.inl
#define VS_SURFACE_FUNCTION Effects_Plg263_Surface
#define VS_SURFACE_VERTICES Effects_Plg263_surfaceVertices
#include "Effects/Templates/Surface.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES

// decomp: PLG272.BIN func_800FB2D8 (src/EFFECT/PLG272.BIN/18.c:16): VS_SURFACE_FUNCTION, instance of Effects/Templates/Surface.inl
#define VS_GRID_FUNCTION Effects_Plg272_Grid
#define VS_CLOSED_RING_FUNCTION Effects_Plg272_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg272_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_SURFACE_FUNCTION Effects_Plg272_Surface
#define VS_SURFACE_VERTICES Effects_Plg272_surfaceVertices
#include "Effects/Templates/Surface.inl"
#undef VS_GRID_FUNCTION
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES

