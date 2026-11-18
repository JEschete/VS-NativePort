// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Effects_ClosedRingState;
struct Effects_GridState;
struct Effects_Mesh10State;
struct Effects_MirrorSurfaceState;
struct Effects_RingState;
struct Effects_RotatingSurfaceState;
struct Effects_SurfaceState;
struct Effects_TiledRingState;
struct Effects_Trail9;
struct Effects_Trail9State;
struct Effects_TransposedRingState;

#include "Effects/Particles.h"

// decomp: vs_closedRingState (src/EFFECT/PLG077.BIN/18.c:25)
struct Effects_ClosedRingState {
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
};

// decomp: vs_gridState (src/EFFECT/grids.h:14)
struct Effects_GridState {
    u_char lifetime, effect, u, v, uScroll, vScroll;
    short tpage, clut;
    short xSpacing, xSpeed, zSpacing, zSpeed, xPhase, zPhase;
    int age;
};
static_assert(sizeof(Effects_GridState) == 0x1C, "Effects_GridState must keep its PS1 layout");

// decomp: vs_mesh10State (src/EFFECT/meshes10.h:16)
struct Effects_Mesh10State {
    u_char lifetime, effect;
    short reserved2;
    int age;
    Effects_ParticleTextureAnimation texture;
    SVECTOR rotation;
};

// decomp: vs_mirrorSurfaceState (src/EFFECT/mirroredSurfaces.h:17)
struct Effects_MirrorSurfaceState {
    u_char lifetime, effect;
    short reserved2;
    int radius, radiusSpeed, height, heightSpeed, age;
    int factors[9];
    Effects_ParticleTextureAnimation texture;
};

// decomp: vs_ringState (src/EFFECT/rings.h:17)
struct Effects_RingState {
    u_char lifetime, effect;
    short reserved2;
    int radius, radiusSpeed, height, heightSpeed, angle, age;
    Effects_ParticleTextureAnimation texture;
};

// decomp: vs_rotatingSurfaceState (src/EFFECT/rotatingSurfaces.h:18)
struct Effects_RotatingSurfaceState {
    u_char lifetime, effect;
    short reserved2;
    int radius, radiusSpeed, height, heightSpeed, angle, age;
    int factors[5];
    Effects_ParticleTextureAnimation texture;
};

// decomp: vs_surfaceState (src/EFFECT/surfaces.h:17)
struct Effects_SurfaceState {
    u_char lifetime, effect;
    short reserved2;
    int radius, radiusSpeed, height, heightSpeed, age;
    int factors[5];
    Effects_ParticleTextureAnimation texture;
};

// decomp: vs_tiledRingState (src/EFFECT/tiledRings.h:22)
struct Effects_TiledRingState {
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
};

// decomp: vs_trail9 (src/EFFECT/trails9Types.h:16)
struct Effects_Trail9 {
    SVECTOR splinePoints[4];
    u_char stepCount;
    u_char splineSampleCount;
    u_char trailCollapseCounter;
    u_char currentSplineSample;
    SVECTOR splineSamples[9];
    int splineProgress;
    int speed;
    int opaque74;
};
static_assert(sizeof(Effects_Trail9) == 0x78, "Effects_Trail9 must keep its PS1 layout");

// decomp: vs_trail9State (src/EFFECT/trails9Types.h:27)
struct Effects_Trail9State {
    u_char lifetime;
    u_char effect;
    u_char trailCount;
    u_char opaque3;
    Effects_Trail9* trails;
    int curveSampleDistance;
    Effects_ParticleTextureAnimation texture;
    u_char sampledTransparencyCurve[0];
};

// decomp: vs_transposedRingState (src/EFFECT/PLG162.BIN/0.c:20)
struct Effects_TransposedRingState {
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
};
