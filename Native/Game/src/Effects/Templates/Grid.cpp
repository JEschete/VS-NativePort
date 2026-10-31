// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg061.h"
#include "Effects/Plugins/Plg073.h"
#include "Effects/Plugins/Plg077.h"
#include "Effects/Plugins/Plg082.h"
#include "Effects/Plugins/Plg085.h"
#include "Effects/Plugins/Plg086.h"
#include "Effects/Plugins/Plg087.h"
#include "Effects/Plugins/Plg128.h"
#include "Effects/Plugins/Plg137.h"
#include "Effects/Plugins/Plg138.h"
#include "Effects/Plugins/Plg150.h"
#include "Effects/Plugins/Plg156.h"
#include "Effects/Plugins/Plg161.h"
#include "Effects/Plugins/Plg165.h"
#include "Effects/Plugins/Plg168.h"
#include "Effects/Plugins/Plg170.h"
#include "Effects/Plugins/Plg242.h"
#include "Effects/Plugins/Plg291.h"
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
#include "Effects/Templates/ParticleRender.h"
#include "Effects/Templates/ParticleTarget.h"
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

// decomp: PLG053.BIN func_800F9818 (src/EFFECT/PLG053.BIN/18.c:8): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_GRID_FUNCTION Effects_Plg053_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_GRID_FUNCTION

// decomp: PLG061.BIN func_800FA3B0 (src/EFFECT/PLG061.BIN/18.c:12): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_SURFACE_FUNCTION Effects_Plg061_Surface
#define VS_SURFACE_VERTICES Effects_Plg061_surfaceVertices
#define VS_GRID_FUNCTION Effects_Plg061_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_GRID_FUNCTION

// decomp: PLG073.BIN func_800FE3EC (src/EFFECT/PLG073.BIN/F8.c:23): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
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
#define VS_GRID_FUNCTION Effects_Plg073_Grid
#include "Effects/Templates/Grid.inl"
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
#undef VS_GRID_FUNCTION

// decomp: PLG077.BIN func_800FB294 (src/EFFECT/PLG077.BIN/18.c:341): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_MESH10_FUNCTION Effects_Plg077_Mesh10
#define VS_MESH10_VERTICES Effects_Plg077_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg077_mesh10Faces
#define VS_GRID_FUNCTION Effects_Plg077_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES
#undef VS_GRID_FUNCTION

// decomp: PLG082.BIN func_800FB5CC (src/EFFECT/PLG082.BIN/128.c:20): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg082_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg082_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg082_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg082_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg082_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg082_particleEmissionCounter
#define VS_GRID_FUNCTION Effects_Plg082_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE
#undef VS_GRID_FUNCTION

// decomp: PLG085.BIN func_800F9818 (src/EFFECT/PLG085.BIN/18.c:2): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_GRID_FUNCTION Effects_Plg085_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_GRID_FUNCTION

// decomp: PLG086.BIN func_800FB720 (src/EFFECT/PLG086.BIN/F8.c:16): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg086_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg086_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg086_RotatingSurface
#define VS_GRID_FUNCTION Effects_Plg086_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_GRID_FUNCTION

// decomp: PLG087.BIN func_800FB4F8 (src/EFFECT/PLG087.BIN/1F0.c:18): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_SURFACE_FUNCTION Effects_Plg087_Surface
#define VS_SURFACE_VERTICES Effects_Plg087_surfaceVertices
#define VS_SURFACE_REPEAT
#define VS_CLOSED_RING_FUNCTION Effects_Plg087_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg087_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_GRID_FUNCTION Effects_Plg087_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_GRID_FUNCTION

// decomp: PLG128.BIN func_800FC2BC (src/EFFECT/PLG128.BIN/F8.c:58): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define TRAIL_SAMPLE_COUNT 5
#define VS_SURFACE_FUNCTION Effects_Plg128_Surface
#define VS_SURFACE_VERTICES Effects_Plg128_surfaceVertices
#define VS_SURFACE_REPEAT
#define VS_CLOSED_RING_FUNCTION Effects_Plg128_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg128_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg128_RotatingSurface
#define VS_GRID_FUNCTION Effects_Plg128_Grid
#include "Effects/Templates/Grid.inl"
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

// decomp: PLG137.BIN func_800FC524 (src/EFFECT/PLG137.BIN/F8.c:17): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
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
#include "Effects/Templates/Grid.inl"
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

// decomp: PLG138.BIN func_800FB2F4 (src/EFFECT/PLG138.BIN/18.c:9): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg138_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg138_Ring
#define VS_RING_COLORS Effects_Plg138_ringColorFactors
#define VS_GRID_FUNCTION Effects_Plg138_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_GRID_FUNCTION

// decomp: PLG139.BIN func_800FA710 (src/EFFECT/PLG139.BIN/18.c:6): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_TILED_RING_FUNCTION Effects_Plg139_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg085_tiledRingColorFactors
#define VS_GRID_FUNCTION Effects_Plg139_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_GRID_FUNCTION

// decomp: PLG142.BIN func_800F9818 (src/EFFECT/PLG142.BIN/18.c:8): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_GRID_FUNCTION Effects_Plg142_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_GRID_FUNCTION

// decomp: PLG150.BIN func_800FA710 (src/EFFECT/PLG150.BIN/18.c:6): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_TILED_RING_FUNCTION Effects_Plg150_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg150_tiledRingColorFactors
#define VS_GRID_FUNCTION Effects_Plg150_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_GRID_FUNCTION

// decomp: PLG152.BIN func_800F9928 (src/EFFECT/PLG152.BIN/128.c:8): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_GRID_FUNCTION Effects_Plg152_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_GRID_FUNCTION

// decomp: PLG156.BIN func_800FB5CC (src/EFFECT/PLG156.BIN/128.c:14): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg156_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg156_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg156_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg156_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg156_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg156_particleEmissionCounter
#define VS_GRID_FUNCTION Effects_Plg156_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE
#undef VS_GRID_FUNCTION

// decomp: PLG161.BIN func_800FD4F4 (src/EFFECT/PLG161.BIN/F8.c:20): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg161_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg161_Ring
#define VS_RING_COLORS Effects_Plg161_ringColorFactors
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
#include "Effects/Templates/Grid.inl"
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

// decomp: PLG165.BIN func_800FC8D0 (src/EFFECT/PLG165.BIN/F8.c:21): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg165_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg165_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg165_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg165_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg165_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg165_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg165_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg165_trailEmissionCounter
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg165_RotatingSurface
#define VS_GRID_FUNCTION Effects_Plg165_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_GRID_FUNCTION

// decomp: PLG168.BIN func_800FD104 (src/EFFECT/PLG168.BIN/1F0.c:36): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg168_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg168_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg168_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg168_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg168_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg168_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg168_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg168_trailEmissionCounter
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg168_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg168_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg168_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg168_ParticleLineRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg168_ParticleDispatch
#define VS_PARTICLE_UNTEXTURED
#define VS_GRID_FUNCTION Effects_Plg168_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_UNTEXTURED
#undef VS_GRID_FUNCTION

// decomp: PLG170.BIN func_800FBF6C (src/EFFECT/PLG170.BIN/18.c:21): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg170_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg170_mirrorSurfaceVertices
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg170_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg170_Ring
#define VS_RING_COLORS Effects_Plg170_ringColorFactors
#define VS_RING_TRANSPOSED
#define VS_GRID_FUNCTION Effects_Plg170_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_ROTATING_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED
#undef VS_GRID_FUNCTION

// decomp: PLG242.BIN func_800FA7F0 (src/EFFECT/PLG242.BIN/F8.c:6): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_TILED_RING_FUNCTION Effects_Plg242_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg242_tiledRingColorFactors
#define VS_GRID_FUNCTION Effects_Plg242_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_GRID_FUNCTION

// decomp: PLG272.BIN func_800F9818 (src/EFFECT/PLG272.BIN/18.c:8): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_GRID_FUNCTION Effects_Plg272_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_GRID_FUNCTION

// decomp: PLG291.BIN func_800FBA18 (src/EFFECT/PLG291.BIN/F8.c:18): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg291_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg291_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg291_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg291_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg291_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg291_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg291_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg291_trailEmissionCounter
#define VS_GRID_FUNCTION Effects_Plg291_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_GRID_FUNCTION

// decomp: PLG297.BIN func_800FB3FC (src/EFFECT/PLG297.BIN/18.c:17): VS_GRID_FUNCTION, instance of Effects/Templates/Grid.inl
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg297_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg297_mirrorSurfaceVertices
#define VS_CLOSED_RING_FUNCTION Effects_Plg297_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg297_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_GRID_FUNCTION Effects_Plg297_Grid
#include "Effects/Templates/Grid.inl"
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_GRID_FUNCTION

}  // extern "C"
