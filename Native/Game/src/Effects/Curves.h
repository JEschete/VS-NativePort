// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_CURVES_H_TYPES
#define VS_EFFECTS_CURVES_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EFFECTS_CURVES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_CURVES_H_DECLS)
#define VS_EFFECTS_CURVES_H_DECLS

void Effects_SampleAllEffectCurves(int sample_distance);
int Effects_SampleEffectCurve(int curveId, int distance);
int Effects_SampleSegmentedEffectCurve(int curve_index, int segment_distance);
int Effects_SegmentedEffectCurveDuration(int curve_index);

#endif // VS_EFFECTS_CURVES_H_DECLS
