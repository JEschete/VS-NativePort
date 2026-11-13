#pragma once
// Hand-maintained (tools/port/owned.csv).

struct State_SceneTimer;

namespace VS::Battle {
void SetLiveActorMarkersHidden(int command);
void SetLiveActorMarker(int actor_slot, int command);
void SuppressLiveAshleyStatusLabels();
void DrawLiveShortcutHud();
void DrawLiveBattleHud();
void DrawLivePuzzleTimer(const State_SceneTimer* timer);
}  // namespace VS::Battle
