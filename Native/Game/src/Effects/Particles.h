// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_PARTICLES_H_TYPES
#define VS_EFFECTS_PARTICLES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Effects_FixedVector Effects_FixedVector;
typedef struct Effects_PackageTextureInfo Effects_PackageTextureInfo;
typedef struct Effects_Particle Effects_Particle;
typedef struct Effects_ParticleAttractionView Effects_ParticleAttractionView;
typedef union Effects_ParticleColorWord Effects_ParticleColorWord;
typedef struct Effects_ParticleDefinition Effects_ParticleDefinition;
typedef struct Effects_ParticleEmitterScratch Effects_ParticleEmitterScratch;
typedef struct Effects_ParticleEndpointView Effects_ParticleEndpointView;
typedef struct Effects_ParticleGravity Effects_ParticleGravity;
typedef struct Effects_ParticleMotion Effects_ParticleMotion;
typedef struct Effects_ParticleMotionView Effects_ParticleMotionView;
typedef struct Effects_ParticleQuadSet Effects_ParticleQuadSet;
typedef struct Effects_ParticleQuadWord Effects_ParticleQuadWord;
typedef struct Effects_ParticleTextureAnimation Effects_ParticleTextureAnimation;
typedef struct Effects_PluginEffectState Effects_PluginEffectState;
typedef struct Effects_Trail Effects_Trail;
typedef struct Effects_TrailQuadSet Effects_TrailQuadSet;

// decomp: D_800F5620_t (src/BATTLE/BATTLE.PRG/5BF94.c:113)
typedef struct Effects_ParticleGravity {
    int x;
    int y;
    int z;
    int reservedC;
} Effects_ParticleGravity;
_Static_assert(sizeof(Effects_ParticleGravity) == 0x10, "Effects_ParticleGravity must keep its PS1 layout");

// decomp: effectAttractionView (src/BATTLE/BATTLE.PRG/5BF94.c:631)
typedef struct Effects_ParticleAttractionView {
    char reserved0[0x62];
    short targetX, targetY, targetZ;
    char reserved68[8];
    int attraction;
} Effects_ParticleAttractionView;
_Static_assert(sizeof(Effects_ParticleAttractionView) == 0x74, "Effects_ParticleAttractionView must keep its PS1 layout");

// decomp: effectMotionState (src/BATTLE/BATTLE.PRG/5BF94.c:625)
typedef struct Effects_ParticleMotionView {
    short reserved0;
    short gravityScale;
    int x, y, z;
    int vx, vy, vz;
    int fx, fy, fz;
    int ax, ay, az;
} Effects_ParticleMotionView;
_Static_assert(sizeof(Effects_ParticleMotionView) == 0x34, "Effects_ParticleMotionView must keep its PS1 layout");

// decomp: func_800CFAAC_t (src/BATTLE/BATTLE.PRG/5BF94.c:177)
typedef struct Effects_ParticleEndpointView {
    char reserved0[0xC];
    short endX;
    short endY;
    short startX;
    short startY;
} Effects_ParticleEndpointView;
_Static_assert(sizeof(Effects_ParticleEndpointView) == 0x14, "Effects_ParticleEndpointView must keep its PS1 layout");

// decomp: func_800D6CF_t (src/BATTLE/BATTLE.PRG/5BF94.h:230)
typedef struct Effects_ParticleTextureAnimation {
    int reserved0;
    short reserved4;
    u_char frameTimer;
    u_char reserved7;
    short frameOffsetX;
    short frameOffsetY;
    short reservedC;
    u_short cursor;
    int reserved10;
    short reserved14;
    u_char textureIndex;
    u_char reserved17;
    u_char* program;
    Effects_PackageTextureInfo* textureInfo;
} Effects_ParticleTextureAnimation;

// layout: ledger (upstream merge) for func_800D52A4_t2
typedef struct Effects_FixedVector {
    /* 0x00 */ int vx;
    /* 0x04 */ int vy;
    /* 0x08 */ int vz;
} Effects_FixedVector;
_Static_assert(sizeof(Effects_FixedVector) == 0xC, "Effects_FixedVector must keep its PS1 layout");

// layout: ledger (upstream merge) for func_800D52A4_t
typedef struct Effects_ParticleMotion {
    /* 0x00 */ short mass;
    /* 0x02 */ short gravityScale;
    /* 0x04 */ Effects_FixedVector position;
    /* 0x10 */ Effects_FixedVector velocity;
    /* 0x1C */ Effects_FixedVector force;
    /* 0x28 */ Effects_FixedVector acceleration;
} Effects_ParticleMotion;
_Static_assert(sizeof(Effects_ParticleMotion) == 0x34, "Effects_ParticleMotion must keep its PS1 layout");

// layout: ledger (upstream merge) for func_800D2904_t
typedef struct Effects_Particle {
    /* 0x00 */ struct Effects_Particle *previous;
    /* 0x04 */ struct Effects_Particle *next;
    /* 0x08 */ Effects_ParticleMotion motion;
    /* 0x3C */ Effects_ParticleTextureAnimation textureAnimation;
    /* 0x5C */ int emitFlags;
    /* 0x60 */ u_char transparency;
    /* 0x61 */ u_int opaque61Bit0 : 4;
    /* 0x61 */ u_int reserved61Bit4 : 4;
    /* 0x62 */ short attractionTarget[3];
    /* 0x68 */ short lifetime;
    /* 0x6A */ u_char transformIndex;
    /* 0x6B */ u_char attractionTransform;
    /* 0x6C */ u_char endEvent;
    /* 0x6D */ u_char tickEvent;
    /* 0x6E */ char reserved6E[2];
    /* 0x70 */ int attraction;
    /* 0x74 */ u_char colorCurves[3];
    /* 0x77 */ u_char age;
} Effects_Particle;

// decomp: func_800FA098_arg0 (src/BATTLE/BATTLE.PRG/5BF94.h:270)
typedef struct Effects_ParticleDefinition {
    u_char animationId;
    u_char textureIndex;
    u_char endEvent;
    u_char tickEvent;
    u_int flags : 26;
    u_int transparencyCurve : 6;
    u_char positionCurve;
    u_char extentCurve;
    u_char directionCurve;
    u_char directionJitterCurve;
    u_char speedCurve;
    u_char forceCurve;
    u_char accelerationCurve;
    u_char attractionOffsetCurve;
    u_char attractionJitterCurve;
    u_char attractionCurve;
    u_char massCurve;
    u_char gravityScaleCurve;
    u_char lifetimeCurve;
    u_char countCurve;
    u_char intervalCurve;
    u_char innerExtentCurve;
    short positionRange[6];
    short extentRange[6];
    short innerExtentRange[2];
    short vectorRanges[8][6];
    short pairRanges[5][4];
    short countRange[2];
    short intervalRange[2];
    u_char rCurve;
    u_char gCurve;
    u_char bCurve;
    u_char reservedC7;
    u_char transformIndex;
    u_char attractionTransform;
    u_char ringSegments;
    u_char ringPositions;
} Effects_ParticleDefinition;
_Static_assert(sizeof(Effects_ParticleDefinition) == 0xCC, "Effects_ParticleDefinition must keep its PS1 layout");

// decomp: func_800D2904_curves (src/BATTLE/BATTLE.PRG/5BF94.h:280)
typedef union Effects_ParticleColorWord {
    int word;
    struct {
        u_char redCurve;
        u_char greenCurve;
        u_char blueCurve;
        u_char age;
    } fields;
} Effects_ParticleColorWord;

// layout: ledger (upstream merge) for func_800FA098_arg1
typedef struct Effects_ParticleEmitterScratch {
    /* 0x00 */ VECTOR opaque0;
    /* 0x10 */ VECTOR reserved10;
    /* 0x20 */ int reserved20;
    /* 0x24 */ u_int flags;
    /* 0x28 */ u_short savedFlags;
    /* 0x2A */ u_short reserved2A;
    /* 0x2C */ SVECTOR vector;
    /* 0x34 */ VECTOR aimPoint;
    /* 0x44 */ VECTOR spawnPoint;
    /* 0x54 */ u_char reserved54[16];
    /* 0x64 */ MATRIX aimMatrix;
    /* 0x84 */ u_char reserved84[20];
    /* 0x98 */ VECTOR position;
    /* 0xA8 */ VECTOR extent;
    /* 0xB8 */ VECTOR direction;
    /* 0xC8 */ VECTOR directionJitter;
    /* 0xD8 */ VECTOR innerExtent;
    /* 0xE8 */ VECTOR baseDirection;
    /* 0xF8 */ SVECTOR forceMin;
    /* 0x100 */ SVECTOR forceMax;
    /* 0x108 */ SVECTOR accelerationMin;
    /* 0x110 */ SVECTOR accelerationMax;
    /* 0x118 */ int speedRange[2];
    /* 0x120 */ int speed;
    /* 0x124 */ int massRange[2];
    /* 0x12C */ int gravityScaleRange[2];
    /* 0x134 */ int lifetimeRange[2];
    /* 0x13C */ SVECTOR attractionOffset;
    /* 0x144 */ SVECTOR attractionJitter;
    /* 0x14C */ int attractionRange[2];
    /* 0x154 */ Effects_ParticleColorWord colorCurves;
    /* 0x158 */ u_char reserved158[16];
    /* 0x168 */ int transformIndex;
    /* 0x16C */ int attractionTransform;
    /* 0x170 */ int sinAngle0;
    /* 0x174 */ int sinAngle1;
    /* 0x178 */ int cosAngle0;
    /* 0x17C */ int cosAngle1;
    /* 0x180 */ int transparency;
    /* 0x184 */ int ringSegments;
    /* 0x188 */ int ringPositions;
} Effects_ParticleEmitterScratch;

// decomp: func_800FA098_arg3 (src/EFFECT/particleSetup.h:27)
typedef struct Effects_ParticleQuadSet {
    u_char opaque0;
    u_char definitionIndex;
    short count;
    SVECTOR* controlPoints;
    int opaque8;
    int age;
    Effects_ParticleTextureAnimation textureAnimation;
} Effects_ParticleQuadSet;

// decomp: func_800FA098_arg3_2 (src/EFFECT/particleSetup.h:17)
typedef struct Effects_ParticleQuadWord {
    short reserved0;
    short reserved2;
} Effects_ParticleQuadWord;
_Static_assert(sizeof(Effects_ParticleQuadWord) == 0x4, "Effects_ParticleQuadWord must keep its PS1 layout");

// decomp: func_800FA76C_arg3 (src/EFFECT/trails.h:27)
typedef struct Effects_TrailQuadSet {
    u_char opaque0;
    u_char definitionIndex;
    u_char trailcount;
    u_char opaque3;
    Effects_Trail* trails;
    int curveSampleDistance;
    Effects_ParticleTextureAnimation textureAnimation;
    u_char sampledTransparencyCurve[0];
} Effects_TrailQuadSet;

// decomp: func_800FB4C0_t (src/BATTLE/BATTLE.PRG/5BF94.h:500)
typedef struct Effects_PluginEffectState {
    u_char reserved0[0xC];
    VECTOR position;
    u_char reserved1C[0x44];
    u_int transparency : 8;
    u_int variant : 4;
    u_int reserved61Bit4 : 20;
    u_char reserved64[20];
} Effects_PluginEffectState;
_Static_assert(sizeof(Effects_PluginEffectState) == 0x78, "Effects_PluginEffectState must keep its PS1 layout");

#endif // VS_EFFECTS_PARTICLES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_PARTICLES_H_DECLS)
#define VS_EFFECTS_PARTICLES_H_DECLS
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

extern int Effects_emissionCounter;
extern u_char Effects_pairedParticleColorPresets[2727][2][4];
extern u_char Effects_particleColorPresetTable[1088][5][4];
extern int Effects_particleCapacity;
extern Effects_Particle* Effects_freeParticles;
extern int Effects_usedParticleCount;
extern Effects_Particle* Effects_particlePool;
extern int Effects_particlePoolReferences;
extern int Effects_particleDrag;
extern Effects_ParticleGravity Effects_particleGravity;
void Effects_StepParticleEndpointTowardStart(Effects_ParticleEndpointView* particle_endpoints);
void Effects_ResetParticlePoolReferences(void);
void Effects_AcquireParticlePool(int count);
void Effects_ReleaseParticlePool(void);
Effects_Particle* Effects_AllocateEffectParticle(Effects_EffectInstance* effect_instance);
void Effects_ReleaseEffectParticle(Effects_Particle* particle_node, Effects_EffectInstance* effect_instance);
void Effects_ReleaseAllEffectParticles(Effects_EffectInstance* effect_instance);
void Effects_SetParticleSpawnPosition(Effects_ParticleEmitterScratch* emitter_scratch, Effects_Particle* particle_node);
void Effects_EmitParticleEvent(
    Effects_EffectInstance* effect_instance, int event, int transparency_override, int particle_variant, Effects_PluginEffectState* parent);
int Effects_TickEffectParticles(Effects_EffectInstance* effect_instance);
void Effects_SetParticleGravity(Effects_ParticleGravity* gravity_vector);
void Effects_SetParticleDrag(int* drag_value);
void Effects_IntegrateParticleMotion(Effects_Particle* node);
void Effects_ClearParticleTextureState(int* storage);
void Effects_InitializeParticleTextureAnimation(Effects_ParticleTextureAnimation* animation, int textureIndex, int programIndex);
void Effects_AdvanceParticleTextureAnimation(Effects_ParticleTextureAnimation* animation);

#endif // VS_EFFECTS_PARTICLES_H_DECLS
