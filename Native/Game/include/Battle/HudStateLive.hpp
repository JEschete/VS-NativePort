#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/HudState.hpp"

struct Battle_ActorStats;
struct Battle_LimbStats;

namespace VS::Battle {
// A fresh value view, not a persistent cache. Commands commit only their own
// byte, so callbacks and the existing reset/draw paths retain their peers.
HudState ReadLiveHudState();

// Original module signatures are retained, including signed int returns and
// non-const pointers; missing used records log and refuse rather than return
// zero.
int SetLiveGameplayHudHidden(int command);
int LiveStatusIconFlags(Battle_ActorStats* stats);
int LiveLimbCondition(Battle_LimbStats* limb);
void ShowLiveShortcutHud();
void ShowLivePuzzleTimer();
void HideLivePuzzleTimer();
void SetLiveHudHidden(int command);
void SetLiveHudBattleMode(int command);
}  // namespace VS::Battle
