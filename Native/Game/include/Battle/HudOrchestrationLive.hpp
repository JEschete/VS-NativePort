#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::State {
struct SceneTimer;
}  // namespace VS::State

namespace VS::Battle {
void SetLiveActorMarkersHidden(int command);
void SetLiveActorMarker(int actor_slot, int command);
void SuppressLiveAshleyStatusLabels();
void DrawLiveShortcutHud();
void DrawLiveBattleHud();
void DrawLivePuzzleTimer(const State::SceneTimer* timer);
}  // namespace VS::Battle
