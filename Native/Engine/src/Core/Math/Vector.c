// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Vector.h"

// decomp: BATTLE.PRG func_80096254 (src/BATTLE/BATTLE.PRG/2842C.c:2576)
int Core_Math_AddPackedScreenCoordinates(int left_coordinates, int right_coordinates)
{
    return (((left_coordinates & 0xFFFF) + (right_coordinates & 0xFFFF)) & 0xFFFF)
         | (((left_coordinates >> 0x10) + (right_coordinates >> 0x10)) << 0x10);
}

// decomp: BATTLE.PRG _vectorAdd (src/BATTLE/BATTLE.PRG/4A0A8.c:4106)
VECTOR* Core_Math_AddVectors(VECTOR* destination, VECTOR* left, VECTOR* right)
{
    setVector(destination, left->vx + right->vx, left->vy + right->vy, left->vz + right->vz);
    return destination;
}

// decomp: BATTLE.PRG _vectorSubtract (src/BATTLE/BATTLE.PRG/4A0A8.c:4112)
VECTOR* Core_Math_SubtractVectors(VECTOR* destination, VECTOR* left, VECTOR* right)
{
    setVector(destination, left->vx - right->vx, left->vy - right->vy, left->vz - right->vz);
    return destination;
}

// decomp: BATTLE.PRG _copyVector (src/BATTLE/BATTLE.PRG/4A0A8.c:4125)
VECTOR* Core_Math_CopyVector(VECTOR* destination, VECTOR* source)
{
    copyVector(destination, source);
    return destination;
}

// decomp: BATTLE.PRG _absMax3 (src/BATTLE/BATTLE.PRG/5BF94.c:5827)
int Core_Math_GreatestMagnitudeComponent(int first, int second, int third)
{
    int select_third;
    int zero = 0;
    int first_magnitude = first >= zero ? first : -first;
    int second_magnitude = second >= zero ? second : -second;
    int third_magnitude = third >= zero ? third : -third;

    if (first_magnitude < second_magnitude) {
        select_third = second_magnitude < third_magnitude;
        first = second;
    } else {
        select_third = first_magnitude < third_magnitude;
    }
    if (select_third != 0) {
        first = third;
    }
    return first;
}

// decomp: BATTLE.PRG _absMax2 (src/BATTLE/BATTLE.PRG/5BF94.c:5849)
int Core_Math_LargerMagnitude(int first, int second)
{
    int abs0 = first >= 0 ? first : -first;

    if (abs0 < (second < 0 ? -second : second)) {
        return second;
    }
    return first;
}

// decomp: BATTLE.PRG _addSVectorToVector (src/BATTLE/BATTLE.PRG/5BF94.c:5911)
void Core_Math_AddShortAndWideVectors(SVECTOR* svec, VECTOR* vec, VECTOR* out)
{
    int v = svec->vy;
    out->vx = vec->vx + svec->vx;
    do {
        out->vy = vec->vy + v;
        out->vz = vec->vz + svec->vz;
    } while (0);
}

// decomp: BATTLE.PRG vs_battle_addVector (src/BATTLE/BATTLE.PRG/5BF94.c:6887)
void Core_Math_AddWideVectors(VECTOR* left, VECTOR* right, VECTOR* destination)
{
    destination->vx = left->vx + right->vx;
    destination->vy = left->vy + right->vy;
    destination->vz = left->vz + right->vz;
}

// decomp: BATTLE.PRG vs_battle_addVecToSvec (src/BATTLE/BATTLE.PRG/5BF94.c:6894)
void Core_Math_AddWideAndShortVectors(VECTOR* wide, SVECTOR* narrow, VECTOR* destination)
{
    destination->vx = wide->vx + narrow->vx;
    destination->vy = wide->vy + narrow->vy;
    destination->vz = wide->vz + narrow->vz;
}

// decomp: BATTLE.PRG vs_battle_svecToVec (src/BATTLE/BATTLE.PRG/5BF94.c:6901)
void Core_Math_WidenShortVector(SVECTOR* source, VECTOR* destination)
{
    destination->vx = source->vx;
    destination->vy = source->vy;
    destination->vz = source->vz;
}

// decomp: BATTLE.PRG vs_battle_vecToSvec (src/BATTLE/BATTLE.PRG/5BF94.c:6908)
void Core_Math_NarrowWideVector(VECTOR* source, SVECTOR* destination)
{
    destination->vx = source->vx;
    destination->vy = source->vy;
    destination->vz = source->vz;
}

