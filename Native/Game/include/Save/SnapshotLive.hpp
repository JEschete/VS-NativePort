#pragma once
// Hand-maintained (tools/port/owned.csv).

// Binds the Ashley save snapshot (Snapshot.hpp) to the field door records,
// the action table, Ashley's battle actor and camera, and
// VS::State::ashleySnapshot, in the shapes the copied callers use.
namespace VS::Save {
// Sets VS::State::ashleySnapshot's pending flag.
void MarkLiveAshleySnapshotValid();
void ClearLiveAshleySnapshotPending();
// Captures the entry door, learned actions, Ashley's statistics and camera,
// then stores the room actors' states and the entrance door for a reload.
void CaptureLiveAshleySnapshot();
}  // namespace VS::Save
