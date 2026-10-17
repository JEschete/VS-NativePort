// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_MATH_FIXEDPOINT_H_TYPES
#define VS_CORE_MATH_FIXEDPOINT_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CORE_MATH_FIXEDPOINT_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_MATH_FIXEDPOINT_H_DECLS)
#define VS_CORE_MATH_FIXEDPOINT_H_DECLS

extern u_char Core_Math_kArcTangentTable[1296];
short Core_Math_ClampSignedShort(short arg0, int arg1, int arg2);
VECTOR* Core_Math_ScaleFixedVector(VECTOR* arg0, VECTOR* arg1, int arg2);
int Core_Math_FixedVectorMagnitude(VECTOR* arg0);
VECTOR* Core_Math_ShortVectorToFixed(VECTOR* arg0, SVECTOR* arg1);
int Core_Math_MultiplyFixed(int a, int b);
void Core_Math_NormalizeFixedVector(VECTOR* arg0, VECTOR* arg1);
int Core_Math_SplitDecimalDigit(int value);
int Core_Math_BuildRotationYxz();
int Core_Math_BuildRotationXyz();
int Core_Math_LookupSineCosinePair();
int Core_Math_Cosine();
int Core_Math_Sine();
long Core_Math_ArcTangent2(long y, long x);
int Core_Math_ScaleMatrixColumns();
int Core_Math_InverseSquareRootParts();
int Core_Math_MultiplyMatrixIntoLeft();
int Core_Math_CrossProductFixed();
int Core_Math_SquareVectorComponents();
int Core_Math_SquareRootFixed();
int Core_Math_NormalizeVector();
int Core_Math_NormalizeVectorToShort();
int Core_Math_NormalizeShortVector();
int Core_Math_NormalizeVectorRegisters();
void Core_Math_MultiplyMatrixIntoRight(MATRIX*, MATRIX*);
int Core_Math_SquareRootInteger(int);

#endif // VS_CORE_MATH_FIXEDPOINT_H_DECLS
