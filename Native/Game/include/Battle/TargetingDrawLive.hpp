#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/TargetingDraw.hpp"

struct Battle_TargetVolume;

// Binding from the battle target sphere to the typed volume wireframe.
namespace VS::Battle {
// Draws the active target sphere's wireframe over `shape` in `color` through
// the frame view, the GTE projection and the sphere wire line primitive.
// Nothing is drawn without an active sphere or volume; aborts with a
// diagnostic when the sphere's vertex count exceeds its wire storage.
void DrawLiveSphereVolumeWireframe(Battle_TargetVolume* shape, int color);
}  // namespace VS::Battle
