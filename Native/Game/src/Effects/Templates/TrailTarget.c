// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg057.h"
#include "Effects/Plugins/Plg073.h"
#include "Effects/Plugins/Plg086.h"
#include "Effects/Plugins/Plg087.h"
#include "Effects/Plugins/Plg128.h"
#include "Effects/Plugins/Plg134.h"
#include "Effects/Plugins/Plg137.h"
#include "Effects/Plugins/Plg141.h"
#include "Effects/Plugins/Plg146.h"
#include "Effects/Plugins/Plg148.h"
#include "Effects/Plugins/Plg242.h"
#include "Effects/Plugins/Plg255.h"
#include "Effects/Plugins/Plg265.h"
#include "Effects/Plugins/Plg266.h"
#include "Effects/Plugins/Plg291.h"
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
#include "Effects/Templates/TiledRing.h"
#include "Effects/Templates/TrailControl.h"
#include "Effects/Templates/TrailDispatch.h"
#include "Effects/Templates/TrailInit.h"
#include "Effects/Templates/TrailOrigin.h"
#include "Effects/Templates/TrailRender.h"
#include "Effects/Templates/TrailTarget.h"
#include "Effects/Templates/TrailUpdate.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"

// decomp: PLG057.BIN func_800FE030 (src/EFFECT/PLG057.BIN/2D0.c:26): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg057_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg057_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg057_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg057_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg057_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg057_particleEmissionCounter
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg057_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg057_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg057_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg057_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg057_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg057_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg057_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg057_trailEmissionCounter2
#include "Effects/Templates/TrailTarget.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG073.BIN func_800FBBB0 (src/EFFECT/PLG073.BIN/F8.c:12): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
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
#include "Effects/Templates/TrailTarget.inl"
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

// decomp: PLG086.BIN func_800FCA0C (src/EFFECT/PLG086.BIN/F8.c:21): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg086_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg086_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg086_RotatingSurface
#define VS_GRID_FUNCTION Effects_Plg086_Grid
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg086_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg086_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg086_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg086_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg086_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg086_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg086_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg086_trailEmissionCounter
#include "Effects/Templates/TrailTarget.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_GRID_FUNCTION
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG087.BIN func_800FC7E4 (src/EFFECT/PLG087.BIN/1F0.c:23): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
#define VS_SURFACE_FUNCTION Effects_Plg087_Surface
#define VS_SURFACE_VERTICES Effects_Plg087_surfaceVertices
#define VS_SURFACE_REPEAT
#define VS_CLOSED_RING_FUNCTION Effects_Plg087_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg087_closedRingColorFactors
#define VS_CLOSED_RING_TEXTURE_COLUMNS 4
#define VS_GRID_FUNCTION Effects_Plg087_Grid
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg087_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg087_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg087_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg087_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg087_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg087_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg087_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg087_trailEmissionCounter
#include "Effects/Templates/TrailTarget.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_SURFACE_REPEAT
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_CLOSED_RING_TEXTURE_COLUMNS
#undef VS_GRID_FUNCTION
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG128.BIN func_800FEB6C (src/EFFECT/PLG128.BIN/F8.c:254): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
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
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg128_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg128_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg128_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg128_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg128_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg128_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg128_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg128_trailEmissionCounter
#include "Effects/Templates/TrailTarget.inl"
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
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG134.BIN func_800FA07C (src/EFFECT/PLG134.BIN/E0.c:10): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg134_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg134_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg134_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg134_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg134_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg134_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg134_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg134_trailEmissionCounter
#include "Effects/Templates/TrailTarget.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG137.BIN func_800FABA0 (src/EFFECT/PLG137.BIN/F8.c:9): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
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
#include "Effects/Templates/TrailTarget.inl"
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

// decomp: PLG141.BIN func_800FA07C (src/EFFECT/PLG141.BIN/E0.c:4): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg141_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg141_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg141_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg141_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg141_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg141_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg141_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg141_trailEmissionCounter
#include "Effects/Templates/TrailTarget.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG146.BIN func_800FDEF8 (src/EFFECT/PLG146.BIN/1D8.c:41): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
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
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg146_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg146_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg146_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg146_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg146_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg146_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg146_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg146_trailEmissionCounter
#include "Effects/Templates/TrailTarget.inl"
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
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG148.BIN func_800FA07C (src/EFFECT/PLG148.BIN/E0.c:4): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg148_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg148_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg148_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg148_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg148_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg148_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg148_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg148_trailEmissionCounter
#include "Effects/Templates/TrailTarget.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG242.BIN func_800FBADC (src/EFFECT/PLG242.BIN/F8.c:11): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
#define VS_TILED_RING_FUNCTION Effects_Plg242_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg242_tiledRingColorFactors
#define VS_GRID_FUNCTION Effects_Plg242_Grid
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg242_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg242_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg242_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg242_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg242_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg242_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg242_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg242_trailEmissionCounter
#include "Effects/Templates/TrailTarget.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_GRID_FUNCTION
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG255.BIN func_800FA174 (src/EFFECT/PLG255.BIN/1D8.c:4): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg255_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg255_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg255_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg255_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg255_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg255_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg255_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg255_trailEmissionCounter
#include "Effects/Templates/TrailTarget.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG265.BIN func_800FC770 (src/EFFECT/PLG265.BIN/2E8.c:32): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
#define VS_TILED_RING_FUNCTION Effects_Plg265_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg265_tiledRingColorFactors
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg265_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg265_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg265_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg265_ParticleLineRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg265_ParticleDispatch
#define VS_PARTICLE_UNTEXTURED
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg265_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg265_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg265_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg265_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg265_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg265_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg265_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg265_trailEmissionCounter
#include "Effects/Templates/TrailTarget.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_UNTEXTURED
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG266.BIN func_800FD51C (src/EFFECT/PLG266.BIN/2E8.c:40): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg266_ParticleDispatch2
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg266_ParticleOrigin2
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg266_ParticleTarget2
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg266_ParticleControl2
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg266_ParticleLineRender
#define VS_PARTICLE_UNTEXTURED
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg266_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg266_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg266_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg266_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg266_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg266_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg266_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg266_trailEmissionCounter
#include "Effects/Templates/TrailTarget.inl"
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_UNTEXTURED
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG291.BIN func_800FA094 (src/EFFECT/PLG291.BIN/F8.c:10): VS_TRAIL_TARGET_FUNCTION, instance of Effects/Templates/TrailTarget.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg291_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg291_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg291_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg291_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg291_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg291_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg291_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg291_trailEmissionCounter
#include "Effects/Templates/TrailTarget.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

