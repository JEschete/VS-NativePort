#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Core/Gte.hpp"
#include "Render/DepthCue.hpp"

// Live binding for depth cueing (DepthCue.cpp): the GTE cue registers and far
// colour.
namespace VS::Render {
// DpqColor's call shape; `output` may alias `color`.
void ApplyLiveDepthCueColor(const VS::Core::Gte::CVECTOR* color,
                            VS::Core::Gte::Word cue,
                            VS::Core::Gte::CVECTOR* output);
void SetLiveFogNearDistance(VS::Core::Gte::Word near_distance,
                            VS::Core::Gte::Word projection_distance);
}  // namespace VS::Render
