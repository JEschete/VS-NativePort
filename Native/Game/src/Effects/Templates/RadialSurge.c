#include "Effects/Templates/RadialSurge.h"

#include "Effects/Templates/TrailControl.h"
#include "Effects/Templates/TrailInit.h"
#include "Effects/Templates/TrailRender.h"
#include "Effects/Templates/TrailTarget.h"
#include "Effects/Templates/TrailUpdate.h"

// PLG074 func_800FB414, original F8.c:7; current trailEffect.h origin stage.
// The code matches PLG073, but the emission cursor belongs to this overlay.
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg074_GenerateTrailOrigins
#define VS_TRAIL_CORNER_STATE Content_plg074TrailEmissionCounter[0]
// These templates share this package; include paths use the project source
// root.
#include "Effects/Templates/TrailOrigin.inl"  // NOLINT(build/include)
#undef VS_TRAIL_CORNER_STATE
#undef VS_TRAIL_ORIGIN_FUNCTION

// PLG074 func_800FCFEC, original F8.c:19; current trailEffect.h dispatch stage.
#define VS_TRAIL_ORIGIN_FUNCTION Effects_Plg074_GenerateTrailOrigins
#define VS_TRAIL_TARGET_FUNCTION Effects_Plg073_TrailTarget
#define VS_TRAIL_CONTROL_FUNCTION Effects_Plg073_TrailControl
#define VS_TRAIL_INIT_FUNCTION Effects_Plg073_TrailInit
#define VS_TRAIL_UPDATE_FUNCTION Effects_Plg073_TrailUpdate
#define VS_TRAIL_RENDER_FUNCTION Effects_Plg073_TrailRender
#define VS_TRAIL_DISPATCH_FUNCTION Effects_Plg074_UpdateTrailEmitter
#include "Effects/Templates/TrailDispatch.inl"  // NOLINT(build/include)
#undef VS_TRAIL_DISPATCH_FUNCTION
#undef VS_TRAIL_RENDER_FUNCTION
#undef VS_TRAIL_UPDATE_FUNCTION
#undef VS_TRAIL_INIT_FUNCTION
#undef VS_TRAIL_CONTROL_FUNCTION
#undef VS_TRAIL_TARGET_FUNCTION
#undef VS_TRAIL_ORIGIN_FUNCTION
