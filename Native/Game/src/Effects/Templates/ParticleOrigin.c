// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg045.h"
#include "Effects/Plugins/Plg057.h"
#include "Effects/Plugins/Plg065.h"
#include "Effects/Plugins/Plg082.h"
#include "Effects/Plugins/Plg087.h"
#include "Effects/Plugins/Plg114.h"
#include "Effects/Plugins/Plg146.h"
#include "Effects/Plugins/Plg147.h"
#include "Effects/Plugins/Plg153.h"
#include "Effects/Plugins/Plg156.h"
#include "Effects/Plugins/Plg168.h"
#include "Effects/Plugins/Plg224.h"
#include "Effects/Plugins/Plg255.h"
#include "Effects/Plugins/Plg263.h"
#include "Effects/Plugins/Plg265.h"
#include "Effects/Plugins/Plg266.h"
#include "Effects/Plugins/Plg300.h"
#include "Effects/Plugins/Plg301.h"
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

// decomp: PLG045.BIN func_800F9910 (src/EFFECT/PLG045.BIN/110.c:1): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg045_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg045_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg045_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg045_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg045_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg045_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG057.BIN func_800F9AD0 (src/EFFECT/PLG057.BIN/2D0.c:2): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg057_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg057_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg057_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg057_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg057_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg057_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG065.BIN func_800FC340 (src/EFFECT/PLG065.BIN/F8.c:19): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg065_RotatingSurface
#define VS_CLOSED_RING_FUNCTION Effects_Plg065_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg065_closedRingColorFactors
#define VS_RING_FUNCTION Effects_Plg065_Ring
#define VS_RING_COLORS Effects_Plg065_ringColorFactors
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg065_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg065_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg065_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg065_ParticleLineRender
#define VS_PARTICLE_CORNER_STATE Effects_Plg065_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG082.BIN func_800F9928 (src/EFFECT/PLG082.BIN/128.c:8): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg082_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg082_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg082_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg082_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg082_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg082_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG087.BIN func_800FE168 (src/EFFECT/PLG087.BIN/1F0.c:31): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
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
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg087_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg087_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg087_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg087_ParticleLineRender
#define VS_PARTICLE_CORNER_STATE Effects_Plg087_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
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
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG114.BIN func_800F9A08 (src/EFFECT/PLG114.BIN/208.c:2): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg114_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg114_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg114_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg114_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg114_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg114_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG114.BIN func_800FB6AC (src/EFFECT/PLG114.BIN/208.c:14): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg114_ParticleOrigin2
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg114_ParticleTarget2
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg114_ParticleControl2
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg114_ParticleLineRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg114_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg114_particleEmissionCounter2
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG146.BIN func_800FB544 (src/EFFECT/PLG146.BIN/1D8.c:10): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_TILED_RING_FUNCTION Effects_Plg146_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg146_tiledRingColorFactors
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg146_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg146_mirrorSurfaceVertices
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg146_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg146_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg146_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg146_ParticleLineRender
#define VS_PARTICLE_CORNER_STATE Effects_Plg146_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG147.BIN func_800FA51C (src/EFFECT/PLG147.BIN/F8.c:6): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_RING_FUNCTION Effects_Plg147_Ring
#define VS_RING_COLORS Effects_Plg147_ringColorFactors
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg147_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg147_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg147_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg147_ParticleLineRender
#define VS_PARTICLE_CORNER_STATE Effects_Plg147_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG152.BIN func_800FA478 (src/EFFECT/PLG152.BIN/128.c:11): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_GRID_FUNCTION Effects_Plg152_Grid
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg152_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg152_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg152_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg152_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg152_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg082_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_GRID_FUNCTION
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG153.BIN func_800FB040 (src/EFFECT/PLG153.BIN/110.c:12): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_MESH10_FUNCTION Effects_Plg153_Mesh10
#define VS_MESH10_VERTICES Effects_Plg153_mesh10Vertices
#define VS_MESH10_FACES Effects_Plg153_mesh10Faces
#define VS_RING_FUNCTION Effects_Plg153_Ring
#define VS_RING_COLORS Effects_Plg153_ringColorFactors
#define VS_RING_TRANSPOSED
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg153_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg153_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg153_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg153_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg153_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg153_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_MESH10_FUNCTION
#undef VS_MESH10_VERTICES
#undef VS_MESH10_FACES
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_RING_TRANSPOSED
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG156.BIN func_800F9928 (src/EFFECT/PLG156.BIN/128.c:2): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg156_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg156_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg156_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg156_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg156_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg156_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG168.BIN func_800FBB10 (src/EFFECT/PLG168.BIN/1F0.c:12): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
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
#define VS_PARTICLE_CORNER_STATE Effects_Plg168_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
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
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG224.BIN func_800F98F8 (src/EFFECT/PLG224.BIN/F8.c:2): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg224_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg224_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg224_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg224_ParticleLineRender
#define VS_PARTICLE_CORNER_STATE Effects_Plg224_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG255.BIN func_800FBAF8 (src/EFFECT/PLG255.BIN/1D8.c:12): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg255_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg255_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg255_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg255_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg255_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg255_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg255_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg255_trailEmissionCounter
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg255_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg255_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg255_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg255_ParticleLineRender
#define VS_PARTICLE_CORNER_STATE Effects_Plg255_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
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
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG263.BIN func_800FC00C (src/EFFECT/PLG263.BIN/110.c:344): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_SURFACE_FUNCTION Effects_Plg263_Surface
#define VS_SURFACE_VERTICES Effects_Plg263_surfaceVertices
#define VS_RING_FUNCTION Effects_Plg263_Ring
#define VS_RING_COLORS Effects_Plg263_ringColorFactors
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg263_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg263_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg263_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg263_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg263_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg263_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_SURFACE_FUNCTION
#undef VS_SURFACE_VERTICES
#undef VS_RING_FUNCTION
#undef VS_RING_COLORS
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG265.BIN func_800FA9E0 (src/EFFECT/PLG265.BIN/2E8.c:6): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_TILED_RING_FUNCTION Effects_Plg265_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg265_tiledRingColorFactors
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg265_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg265_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg265_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg265_ParticleLineRender
#define VS_PARTICLE_CORNER_STATE Effects_Plg265_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_TILED_RING_FUNCTION
#undef VS_TILED_RING_COLORS
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG265.BIN func_800FE0F4 (src/EFFECT/PLG265.BIN/2E8.c:40): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_TILED_RING_FUNCTION Effects_Plg265_TiledRing
#define VS_TILED_RING_COLORS Effects_Plg265_tiledRingColorFactors
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg265_ParticleOrigin2
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg265_ParticleTarget2
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg265_ParticleControl2
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg265_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg265_ParticleDispatch2
#define VS_PARTICLE_UNTEXTURED
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg265_TrailOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg265_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg265_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg265_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg265_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg265_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg265_TrailDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg265_trailEmissionCounter
#define VS_PARTICLE_CORNER_STATE Effects_Plg265_particleEmissionCounter2
#include "Effects/Templates/ParticleOrigin.inl"
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
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG266.BIN func_800F9AE8 (src/EFFECT/PLG266.BIN/2E8.c:2): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg266_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg266_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg266_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg266_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg266_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg266_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG266.BIN func_800FB78C (src/EFFECT/PLG266.BIN/2E8.c:14): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg266_ParticleOrigin2
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg266_ParticleTarget2
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg266_ParticleControl2
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg266_ParticleLineRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg266_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg266_particleEmissionCounter2
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG300.BIN func_800FA584 (src/EFFECT/PLG300.BIN/110.c:12): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_MIRROR_SURFACE_FUNCTION Effects_Plg300_MirrorSurface
#define VS_MIRROR_SURFACE_VERTICES Effects_Plg300_mirrorSurfaceVertices
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg300_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg300_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg300_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg300_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg300_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg300_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_MIRROR_SURFACE_FUNCTION
#undef VS_MIRROR_SURFACE_VERTICES
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

// decomp: PLG301.BIN func_800F9910 (src/EFFECT/PLG301.BIN/110.c:8): VS_PARTICLE_ORIGIN_FUNCTION, instance of Effects/Templates/ParticleOrigin.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg301_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg301_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg301_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg301_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg301_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg301_particleEmissionCounter
#include "Effects/Templates/ParticleOrigin.inl"
#undef VS_PARTICLE_ORIGIN_FUNCTION
#undef VS_PARTICLE_TARGET_FUNCTION
#undef VS_PARTICLE_CONTROL_FUNCTION
#undef VS_PARTICLE_RENDER_FUNCTION
#undef VS_PARTICLE_DISPATCH_FUNCTION
#undef VS_PARTICLE_CORNER_STATE

