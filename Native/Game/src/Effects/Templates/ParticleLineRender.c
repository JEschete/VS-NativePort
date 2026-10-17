// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg065.h"
#include "Effects/Plugins/Plg087.h"
#include "Effects/Plugins/Plg146.h"
#include "Effects/Plugins/Plg147.h"
#include "Effects/Plugins/Plg168.h"
#include "Effects/Plugins/Plg255.h"
#include "Effects/Plugins/Plg265.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Templates/Grid.h"
#include "Effects/Templates/MirrorSurface.h"
#include "Effects/Templates/ParticleDispatch.h"
#include "Effects/Templates/ParticleLineRender.h"
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

// decomp: PLG065.BIN func_800FD190 (src/EFFECT/PLG065.BIN/F8.c:23): VS_PARTICLE_RENDER_FUNCTION, instance of Effects/Templates/ParticleLineRender.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg065_RotatingSurface
#define VS_CLOSED_RING_FUNCTION Effects_Plg065_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg065_closedRingColorFactors
#define VS_RING_FUNCTION Effects_Plg065_Ring
#define VS_RING_COLORS Effects_Plg065_ringColorFactors
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg065_ParticleLineRender
#include "Effects/Templates/ParticleLineRender.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_PARTICLE_RENDER_FUNCTION

// decomp: PLG087.BIN func_800FEFB8 (src/EFFECT/PLG087.BIN/1F0.c:35): VS_PARTICLE_RENDER_FUNCTION, instance of Effects/Templates/ParticleLineRender.inl
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
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg087_ParticleLineRender
#include "Effects/Templates/ParticleLineRender.inl"
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
#undef VS_PARTICLE_RENDER_FUNCTION

// decomp: PLG114.BIN func_800FC4FC (src/EFFECT/PLG114.BIN/208.c:18): VS_PARTICLE_RENDER_FUNCTION, instance of Effects/Templates/ParticleLineRender.inl
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg114_ParticleDispatch
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg114_ParticleLineRender
#include "Effects/Templates/ParticleLineRender.inl"
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION

// decomp: PLG146.BIN func_800FC394 (src/EFFECT/PLG146.BIN/1D8.c:14): VS_PARTICLE_RENDER_FUNCTION, instance of Effects/Templates/ParticleLineRender.inl
#define VS_TILED_RING_FUNCTION Effects_Plg146_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg146_tiledRingColorFactors
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg146_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg146_mirrorSurfaceVertices
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg146_ParticleLineRender
#include "Effects/Templates/ParticleLineRender.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_PARTICLE_RENDER_FUNCTION

// decomp: PLG147.BIN func_800FB36C (src/EFFECT/PLG147.BIN/F8.c:10): VS_PARTICLE_RENDER_FUNCTION, instance of Effects/Templates/ParticleLineRender.inl
#define VS_RING_FUNCTION Effects_Plg147_Ring
#define VS_RING_COLORS Effects_Plg147_ringColorFactors
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg147_ParticleLineRender
#include "Effects/Templates/ParticleLineRender.inl"
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_PARTICLE_RENDER_FUNCTION

// decomp: PLG168.BIN func_800FC960 (src/EFFECT/PLG168.BIN/1F0.c:16): VS_PARTICLE_RENDER_FUNCTION, instance of Effects/Templates/ParticleLineRender.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg168_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg168_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg168_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg168_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg168_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg168_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg168_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg168_trailEmissionCounter
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg168_ParticleLineRender
#include "Effects/Templates/ParticleLineRender.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_PARTICLE_RENDER_FUNCTION

// decomp: PLG224.BIN func_800FA748 (src/EFFECT/PLG224.BIN/F8.c:6): VS_PARTICLE_RENDER_FUNCTION, instance of Effects/Templates/ParticleLineRender.inl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg224_ParticleLineRender
#include "Effects/Templates/ParticleLineRender.inl"
#undef VS_PARTICLE_RENDER_FUNCTION

// decomp: PLG255.BIN func_800FC948 (src/EFFECT/PLG255.BIN/1D8.c:16): VS_PARTICLE_RENDER_FUNCTION, instance of Effects/Templates/ParticleLineRender.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg255_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg255_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg255_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg255_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg255_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg255_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg255_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg255_trailEmissionCounter
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg255_ParticleLineRender
#include "Effects/Templates/ParticleLineRender.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE
#undef VS_PARTICLE_RENDER_FUNCTION

// decomp: PLG265.BIN func_800FB830 (src/EFFECT/PLG265.BIN/2E8.c:10): VS_PARTICLE_RENDER_FUNCTION, instance of Effects/Templates/ParticleLineRender.inl
#define VS_TILED_RING_FUNCTION Effects_Plg265_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg265_tiledRingColorFactors
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg265_ParticleLineRender
#include "Effects/Templates/ParticleLineRender.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_PARTICLE_RENDER_FUNCTION

// decomp: PLG266.BIN func_800FC5DC (src/EFFECT/PLG266.BIN/2E8.c:18): VS_PARTICLE_RENDER_FUNCTION, instance of Effects/Templates/ParticleLineRender.inl
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg266_ParticleDispatch
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg266_ParticleLineRender
#include "Effects/Templates/ParticleLineRender.inl"
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION

