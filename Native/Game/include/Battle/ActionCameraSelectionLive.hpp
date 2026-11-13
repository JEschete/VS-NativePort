#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Legacy/Psx.h"

namespace VS::Battle {
void LiveActionTargetLimbPosition(int actor, int limb, SVECTOR* output);
int LiveActionTargetDistance();
void LiveActionCameraMidpoint(SVECTOR* output);
void LiveStepCameraTowardActionMidpoint(int execute);
void LiveUpdateInitialTargetSelection();
void LiveUpdateActionAreaSelection();
void LiveUpdateFinalTargetSelection();
void LiveUpdateActionCameraZoom();
}  // namespace VS::Battle
