#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle {
// Original module entry signatures. Missing used state, a bad used index or
// an unavailable packet target is diagnosed on battle.hud and refused.
// decomp: F01354 _renderStatBar (src/BATTLE/BATTLE.PRG/5BF94.c:3088).
void DrawLiveHudStatGauge(int color_index, int current, int maximum);
// decomp: F01355 func_800C9CB4 (src/BATTLE/BATTLE.PRG/5BF94.c:3112).
void DrawLiveSlidingStatusIcon(int icon, int slide, int slot);
// decomp: F01358 func_800C9EB8 (src/BATTLE/BATTLE.PRG/5BF94.c:3197).
int LiveShortcutIconBrightness(int shortcut);
// decomp: F01360 func_800C9F98 (src/BATTLE/BATTLE.PRG/5BF94.c:3241).
void DrawLiveShortcutButtonMarker(int position, int brightness_transparency);
}  // namespace VS::Battle
