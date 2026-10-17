// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg049.h"
#include "Effects/Plugins/Plg073.h"
#include "Effects/Plugins/Plg086.h"
#include "Effects/Plugins/Plg128.h"
#include "Effects/Plugins/Plg129.h"
#include "Effects/Plugins/Plg142.h"
#include "Effects/Plugins/Plg158.h"
#include "Effects/Plugins/Plg165.h"
#include "Effects/Plugins/Plg167.h"
#include "Effects/Plugins/Plg170.h"
#include "Effects/Plugins/Plg236.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Templates/Grid.h"
#include "Effects/Templates/Mesh10.h"
#include "Effects/Templates/MirrorSurface.h"
#include "Effects/Templates/Ring.h"
#include "Effects/Templates/RotatingSurface.h"
#include "Effects/Templates/Surface.h"
#include "Effects/Templates/TemplateState.h"
#include "Effects/Templates/TiledRing.h"
#include "Effects/Templates/TrailControl.h"
#include "Effects/Templates/TrailDispatch.h"
#include "Effects/Templates/TrailInit.h"
#include "Effects/Templates/TrailNineControl.h"
#include "Effects/Templates/TrailNineDispatch.h"
#include "Effects/Templates/TrailNineInit.h"
#include "Effects/Templates/TrailNineOrigin.h"
#include "Effects/Templates/TrailNineRender.h"
#include "Effects/Templates/TrailNineTarget.h"
#include "Effects/Templates/TrailNineUpdate.h"
#include "Effects/Templates/TrailOrigin.h"
#include "Effects/Templates/TrailRender.h"
#include "Effects/Templates/TrailTarget.h"
#include "Effects/Templates/TrailUpdate.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"

// decomp: PLG049.BIN func_800FD51C (src/EFFECT/PLG049.BIN/E0.c:24): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg049_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg049_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg049_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg049_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg049_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg049_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg049_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg049_trailEmissionCounter
#define VS_RING_FUNCTION Effects_Plg049_Ring
#define VS_RING_COLORS Effects_Plg049_ringColorFactors
#define VS_TILED_RING_FUNCTION Effects_Plg049_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg049_tiledRingColorFactors
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg049_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG065.BIN func_800F98F8 (src/EFFECT/PLG065.BIN/F8.c:8): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg065_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG073.BIN func_800FD534 (src/EFFECT/PLG073.BIN/F8.c:20): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_TILED_RING_FUNCTION Effects_Plg073_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg073_tiledRingColorFactors
#define VS_RING_FUNCTION Effects_Plg073_Ring
#define VS_RING_COLORS Effects_Plg073_ringColorFactors
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg073_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg073_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg073_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg073_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg073_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg073_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg073_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg073_trailEmissionCounter
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg073_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG081.BIN func_800F9800 (src/EFFECT/PLG081.BIN/0.c:8): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg081_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG086.BIN func_800FA868 (src/EFFECT/PLG086.BIN/F8.c:13): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg086_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg086_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg086_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG128.BIN func_800FB400 (src/EFFECT/PLG128.BIN/F8.c:56): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define TRAIL_SAMPLE_COUNT 5
#define VS_SURFACE_FUNCTION Effects_Plg128_Surface
#define VS_SURFACE_VERTICES Effects_Plg128_surfaceVertices
#define VS_SURFACE_REPEAT
#define VS_CLOSED_RING_FUNCTION Effects_Plg128_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg128_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg128_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef TRAIL_SAMPLE_COUNT
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_ROTATING_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG129.BIN func_800FC6EC (src/EFFECT/PLG129.BIN/0.c:23): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_RING_FUNCTION Effects_Plg129_Ring2
#define VS_RING_COLORS Effects_Plg129_ringColorFactors2
#define VS_RING_TRANSPOSED
#define VS_MESH10_FUNCTION Effects_Plg129_Mesh10
#define VS_MESH10_VERTICES Effects_Plg129_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg129_mesh10Faces
#define VS_SURFACE_FUNCTION Effects_Plg129_Surface
#define VS_SURFACE_VERTICES Effects_Plg129_surfaceVertices
#define VS_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg129_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG138.BIN func_800F9818 (src/EFFECT/PLG138.BIN/18.c:2): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg138_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG142.BIN func_800FBEFC (src/EFFECT/PLG142.BIN/18.c:22): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_GRID_FUNCTION Effects_Plg142_Grid
#define VS_CLOSED_RING_FUNCTION Effects_Plg142_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg142_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_RING_FUNCTION Effects_Plg142_Ring
#define VS_RING_COLORS Effects_Plg142_ringColorFactors
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg142_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_GRID_FUNCTION
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_ROTATING_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG158.BIN func_800FBF7C (src/EFFECT/PLG158.BIN/0.c:24): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_SURFACE_FUNCTION Effects_Plg158_Surface
#define VS_SURFACE_VERTICES Effects_Plg158_surfaceVertices
#define VS_SURFACE_REPEAT
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg158_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg158_mirrorSurfaceVertices
#define VS_CLOSED_RING_FUNCTION Effects_Plg158_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg158_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg158_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_ROTATING_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG161.BIN func_800F98F8 (src/EFFECT/PLG161.BIN/F8.c:2): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg161_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG162.BIN func_800FA740 (src/EFFECT/PLG162.BIN/0.c:336): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg162_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG165.BIN func_800FBA18 (src/EFFECT/PLG165.BIN/F8.c:18): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg165_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg165_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg165_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg165_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg165_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg165_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg165_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg165_trailEmissionCounter
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg165_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG166.BIN func_800F9800 (src/EFFECT/PLG166.BIN/0.c:8): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg166_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG167.BIN func_800FA84C (src/EFFECT/PLG167.BIN/E0.c:12): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg167_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg167_closedRingColorFactors
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg167_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG170.BIN func_800FA48C (src/EFFECT/PLG170.BIN/18.c:14): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg170_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg170_mirrorSurfaceVertices
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg170_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_ROTATING_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG236.BIN func_800FA770 (src/EFFECT/PLG236.BIN/0.c:15): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg236_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg236_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg236_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_ROTATING_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG246.BIN func_800F9800 (src/EFFECT/PLG246.BIN/0.c:2): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg246_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_ROTATING_SURFACE_FUNCTION

// decomp: PLG250.BIN func_800F9800 (src/EFFECT/PLG250.BIN/0.c:10): VS_ROTATING_SURFACE_FUNCTION, instance of Effects/Templates/RotatingSurface.inl
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg250_RotatingSurface
#include "Effects/Templates/RotatingSurface.inl"
#undef VS_ROTATING_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_FUNCTION

