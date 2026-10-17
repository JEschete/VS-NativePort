// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg049.h"
#include "Effects/Plugins/Plg050.h"
#include "Effects/Plugins/Plg061.h"
#include "Effects/Plugins/Plg065.h"
#include "Effects/Plugins/Plg073.h"
#include "Effects/Plugins/Plg081.h"
#include "Effects/Plugins/Plg129.h"
#include "Effects/Plugins/Plg138.h"
#include "Effects/Plugins/Plg142.h"
#include "Effects/Plugins/Plg146.h"
#include "Effects/Plugins/Plg147.h"
#include "Effects/Plugins/Plg149.h"
#include "Effects/Plugins/Plg150.h"
#include "Effects/Plugins/Plg153.h"
#include "Effects/Plugins/Plg160.h"
#include "Effects/Plugins/Plg161.h"
#include "Effects/Plugins/Plg162.h"
#include "Effects/Plugins/Plg166.h"
#include "Effects/Plugins/Plg169.h"
#include "Effects/Plugins/Plg170.h"
#include "Effects/Plugins/Plg236.h"
#include "Effects/Plugins/Plg263.h"
#include "Effects/Plugins/Plg297.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Templates/Grid.h"
#include "Effects/Templates/Mesh10.h"
#include "Effects/Templates/MirrorSurface.h"
#include "Effects/Templates/ParticleControl.h"
#include "Effects/Templates/ParticleDispatch.h"
#include "Effects/Templates/ParticleLineRender.h"
#include "Effects/Templates/ParticleOrigin.h"
#include "Effects/Templates/ParticleTarget.h"
#include "Effects/Templates/Ring.h"
#include "Effects/Templates/RotatingSurface.h"
#include "Effects/Templates/Surface.h"
#include "Effects/Templates/TemplateState.h"
#include "Effects/Templates/TiledRing.h"
#include "Effects/Templates/TrailNineControl.h"
#include "Effects/Templates/TrailNineDispatch.h"
#include "Effects/Templates/TrailNineInit.h"
#include "Effects/Templates/TrailNineOrigin.h"
#include "Effects/Templates/TrailNineRender.h"
#include "Effects/Templates/TrailNineTarget.h"
#include "Effects/Templates/TrailNineUpdate.h"
#include "Effects/Transforms.h"

// decomp: PLG049.BIN func_800FBA00 (src/EFFECT/PLG049.BIN/E0.c:10): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
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
#include "Effects/Templates/Ring.inl"
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

// decomp: PLG050.BIN func_800F98E0 (src/EFFECT/PLG050.BIN/E0.c:2): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_RING_FUNCTION Effects_Plg050_Ring
#define VS_RING_COLORS Effects_Plg050_ringColorFactors
#include "Effects/Templates/Ring.inl"
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS

// decomp: PLG061.BIN func_800FAF00 (src/EFFECT/PLG061.BIN/18.c:15): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_SURFACE_FUNCTION Effects_Plg061_Surface
#define VS_SURFACE_VERTICES Effects_Plg061_surfaceVertices
#define VS_GRID_FUNCTION Effects_Plg061_Grid
#define VS_RING_FUNCTION Effects_Plg061_Ring
#define VS_RING_COLORS Effects_Plg061_ringColorFactors
#include "Effects/Templates/Ring.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_GRID_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS

// decomp: PLG065.BIN func_800FB71C (src/EFFECT/PLG065.BIN/F8.c:15): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg065_RotatingSurface
#define VS_CLOSED_RING_FUNCTION Effects_Plg065_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg065_closedRingColorFactors
#define VS_RING_FUNCTION Effects_Plg065_Ring
#define VS_RING_COLORS Effects_Plg065_ringColorFactors
#include "Effects/Templates/Ring.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS

// decomp: PLG073.BIN func_800FA7F0 (src/EFFECT/PLG073.BIN/F8.c:6): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_TILED_RING_FUNCTION Effects_Plg073_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg073_tiledRingColorFactors
#define VS_RING_FUNCTION Effects_Plg073_Ring
#define VS_RING_COLORS Effects_Plg073_ringColorFactors
#include "Effects/Templates/Ring.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS

// decomp: PLG081.BIN func_800FA6B8 (src/EFFECT/PLG081.BIN/0.c:11): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg081_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg081_Ring
#define VS_RING_COLORS Effects_Plg081_ringColorFactors
#include "Effects/Templates/Ring.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS

// decomp: PLG129.BIN func_800F9800 (src/EFFECT/PLG129.BIN/0.c:2): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_RING_FUNCTION Effects_Plg129_Ring
#define VS_RING_COLORS Effects_Plg129_ringColorFactors
#define VS_RING_TRANSPOSED
#include "Effects/Templates/Ring.inl"
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED

// decomp: PLG129.BIN func_800FBAC8 (src/EFFECT/PLG129.BIN/0.c:17): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_RING_FUNCTION Effects_Plg129_Ring2
#define VS_RING_COLORS Effects_Plg129_ringColorFactors2
#define VS_RING_TRANSPOSED
#define VS_MESH10_FUNCTION Effects_Plg129_Mesh10
#define VS_MESH10_VERTICES Effects_Plg129_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg129_mesh10Faces
#define VS_SURFACE_FUNCTION Effects_Plg129_Surface
#define VS_SURFACE_VERTICES Effects_Plg129_surfaceVertices
#define VS_SURFACE_REPEAT
#include "Effects/Templates/Ring.inl"
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT

// decomp: PLG138.BIN func_800FA6D0 (src/EFFECT/PLG138.BIN/18.c:5): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg138_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg138_Ring
#define VS_RING_COLORS Effects_Plg138_ringColorFactors
#include "Effects/Templates/Ring.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS

// decomp: PLG142.BIN func_800FB2D8 (src/EFFECT/PLG142.BIN/18.c:16): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_GRID_FUNCTION Effects_Plg142_Grid
#define VS_CLOSED_RING_FUNCTION Effects_Plg142_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg142_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_RING_FUNCTION Effects_Plg142_Ring
#define VS_RING_COLORS Effects_Plg142_ringColorFactors
#include "Effects/Templates/Ring.inl"
#undef VS_GRID_FUNCTION
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS

// decomp: PLG146.BIN func_800FCB38 (src/EFFECT/PLG146.BIN/1D8.c:34): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_TILED_RING_FUNCTION Effects_Plg146_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg146_tiledRingColorFactors
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg146_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg146_mirrorSurfaceVertices
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg146_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg146_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg146_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg146_ParticleLineRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg146_ParticleDispatch
#define VS_PARTICLE_UNTEXTURED
#define VS_RING_FUNCTION Effects_Plg146_Ring
#define VS_RING_COLORS Effects_Plg146_ringColorFactors
#define VS_RING_TRANSPOSED
#include "Effects/Templates/Ring.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_UNTEXTURED
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED

// decomp: PLG147.BIN func_800F98F8 (src/EFFECT/PLG147.BIN/F8.c:2): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_RING_FUNCTION Effects_Plg147_Ring
#define VS_RING_COLORS Effects_Plg147_ringColorFactors
#include "Effects/Templates/Ring.inl"
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS

// decomp: PLG149.BIN func_800F9800 (src/EFFECT/PLG149.BIN/0.c:2): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_RING_FUNCTION Effects_Plg149_Ring
#define VS_RING_COLORS Effects_Plg149_ringColorFactors
#include "Effects/Templates/Ring.inl"
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS

// decomp: PLG150.BIN func_800FBDF8 (src/EFFECT/PLG150.BIN/18.c:13): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_TILED_RING_FUNCTION Effects_Plg150_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg150_tiledRingColorFactors
#define VS_GRID_FUNCTION Effects_Plg150_Grid
#define VS_SURFACE_FUNCTION Effects_Plg150_Surface
#define VS_SURFACE_VERTICES Effects_Plg150_surfaceVertices
#define VS_RING_FUNCTION Effects_Plg150_Ring
#define VS_RING_COLORS Effects_Plg150_ringColorFactors
#define VS_RING_TRANSPOSED
#include "Effects/Templates/Ring.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_GRID_FUNCTION
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED

// decomp: PLG153.BIN func_800FA41C (src/EFFECT/PLG153.BIN/110.c:7): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_MESH10_FUNCTION Effects_Plg153_Mesh10
#define VS_MESH10_VERTICES Effects_Plg153_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg153_mesh10Faces
#define VS_RING_FUNCTION Effects_Plg153_Ring
#define VS_RING_COLORS Effects_Plg153_ringColorFactors
#define VS_RING_TRANSPOSED
#include "Effects/Templates/Ring.inl"
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED

// decomp: PLG160.BIN func_800FA6F8 (src/EFFECT/PLG160.BIN/0.c:6): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_TILED_RING_FUNCTION Effects_Plg160_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg160_tiledRingColorFactors
#define VS_RING_FUNCTION Effects_Plg160_Ring
#define VS_RING_COLORS Effects_Plg160_ringColorFactors
#include "Effects/Templates/Ring.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS

// decomp: PLG161.BIN func_800FA7B0 (src/EFFECT/PLG161.BIN/F8.c:5): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg161_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg161_Ring
#define VS_RING_COLORS Effects_Plg161_ringColorFactors
#define VS_RING_TRANSPOSED
#include "Effects/Templates/Ring.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED

// decomp: PLG161.BIN func_800FE044 (src/EFFECT/PLG161.BIN/F8.c:23): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg161_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg161_Ring2
#define VS_RING_COLORS Effects_Plg161_ringColorFactors2
#define VS_RING_TRANSPOSED
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg161_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg161_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg161_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg161_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg161_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg161_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg161_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg161_trailEmissionCounter
#define VS_GRID_FUNCTION Effects_Plg161_Grid
#include "Effects/Templates/Ring.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_GRID_FUNCTION

// decomp: PLG162.BIN func_800FB5F8 (src/EFFECT/PLG162.BIN/0.c:339): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg162_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg162_Ring
#define VS_RING_COLORS Effects_Plg162_ringColorFactors2
#include "Effects/Templates/Ring.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS

// decomp: PLG166.BIN func_800FC29C (src/EFFECT/PLG166.BIN/0.c:20): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg166_RotatingSurface
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg166_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg166_mirrorSurfaceVertices
#define VS_CLOSED_RING_FUNCTION Effects_Plg166_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg166_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_RING_FUNCTION Effects_Plg166_Ring
#define VS_RING_COLORS Effects_Plg166_ringColorFactors
#define VS_RING_TRANSPOSED
#include "Effects/Templates/Ring.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED

// decomp: PLG169.BIN func_800FBA00 (src/EFFECT/PLG169.BIN/E0.c:18): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg169_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg169_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg169_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg169_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg169_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg169_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg169_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg169_trailEmissionCounter
#define VS_RING_FUNCTION Effects_Plg169_Ring
#define VS_RING_COLORS Effects_Plg169_ringColorFactors
#include "Effects/Templates/Ring.inl"
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

// decomp: PLG170.BIN func_800FB348 (src/EFFECT/PLG170.BIN/18.c:16): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg170_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg170_mirrorSurfaceVertices
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg170_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg170_Ring
#define VS_RING_COLORS Effects_Plg170_ringColorFactors
#define VS_RING_TRANSPOSED
#include "Effects/Templates/Ring.inl"
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_ROTATING_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED

// decomp: PLG198.BIN func_800F9800 (src/EFFECT/PLG198.BIN/0.c:2): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_RING_FUNCTION Effects_Plg198_Ring
#define VS_RING_COLORS Effects_Plg149_ringColorFactors
#define VS_RING_TRANSPOSED
#include "Effects/Templates/Ring.inl"
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED

// decomp: PLG236.BIN func_800FC56C (src/EFFECT/PLG236.BIN/0.c:345): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg236_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg236_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg236_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg236_Ring
#define VS_RING_COLORS Effects_Plg236_ringColorFactors2
#define VS_RING_TRANSPOSED
#include "Effects/Templates/Ring.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_ROTATING_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED

// decomp: PLG263.BIN func_800FA4A8 (src/EFFECT/PLG263.BIN/110.c:6): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_SURFACE_FUNCTION Effects_Plg263_Surface
#define VS_SURFACE_VERTICES Effects_Plg263_surfaceVertices
#define VS_RING_FUNCTION Effects_Plg263_Ring
#define VS_RING_COLORS Effects_Plg263_ringColorFactors
#include "Effects/Templates/Ring.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS

// decomp: PLG297.BIN func_800FBF4C (src/EFFECT/PLG297.BIN/18.c:20): VS_RING_FUNCTION, instance of Effects/Templates/Ring.inl
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg297_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg297_mirrorSurfaceVertices
#define VS_CLOSED_RING_FUNCTION Effects_Plg297_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg297_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_GRID_FUNCTION Effects_Plg297_Grid
#define VS_RING_FUNCTION Effects_Plg297_Ring
#define VS_RING_COLORS Effects_Plg297_ringColorFactors
#define VS_RING_TRANSPOSED
#include "Effects/Templates/Ring.inl"
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_GRID_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED

