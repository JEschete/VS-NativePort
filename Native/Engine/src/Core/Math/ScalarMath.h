#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_CORE_MATH_SCALARMATH_H
#define VS_OWNED_CORE_MATH_SCALARMATH_H
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

int Core_Math_MultiplyFixed(int left, int right);

void Core_Math_MultiplyMatrixIntoRight(MATRIX* left, MATRIX* right);

int Core_Math_SquareRootInteger(int value);

void Core_Math_BuildEulerRotationMatrixQuadrant(SVECTOR* rotation, MATRIX* output);

void Core_Math_PremultiplyRotationMatrix(MATRIX* left, MATRIX* right);

MATRIX* Core_Math_ScaleMatrixColumns(MATRIX* matrix, VECTOR* scale);

#ifdef __cplusplus
}
#endif

#endif // VS_OWNED_CORE_MATH_SCALARMATH_H
