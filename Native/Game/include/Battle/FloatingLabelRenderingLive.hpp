#pragma once

struct Battle_FloatingLabel;

namespace VS::Battle {
int DrawLiveValueLabelCaption(int x, int y, Battle_FloatingLabel* label);
void DrawLiveLockedFloatingLabel(int x, int y, Battle_FloatingLabel* label);
void DrawLiveStatusRemovalMarker(int x, int y, int animation_step);
void DrawLiveAbilityTimingResult(int x, int y, Battle_FloatingLabel* label);
void DrawLiveChainCount(int x, int y, Battle_FloatingLabel* label);
void DrawLiveAbilityTimingCue();
}  // namespace VS::Battle
