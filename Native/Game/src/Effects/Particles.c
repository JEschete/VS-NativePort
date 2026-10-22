// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/Skeleton.h"
#include "Core/Math/Interpolation.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Math/Vector.h"
#include "Core/Memory/Heap.h"
#include "Core/Random.h"
#include "Effects/Package.h"
#include "Effects/ParticleRender.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

// decomp: D_800EC328 (D_800EC328@BATTLE/BATTLE.PRG)
int Effects_emissionCounter; /* contents: module image (ModuleData.c) */

// decomp: D_800EC330 (D_800EC330@BATTLE/BATTLE.PRG)
u_char Effects_pairedParticleColorPresets[2727][2][4]; /* contents: module image (ModuleData.c) */

// decomp: D_800EC368 (D_800EC368@BATTLE/BATTLE.PRG)
u_char Effects_particleColorPresetTable[1088][5][4]; /* contents: module image (ModuleData.c) */

// decomp: D_800F55F0 (D_800F55F0@BATTLE/BATTLE.PRG)
int Effects_particleCapacity;

// decomp: D_800F55F4 (D_800F55F4@BATTLE/BATTLE.PRG)
Effects_Particle* Effects_freeParticles;

// decomp: D_800F55F8 (D_800F55F8@BATTLE/BATTLE.PRG)
int Effects_usedParticleCount;

// decomp: D_800F55FC (D_800F55FC@BATTLE/BATTLE.PRG)
Effects_Particle* Effects_particlePool;

// decomp: D_800F5600 (D_800F5600@BATTLE/BATTLE.PRG)
int Effects_particlePoolReferences;

// decomp: D_800F5618 (D_800F5618@BATTLE/BATTLE.PRG)
int Effects_particleDrag;

// decomp: D_800F5620 (D_800F5620@BATTLE/BATTLE.PRG)
Effects_ParticleGravity Effects_particleGravity;

// decomp: BATTLE.PRG func_800CFAAC (src/BATTLE/BATTLE.PRG/5BF94.c:5770)
void Effects_StepParticleEndpointTowardStart(Effects_ParticleEndpointView* arg0)
{
    int dx;
    int dy;
    int sx;
    int sy;

    if (*(int*)&arg0->endX == *(int*)&arg0->startX) {
        return;
    }
    sx = -1;
    sy = -1;
    dx = arg0->startX - arg0->endX;
    dy = arg0->startY - arg0->endY;
    if (dx < 0) {
        dx = -dx;
        sx = 1;
    }
    if (dy < 0) {
        dy = -dy;
        sy = 1;
    }
    if (dy < dx) {
        arg0->startX += sx;
        if (dx < dy * 2) {
            arg0->startY += sy;
        }
    } else {
        arg0->startY += sy;
        if (dy < dx * 2) {
            arg0->startX += sx;
        }
    }
}

// decomp: BATTLE.PRG func_800D268C (src/BATTLE/BATTLE.PRG/5BF94.c:6777)
void Effects_ResetParticlePoolReferences(void) { Effects_particlePoolReferences = 0; }

// decomp: BATTLE.PRG func_800D2698 (src/BATTLE/BATTLE.PRG/5BF94.c:6779)
void Effects_AcquireParticlePool(int count)
{
    int i;

    if (Effects_particlePoolReferences != 0) {
        ++Effects_particlePoolReferences;
        return;
    }

    Effects_particlePool = Core_Memory_AllocateFromHighEnd(sizeof *Effects_particlePool * count);
    Effects_particlePool[0].previous = NULL;
    Effects_particlePool[0].next = &Effects_particlePool[1];

    for (i = 1; i < count - 1; ++i) {
        Effects_particlePool[i].previous = &Effects_particlePool[i - 1];
        Effects_particlePool[i].next = &Effects_particlePool[i + 1];
    }

    Effects_usedParticleCount = 0;
    Effects_particleCapacity = count;
    Effects_particlePoolReferences = 1;
    Effects_particlePool[i].previous = &Effects_particlePool[i - 1];
    Effects_particlePool[i].next = NULL;
    Effects_freeParticles = Effects_particlePool;
}

// decomp: BATTLE.PRG func_800D278C (src/BATTLE/BATTLE.PRG/5BF94.c:6805)
void Effects_ReleaseParticlePool(void)
{
    if (Effects_particlePoolReferences == 1) {
        if (Effects_particlePool != NULL) {
            Core_Memory_FreeFromHighEnd(Effects_particlePool);
            Effects_particlePool = NULL;
        }
        Effects_particlePoolReferences = 0;
        return;
    }
    --Effects_particlePoolReferences;
}

// decomp: BATTLE.PRG func_800D27F0 (src/BATTLE/BATTLE.PRG/5BF94.c:6818)
Effects_Particle* Effects_AllocateEffectParticle(Effects_EffectInstance* arg0)
{
    Effects_Particle* node = Effects_freeParticles;

    if (node == NULL) {
        Effects_IgnoreRuntimeDiagnostic(0x14);
    }

    Effects_ClearParticleTextureState((int*)&node->textureAnimation);

    node->age = 0xFF;
    node->previous = NULL;
    ++Effects_usedParticleCount;
    Effects_freeParticles = node->next;
    node->next = arg0->particles;

    if (arg0->particles != NULL) {
        arg0->particles->previous = node;
    }

    arg0->particles = node;
    ++arg0->particleCount;

    do {
    } while (0);

    return node;
}

// decomp: BATTLE.PRG func_800D2888 (src/BATTLE/BATTLE.PRG/5BF94.c:6847)
void Effects_ReleaseEffectParticle(Effects_Particle* arg0, Effects_EffectInstance* arg1)
{
    if (arg0->next != NULL) {
        arg0->next->previous = arg0->previous;
    }

    if (arg0->previous != NULL) {
        arg0->previous->next = arg0->next;
    } else {
        arg1->particles = arg0->next;
    }

    arg0->next = Effects_freeParticles;
    Effects_freeParticles = arg0;
    --Effects_usedParticleCount;
    --arg1->particleCount;
}

// decomp: BATTLE.PRG func_800D2904 (src/BATTLE/BATTLE.PRG/5BF94.c:6865)
void Effects_ReleaseAllEffectParticles(Effects_EffectInstance* arg0)
{
    Effects_Particle* temp_a1;
    Effects_Particle* var_a0;

    temp_a1 = arg0->particles;
    var_a0 = temp_a1;

    if (temp_a1 == 0) {
        return;
    }

    while (var_a0->next != 0) {
        var_a0 = var_a0->next;
    }

    var_a0->next = Effects_freeParticles;
    Effects_freeParticles = temp_a1;
    Effects_usedParticleCount -= arg0->particleCount;
    arg0->particleCount = 0;
}

// decomp: BATTLE.PRG func_800D2A38 (src/BATTLE/BATTLE.PRG/5BF94.c:6915)
void Effects_SetParticleSpawnPosition(Effects_ParticleEmitterScratch* arg0, Effects_Particle* arg1)
{
    if (arg0->flags & 0x40000) {
        Core_Gte_ApplyRotMatrix(&arg0->vector, &arg0->aimPoint);
    } else {
        arg0->aimPoint.vx = arg0->vector.vx;
        arg0->aimPoint.vy = arg0->vector.vy;
        arg0->aimPoint.vz = arg0->vector.vz;
    }
    arg1->motion.position.vx = arg0->position.vx + arg0->aimPoint.vx * ONE;
    arg1->motion.position.vy = arg0->position.vy + arg0->aimPoint.vy * ONE;
    arg1->motion.position.vz = arg0->position.vz + arg0->aimPoint.vz * ONE;
}

// decomp: src/BATTLE/BATTLE.PRG/5BF94.c:6930
#define RAND_UNIFORM_WIDE(a, b)                                                    \
    ((a) == (b)  ? (a)                                                             \
     : (b) < (a) ? (Core_Random_Rand() + (Core_Random_Rand() << 16)) % ((a) - (b) + 1) + (b)               \
                 : (Core_Random_Rand() + (Core_Random_Rand() << 16)) % ((b) - (a) + 1) + (a))
// decomp: BATTLE.PRG func_800D2ADC (src/BATTLE/BATTLE.PRG/5BF94.c:6935)
void Effects_EmitParticleEvent(
    Effects_EffectInstance* arg0, int event, int arg2, int arg3, Effects_PluginEffectState* parent)
{
    int _[4] __attribute__((unused));
    int frame;
    int count;
    int i;
    int sample;
    int step;
    int angle;
    int x;
    int z;
    int value;
    Effects_Particle* node;
    Effects_Particle* first;
    MATRIX* matrix;
    int prevX;
    int prevZ;
    int range;
    Effects_ParticleEmitterScratch* scratch = (Effects_ParticleEmitterScratch*)((uintptr_t)Core_ScratchBase() + 0x1D0);
    Effects_ParticleDefinition* def = &Effects_package->block5Data->definitions[event];

    scratch->flags = def->flags;
    scratch->savedFlags = scratch->flags;
    scratch->transformIndex = def->transformIndex;
    scratch->attractionTransform = def->attractionTransform;

    if (arg2 != 0) {
        scratch->transparency = arg2;
    } else if (def->transparencyCurve != 0) {
        scratch->transparency = def->transparencyCurve;
    } else if (parent != NULL) {
        scratch->transparency = parent->transparency;
    } else {
        scratch->transparency = 4;
    }

    if (parent != NULL && arg3 == 0) {
        arg3 = parent->variant & 0xF;
    }

    frame = arg0->frame;
    sample = def->intervalCurve;
    if (frame % Core_Math_InterpolateShortPair(def->intervalRange, Effects_curveSamples[sample]) != 0) {
        return;
    }

    Core_Gte_ClearTranslation();

    matrix = &arg0->transforms[scratch->transformIndex].world;
    sample = def->countCurve;
    count = Core_Math_InterpolateShortPair(def->countRange, Effects_curveSamples[sample]);
    if (count + Effects_usedParticleCount >= Effects_particleCapacity) {
        count = Effects_particleCapacity - Effects_usedParticleCount;
    }

    if (count <= 0) {
        Core_Debugf("effects", "particle emission skipped: count=%d used=%d capacity=%d",
            count, Effects_usedParticleCount, Effects_particleCapacity);
        return;
    }

    for (i = 0; i < count; ++i) {
        node = Effects_AllocateEffectParticle(arg0);
        node->transformIndex = scratch->transformIndex;
        node->attractionTransform = scratch->attractionTransform;
        Effects_InitializeParticleTextureAnimation(&node->textureAnimation, def->textureIndex, def->animationId);
    }

    sample = def->positionCurve;
    Core_Math_InterpolatePairsToWideVector(def->positionRange, Effects_curveSamples[sample], &scratch->position);
    first = node;

    switch (scratch->flags & 7) {
    case 0:
        break;
    case 1:
        Core_Math_AddWideAndShortVectors(&scratch->position, Effects_effectOrigin, &scratch->position);
        break;
    case 2:
        scratch->position.vx += matrix->t[0];
        scratch->position.vy += matrix->t[1];
        scratch->position.vz += matrix->t[2];
        break;
    case 4:
        if (parent != NULL) {
            scratch->position.vx += parent->position.vx >> 12;
            scratch->position.vy += parent->position.vy >> 12;
            scratch->position.vz += parent->position.vz >> 12;
        }
        break;
    }

    scratch->position.vx <<= 12;
    scratch->position.vy <<= 12;
    scratch->position.vz <<= 12;

    sample = def->extentCurve;
    Core_Math_InterpolatePairsToWideVector(def->extentRange, Effects_curveSamples[sample], &scratch->extent);
    node = first;
    sample = def->innerExtentCurve;
    sample = Core_Math_InterpolateShortPair(def->innerExtentRange, Effects_curveSamples[sample]);
    Core_Gte_SetRotMatrix(matrix);
    Core_Gte_ClearTranslation();

    switch (scratch->flags & 0x38) {
    case 0:
        scratch->innerExtent.vx = (scratch->extent.vx * sample) >> 12;
        scratch->innerExtent.vy = (scratch->extent.vy * sample) >> 12;
        scratch->innerExtent.vz = (scratch->extent.vz * sample) >> 12;
        for (i = 0; i < count; ++i) {
            angle = Core_Random_Rand();
            scratch->sinAngle0 = Core_Gte_Sin(angle);
            scratch->cosAngle0 = Core_Gte_Cos(angle);
            angle = Core_Random_Rand();
            scratch->sinAngle1 = Core_Gte_Sin(angle);
            scratch->cosAngle1 = Core_Gte_Cos(angle);
            switch (scratch->flags & 0x1800) {
            case 0:
                scratch->vector.vx =
                    Core_Random_RandomRangeExclusive(scratch->extent.vx, scratch->innerExtent.vx);
                scratch->vector.vy =
                    Core_Random_RandomRangeExclusive(scratch->extent.vy, scratch->innerExtent.vy);
                scratch->vector.vz =
                    Core_Random_RandomRangeExclusive(scratch->extent.vz, scratch->innerExtent.vz);
                goto scale0;
            case 0x800:
            case 0x1000:
                Core_Math_NarrowWideVector(&scratch->extent, &scratch->vector);
            scale0:
                scratch->vector.vx =
                    (((scratch->vector.vx * scratch->cosAngle0) >> 12) * scratch->cosAngle1)
                    >> 12;
                scratch->vector.vy =
                    (((scratch->vector.vy * scratch->cosAngle0) >> 12) * scratch->sinAngle1)
                    >> 12;
                scratch->vector.vz = (scratch->vector.vz * scratch->sinAngle0) >> 12;
                break;
            }
            Effects_SetParticleSpawnPosition(scratch, node);
            node = node->next;
        }
        break;

    case 8:
        scratch->innerExtent.vx = (scratch->extent.vx * sample) >> 12;
        scratch->innerExtent.vy = (scratch->extent.vy * sample) >> 12;
        scratch->innerExtent.vz = (scratch->extent.vz * sample) >> 12;
        for (i = 0; i < count; ++i) {
            switch (scratch->flags & 0x1800) {
            case 0:
                angle = Core_Random_Rand();
                if (angle & 1) {
                    scratch->vector.vx =
                        Core_Random_RandomRangeExclusive(scratch->extent.vx, scratch->innerExtent.vx);
                } else {
                    scratch->vector.vx =
                        Core_Random_RandomRangeExclusive(-scratch->extent.vx, -scratch->innerExtent.vx);
                }
                if (angle & 2) {
                    scratch->vector.vy =
                        Core_Random_RandomRangeExclusive(scratch->extent.vy, scratch->innerExtent.vy);
                } else {
                    scratch->vector.vy =
                        Core_Random_RandomRangeExclusive(-scratch->extent.vy, -scratch->innerExtent.vy);
                }
                if (angle & 4) {
                    scratch->vector.vz =
                        Core_Random_RandomRangeExclusive(scratch->extent.vz, scratch->innerExtent.vz);
                } else {
                    scratch->vector.vz =
                        Core_Random_RandomRangeExclusive(-scratch->extent.vz, -scratch->innerExtent.vz);
                }
                break;
            case 0x800:
                switch (Core_Random_Rand() % 6) {
                case 0:
                    scratch->vector.vx = scratch->extent.vx;
                    goto face_x;
                case 1:
                    scratch->vector.vx = -scratch->extent.vx;
                face_x:
                    scratch->vector.vy =
                        Core_Random_RandomRangeExclusive(scratch->extent.vy, -scratch->extent.vy);
                    scratch->vector.vz =
                        Core_Random_RandomRangeExclusive(scratch->extent.vz, -scratch->extent.vz);
                    break;
                case 2:
                    scratch->vector.vy = scratch->extent.vy;
                    goto face_y;
                case 3:
                    scratch->vector.vy = -scratch->extent.vy;
                face_y:
                    scratch->vector.vx =
                        Core_Random_RandomRangeExclusive(scratch->extent.vx, -scratch->extent.vx);
                    scratch->vector.vz =
                        Core_Random_RandomRangeExclusive(scratch->extent.vz, -scratch->extent.vz);
                    break;
                case 4:
                    scratch->vector.vz = scratch->extent.vz;
                    goto face_z;
                case 5:
                    scratch->vector.vz = -scratch->extent.vz;
                face_z:
                    scratch->vector.vx =
                        Core_Random_RandomRangeExclusive(scratch->extent.vx, -scratch->extent.vx);
                    scratch->vector.vy =
                        Core_Random_RandomRangeExclusive(scratch->extent.vy, -scratch->extent.vy);
                    break;
                }
                break;
            case 0x1000:
                Effects_emissionCounter = (Effects_emissionCounter + 1) & 7;
                scratch->vector.vx =
                    (Effects_emissionCounter & 1) ? scratch->extent.vx : -scratch->extent.vx;
                scratch->vector.vy =
                    (Effects_emissionCounter & 2) ? scratch->extent.vy : -scratch->extent.vy;
                scratch->vector.vz =
                    (Effects_emissionCounter & 4) ? scratch->extent.vz : -scratch->extent.vz;
                break;
            }
            Effects_SetParticleSpawnPosition(scratch, node);
            node = node->next;
        }
        break;

    case 16:
        scratch->innerExtent.vx = (scratch->extent.vx * sample) >> 12;
        scratch->innerExtent.vy = scratch->extent.vy;
        scratch->innerExtent.vz = (scratch->extent.vz * sample) >> 12;
        for (i = 0; i < count; ++i) {
            angle = Core_Random_Rand();
            scratch->sinAngle0 = Core_Gte_Sin(angle);
            scratch->cosAngle0 = Core_Gte_Cos(angle);
            switch (scratch->flags & 0x1800) {
            case 0:
                scratch->vector.vx =
                    Core_Random_RandomRangeExclusive(scratch->extent.vx, scratch->innerExtent.vx);
                scratch->vector.vz =
                    Core_Random_RandomRangeExclusive(scratch->extent.vz, scratch->innerExtent.vz);
                goto scale16;
            case 0x800:
            case 0x1000:
                scratch->vector.vx = scratch->extent.vx;
                scratch->vector.vz = scratch->extent.vz;
            scale16:
                scratch->vector.vy = Core_Math_SquareRootInteger(
                    RAND_UNIFORM_WIDE(scratch->extent.vy * scratch->extent.vy, 0));
                if (scratch->extent.vy == 0) {
                    // BATTLE.PRG, DIV at 0x800D34BC/0x800D34F8:
                    // zero height makes both dividends zero; the original LO is -1.
                    scratch->vector.vx = scratch->vector.vz = -1;
                } else {
                    scratch->vector.vx =
                        (((scratch->vector.vx * scratch->cosAngle0) >> 12) * scratch->vector.vy)
                        / scratch->extent.vy;
                    scratch->vector.vz =
                        (((scratch->vector.vz * scratch->sinAngle0) >> 12) * scratch->vector.vy)
                        / scratch->extent.vy;
                }
                break;
            }
            Effects_SetParticleSpawnPosition(scratch, node);
            node = node->next;
        }
        break;

    case 24:
        scratch->innerExtent.vx = (scratch->extent.vx * sample) >> 12;
        scratch->innerExtent.vz = (scratch->extent.vz * sample) >> 12;
        for (i = 0; i < count; ++i) {
            angle = Core_Random_Rand();
            scratch->sinAngle0 = Core_Gte_Sin(angle);
            scratch->cosAngle0 = Core_Gte_Cos(angle);
            switch (scratch->flags & 0x1800) {
            case 0:
                scratch->vector.vx =
                    Core_Math_SquareRootInteger(RAND_UNIFORM_WIDE(scratch->extent.vx * scratch->extent.vx,
                        scratch->innerExtent.vx * scratch->innerExtent.vx));
                scratch->vector.vz =
                    Core_Math_SquareRootInteger(RAND_UNIFORM_WIDE(scratch->extent.vz * scratch->extent.vz,
                        scratch->innerExtent.vz * scratch->innerExtent.vz));
                goto scale24;
            case 0x800:
            case 0x1000:
                scratch->vector.vx = scratch->extent.vx;
                scratch->vector.vz = scratch->extent.vz;
            scale24:
                scratch->vector.vx = (scratch->vector.vx * scratch->cosAngle0) >> 12;
                scratch->vector.vy = Core_Random_RandomRangeExclusive(scratch->extent.vy, 0);
                scratch->vector.vz = (scratch->vector.vz * scratch->sinAngle0) >> 12;
                break;
            }
            Effects_SetParticleSpawnPosition(scratch, node);
            node = node->next;
        }
        break;

    case 32:
        scratch->innerExtent.vx = (scratch->extent.vx * sample) >> 12;
        scratch->innerExtent.vz = (scratch->extent.vz * sample) >> 12;
        for (i = 0; i < count; ++i) {
            angle = Core_Random_Rand();
            scratch->sinAngle0 = Core_Gte_Sin(angle);
            scratch->cosAngle0 = Core_Gte_Cos(angle);
            switch (scratch->flags & 0x1800) {
            case 0:
                scratch->vector.vx =
                    Core_Math_SquareRootInteger(RAND_UNIFORM_WIDE(scratch->extent.vx * scratch->extent.vx,
                        scratch->innerExtent.vx * scratch->innerExtent.vx));
                scratch->vector.vz =
                    Core_Math_SquareRootInteger(RAND_UNIFORM_WIDE(scratch->extent.vz * scratch->extent.vz,
                        scratch->innerExtent.vz * scratch->innerExtent.vz));
                goto scale32;
            case 0x800:
            case 0x1000:
                scratch->vector.vx = scratch->extent.vx;
                scratch->vector.vz = scratch->extent.vz;
            scale32:
                scratch->vector.vy = 0;
                scratch->vector.vx = (scratch->vector.vx * scratch->cosAngle0) >> 12;
                scratch->vector.vz = (scratch->vector.vz * scratch->sinAngle0) >> 12;
                break;
            }
            Effects_SetParticleSpawnPosition(scratch, node);
            node = node->next;
        }
        break;

    case 40:
        scratch->ringSegments = 4;
        scratch->ringPositions = 4;
        goto ring;

    case 48:
        scratch->ringSegments = def->ringSegments;
        if (scratch->ringSegments == 0) {
            scratch->ringSegments = 1;
        }
        scratch->ringPositions = def->ringPositions;
        if (scratch->ringPositions == 0) {
            scratch->ringPositions = scratch->ringSegments;
        }
    ring:
        step = ONE / scratch->ringSegments;
        if ((scratch->flags & 0x1800) != 0x1000) {
            Effects_emissionCounter %= scratch->ringPositions;
            angle = Effects_emissionCounter * step + 0x200;
            prevX = (Core_Gte_Cos(angle) * scratch->extent.vx) >> 12;
            prevZ = (Core_Gte_Sin(angle) * scratch->extent.vz) >> 12;
        }
        for (i = 0; i < count; ++i) {
            range = 0x1194 - sample;
            Effects_emissionCounter = (Effects_emissionCounter + 1) % scratch->ringPositions;
            angle = Effects_emissionCounter * step + 0x200;
            x = (Core_Gte_Cos(angle) * scratch->extent.vx) >> 12;
            z = (Core_Gte_Sin(angle) * scratch->extent.vz) >> 12;
            if ((scratch->flags & 0x1800) == 0x1000) {
                scratch->vector.vx = x;
                scratch->vector.vz = z;
            } else {
                value = Core_Random_Rand() >> 3;
                scratch->vector.vx = (((x - prevX) * value) >> 12) + prevX;
                scratch->vector.vz = (((z - prevZ) * value) >> 12) + prevZ;
                prevX = x;
                prevZ = z;
                if ((scratch->flags & 0x1800) != 0x800) {
                    value = Core_Gte_SquareRoot12(((Core_Random_Rand() * range) >> 15) + sample);
                    if (value > ONE) {
                        value = ONE;
                    }
                    scratch->vector.vx = (scratch->vector.vx * value) >> 12;
                    scratch->vector.vz = (scratch->vector.vz * value) >> 12;
                }
            }
            scratch->vector.vy = 0;
            Effects_SetParticleSpawnPosition(scratch, node);
            node = node->next;
        }
        break;
    }

    sample = def->speedCurve;
    Core_Math_InterpolatePairsToPlaneVector(def->pairRanges[2], Effects_curveSamples[sample], scratch->speedRange);
    scratch->speedRange[0] <<= 5;
    scratch->speedRange[1] <<= 5;
    sample = def->directionJitterCurve;
    Core_Math_InterpolatePairsToWideVector(def->vectorRanges[1], Effects_curveSamples[sample], &scratch->directionJitter);
    sample = def->directionCurve;
    Core_Math_InterpolatePairsToWideVector(def->vectorRanges[0], Effects_curveSamples[sample], &scratch->baseDirection);
    node = first;

    switch (scratch->flags & 0xE000) {
    case 0x4000:
        if (arg0->context.emitFrom.type == 4) {
            Actors_ActorBonePosition(((u_char*)&arg0->context.emitFrom.value)[0],
                ((u_char*)&arg0->context.emitFrom.value)[2], &scratch->vector, 0);
        } else {
            scratch->vector = arg0->context.emitFrom.value.position;
        }
        goto aim;
    case 0x6000:
        if (arg0->context.emitTo.type == 4) {
            Actors_ActorBonePosition(((u_char*)&arg0->context.emitTo.value)[0],
                ((u_char*)&arg0->context.emitTo.value)[2], &scratch->vector, 0);
        } else {
            scratch->vector = arg0->context.emitTo.value.position;
        }
    aim:
        Core_Math_WidenShortVector(&scratch->vector, &scratch->aimPoint);
        scratch->spawnPoint.vx = scratch->position.vx >> 12;
        scratch->spawnPoint.vy = scratch->position.vy >> 12;
        scratch->spawnPoint.vz = scratch->position.vz >> 12;
        Core_Math_BuildLookAtOrientation(&scratch->spawnPoint, &scratch->aimPoint, &scratch->aimMatrix);
        matrix = &scratch->aimMatrix;
    case 0:
        for (i = 0; i < count; ++i) {
            scratch->vector.vx =
                scratch->baseDirection.vx
                + Core_Random_RandomRangeExclusive(scratch->directionJitter.vx, -scratch->directionJitter.vx);
            scratch->vector.vy =
                scratch->baseDirection.vy
                + Core_Random_RandomRangeExclusive(scratch->directionJitter.vy, -scratch->directionJitter.vy);
            scratch->vector.vz =
                scratch->baseDirection.vz
                + Core_Random_RandomRangeExclusive(scratch->directionJitter.vz, -scratch->directionJitter.vz);
            Core_Gte_RotMatrix(&scratch->vector, (MATRIX*)scratch);
            Core_Gte_SetRotMatrix(scratch);
            *(int*)&scratch->vector = 0;
            scratch->vector.vz = ONE;
            Core_Gte_RotTrans(&scratch->vector, &scratch->direction, &scratch->direction.pad);
            if (scratch->flags & 0x40000) {
                Core_Math_NarrowWideVector(&scratch->direction, &scratch->vector);
                Core_Gte_SetRotMatrix(matrix);
                Core_Gte_ApplyRotMatrix(&scratch->vector, &scratch->direction);
            }
            scratch->speed =
                Core_Random_RandomRangeExclusive(scratch->speedRange[0], scratch->speedRange[1]);
            node->motion.velocity.vx = (scratch->direction.vx * scratch->speed) >> 12;
            node->motion.velocity.vy = (scratch->direction.vy * scratch->speed) >> 12;
            node->motion.velocity.vz = (scratch->direction.vz * scratch->speed) >> 12;
            node = node->next;
        }
        break;
    case 0x8000:
        for (i = 0; i < count; ++i) {
            scratch->vector.vx =
                scratch->baseDirection.vx
                + Core_Random_RandomRangeExclusive(scratch->directionJitter.vx, -scratch->directionJitter.vx);
            scratch->vector.vy =
                scratch->baseDirection.vy
                + Core_Random_RandomRangeExclusive(scratch->directionJitter.vy, -scratch->directionJitter.vy);
            scratch->vector.vz =
                scratch->baseDirection.vz
                + Core_Random_RandomRangeExclusive(scratch->directionJitter.vz, -scratch->directionJitter.vz);
            Core_Gte_RotMatrix(&scratch->vector, (MATRIX*)scratch);
            Core_Gte_SetRotMatrix(scratch);
            *(int*)&scratch->vector = 0;
            scratch->vector.vz = ONE;
            Core_Gte_RotTrans(&scratch->vector, &scratch->direction, &scratch->direction.pad);
            Core_Gte_SetRotMatrix(matrix);
            Core_Math_NarrowWideVector(&scratch->direction, &scratch->vector);
            Core_Gte_RotTrans(&scratch->vector, &scratch->direction, &scratch->direction.pad);
            scratch->speed =
                Core_Random_RandomRangeExclusive(scratch->speedRange[0], scratch->speedRange[1]);
            node->motion.velocity.vx = (scratch->direction.vx * scratch->speed) >> 12;
            node->motion.velocity.vy = (scratch->direction.vy * scratch->speed) >> 12;
            node->motion.velocity.vz = (scratch->direction.vz * scratch->speed) >> 12;
            node = node->next;
        }
        break;
    case 0x2000:
        for (i = 0; i < count; ++i) {
            scratch->direction.vx = (scratch->position.vx - node->motion.position.vx) >> 12;
            scratch->direction.vy = (scratch->position.vy - node->motion.position.vy) >> 12;
            scratch->direction.vz = (scratch->position.vz - node->motion.position.vz) >> 12;
            scratch->speed =
                Core_Random_RandomRangeExclusive(scratch->speedRange[0], scratch->speedRange[1]);
            if (scratch->direction.vx != 0 || scratch->direction.vy != 0
                || scratch->direction.vz != 0) {
                Core_Gte_VectorNormal(&scratch->direction, &scratch->direction);
                node->motion.velocity.vx = (scratch->direction.vx * scratch->speed) >> 12;
                node->motion.velocity.vy = (scratch->direction.vy * scratch->speed) >> 12;
                node->motion.velocity.vz = (scratch->direction.vz * scratch->speed) >> 12;
            } else {
                node->motion.velocity.vy = scratch->speed;
                node->motion.velocity.vz = 0;
                node->motion.velocity.vx = 0;
            }
            node = node->next;
        }
        break;
    }

    sample = def->forceCurve;
    Core_Math_InterpolatePairsToShortVector(def->vectorRanges[2], Effects_curveSamples[sample], &scratch->forceMin);
    Core_Math_InterpolatePairsToShortVector(def->vectorRanges[3], Effects_curveSamples[sample], &scratch->forceMax);
    sample = def->accelerationCurve;
    Core_Math_InterpolatePairsToShortVector(def->vectorRanges[4], Effects_curveSamples[sample], &scratch->accelerationMin);
    Core_Math_InterpolatePairsToShortVector(def->vectorRanges[5], Effects_curveSamples[sample], &scratch->accelerationMax);
    node = first;
    for (i = 0; i < count; ++i) {
        node->motion.force.vx = Core_Random_RandomRangeExclusive(scratch->forceMin.vx, scratch->forceMax.vx);
        node->motion.force.vy = Core_Random_RandomRangeExclusive(scratch->forceMin.vy, scratch->forceMax.vy);
        node->motion.force.vz = Core_Random_RandomRangeExclusive(scratch->forceMin.vz, scratch->forceMax.vz);
        node->motion.acceleration.vx = Core_Random_RandomRangeExclusive(scratch->accelerationMin.vx, scratch->accelerationMax.vx);
        node->motion.acceleration.vy = Core_Random_RandomRangeExclusive(scratch->accelerationMin.vy, scratch->accelerationMax.vy);
        node->motion.acceleration.vz = Core_Random_RandomRangeExclusive(scratch->accelerationMin.vz, scratch->accelerationMax.vz);
        node = node->next;
    }

    sample = def->massCurve;
    Core_Math_InterpolatePairsToPlaneVector(def->pairRanges[0], Effects_curveSamples[sample], scratch->massRange);
    sample = def->gravityScaleCurve;
    Core_Math_InterpolatePairsToPlaneVector(def->pairRanges[1], Effects_curveSamples[sample], scratch->gravityScaleRange);
    sample = def->lifetimeCurve;
    Core_Math_InterpolatePairsToPlaneVector(def->pairRanges[4], Effects_curveSamples[sample], scratch->lifetimeRange);
    node = first;
    for (i = 0; i < count; ++i) {
        node->motion.mass = Core_Random_RandomRangeExclusive(scratch->massRange[0], scratch->massRange[1]);
        node->motion.gravityScale = Core_Random_RandomRangeExclusive(scratch->gravityScaleRange[0], scratch->gravityScaleRange[1]);
        node->lifetime = Core_Random_RandomRangeExclusive(scratch->lifetimeRange[0], scratch->lifetimeRange[1]);
        node = node->next;
    }

    sample = def->attractionOffsetCurve;
    Core_Math_InterpolatePairsToShortVector(def->vectorRanges[6], Effects_curveSamples[sample], &scratch->attractionOffset);
    sample = def->attractionJitterCurve;
    Core_Math_InterpolatePairsToShortVector(def->vectorRanges[7], Effects_curveSamples[sample], &scratch->attractionJitter);
    node = first;

    switch (scratch->flags & 0x1C0) {
    case 0:
    case 0x40:
        for (i = 0; i < count; ++i) {
            node->attractionTarget[0] = scratch->attractionOffset.vx;
            node->attractionTarget[1] = scratch->attractionOffset.vy;
            node->attractionTarget[2] = scratch->attractionOffset.vz;
            node = node->next;
        }
        break;
    case 0xC0:
        for (i = 0; i < count; ++i) {
            node->attractionTarget[0] =
                arg0->transforms[scratch->attractionTransform].world.t[0] + scratch->attractionOffset.vx
                + Core_Random_RandomRangeExclusive(scratch->attractionJitter.vx, -scratch->attractionJitter.vy);
            node->attractionTarget[1] =
                arg0->transforms[scratch->attractionTransform].world.t[1] + scratch->attractionOffset.vy
                + Core_Random_RandomRangeExclusive(scratch->attractionJitter.vy, -scratch->attractionJitter.vy);
            node->attractionTarget[2] =
                arg0->transforms[scratch->attractionTransform].world.t[2] + scratch->attractionOffset.vz
                + Core_Random_RandomRangeExclusive(scratch->attractionJitter.vz, -scratch->attractionJitter.vz);
            node = node->next;
        }
        break;
    case 0x80:
    case 0x140:
        for (i = 0; i < count; ++i) {
            node->attractionTarget[2] = 0;
            node->attractionTarget[1] = 0;
            node->attractionTarget[0] = 0;
            node = node->next;
        }
        break;
    }

    sample = def->attractionCurve;
    Core_Math_InterpolatePairsToPlaneVector(def->pairRanges[3], Effects_curveSamples[sample], scratch->attractionRange);
    node = first;
    scratch->colorCurves.word = *(int*)&def->rCurve;
    scratch->colorCurves.fields.age = node->age;
    for (i = 0; i < count; ++i) {
        node->emitFlags = scratch->flags;
        value = Core_Random_RandomRangeExclusive(scratch->attractionRange[0], scratch->attractionRange[1]);
        *(int*)node->colorCurves = scratch->colorCurves.word;
        node->attraction = value;
        *(u_short*)&node->endEvent = *(u_short*)&def->endEvent;
        node->transparency = scratch->transparency;
        node->opaque61Bit0 = arg3;
        node = node->next;
    }
}
#undef RAND_UNIFORM_WIDE

// decomp: BATTLE.PRG func_800D4C18 (src/BATTLE/BATTLE.PRG/5BF94.c:7658)
int Effects_TickEffectParticles(Effects_EffectInstance* arg0)
{
    Effects_Particle* next = arg0->particles;
    Effects_Particle* node;

    Effects_IntegrateParticleMotion(next);
    Effects_DrawEffectParticleList(arg0->particles);
    node = next;
    while (next != NULL) {
        next = node->next;
        ++node->age;
        if (node->tickEvent != 0) {
            Effects_EmitParticleEvent(arg0, node->tickEvent - 1, 0, 0, (void*)node);
        }
        if (node->lifetime == -1) {
            if (node->textureAnimation.frameTimer == 0) {
                if (node->endEvent != 0) {
                    Effects_EmitParticleEvent(arg0, node->endEvent - 1, 0, 0, (void*)node);
                }
                Effects_ReleaseEffectParticle(node, arg0);
            }
        } else if (--node->lifetime == 0) {
            if (node->endEvent != 0) {
                Effects_EmitParticleEvent(arg0, node->endEvent - 1, 0, 0, (void*)node);
            }
            Effects_ReleaseEffectParticle(node, arg0);
        }
        node = next;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800D5260 (src/BATTLE/BATTLE.PRG/5BF94.c:7826)
void Effects_SetParticleGravity(Effects_ParticleGravity* arg0) { Effects_particleGravity = *arg0; }

// decomp: BATTLE.PRG func_800D5294 (src/BATTLE/BATTLE.PRG/5BF94.c:7828)
void Effects_SetParticleDrag(int* arg0) { Effects_particleDrag = *arg0; }

// decomp: BATTLE.PRG func_800D52A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7830)
void Effects_IntegrateParticleMotion(Effects_Particle* node)
{
    VECTOR direction;
    Effects_ParticleMotionView* state;
    int damping;
    int drag = Effects_particleDrag;
    int gravityX = Effects_particleGravity.x;
    int gravityY = Effects_particleGravity.y;
    int gravityZ = Effects_particleGravity.z;

    for (; node != NULL; node = node->next) {
        state = (Effects_ParticleMotionView*)&node->motion.mass;
        damping = node->motion.mass - drag;
        state->vx = (state->vx * damping + (state->fx << 12)) / node->motion.mass
                  + ((gravityX * state->gravityScale) >> 12);
        state->x += state->vx;
        state->vy = (state->vy * damping + (state->fy << 12)) / node->motion.mass
                  + ((gravityY * state->gravityScale) >> 12);
        state->y += state->vy;
        state->vz = (state->vz * damping + (state->fz << 12)) / node->motion.mass
                  + ((gravityZ * state->gravityScale) >> 12);
        state->z += state->vz;
        damping = ((Effects_ParticleAttractionView*)node)->attraction;
        if (damping != 0) {
            direction.vx = ((Effects_ParticleAttractionView*)node)->targetX - (state->x >> 12);
            direction.vy = ((Effects_ParticleAttractionView*)node)->targetY - (state->y >> 12);
            direction.vz = ((Effects_ParticleAttractionView*)node)->targetZ - (state->z >> 12);
            if (direction.vx != 0 || direction.vy != 0 || direction.vz != 0) {
                Core_Gte_VectorNormal(&direction, &direction);
                direction.vx = (direction.vx * damping) >> 12;
                direction.vy = (direction.vy * damping) >> 12;
                direction.vz = (direction.vz * damping) >> 12;
            }
            state->fx += state->ax + direction.vx;
            state->fy += state->ay + direction.vy;
            state->fz += state->az + direction.vz;
        } else {
            state->fx += state->ax;
            state->fy += state->ay;
            state->fz += state->az;
        }
    }
}

// decomp: BATTLE.PRG func_800D6CCC (src/BATTLE/BATTLE.PRG/5BF94.c:8501)
void Effects_ClearParticleTextureState(int* arg0)
{
    arg0[0] = 0;
    arg0[1] = 0;
    arg0[2] = 0;
    arg0[3] = 0;
    arg0[4] = 0;
    arg0[5] = 0;
    arg0[6] = 0;
    arg0[7] = 0;
}

// decomp: BATTLE.PRG func_800D6CF0 (src/BATTLE/BATTLE.PRG/5BF94.c:8513)
void Effects_InitializeParticleTextureAnimation(Effects_ParticleTextureAnimation* arg0, int textureIndex, int arg2)
{
    arg0->textureIndex = textureIndex;
    arg0->program = (u_char*)Effects_package->block2Data + Effects_package->block2Data[arg2 + 2];
    arg0->cursor = 0;
    arg0->frameTimer = 0;
}

// decomp: BATTLE.PRG func_800D6D24 (src/BATTLE/BATTLE.PRG/5BF94.c:8521)
void Effects_AdvanceParticleTextureAnimation(Effects_ParticleTextureAnimation* arg0)
{
    if (arg0->frameTimer != 0) {
        arg0->frameTimer -= 2;
        if (arg0->frameTimer != 0) {
            return;
        }
    }

    while (1) {
        int w = *(int*)(arg0->program + arg0->cursor);

        switch (w & 0xF0000000) {
        case 0:
            arg0->textureInfo =
                Effects_package->textureInfoTable[arg0->textureIndex] + (w & 0xFFFF);
            *(short*)&arg0->frameTimer = (w >> 16) & 0x3FFF;
            arg0->cursor += 4;
            return;
        case 0x10000000:
            arg0->frameOffsetX = (w << 18) >> 18;
            arg0->frameOffsetY = (w << 4) >> 18;
            arg0->cursor += 4;
            break;
        case 0x20000000:
            arg0->cursor = 0;
            break;
        case 0x30000000:
            return;
        }
    }
}

