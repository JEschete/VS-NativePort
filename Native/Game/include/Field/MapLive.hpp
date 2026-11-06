#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>

struct MATRIX;

// Binds the typed map progress rules (Map.hpp) to the minimap flag, the room
// visit progress and the location table, in the shapes the copied callers use.
namespace VS::Field {
// Stores the low byte of `visible` as the minimap flag and returns it.
int SetLiveMinimapVisible(int visible);

// Marks the visited rooms of the resolved `size`-byte scene at `scene` (a room
// scene or the map menu's ARM buffer); a missing or truncated scene is refused
// with a diagnostic and left unchanged.
void RestoreLiveSceneRoomVisits(void* scene, std::size_t size);

// Writes the minimap rotation into `rotation`'s 3x3 part; the translation is
// left alone.
void BuildLiveMinimapRotation(MATRIX* rotation, int yaw_angle);

// The map completion percent of the current visit progress.
int LiveMapCompletionPercent();
}  // namespace VS::Field
