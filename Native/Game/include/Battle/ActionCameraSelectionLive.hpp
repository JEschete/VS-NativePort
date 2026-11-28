#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Core/Gte.hpp"

namespace VS::Battle {
void LiveActionTargetLimbPosition(int actor, int limb,
                                  VS::Core::Gte::SVECTOR* output);
int LiveActionTargetDistance();
void LiveActionCameraMidpoint(VS::Core::Gte::SVECTOR* output);
void LiveStepCameraTowardActionMidpoint(int execute);
void LiveUpdateInitialTargetSelection();
void LiveUpdateActionAreaSelection();
void LiveUpdateFinalTargetSelection();
void LiveUpdateActionCameraZoom();
}  // namespace VS::Battle
