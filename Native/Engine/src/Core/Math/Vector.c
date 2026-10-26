// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/Vector.h"

// decomp: BATTLE.PRG func_80096254 (src/BATTLE/BATTLE.PRG/2842C.c:2576)
int Core_Math_AddPackedScreenCoordinates(int arg0, int arg1)
{
    return (((arg0 & 0xFFFF) + (arg1 & 0xFFFF)) & 0xFFFF)
         | (((arg0 >> 0x10) + (arg1 >> 0x10)) << 0x10);
}

// decomp: BATTLE.PRG _vectorAdd (src/BATTLE/BATTLE.PRG/4A0A8.c:4106)
VECTOR* Core_Math_AddVectors(VECTOR* arg0, VECTOR* arg1, VECTOR* arg2)
{
    setVector(arg0, arg1->vx + arg2->vx, arg1->vy + arg2->vy, arg1->vz + arg2->vz);
    return arg0;
}

// decomp: BATTLE.PRG _vectorSubtract (src/BATTLE/BATTLE.PRG/4A0A8.c:4112)
VECTOR* Core_Math_SubtractVectors(VECTOR* arg0, VECTOR* arg1, VECTOR* arg2)
{
    setVector(arg0, arg1->vx - arg2->vx, arg1->vy - arg2->vy, arg1->vz - arg2->vz);
    return arg0;
}

// decomp: BATTLE.PRG _copyVector (src/BATTLE/BATTLE.PRG/4A0A8.c:4125)
VECTOR* Core_Math_CopyVector(VECTOR* arg0, VECTOR* arg1)
{
    copyVector(arg0, arg1);
    return arg0;
}

// decomp: BATTLE.PRG _absMax3 (src/BATTLE/BATTLE.PRG/5BF94.c:5827)
int Core_Math_GreatestMagnitudeComponent(int arg0, int arg1, int arg2)
{
    int var_v0;
    int new_var = 0;
    int var_t0 = arg0 >= new_var ? arg0 : -arg0;
    int var_a3 = arg1 >= new_var ? arg1 : -arg1;
    int var_v1 = arg2 >= new_var ? arg2 : -arg2;

    if (var_t0 < var_a3) {
        var_v0 = var_a3 < var_v1;
        arg0 = arg1;
    } else {
        var_v0 = var_t0 < var_v1;
    }
    if (var_v0 != 0) {
        arg0 = arg2;
    }
    return arg0;
}

// decomp: BATTLE.PRG _absMax2 (src/BATTLE/BATTLE.PRG/5BF94.c:5849)
int Core_Math_LargerMagnitude(int arg0, int arg1)
{
    int abs0 = arg0 >= 0 ? arg0 : -arg0;

    if (abs0 < (arg1 < 0 ? -arg1 : arg1)) {
        return arg1;
    }
    return arg0;
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
void Core_Math_AddWideVectors(VECTOR* arg0, VECTOR* arg1, VECTOR* arg2)
{
    arg2->vx = arg0->vx + arg1->vx;
    arg2->vy = arg0->vy + arg1->vy;
    arg2->vz = arg0->vz + arg1->vz;
}

// decomp: BATTLE.PRG vs_battle_addVecToSvec (src/BATTLE/BATTLE.PRG/5BF94.c:6894)
void Core_Math_AddWideAndShortVectors(VECTOR* arg0, SVECTOR* arg1, VECTOR* arg2)
{
    arg2->vx = arg0->vx + arg1->vx;
    arg2->vy = arg0->vy + arg1->vy;
    arg2->vz = arg0->vz + arg1->vz;
}

// decomp: BATTLE.PRG vs_battle_svecToVec (src/BATTLE/BATTLE.PRG/5BF94.c:6901)
void Core_Math_WidenShortVector(SVECTOR* arg0, VECTOR* arg1)
{
    arg1->vx = arg0->vx;
    arg1->vy = arg0->vy;
    arg1->vz = arg0->vz;
}

// decomp: BATTLE.PRG vs_battle_vecToSvec (src/BATTLE/BATTLE.PRG/5BF94.c:6908)
void Core_Math_NarrowWideVector(VECTOR* arg0, SVECTOR* arg1)
{
    arg1->vx = arg0->vx;
    arg1->vy = arg0->vy;
    arg1->vz = arg0->vz;
}

