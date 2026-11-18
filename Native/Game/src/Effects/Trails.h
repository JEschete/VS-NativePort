// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Effects_BoneTrail;
struct Effects_Trail;

#include "Battle/BattleTypes.h"

// decomp: src/EFFECT/trails5.h:5
#define TRAIL_SAMPLE_COUNT 5

// decomp: _trail (src/EFFECT/trails.h:16)
struct Effects_Trail {
    SVECTOR splinePoints[4];
    u_char stepCount;
    u_char splineSampleCount;
    u_char trailCollapseCounter;
    u_char currentSplineSample;
    SVECTOR splineSamples[TRAIL_SAMPLE_COUNT];
    int splineProgress;
    int speed;
    int opaque54;
};
static_assert(sizeof(Effects_Trail) == 0x58, "Effects_Trail must keep its PS1 layout");

// decomp: effectTrailState (src/BATTLE/BATTLE.PRG/5BF94.c:135)
struct Effects_BoneTrail {
    Battle_EffectTrailConfig config;
    int index;
    int age;
    SVECTOR first[8];
    SVECTOR second[8];
    SVECTOR previousFirst;
    SVECTOR currentFirst;
    SVECTOR previousSecond;
    SVECTOR currentSecond;
    long projectionDepth;
    long projectionFlags;
};
static_assert(sizeof(Effects_BoneTrail) == 0xB4, "Effects_BoneTrail must keep its PS1 layout");

extern CVECTOR Effects_trailAdditiveColors[5473];
extern CVECTOR Effects_trailColors[5465];
