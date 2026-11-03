#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Core/Gte.h"
#include "Render/DepthCue.hpp"

// Live binding for depth cueing (DepthCue.cpp): the GTE cue registers and far
// colour.
namespace VS::Render {
// DpqColor's call shape; `output` may alias `color`.
void ApplyLiveDepthCueColor(const CVECTOR* color, Core_GteWord cue,
                            CVECTOR* output);
void SetLiveFogNearDistance(Core_GteWord near_distance,
                            Core_GteWord projection_distance);
}  // namespace VS::Render
