// Hand-maintained (tools/port/owned.csv).
#include "Menu/MapNavigation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Field/MinimapScene.hpp"

namespace VS::Menu::map {
namespace {
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingSubtract;
using Core::Math::WrapSigned16;

constexpr std::int16_t kEmptyMinimum = 0x7FFF;
constexpr std::int16_t kEmptyMaximum = -0x8000;
constexpr std::int32_t kWholeStepBelow = 8;
constexpr std::int32_t kFixedStep = 8;
constexpr std::int32_t kFixedStepUpTo = 0x80;
constexpr std::int32_t kEighthRounding = 7;
constexpr unsigned kEighthShift = 3;

[[nodiscard]] std::int16_t Wrap(std::int32_t value) {
  return WrapSigned16(static_cast<std::uint32_t>(value));
}

// (low + high) / 2, rounded toward zero as the original's add-sign-and-shift.
[[nodiscard]] std::int16_t Midpoint(std::int16_t low, std::int16_t high) {
  const std::int32_t sum = WrappingAdd(low, high);
  const auto round =
      static_cast<std::int32_t>(static_cast<std::uint32_t>(sum) >> 31U);
  return Wrap(WrappingAdd(sum, round) >> 1U);
}

[[nodiscard]] std::int16_t StepOf(std::int16_t remaining) {
  const std::int32_t value = remaining;
  const std::int32_t magnitude = std::abs(value);
  if (magnitude < kWholeStepBelow) return remaining;
  if (magnitude > kFixedStepUpTo) {
    return Wrap((value < 0 ? value + kEighthRounding : value) >> kEighthShift);
  }
  return Wrap(value > 0 ? kFixedStep : -kFixedStep);
}

void Translate(std::span<const RoomVertices> rooms, MapPoint offset) {
  for (const RoomVertices room : rooms) {
    for (MapVertex& vertex : room) {
      vertex.x = Wrap(WrappingSubtract(vertex.x, offset.x));
      vertex.y = Wrap(WrappingSubtract(vertex.y, offset.y));
      vertex.z = Wrap(WrappingSubtract(vertex.z, offset.z));
    }
  }
}
}  // namespace

// decomp: MENU5.PRG func_8010467C (src/MENU/MENU5.PRG/4D8.c:1054).
void MarkAllRoomsVisited(std::span<MapRoom> rooms) {
  for (MapRoom& room : rooms) room.visited = 1;
}

// decomp: MENU5.PRG setCenterpoint (src/MENU/MENU5.PRG/4D8.c:1583; was
// _setCenterpoint).
MapPoint RoomCenterOf(std::span<const MapVertex> vertices) {
  MapVertex minimum{kEmptyMinimum, kEmptyMinimum, kEmptyMinimum};
  MapVertex maximum{kEmptyMaximum, kEmptyMaximum, kEmptyMaximum};
  for (const MapVertex& vertex : vertices) {
    minimum.x = std::min(minimum.x, vertex.x);
    minimum.y = std::min(minimum.y, vertex.y);
    minimum.z = std::min(minimum.z, vertex.z);
    maximum.x = std::max(maximum.x, vertex.x);
    maximum.y = std::max(maximum.y, vertex.y);
    maximum.z = std::max(maximum.z, vertex.z);
  }
  return {.x = Midpoint(minimum.x, maximum.x),
          .y = maximum.y,
          .z = Midpoint(minimum.z, maximum.z)};
}

// decomp: MENU5.PRG snapMapToRoom (src/MENU/MENU5.PRG/4D8.c:1516; was
// _snapMapToRoom).
bool CenterOnRoom(std::span<const RoomVertices> rooms, std::size_t room,
                  MapPoint& center) {
  if (room >= rooms.size()) return false;
  center = RoomCenterOf(rooms[room]);
  Translate(rooms, center);
  center = {};
  return true;
}

// decomp: MENU5.PRG smoothMapToCenterpoint (src/MENU/MENU5.PRG/4D8.c:1634; was
// _smoothMapToCenterpoint), the step.
MapPoint CenteringStepOf(MapPoint remaining) {
  return {.x = StepOf(remaining.x),
          .y = StepOf(remaining.y),
          .z = StepOf(remaining.z)};
}

// decomp: MENU5.PRG smoothMapToCenterpoint (src/MENU/MENU5.PRG/4D8.c:1634; was
// _smoothMapToCenterpoint).
void AnimateCentering(std::span<const RoomVertices> rooms, MapPoint& center) {
  if (center == MapPoint{}) return;
  const MapPoint step = CenteringStepOf(center);
  Translate(rooms, step);
  center = {.x = Wrap(WrappingSubtract(center.x, step.x)),
            .y = Wrap(WrappingSubtract(center.y, step.y)),
            .z = Wrap(WrappingSubtract(center.z, step.z))};
}

// decomp: MENU5.PRG scaleRoomVertices (src/MENU/MENU5.PRG/4D8.c:1733; was
// _scaleRoomVertices).
void ScaleRoomVertices(std::span<const RoomVertices> rooms,
                       std::int32_t factor) {
  for (const RoomVertices room : rooms) {
    for (MapVertex& vertex : room) {
      vertex.x = Wrap(WrappingMultiply(vertex.x, factor));
      vertex.y = Wrap(WrappingMultiply(vertex.y, factor));
      vertex.z = Wrap(WrappingMultiply(vertex.z, factor));
    }
  }
}

// decomp: MENU5.PRG getCurrentRoomIndex (src/MENU/MENU5.PRG/4D8.c:1839; was
// _getCurrentRoomIndex).
InitialRoom InitialRoomOf(
    std::span<const MapRoom> rooms,
    const std::optional<Field::SceneRoomAddress>& current) {
  if (current.has_value()) {
    const auto here = std::ranges::find_if(rooms, [&](const MapRoom& room) {
      return room.zone_id == current->zone_id && room.map_id == current->map_id;
    });
    if (here != rooms.end()) {
      const auto index = static_cast<std::int32_t>(here - rooms.begin());
      return {.selected = index, .current = index};
    }
  }
  const auto visited = std::ranges::find_if(
      rooms, [](const MapRoom& room) { return room.visited != 0; });
  return {.selected = visited != rooms.end()
                          ? static_cast<std::int32_t>(visited - rooms.begin())
                          : 0,
          .current = -1};
}

// decomp: MENU5.PRG updateRoomIndex (src/MENU/MENU5.PRG/4D8.c:1867; was
// _updateRoomIndex).
std::optional<std::int32_t> SelectAdjacentVisitedRoom(
    MapNavigationHost& host, std::span<const RoomLink> links,
    std::span<const MapRoom> rooms, std::int32_t selected,
    std::int32_t search_forward) {
  const std::size_t count = std::min(links.size(), rooms.size());
  std::int32_t index = selected;
  for (std::size_t step = 0; step < count; ++step) {
    if (index < 0 || static_cast<std::size_t>(index) >= count) {
      return std::nullopt;
    }
    const RoomLink& link = links[static_cast<std::size_t>(index)];
    index = search_forward > 0 ? link.next : link.previous;
    if (index < 0 || static_cast<std::size_t>(index) >= count) {
      return std::nullopt;
    }
    if (rooms[static_cast<std::size_t>(index)].visited != 0) {
      if (index != selected) {
        host.PlaySound(kRoomChangeSound, kRoomChangeSoundVolume);
      }
      return index;
    }
  }
  return std::nullopt;
}
}  // namespace VS::Menu::map
