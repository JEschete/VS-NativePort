// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg049.h"
#include "Effects/Plugins/Plg050.h"
#include "Effects/Plugins/Plg057.h"
#include "Effects/Plugins/Plg069.h"
#include "Effects/Plugins/Plg135.h"
#include "Effects/Plugins/Plg161.h"
#include "Effects/Plugins/Plg165.h"
#include "Effects/Plugins/Plg167.h"
#include "Effects/Plugins/Plg168.h"
#include "Effects/Plugins/Plg169.h"
#include "Effects/Plugins/Plg232.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.h"
#include "Effects/Templates/Mesh10.h"
#include "Effects/Templates/ParticleControl.h"
#include "Effects/Templates/ParticleDispatch.h"
#include "Effects/Templates/ParticleOrigin.h"
#include "Effects/Templates/ParticleRender.h"
#include "Effects/Templates/ParticleTarget.h"
#include "Effects/Templates/Ring.h"
#include "Effects/Templates/RotatingSurface.h"
#include "Effects/Templates/TemplateState.h"
#include "Effects/Templates/TrailNineControl.h"
#include "Effects/Templates/TrailNineDispatch.h"
#include "Effects/Templates/TrailNineInit.h"
#include "Effects/Templates/TrailNineOrigin.h"
#include "Effects/Templates/TrailNineRender.h"
#include "Effects/Templates/TrailNineTarget.h"
#include "Effects/Templates/TrailNineUpdate.h"
#include "Effects/Transforms.h"

// decomp: PLG049.BIN vs_spiritSurge_renderTrails (src/EFFECT/PLG049.BIN/E0.c:7): VS_TRAIL_DISPATCH_FUNCTION, instance of Effects/Templates/TrailNineDispatch.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg049_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg049_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg049_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg049_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg049_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg049_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg049_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg049_trailEmissionCounter
#include "Effects/Templates/TrailNineDispatch.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG050.BIN func_800FC0E4 (src/EFFECT/PLG050.BIN/E0.c:13): VS_TRAIL_DISPATCH_FUNCTION, instance of Effects/Templates/TrailNineDispatch.inl
#define VS_RING_FUNCTION Effects_Plg050_Ring
#define VS_RING_COLORS Effects_Plg050_ringColorFactors
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg050_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg050_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg050_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg050_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg050_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg050_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg050_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg050_trailEmissionCounter
#include "Effects/Templates/TrailNineDispatch.inl"
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

// decomp: PLG057.BIN func_800FD354 (src/EFFECT/PLG057.BIN/2D0.c:21): VS_TRAIL_DISPATCH_FUNCTION, instance of Effects/Templates/TrailNineDispatch.inl
#define VS_PARTICLE_ORIGIN_FUNCTION Effects_Plg057_ParticleOrigin
#define VS_PARTICLE_TARGET_FUNCTION Effects_Plg057_ParticleTarget
#define VS_PARTICLE_CONTROL_FUNCTION Effects_Plg057_ParticleControl
#define VS_PARTICLE_RENDER_FUNCTION Effects_Plg057_ParticleRender
#define VS_PARTICLE_DISPATCH_FUNCTION Effects_Plg057_ParticleDispatch
#define VS_PARTICLE_CORNER_STATE Effects_Plg057_particleEmissionCounter
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg057_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg057_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg057_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg057_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg057_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg057_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg057_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg057_trailEmissionCounter
#include "Effects/Templates/TrailNineDispatch.inl"
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

// decomp: PLG069.BIN func_800FBFCC (src/EFFECT/PLG069.BIN/E0.c:14): VS_TRAIL_DISPATCH_FUNCTION, instance of Effects/Templates/TrailNineDispatch.inl
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
#include "Effects/Templates/TrailNineDispatch.inl"
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

// decomp: PLG135.BIN func_800FC42C (src/EFFECT/PLG135.BIN/E0.c:19): VS_TRAIL_DISPATCH_FUNCTION, instance of Effects/Templates/TrailNineDispatch.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg135_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg135_closedRingColorFactors
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg135_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg135_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg135_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg135_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg135_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg135_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg135_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg135_trailEmissionCounter
#include "Effects/Templates/TrailNineDispatch.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG161.BIN func_800FCFB4 (src/EFFECT/PLG161.BIN/F8.c:17): VS_TRAIL_DISPATCH_FUNCTION, instance of Effects/Templates/TrailNineDispatch.inl
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
#include "Effects/Templates/TrailNineDispatch.inl"
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

// decomp: PLG165.BIN func_800FB4D8 (src/EFFECT/PLG165.BIN/F8.c:15): VS_TRAIL_DISPATCH_FUNCTION, instance of Effects/Templates/TrailNineDispatch.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg165_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg165_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg165_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg165_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg165_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg165_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg165_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg165_trailEmissionCounter
#include "Effects/Templates/TrailNineDispatch.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG167.BIN func_800FD2E4 (src/EFFECT/PLG167.BIN/E0.c:22): VS_TRAIL_DISPATCH_FUNCTION, instance of Effects/Templates/TrailNineDispatch.inl
#define VS_CLOSED_RING_FUNCTION Effects_Plg167_ClosedRing
#define VS_CLOSED_RING_COLORS Effects_Plg167_closedRingColorFactors
#define VS_ROTATING_SURFACE_FUNCTION Effects_Plg167_RotatingSurface
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg167_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg167_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg167_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg167_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg167_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg167_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg167_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg167_trailEmissionCounter
#include "Effects/Templates/TrailNineDispatch.inl"
#undef VS_CLOSED_RING_FUNCTION
#undef VS_CLOSED_RING_COLORS
#undef VS_ROTATING_SURFACE_FUNCTION
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG168.BIN func_800FB5D0 (src/EFFECT/PLG168.BIN/1F0.c:9): VS_TRAIL_DISPATCH_FUNCTION, instance of Effects/Templates/TrailNineDispatch.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg168_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg168_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg168_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg168_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg168_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg168_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg168_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg168_trailEmissionCounter
#include "Effects/Templates/TrailNineDispatch.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG169.BIN func_800FB4C0 (src/EFFECT/PLG169.BIN/E0.c:15): VS_TRAIL_DISPATCH_FUNCTION, instance of Effects/Templates/TrailNineDispatch.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg169_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg169_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg169_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg169_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg169_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg169_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg169_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg169_trailEmissionCounter
#include "Effects/Templates/TrailNineDispatch.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

// decomp: PLG232.BIN func_800FB4C0 (src/EFFECT/PLG232.BIN/E0.c:9): VS_TRAIL_DISPATCH_FUNCTION, instance of Effects/Templates/TrailNineDispatch.inl
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg232_TrailNineOrigin
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg232_TrailNineTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg232_TrailNineControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg232_TrailNineInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg232_TrailNineUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg232_TrailNineRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg232_TrailNineDispatch
#define VS_TRAIL_CORNER_STATE Effects_Plg232_trailEmissionCounter
#include "Effects/Templates/TrailNineDispatch.inl"
#undef VS_TRAIL_ORIGIN_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_CORNER_STATE

