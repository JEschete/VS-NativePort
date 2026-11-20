#pragma once

#include "Battle/FloatingLabelRendering.hpp"

namespace VS::Battle {
struct FloatingLabel;
int DrawLiveValueLabelCaption(int x, int y, VS::Battle::FloatingLabel* label);
void DrawLiveLockedFloatingLabel(int x, int y,
                                 VS::Battle::FloatingLabel* label);
void DrawLiveStatusRemovalMarker(int x, int y, int animation_step);
void DrawLiveAbilityTimingResult(int x, int y,
                                 VS::Battle::FloatingLabel* label);
void DrawLiveChainCount(int x, int y, VS::Battle::FloatingLabel* label);
void DrawLiveAbilityTimingCue();
// The shared label renderer and its quad draw on a scene layer (OT entries
// relative to the current scene head).
FloatingLabelRenderWorld& LiveFloatingLabelRenderer();
bool DrawLiveFloatingLabelQuad(const FloatingLabelQuad& quad, int layer);
}  // namespace VS::Battle
