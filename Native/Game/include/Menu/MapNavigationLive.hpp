#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binds MapNavigation.hpp to the generated map menu: the ARM scene buffer
// (Battle_unusedSceneBuffer, sized by kSceneArmFiles) or the room's
// minimap scene, centerPoint, currentRoomIndex,
// isCurrentScene and roomNamesTable. Each entry keeps the
// original call shape. A scene that is neither buffer, a room record or vertex
// block outside it and a room index outside the scene are refused and logged,
// where the original reads or writes unchecked.

struct Field_MapScene;

namespace VS::Menu::map {
void LiveMarkAllRoomsVisited(int* scene);
void LiveCenterOnRoom(Field_MapScene* scene, int room_id);
void LiveSetRoomCenterTarget(Field_MapScene* scene, int room_id);
void LiveAnimateCentering(Field_MapScene* scene);
void LiveScaleRoomVertices(Field_MapScene* scene, int factor);
// A missing or empty minimap scene is refused and matches no room.
int LiveInitialRoomIndex(Field_MapScene* scene);
// A refused step leaves currentRoomIndex unchanged.
void LiveSelectAdjacentVisitedRoom(int search_forward);
}  // namespace VS::Menu::map
