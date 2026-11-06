// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomState.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace VS::Field {
namespace {
// Sets `id`'s bit; false when the bitset has no such bit.
bool MarkVisited(std::span<std::uint32_t> words, int id) {
  if (id < 0 || static_cast<std::size_t>(id) >= words.size() * kVisitWordBits) {
    return false;
  }
  const auto bit = static_cast<std::size_t>(id);
  words[bit / kVisitWordBits] |= std::uint32_t{1} << (bit % kVisitWordBits);
  return true;
}

bool SetOwnedRoomDisplayed(RoomDisplay& display, int owner_room_index,
                           bool displayed) {
  if (!display.loaded || display.owner_room_index != owner_room_index) {
    return false;
  }
  display.displayed = displayed;
  return true;
}
}  // namespace

int FindLocationId(std::span<const LocationDestination> locations, int zone_id,
                   int room_id) {
  for (std::size_t id = 0; id < locations.size(); ++id) {
    if (zone_id == locations[id].zone_id && room_id == locations[id].room_id) {
      return static_cast<int>(id);
    }
  }
  return kUnknownLocationId;
}

int CurrentRoomId(std::span<const LocationDestination> locations,
                  const CurrentRoom& room) {
  return FindLocationId(locations, room.zone_id, room.room_id);
}

void MarkCurrentRoomVisited(RoomVisitState& state, RoomVisitHost& host) {
  if (!state.pending) {
    return;
  }
  const int scene = host.CurrentSceneId();
  if (!MarkVisited(state.visited.areas, scene)) {
    host.ReportRefusedVisit("area", scene);
  }
  const int room = host.CurrentRoomId();
  state.pending = false;
  if (!MarkVisited(state.visited.rooms, room)) {
    host.ReportRefusedVisit("room", room);
  }
}

void CancelPendingRoomVisit(RoomVisitState& state) { state.pending = false; }

bool EnableOwnedRoomRendering(RoomDisplay& display, int owner_room_index) {
  return SetOwnedRoomDisplayed(display, owner_room_index, true);
}

bool DisableOwnedRoomRendering(RoomDisplay& display, int owner_room_index) {
  return SetOwnedRoomDisplayed(display, owner_room_index, false);
}

bool ShowPrimaryRoomGeometry(RoomDisplay& display) {
  return EnableOwnedRoomRendering(display, kPrimaryRoomOwner);
}

bool HidePrimaryRoomGeometry(RoomDisplay& display) {
  return DisableOwnedRoomRendering(display, kPrimaryRoomOwner);
}

void InitializeRoomRuntimeState(RoomRuntimeHost& host,
                                const RoomRuntimeDefaults& defaults) {
  for (const RoomRecord record : kClearedRoomRecords) {
    host.Clear(record);
  }
  for (const RoomBuffer buffer : kRoomBuffers) {
    host.Allocate(buffer);
  }
  host.ApplyDefaults(defaults);
  host.InitializeRoomBackground();
  host.ResetGimmickAnimationQueue();
  host.ApplyBackgroundReferenceColors();
}
}  // namespace VS::Field
