#pragma once

#include "Battle/FloatingLabelRendering.hpp"

struct Battle_FloatingLabel;

namespace VS::Battle {
int DrawLiveValueLabelCaption(int x, int y, Battle_FloatingLabel* label);
void DrawLiveLockedFloatingLabel(int x, int y, Battle_FloatingLabel* label);
void DrawLiveStatusRemovalMarker(int x, int y, int animation_step);
void DrawLiveAbilityTimingResult(int x, int y, Battle_FloatingLabel* label);
void DrawLiveChainCount(int x, int y, Battle_FloatingLabel* label);
void DrawLiveAbilityTimingCue();
// The shared label renderer and its quad draw on a scene layer (OT entries
// relative to the current scene head).
FloatingLabelRenderWorld& LiveFloatingLabelRenderer();
bool DrawLiveFloatingLabelQuad(const FloatingLabelQuad& quad, int layer);
}  // namespace VS::Battle
