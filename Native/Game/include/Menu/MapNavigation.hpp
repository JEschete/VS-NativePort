#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Field/MinimapScene.hpp"

// The area map's room navigation and geometry (MENU5.PRG 4D8.c:1048-1055,
// 1511-1763 and 1834-1878): marking every room visited, the selected room's
// bounding-box center, snapping and easing the map onto it, scaling the room
// vertices, the initial room and stepping to the next or previous visited
// room. The rules work on typed room records, room-name links and per-room
// vertex views; MapNavigationLive.hpp binds them to the ARM scene buffer and
// the copied map globals. Nothing here projects or draws a room.
namespace VS::Menu::map {
// The first three halfwords of a room vertex (SVECTOR); the fourth is never
// read or written here.
struct MapVertex {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;

  friend bool operator==(const MapVertex&, const MapVertex&) = default;
};

// _centerPoint: the offset the map still has to move to reach its target.
struct MapPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;

  friend bool operator==(const MapPoint&, const MapPoint&) = default;
};

// One room's vertices, edited in place.
using RoomVertices = std::span<MapVertex>;

// vs_battle_room without its vertex offset: the visited word and the room's
// zone and map.
struct MapRoom {
  std::int32_t visited = 0;
  std::int16_t zone_id = 0;
  std::int16_t map_id = 0;

  friend bool operator==(const MapRoom&, const MapRoom&) = default;
};

// vs_battle_roomName's links to the previous and next room in name order.
struct RoomLink {
  std::int16_t previous = 0;
  std::int16_t next = 0;
};

// _getCurrentRoomIndex's result and the _isCurrentScene it caches.
struct InitialRoom {
  std::int32_t selected = 0;
  // The room Ashley is in, -1 when it is not on this map.
  std::int32_t current = -1;

  friend bool operator==(const InitialRoom&, const InitialRoom&) = default;
};

// The selected-room sound.
inline constexpr std::int32_t kRoomChangeSound = 0x7E;
inline constexpr std::int32_t kRoomChangeSoundVolume = 0xB;

class MapNavigationHost {
 public:
  MapNavigationHost() = default;
  MapNavigationHost(const MapNavigationHost&) = delete;
  MapNavigationHost& operator=(const MapNavigationHost&) = delete;
  MapNavigationHost(MapNavigationHost&&) = delete;
  MapNavigationHost& operator=(MapNavigationHost&&) = delete;
  virtual ~MapNavigationHost() = default;

  // vs_main_playSfxDefault.
  virtual void PlaySound(std::int32_t sound, std::int32_t volume) = 0;
};

// func_8010467C: sets every room's visited word to 1.
void MarkAllRoomsVisited(std::span<MapRoom> rooms);

// _setCenterpoint: the bounding box's horizontal center, each half sum
// rounded toward zero, and its largest y (the room's floor). No vertices give
// {0, -0x8000, 0}, the empty box's center.
[[nodiscard]] MapPoint RoomCenterOf(std::span<const MapVertex> vertices);

// _snapMapToRoom: moves every room's vertices by `room`'s center, so that
// room sits at the origin, and clears `center`. False, with nothing changed,
// when `room` is not one of `rooms`.
[[nodiscard]] bool CenterOnRoom(std::span<const RoomVertices> rooms,
                                std::size_t room, MapPoint& center);

// _smoothMapToCenterpoint's step on each axis: the whole remainder under 8,
// 8 toward it up to 128 and an eighth (rounded toward zero) above.
[[nodiscard]] MapPoint CenteringStepOf(MapPoint remaining);

// _smoothMapToCenterpoint: unless `center` is zero, moves every vertex and
// `center` by one step toward it, each halfword wrapping.
void AnimateCentering(std::span<const RoomVertices> rooms, MapPoint& center);

// _scaleRoomVertices: multiplies every vertex by `factor`, keeping each
// product's low halfword.
void ScaleRoomVertices(std::span<const RoomVertices> rooms,
                       std::int32_t factor);

// _getCurrentRoomIndex: the first room at `current`'s zone and map, or else
// the first visited room (0 when none). Absent `current` matches no room.
[[nodiscard]] InitialRoom InitialRoomOf(
    std::span<const MapRoom> rooms,
    const std::optional<Field::SceneRoomAddress>& current);

// _updateRoomIndex: follows the next (`search_forward` > 0) or previous links
// from `selected` until a visited room, playing the room-change sound when it
// differs from `selected`. Absent (refused, nothing played) when a link leaves
// the rooms or no visited room is reached within one pass, where the original
// reads out of bounds or loops forever.
[[nodiscard]] std::optional<std::int32_t> SelectAdjacentVisitedRoom(
    MapNavigationHost& host, std::span<const RoomLink> links,
    std::span<const MapRoom> rooms, std::int32_t selected,
    std::int32_t search_forward);
}  // namespace VS::Menu::map
