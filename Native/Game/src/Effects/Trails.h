// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_TRAILS_H_TYPES
#define VS_EFFECTS_TRAILS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Effects_BoneTrail Effects_BoneTrail;
typedef struct Effects_Trail Effects_Trail;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_EFFECTS_TRAILS_H_SET_TYPES_ONLY
#endif
#include "Battle/BattleTypes.h"
#ifdef VS_EFFECTS_TRAILS_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_EFFECTS_TRAILS_H_SET_TYPES_ONLY
#endif

// decomp: src/EFFECT/trails5.h:5
#define TRAIL_SAMPLE_COUNT 5

// decomp: _trail (src/EFFECT/trails.h:16)
typedef struct Effects_Trail {
    SVECTOR splinePoints[4];
    u_char stepCount;
    u_char splineSampleCount;
    u_char trailCollapseCounter;
    u_char currentSplineSample;
    SVECTOR splineSamples[TRAIL_SAMPLE_COUNT];
    int splineProgress;
    int speed;
    int opaque54;
} Effects_Trail;
static_assert(sizeof(Effects_Trail) == 0x58, "Effects_Trail must keep its PS1 layout");

// decomp: effectTrailState (src/BATTLE/BATTLE.PRG/5BF94.c:135)
typedef struct Effects_BoneTrail {
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
} Effects_BoneTrail;
static_assert(sizeof(Effects_BoneTrail) == 0xB4, "Effects_BoneTrail must keep its PS1 layout");

#endif // VS_EFFECTS_TRAILS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_TRAILS_H_DECLS)
#define VS_EFFECTS_TRAILS_H_DECLS

extern CVECTOR Effects_trailAdditiveColors[5473];
extern CVECTOR Effects_trailColors[5465];

#endif // VS_EFFECTS_TRAILS_H_DECLS
