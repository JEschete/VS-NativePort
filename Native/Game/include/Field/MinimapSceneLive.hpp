#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binds the typed map scene to generated callers: the room and ARM scene
// buffers, GTE projection, the frame primitive buffer and door state flags.

#include <cstddef>

namespace VS::Field {

struct MapRoomName;

// Resolves a loaded scene in place and returns its room-name table, which
// stays inside (and lives as long as) the caller's scene buffer. Throws
// std::runtime_error for a missing or malformed scene.
MapRoomName* ResolveLiveSceneRoomData(void* data, std::size_t size);

// Draws a resolved scene into the current frame's ordering table. Throws
// std::runtime_error for a missing or malformed scene or a full frame.
void DrawLiveMinimapScene(const void* data, std::size_t size);

// The scene door id of a room door, and marking the entrance door on the map;
// the original BATTLE.PRG entries.
int SceneDoorIdEntry(int door_index);
void MarkEntranceDoorOnSceneMapEntry(int door_index);

}  // namespace VS::Field
