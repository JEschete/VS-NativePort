// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg044.h"
#include "Effects/Plugins/Plg049.h"
#include "Effects/Plugins/Plg069.h"
#include "Effects/Plugins/Plg073.h"
#include "Effects/Plugins/Plg085.h"
#include "Effects/Plugins/Plg137.h"
#include "Effects/Plugins/Plg138.h"
#include "Effects/Plugins/Plg143.h"
#include "Effects/Plugins/Plg146.h"
#include "Effects/Plugins/Plg148.h"
#include "Effects/Plugins/Plg150.h"
#include "Effects/Plugins/Plg160.h"
#include "Effects/Plugins/Plg242.h"
#include "Effects/Plugins/Plg265.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/Grid.h"
#include "Effects/Templates/Mesh10.h"
#include "Effects/Templates/Ring.h"
#include "Effects/Templates/RotatingSurface.h"
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

// decomp: PLG044.BIN vs_effect_solidShock (src/EFFECT/PLG044.BIN/0.c:4): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_TILED_RING_FUNCTION Effects_Plg044_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg044_tiledRingColorFactorsExtern
#include "Effects/Templates/TiledRing.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

// decomp: PLG049.BIN func_800FC624 (src/EFFECT/PLG049.BIN/E0.c:17): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
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
#include "Effects/Templates/TiledRing.inl"
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

// decomp: PLG069.BIN func_800FC50C (src/EFFECT/PLG069.BIN/E0.c:17): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_MESH10_FUNCTION Effects_Plg069_Mesh10
#define VS_MESH10_VERTICES Effects_Plg069_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg069_mesh10Faces
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg069_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg069_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg069_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg069_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg069_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg069_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg069_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg069_trailEmissionCounter
#define VS_TILED_RING_FUNCTION Effects_Plg069_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg069_tiledRingColorFactors
#include "Effects/Templates/TiledRing.inl"
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

// decomp: PLG073.BIN func_800F98F8 (src/EFFECT/PLG073.BIN/F8.c:2): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_TILED_RING_FUNCTION Effects_Plg073_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg073_tiledRingColorFactors
#include "Effects/Templates/TiledRing.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

// decomp: PLG085.BIN func_800FA368 (src/EFFECT/PLG085.BIN/18.c:5): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_GRID_FUNCTION Effects_Plg085_Grid
#define VS_TILED_RING_FUNCTION Effects_Plg085_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg085_tiledRingColorFactors
#include "Effects/Templates/TiledRing.inl"
#undef VS_GRID_FUNCTION
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

// decomp: PLG137.BIN func_800FD074 (src/EFFECT/PLG137.BIN/F8.c:20): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_MESH10_FUNCTION Effects_Plg137_Mesh10
#define VS_MESH10_VERTICES Effects_Plg137_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg137_mesh10Faces
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg137_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg137_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg137_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg137_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg137_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg137_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg137_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg137_trailEmissionCounter
#define VS_GRID_FUNCTION Effects_Plg137_Grid
#define VS_TILED_RING_FUNCTION Effects_Plg137_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg137_tiledRingColorFactors
#include "Effects/Templates/TiledRing.inl"
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_GRID_FUNCTION
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

// decomp: PLG138.BIN func_800FBE44 (src/EFFECT/PLG138.BIN/18.c:12): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg138_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg138_Ring
#define VS_RING_COLORS Effects_Plg138_ringColorFactors
#define VS_GRID_FUNCTION Effects_Plg138_Grid
#define VS_TILED_RING_FUNCTION Effects_Plg138_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg138_tiledRingColorFactors
#include "Effects/Templates/TiledRing.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_GRID_FUNCTION
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

// decomp: PLG139.BIN func_800F9818 (src/EFFECT/PLG139.BIN/18.c:2): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_TILED_RING_FUNCTION Effects_Plg139_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg085_tiledRingColorFactors
#include "Effects/Templates/TiledRing.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

// decomp: PLG143.BIN func_800F9800 (src/EFFECT/PLG143.BIN/0.c:8): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_TILED_RING_FUNCTION Effects_Plg143_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg143_tiledRingColorFactors
#include "Effects/Templates/TiledRing.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

// decomp: PLG146.BIN func_800F99D8 (src/EFFECT/PLG146.BIN/1D8.c:2): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_TILED_RING_FUNCTION Effects_Plg146_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg146_tiledRingColorFactors
#include "Effects/Templates/TiledRing.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

// decomp: PLG148.BIN func_800FBA00 (src/EFFECT/PLG148.BIN/E0.c:12): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg148_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg148_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg148_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg148_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg148_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg148_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg148_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg148_trailEmissionCounter
#define VS_TILED_RING_FUNCTION Effects_Plg148_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg148_tiledRingColorFactors
#include "Effects/Templates/TiledRing.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

// decomp: PLG150.BIN func_800F9818 (src/EFFECT/PLG150.BIN/18.c:2): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_TILED_RING_FUNCTION Effects_Plg150_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg150_tiledRingColorFactors
#include "Effects/Templates/TiledRing.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

// decomp: PLG160.BIN func_800F9800 (src/EFFECT/PLG160.BIN/0.c:2): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_TILED_RING_FUNCTION Effects_Plg160_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg160_tiledRingColorFactors
#include "Effects/Templates/TiledRing.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

// decomp: PLG242.BIN func_800F98F8 (src/EFFECT/PLG242.BIN/F8.c:2): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_TILED_RING_FUNCTION Effects_Plg242_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg242_tiledRingColorFactors
#include "Effects/Templates/TiledRing.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

// decomp: PLG265.BIN func_800F9AE8 (src/EFFECT/PLG265.BIN/2E8.c:2): VS_TILED_RING_FUNCTION, instance of Effects/Templates/TiledRing.inl
#define VS_TILED_RING_FUNCTION Effects_Plg265_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg265_tiledRingColorFactors
#include "Effects/Templates/TiledRing.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS

}  // extern "C"
