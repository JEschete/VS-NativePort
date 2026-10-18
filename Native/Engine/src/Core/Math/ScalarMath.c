// Hand-maintained (tools/port/owned.csv).
#include "Core/Math/ScalarMath.h"
#include "Legacy/Psx.h"
#include "Core/Math/Vector.h"

#include <stdint.h>
#include <string.h>

// decomp: BATTLE.PRG _fixedPointMult (src/BATTLE/BATTLE.PRG/4A0A8.c:4163).
int Core_Math_MultiplyFixed(int left, int right)
{
    const uint32_t bits = (uint32_t)((uint64_t)((int64_t)left * right) >> 12);
    int result;
    memcpy(&result, &bits, sizeof result);
    return result;
}

// decomp: SLUS_010.40 func_80041C68 (src/SLUS_010.40/32154.s:280).
void Core_Math_MultiplyMatrixIntoRight(MATRIX* left, MATRIX* right)
{
    const MATRIX first = *left;
    const MATRIX second = *right;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            int64_t product = 0;
            for (int component = 0; component < 3; ++component) {
                product += (int64_t)first.m[row][component] * second.m[component][column];
            }
            product >>= 12;
            if (product > 32767) {
                product = 32767;
            } else if (product < -32768) {
                product = -32768;
            }
            right->m[row][column] = (short)product;
        }
    }
}

static unsigned IntegerSquareRoot(unsigned value)
{
    unsigned result = 0;
    unsigned bit = 1u << 30;
    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

// decomp: SLUS_010.40 vs_gte_rsqrt (src/SLUS_010.40/32154.s:207).
// D_80040C18 contains floor(sqrt(index << 18)) for indices 64..255.
int Core_Math_SquareRootInteger(int value)
{
    if (value <= 0) {
        return 0;
    }
    unsigned leading = 0;
    unsigned shifted = (unsigned)value;
    while ((shifted & 0x80000000u) == 0) {
        shifted <<= 1;
        ++leading;
    }
    leading &= 30;
    const unsigned normalized = leading >= 24 ? (unsigned)value << (leading - 24)
                                             : (unsigned)value >> (24 - leading);
    const unsigned sample = IntegerSquareRoot(normalized << 18);
    return (int)((sample << ((31 - leading) >> 1)) >> 12);
}

// decomp: BATTLE.PRG func_800B17F0 (src/BATTLE/BATTLE.PRG/48C7C.s:254).
void Core_Math_BuildEulerRotationMatrixQuadrant(SVECTOR* rotation, MATRIX* output)
{
    const int sinX = Core_Gte_Sin(rotation->vx), cosX = Core_Gte_Cos(rotation->vx);
    const int sinY = Core_Gte_Sin(rotation->vy), cosY = Core_Gte_Cos(rotation->vy);
    const int sinZ = Core_Gte_Sin(rotation->vz), cosZ = Core_Gte_Cos(rotation->vz);
    const int sinZSinX = (sinZ * sinX * 4) >> 12;
    const int sinZCosX = (sinZ * cosX * 4) >> 12;
    const int cosZSinX = (cosZ * sinX * 4) >> 12;
    const int cosZCosX = (cosZ * cosX * 4) >> 12;
    output->m[0][0] = (short)((cosZ * cosY) >> 12);
    output->m[0][1] = (short)((((sinY * cosZSinX) >> 12) - sinZCosX) >> 2);
    output->m[0][2] = (short)((((sinY * cosZCosX) >> 12) + sinZSinX) >> 2);
    output->m[1][0] = (short)((sinZ * cosY) >> 12);
    output->m[1][1] = (short)((((sinY * sinZSinX) >> 12) + cosZCosX) >> 2);
    output->m[1][2] = (short)((((sinY * sinZCosX) >> 12) - cosZSinX) >> 2);
    output->m[2][0] = (short)-sinY;
    output->m[2][1] = (short)((cosY * sinX) >> 12);
    output->m[2][2] = (short)((cosY * cosX) >> 12);
}

// decomp: BATTLE.PRG func_800B196C (src/BATTLE/BATTLE.PRG/48C7C.s:365).
void Core_Math_PremultiplyRotationMatrix(MATRIX* left, MATRIX* right)
{
    Core_Math_MultiplyMatrixIntoRight(left, right);
}

// decomp: SLUS_010.40 ScaleMatrix (src/SLUS_010.40/31C0C.s); BATTLE 4B16C.s:8 is identical.
MATRIX* Core_Math_ScaleMatrixColumns(MATRIX* matrix, VECTOR* scale)
{
    const short values[3] = {(short)scale->vx, (short)scale->vy, (short)scale->vz};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            int product = (matrix->m[row][column] * values[column]) >> 12;
            if (product > 32767) product = 32767;
            if (product < -32768) product = -32768;
            matrix->m[row][column] = (short)product;
        }
    }
    return matrix;
}
