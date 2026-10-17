// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_TEMPLATES_TEMPLATESTATE_H_TYPES
#define VS_EFFECTS_TEMPLATES_TEMPLATESTATE_H_TYPES
#include "Legacy/Psx.h"

typedef struct Effects_ClosedRingState Effects_ClosedRingState;
typedef struct Effects_GridState Effects_GridState;
typedef struct Effects_Mesh10State Effects_Mesh10State;
typedef struct Effects_MirrorSurfaceState Effects_MirrorSurfaceState;
typedef struct Effects_RingState Effects_RingState;
typedef struct Effects_RotatingSurfaceState Effects_RotatingSurfaceState;
typedef struct Effects_SurfaceState Effects_SurfaceState;
typedef struct Effects_TiledRingState Effects_TiledRingState;
typedef struct Effects_Trail9 Effects_Trail9;
typedef struct Effects_Trail9State Effects_Trail9State;
typedef struct Effects_TransposedRingState Effects_TransposedRingState;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_EFFECTS_TEMPLATES_TEMPLATESTATE_H_SET_TYPES_ONLY
#endif
#include "Effects/Particles.h"
#ifdef VS_EFFECTS_TEMPLATES_TEMPLATESTATE_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_EFFECTS_TEMPLATES_TEMPLATESTATE_H_SET_TYPES_ONLY
#endif

// decomp: vs_closedRingState (src/EFFECT/PLG077.BIN/18.c:25)
typedef struct Effects_ClosedRingState {
    u_char lifetime;
    u_char effect;
    short reserved2;
    int radius;
    int radiusSpeed;
    int height;
    int heightSpeed;
    int angle;
    int age;
    Effects_ParticleTextureAnimation texture;
} Effects_ClosedRingState;

// decomp: vs_gridState (src/EFFECT/grids.h:14)
typedef struct Effects_GridState {
    u_char lifetime, effect, u, v, uScroll, vScroll;
    short tpage, clut;
    short xSpacing, xSpeed, zSpacing, zSpeed, xPhase, zPhase;
    int age;
} Effects_GridState;

// decomp: vs_mesh10State (src/EFFECT/meshes10.h:16)
typedef struct Effects_Mesh10State {
    u_char lifetime, effect;
    short reserved2;
    int age;
    Effects_ParticleTextureAnimation texture;
    SVECTOR rotation;
} Effects_Mesh10State;

// decomp: vs_mirrorSurfaceState (src/EFFECT/mirroredSurfaces.h:17)
typedef struct Effects_MirrorSurfaceState {
    u_char lifetime, effect;
    short reserved2;
    int radius, radiusSpeed, height, heightSpeed, age;
    int factors[9];
    Effects_ParticleTextureAnimation texture;
} Effects_MirrorSurfaceState;

// decomp: vs_ringState (src/EFFECT/rings.h:17)
typedef struct Effects_RingState {
    u_char lifetime, effect;
    short reserved2;
    int radius, radiusSpeed, height, heightSpeed, angle, age;
    Effects_ParticleTextureAnimation texture;
} Effects_RingState;

// decomp: vs_rotatingSurfaceState (src/EFFECT/rotatingSurfaces.h:18)
typedef struct Effects_RotatingSurfaceState {
    u_char lifetime, effect;
    short reserved2;
    int radius, radiusSpeed, height, heightSpeed, angle, age;
    int factors[5];
    Effects_ParticleTextureAnimation texture;
} Effects_RotatingSurfaceState;

// decomp: vs_surfaceState (src/EFFECT/surfaces.h:17)
typedef struct Effects_SurfaceState {
    u_char lifetime, effect;
    short reserved2;
    int radius, radiusSpeed, height, heightSpeed, age;
    int factors[5];
    Effects_ParticleTextureAnimation texture;
} Effects_SurfaceState;

// decomp: vs_tiledRingState (src/EFFECT/tiledRings.h:22)
typedef struct Effects_TiledRingState {
    u_char lifetime;
    u_char effect;
    short reserved2;
    int radius;
    int radiusSpeed;
    int height;
    int heightSpeed;
    int angle;
    int age;
    Effects_ParticleTextureAnimation texture;
} Effects_TiledRingState;

// decomp: vs_trail9 (src/EFFECT/trails9Types.h:16)
typedef struct Effects_Trail9 {
    SVECTOR splinePoints[4];
    u_char stepCount;
    u_char splineSampleCount;
    u_char trailCollapseCounter;
    u_char currentSplineSample;
    SVECTOR splineSamples[9];
    int splineProgress;
    int speed;
    int opaque74;
} Effects_Trail9;

// decomp: vs_trail9State (src/EFFECT/trails9Types.h:27)
typedef struct Effects_Trail9State {
    u_char lifetime;
    u_char effect;
    u_char trailCount;
    u_char opaque3;
    Effects_Trail9* trails;
    int curveSampleDistance;
    Effects_ParticleTextureAnimation texture;
    u_char sampledTransparencyCurve[0];
} Effects_Trail9State;

// decomp: vs_transposedRingState (src/EFFECT/PLG162.BIN/0.c:20)
typedef struct Effects_TransposedRingState {
    u_char lifetime;
    u_char effect;
    short reserved2;
    int radius;
    int radiusSpeed;
    int height;
    int heightSpeed;
    int angle;
    int age;
    Effects_ParticleTextureAnimation texture;
} Effects_TransposedRingState;

#endif // VS_EFFECTS_TEMPLATES_TEMPLATESTATE_H_TYPES
