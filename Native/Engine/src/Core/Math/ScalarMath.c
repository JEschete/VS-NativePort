// Hand-maintained (tools/port/owned.csv).
#include "Core/Math/ScalarMath.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Core/Math/Vector.h"
#include "Legacy/Psx.h"

enum {
  kVectorDimensions = 3,
  kFixedFractionBits = 12,
  kIntermediateScale = 4,
  kWordBits = sizeof(uint32_t) * CHAR_BIT,
  kNormalizedIndexShift = 24,
  kRootLookupFractionBits = 18
};

int16_t Core_Math_WrapSigned16(uint32_t value) {
  const uint16_t bits = (uint16_t)value;
  int16_t result;
  memcpy(&result, &bits, sizeof result);
  return result;
}

// decomp: SLUS_010.40 Square0 (src/SLUS_010.40/32154.s:24).
VECTOR* Core_Math_SquareVectorComponents(const VECTOR* input, VECTOR* output) {
  if (input == NULL || output == NULL) {
    Core_Debugf("math", "Vector square requires input and output");
    abort();
  }
  const int32_t x = Core_Math_WrapSigned16((uint32_t)input->vx);
  const int32_t y = Core_Math_WrapSigned16((uint32_t)input->vy);
  const int32_t z = Core_Math_WrapSigned16((uint32_t)input->vz);
  output->vx = x * x;
  output->vy = y * y;
  output->vz = z * z;
  return output;
}

// decomp: BATTLE.PRG _fixedPointMult (src/BATTLE/BATTLE.PRG/4A0A8.c:4163).
int Core_Math_MultiplyFixed(int left, int right) {
  const uint32_t bits =
      (uint32_t)((uint64_t)((int64_t)left * right) >> kFixedFractionBits);
  int result;
  memcpy(&result, &bits, sizeof result);
  return result;
}

static void MultiplyRotationMatrices(const MATRIX* left, const MATRIX* right,
                                     MATRIX* output) {
  if (left == NULL || right == NULL || output == NULL) {
    Core_Debugf("math", "Rotation matrix product requires all matrices");
    abort();
  }
  const MATRIX first = *left;
  const MATRIX second = *right;
  for (int row = 0; row < kVectorDimensions; ++row) {
    for (int column = 0; column < kVectorDimensions; ++column) {
      int64_t product = 0;
      for (int component = 0; component < kVectorDimensions; ++component) {
        product +=
            (int64_t)first.m[row][component] * second.m[component][column];
      }
      product >>= kFixedFractionBits;
      if (product > INT16_MAX) {
        product = INT16_MAX;
      } else if (product < INT16_MIN) {
        product = INT16_MIN;
      }
      output->m[row][column] = (int16_t)product;
    }
  }
}

// decomp: SLUS_010.40 func_80041C68 (src/SLUS_010.40/32154.s:280).
void Core_Math_MultiplyMatrixIntoRight(MATRIX* left, MATRIX* right) {
  MultiplyRotationMatrices(left, right, right);
}

// decomp: SLUS_010.40 MulMatrix (src/SLUS_010.40/32154.s:275).
MATRIX* Core_Math_MultiplyMatrixIntoLeft(MATRIX* left, MATRIX* right) {
  MultiplyRotationMatrices(left, right, left);
  return left;
}

// decomp: BATTLE.PRG func_800989B8 (src/BATTLE/BATTLE.PRG/2EA3C.s:1664).
MATRIX* Core_Math_MultiplyRotationMatrixInPlace(MATRIX* left, MATRIX* right) {
  MultiplyRotationMatrices(left, right, right);
  return right;
}

static unsigned IntegerSquareRoot(unsigned value) {
  unsigned result = 0;
  unsigned bit = 1u << (kWordBits - 2);
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
int Core_Math_SquareRootInteger(int value) {
  if (value <= 0) {
    return 0;
  }
  unsigned leading = 0;
  unsigned shifted = (unsigned)value;
  while ((shifted & (1u << (kWordBits - 1))) == 0) {
    shifted <<= 1;
    ++leading;
  }
  leading &= kWordBits - 2;
  const unsigned normalized =
      leading >= kNormalizedIndexShift
          ? (unsigned)value << (leading - kNormalizedIndexShift)
          : (unsigned)value >> (kNormalizedIndexShift - leading);
  const unsigned sample =
      IntegerSquareRoot(normalized << kRootLookupFractionBits);
  return (int)((sample << ((kWordBits - 1 - leading) >> 1)) >>
               kFixedFractionBits);
}

// decomp: BATTLE.PRG func_800988BC (src/BATTLE/BATTLE.PRG/2EA3C.s:1597).
// func_800B16F4 (src/BATTLE/BATTLE.PRG/48C7C.s:188) has identical octant
// blocks.
Core_Math_RotationTrigPair Core_Math_LookupRotationSineCosine(int angle) {
  const Core_Math_RotationTrigPair pair = {Core_Gte_Sin(angle),
                                           Core_Gte_Cos(angle)};
  return pair;
}

// decomp: BATTLE.PRG func_8009874C (src/BATTLE/BATTLE.PRG/2EA3C.s:1499).
// Keep the GPF stages' two extra fractional bits until each final sum.
MATRIX* Core_Math_BuildEulerRotationMatrix(const SVECTOR* rotation,
                                           MATRIX* output) {
  if (rotation == NULL || output == NULL) {
    Core_Debugf("math", "Euler rotation requires input and output");
    abort();
  }
  const Core_Math_RotationTrigPair x =
      Core_Math_LookupRotationSineCosine(rotation->vx);
  const Core_Math_RotationTrigPair y =
      Core_Math_LookupRotationSineCosine(rotation->vy);
  const Core_Math_RotationTrigPair z =
      Core_Math_LookupRotationSineCosine(rotation->vz);
  const int sin_z_sin_y =
      Core_Math_MultiplyFixed(z.sine * kIntermediateScale, y.sine);
  const int sin_z_cos_y =
      Core_Math_MultiplyFixed(z.sine * kIntermediateScale, y.cosine);
  const int cos_z_sin_y =
      Core_Math_MultiplyFixed(z.cosine * kIntermediateScale, y.sine);
  const int cos_z_cos_y =
      Core_Math_MultiplyFixed(z.cosine * kIntermediateScale, y.cosine);
  output->m[0][0] =
      (int16_t)((Core_Math_MultiplyFixed(x.sine, sin_z_sin_y) + cos_z_cos_y) >>
                2);
  output->m[0][1] =
      (int16_t)((Core_Math_MultiplyFixed(x.sine, cos_z_sin_y) - sin_z_cos_y) >>
                2);
  output->m[0][2] = (int16_t)Core_Math_MultiplyFixed(x.cosine, y.sine);
  output->m[1][0] = (int16_t)Core_Math_MultiplyFixed(z.sine, x.cosine);
  output->m[1][1] = (int16_t)Core_Math_MultiplyFixed(z.cosine, x.cosine);
  output->m[1][2] = (int16_t)-x.sine;
  output->m[2][0] =
      (int16_t)((Core_Math_MultiplyFixed(x.sine, sin_z_cos_y) - cos_z_sin_y) >>
                2);
  output->m[2][1] =
      (int16_t)((Core_Math_MultiplyFixed(x.sine, cos_z_cos_y) + sin_z_sin_y) >>
                2);
  output->m[2][2] = (int16_t)Core_Math_MultiplyFixed(x.cosine, y.cosine);
  return output;
}

// decomp: BATTLE.PRG func_800B17F0 (src/BATTLE/BATTLE.PRG/48C7C.s:254).
void Core_Math_BuildEulerRotationMatrixQuadrant(SVECTOR* rotation,
                                                MATRIX* output) {
  const int sin_x = Core_Gte_Sin(rotation->vx),
            cos_x = Core_Gte_Cos(rotation->vx);
  const int sin_y = Core_Gte_Sin(rotation->vy),
            cos_y = Core_Gte_Cos(rotation->vy);
  const int sin_z = Core_Gte_Sin(rotation->vz),
            cos_z = Core_Gte_Cos(rotation->vz);
  const int sin_z_sin_x =
      (sin_z * sin_x * kIntermediateScale) >> kFixedFractionBits;
  const int sin_z_cos_x =
      (sin_z * cos_x * kIntermediateScale) >> kFixedFractionBits;
  const int cos_z_sin_x =
      (cos_z * sin_x * kIntermediateScale) >> kFixedFractionBits;
  const int cos_z_cos_x =
      (cos_z * cos_x * kIntermediateScale) >> kFixedFractionBits;
  output->m[0][0] = (int16_t)((cos_z * cos_y) >> kFixedFractionBits);
  output->m[0][1] =
      (int16_t)((((sin_y * cos_z_sin_x) >> kFixedFractionBits) - sin_z_cos_x) >>
                2);
  output->m[0][2] =
      (int16_t)((((sin_y * cos_z_cos_x) >> kFixedFractionBits) + sin_z_sin_x) >>
                2);
  output->m[1][0] = (int16_t)((sin_z * cos_y) >> kFixedFractionBits);
  output->m[1][1] =
      (int16_t)((((sin_y * sin_z_sin_x) >> kFixedFractionBits) + cos_z_cos_x) >>
                2);
  output->m[1][2] =
      (int16_t)((((sin_y * sin_z_cos_x) >> kFixedFractionBits) - cos_z_sin_x) >>
                2);
  output->m[2][0] = (int16_t)-sin_y;
  output->m[2][1] = (int16_t)((cos_y * sin_x) >> kFixedFractionBits);
  output->m[2][2] = (int16_t)((cos_y * cos_x) >> kFixedFractionBits);
}

// decomp: BATTLE.PRG func_800B196C (src/BATTLE/BATTLE.PRG/48C7C.s:365).
void Core_Math_PremultiplyRotationMatrix(MATRIX* left, MATRIX* right) {
  Core_Math_MultiplyMatrixIntoRight(left, right);
}

// decomp: SLUS_010.40 ScaleMatrix (src/SLUS_010.40/31C0C.s); BATTLE 4B16C.s:8
// is identical.
MATRIX* Core_Math_ScaleMatrixColumns(MATRIX* matrix, VECTOR* scale) {
  const int16_t values[kVectorDimensions] = {
      (int16_t)scale->vx, (int16_t)scale->vy, (int16_t)scale->vz};
  for (int row = 0; row < kVectorDimensions; ++row) {
    for (int column = 0; column < kVectorDimensions; ++column) {
      int product =
          (matrix->m[row][column] * values[column]) >> kFixedFractionBits;
      if (product > INT16_MAX) product = INT16_MAX;
      if (product < INT16_MIN) product = INT16_MIN;
      matrix->m[row][column] = (int16_t)product;
    }
  }
  return matrix;
}

// decomp: BATTLE.PRG func_800E4624 (src/BATTLE/BATTLE.PRG/7BDD4.s:49).
int Core_Math_SquaredDistanceWithHeightOffsets(int16_t* source, int16_t* target,
                                               int source_height,
                                               int target_height) {
  return (int)Core_Math_SquaredVectorLength(
      source[0] - target[0],
      source[1] + source_height - target[1] - target_height,
      source[2] - target[2]);
}

// decomp: BATTLE.PRG func_800E4660 (src/BATTLE/BATTLE.PRG/7BDD4.s:68).
unsigned int Core_Math_SquaredVectorLength(int delta_x, int delta_y,
                                           int delta_z) {
  const int components[kVectorDimensions] = {(int16_t)delta_x, (int16_t)delta_y,
                                             (int16_t)delta_z};
  unsigned int squared = 0;
  for (unsigned axis = 0; axis < kVectorDimensions; ++axis) {
    squared += (unsigned int)(components[axis] * components[axis]);
  }
  return squared;
}
