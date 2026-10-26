#pragma once
#include <stdint.h>
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_CORE_MATH_SCALARMATH_H
#define VS_OWNED_CORE_MATH_SCALARMATH_H
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

int Core_Math_MultiplyFixed(int left, int right);

int16_t Core_Math_WrapSigned16(uint32_t value);
VECTOR* Core_Math_SquareVectorComponents(const VECTOR* input, VECTOR* output);

void Core_Math_MultiplyMatrixIntoRight(MATRIX* left, MATRIX* right);
MATRIX* Core_Math_MultiplyMatrixIntoLeft(MATRIX* left, MATRIX* right);
MATRIX* Core_Math_MultiplyRotationMatrixInPlace(MATRIX* left, MATRIX* right);

int Core_Math_SquareRootInteger(int value);

typedef struct Core_Math_RotationTrigPair {
  int sine;
  int cosine;
} Core_Math_RotationTrigPair;

Core_Math_RotationTrigPair Core_Math_LookupRotationSineCosine(int angle);
MATRIX* Core_Math_BuildEulerRotationMatrix(const SVECTOR* rotation,
                                           MATRIX* output);

void Core_Math_BuildEulerRotationMatrixQuadrant(SVECTOR* rotation,
                                                MATRIX* output);

void Core_Math_PremultiplyRotationMatrix(MATRIX* left, MATRIX* right);

MATRIX* Core_Math_ScaleMatrixColumns(MATRIX* matrix, VECTOR* scale);

int Core_Math_SquaredDistanceWithHeightOffsets(int16_t* source, int16_t* target,
                                               int source_height,
                                               int target_height);

unsigned int Core_Math_SquaredVectorLength(int delta_x, int delta_y,
                                           int delta_z);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_CORE_MATH_SCALARMATH_H
