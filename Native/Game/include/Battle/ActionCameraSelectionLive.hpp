#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Legacy/Psx.hpp"

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
