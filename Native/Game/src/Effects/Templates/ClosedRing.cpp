// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg047.h"
#include "Effects/Plugins/Plg051.h"
#include "Effects/Plugins/Plg053.h"
#include "Effects/Plugins/Plg061.h"
#include "Effects/Plugins/Plg065.h"
#include "Effects/Plugins/Plg081.h"
#include "Effects/Plugins/Plg082.h"
#include "Effects/Plugins/Plg086.h"
#include "Effects/Plugins/Plg087.h"
#include "Effects/Plugins/Plg128.h"
#include "Effects/Plugins/Plg134.h"
#include "Effects/Plugins/Plg135.h"
#include "Effects/Plugins/Plg142.h"
#include "Effects/Plugins/Plg143.h"
#include "Effects/Plugins/Plg154.h"
#include "Effects/Plugins/Plg158.h"
#include "Effects/Plugins/Plg162.h"
#include "Effects/Plugins/Plg165.h"
#include "Effects/Plugins/Plg166.h"
#include "Effects/Plugins/Plg167.h"
#include "Effects/Plugins/Plg169.h"
#include "Effects/Plugins/Plg170.h"
#include "Effects/Plugins/Plg236.h"
#include "Effects/Plugins/Plg250.h"
#include "Effects/Plugins/Plg272.h"
#include "Effects/Plugins/Plg291.h"
#include "Effects/Plugins/Plg297.h"
#include "Effects/Plugins/Plg301.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Templates/Grid.h"
#include "Effects/Templates/Mesh10.h"
#include "Effects/Templates/MirrorSurface.h"
#include "Effects/Templates/ParticleControl.h"
#include "Effects/Templates/ParticleDispatch.h"
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

// decomp: PLG047.BIN vs_effect_vulcanLance (src/EFFECT/PLG047.BIN/0.c:4): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg047_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg047_closedRingColorFactorsExtern
#include "Effects/Templates/ClosedRing.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS

// decomp: PLG051.BIN vs_effect_exorcism (src/EFFECT/PLG051.BIN/0.c:3): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg051_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg051_closedRingColorFactorsExtern
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG053.BIN func_800FA368 (src/EFFECT/PLG053.BIN/18.c:11): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_GRID_FUNCTION Effects_Plg053_Grid
#define VS_CLOSED_RING_FUNCTION Effects_Plg053_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg053_closedRingColorFactors
#include "Effects/Templates/ClosedRing.inl"
#undef VS_GRID_FUNCTION
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS

// decomp: PLG061.BIN func_800FBB24 (src/EFFECT/PLG061.BIN/18.c:19): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_SURFACE_FUNCTION Effects_Plg061_Surface
#define VS_SURFACE_VERTICES Effects_Plg061_surfaceVertices
#define VS_GRID_FUNCTION Effects_Plg061_Grid
#define VS_RING_FUNCTION Effects_Plg061_Ring
#define VS_RING_COLORS Effects_Plg061_ringColorFactors
#define VS_CLOSED_RING_FUNCTION Effects_Plg061_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg061_closedRingColorFactors
#include "Effects/Templates/ClosedRing.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_GRID_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS

// decomp: PLG065.BIN func_800FA7B0 (src/EFFECT/PLG065.BIN/F8.c:11): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg065_RotatingSurface
#define VS_CLOSED_RING_FUNCTION Effects_Plg065_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg065_closedRingColorFactors
#include "Effects/Templates/ClosedRing.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS

// decomp: PLG081.BIN func_800FB2DC (src/EFFECT/PLG081.BIN/0.c:15): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg081_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg081_Ring
#define VS_RING_COLORS Effects_Plg081_ringColorFactors
#define VS_CLOSED_RING_FUNCTION Effects_Plg081_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg081_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG082.BIN func_800FC11C (src/EFFECT/PLG082.BIN/128.c:23): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg082_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg082_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg082_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg082_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg082_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg082_particleEmissionCounter
#define VS_GRID_FUNCTION Effects_Plg082_Grid
#define VS_CLOSED_RING_FUNCTION Effects_Plg082_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg082_closedRingColorFactors
#include "Effects/Templates/ClosedRing.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE
#undef VS_GRID_FUNCTION
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS

// decomp: PLG086.BIN func_800F98F8 (src/EFFECT/PLG086.BIN/F8.c:8): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg086_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg086_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG087.BIN func_800FA588 (src/EFFECT/PLG087.BIN/1F0.c:13): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_SURFACE_FUNCTION Effects_Plg087_Surface
#define VS_SURFACE_VERTICES Effects_Plg087_surfaceVertices
#define VS_SURFACE_REPEAT
#define VS_CLOSED_RING_FUNCTION Effects_Plg087_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg087_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG128.BIN func_800FA490 (src/EFFECT/PLG128.BIN/F8.c:49): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define TRAIL_SAMPLE_COUNT 5
#define VS_SURFACE_FUNCTION Effects_Plg128_Surface
#define VS_SURFACE_VERTICES Effects_Plg128_surfaceVertices
#define VS_SURFACE_REPEAT
#define VS_CLOSED_RING_FUNCTION Effects_Plg128_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg128_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef TRAIL_SAMPLE_COUNT
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG134.BIN func_800FBA00 (src/EFFECT/PLG134.BIN/E0.c:18): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg134_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg134_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg134_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg134_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg134_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg134_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg134_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg134_trailEmissionCounter
#define VS_CLOSED_RING_FUNCTION Effects_Plg134_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg134_closedRingColorFactors
#include "Effects/Templates/ClosedRing.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS

// decomp: PLG135.BIN func_800F98E0 (src/EFFECT/PLG135.BIN/E0.c:8): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg135_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg135_closedRingColorFactors
#include "Effects/Templates/ClosedRing.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS

// decomp: PLG142.BIN func_800FA368 (src/EFFECT/PLG142.BIN/18.c:11): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_GRID_FUNCTION Effects_Plg142_Grid
#define VS_CLOSED_RING_FUNCTION Effects_Plg142_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg142_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_GRID_FUNCTION
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG143.BIN func_800FA6F8 (src/EFFECT/PLG143.BIN/0.c:12): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_TILED_RING_FUNCTION Effects_Plg143_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg143_tiledRingColorFactors
#define VS_CLOSED_RING_FUNCTION Effects_Plg143_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg143_closedRingColorFactors
#include "Effects/Templates/ClosedRing.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS

// decomp: PLG152.BIN func_800FC11C (src/EFFECT/PLG152.BIN/128.c:23): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_GRID_FUNCTION Effects_Plg152_Grid
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg152_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg152_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg152_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg152_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg152_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg082_particleEmissionCounter
#define VS_CLOSED_RING_FUNCTION Effects_Plg152_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg082_closedRingColorFactors
#include "Effects/Templates/ClosedRing.inl"
#undef VS_GRID_FUNCTION
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS

// decomp: PLG154.BIN func_800FA30C (src/EFFECT/PLG154.BIN/0.c:13): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_MESH10_FUNCTION Effects_Plg154_Mesh10
#define VS_MESH10_VERTICES Effects_Plg154_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg154_mesh10Faces
#define VS_CLOSED_RING_FUNCTION Effects_Plg154_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg154_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG158.BIN func_800FB00C (src/EFFECT/PLG158.BIN/0.c:17): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_SURFACE_FUNCTION Effects_Plg158_Surface
#define VS_SURFACE_VERTICES Effects_Plg158_surfaceVertices
#define VS_SURFACE_REPEAT
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg158_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg158_mirrorSurfaceVertices
#define VS_CLOSED_RING_FUNCTION Effects_Plg158_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg158_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG162.BIN func_800FC21C (src/EFFECT/PLG162.BIN/0.c:343): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg162_RotatingSurface
#define VS_RING_FUNCTION Effects_Plg162_Ring
#define VS_RING_COLORS Effects_Plg162_ringColorFactors2
#define VS_CLOSED_RING_FUNCTION Effects_Plg162_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg162_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG165.BIN func_800FD420 (src/EFFECT/PLG165.BIN/F8.c:24): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
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
#define VS_CLOSED_RING_FUNCTION Effects_Plg165_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg086_closedRingColorFactors
#include "Effects/Templates/ClosedRing.inl"
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
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS

// decomp: PLG166.BIN func_800FB32C (src/EFFECT/PLG166.BIN/0.c:15): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg166_RotatingSurface
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg166_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg166_mirrorSurfaceVertices
#define VS_CLOSED_RING_FUNCTION Effects_Plg166_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg166_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG167.BIN func_800F98E0 (src/EFFECT/PLG167.BIN/E0.c:8): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg167_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg167_closedRingColorFactors
#include "Effects/Templates/ClosedRing.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS

// decomp: PLG169.BIN func_800FC624 (src/EFFECT/PLG169.BIN/E0.c:22): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
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
#define VS_CLOSED_RING_FUNCTION Effects_Plg169_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg169_closedRingColorFactors
#include "Effects/Templates/ClosedRing.inl"
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
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS

// decomp: PLG170.BIN func_800FD654 (src/EFFECT/PLG170.BIN/18.c:29): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
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
#define VS_CLOSED_RING_FUNCTION Effects_Plg170_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg170_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
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
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG236.BIN func_800F9800 (src/EFFECT/PLG236.BIN/0.c:8): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg236_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg236_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG250.BIN func_800FA6BC (src/EFFECT/PLG250.BIN/0.c:12): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_ROTATING_SURFACE_REPEAT
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg250_RotatingSurface
#define VS_CLOSED_RING_FUNCTION Effects_Plg250_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg250_closedRingColorFactors
#include "Effects/Templates/ClosedRing.inl"
#undef VS_ROTATING_SURFACE_REPEAT
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS

// decomp: PLG272.BIN func_800FA368 (src/EFFECT/PLG272.BIN/18.c:11): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_GRID_FUNCTION Effects_Plg272_Grid
#define VS_CLOSED_RING_FUNCTION Effects_Plg272_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg272_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_GRID_FUNCTION
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG291.BIN func_800FC568 (src/EFFECT/PLG291.BIN/F8.c:21): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg291_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg291_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg291_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg291_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg291_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg291_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg291_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg291_trailEmissionCounter
#define VS_GRID_FUNCTION Effects_Plg291_Grid
#define VS_CLOSED_RING_FUNCTION Effects_Plg291_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg291_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_GRID_FUNCTION
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG297.BIN func_800FA48C (src/EFFECT/PLG297.BIN/18.c:12): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg297_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg297_mirrorSurfaceVertices
#define VS_CLOSED_RING_FUNCTION Effects_Plg297_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg297_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

// decomp: PLG301.BIN func_800FB5B4 (src/EFFECT/PLG301.BIN/110.c:20): VS_CLOSED_RING_FUNCTION, instance of Effects/Templates/ClosedRing.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg301_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg301_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg301_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg301_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg301_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg301_particleEmissionCounter
#define VS_CLOSED_RING_FUNCTION Effects_Plg301_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg301_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#include "Effects/Templates/ClosedRing.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS

}  // extern "C"
