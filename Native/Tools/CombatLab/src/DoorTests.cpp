#include "CombatLab/DoorTests.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "Input/Pad.hpp"

namespace VS::combat_lab {
namespace {
bool FindStart(const Game::EncounterRoom& room, Game::EncounterPosition& start,
               const Game::EncounterDoor& selected) {
  int best = std::numeric_limits<int>::max();
  for (int z = 0; z < room.depth; ++z) {
    for (int x = 0; x < room.width; ++x) {
      const auto index = static_cast<std::size_t>(z) * room.width + x;
      if (index >= room.floor.size() ||
          room.floor[index] != Game::EncounterFloor::kSupported ||
          std::ranges::any_of(room.doors, [x, z](const auto& door) {
            return door.tile_x == x && door.tile_z == z;
          })) {
        continue;
      }
      const int distance =
          std::abs(x - selected.tile_x) + std::abs(z - selected.tile_z);
      if (distance < best) {
        best = distance;
        start = {x, z, 0, 0};
      }
    }
  }
  return best != std::numeric_limits<int>::max();
}

std::string DestinationIssue(const std::vector<Game::EncounterZone>& catalog,
                             const Game::EncounterDoor& door) {
  const auto zone = std::ranges::find(catalog, door.destination_zone,
                                      &Game::EncounterZone::id);
  if (zone == catalog.end()) return "destination zone unavailable";
  const auto room = std::ranges::find(zone->rooms, door.destination_room,
                                      &Game::EncounterRoom::index);
  if (room == zone->rooms.end()) return "destination room unavailable";
  return room->unavailable_reason;
}
}  // namespace

std::vector<DoorTestCase> EnumerateDoorTests(
    const std::vector<Game::EncounterZone>& catalog) {
  std::vector<DoorTestCase> result;
  for (const auto& zone : catalog) {
    for (const auto& room : zone.rooms) {
      for (const auto& door : room.doors) {
        DoorTestCase test;
        test.id = std::format("z{:03}-r{:03}-d{:03}", zone.id, room.index,
                              door.index);
        test.setup.zone = zone.id;
        test.setup.room = room.index;
        test.setup.profile = Game::EncounterProfile::kPostPrologue;
        test.setup.room_policy = Game::EncounterRoomPolicy::kOriginalRoom;
        test.door = door;
        test.unavailable = room.unavailable_reason;
        if (test.unavailable.empty() &&
            !FindStart(room, test.setup.player, door)) {
          test.unavailable = "no supported non-door source fixture tile";
        }
        if (test.unavailable.empty()) {
          test.unavailable = DestinationIssue(catalog, door);
        }
        result.push_back(std::move(test));
      }
    }
  }
  return result;
}

nlohmann::json EncodeDoorTest(const DoorTestCase& test) {
  return {{"id", test.id},
          {"coverage", "loader_entry_event"},
          {"source_zone", test.setup.zone},
          {"source_room", test.setup.room},
          {"source_door", test.door.index},
          {"destination_zone", test.door.destination_zone},
          {"destination_room", test.door.destination_room},
          {"trigger", {test.door.tile_x, test.door.tile_z, test.door.layer}},
          {"entry",
           {test.door.destination.x, test.door.destination.z,
            test.door.destination.elevation, test.door.destination.direction}},
          {"entry_step", test.door.entry_step},
          {"door_id", test.door.door_id},
          {"required_flag", test.door.required_flag},
          {"required_value", test.door.required_value},
          {"unavailable", test.unavailable}};
}

DoorTrial::DoorTrial(DoorTestCase test) : test_(std::move(test)) {
  if (!test_.unavailable.empty()) {
    status_ = "blocked";
    reason_ = test_.unavailable;
  }
}

bool DoorTrial::Observe(const Game::EncounterState& state, bool event_idle,
                        std::uint64_t tick, bool dialogue_waiting) {
  if (Finished()) return false;
  dialogue_waiting_ = dialogue_waiting;
  last_zone_ = state.zone;
  last_room_ = state.room;
  last_control_ = state.control_mode;
  last_ready_ = state.ready;
  if (state.game_over) {
    status_ = "blocked";
    reason_ = "fixture defeated before transition acceptance";
    return false;
  }
  if (!requested_) {
    return state.ready && state.zone == test_.setup.zone &&
           state.room == test_.setup.room && state.control_mode == 1 &&
           state.transition == 0 && event_idle;
  }
  loading_seen_ |= !state.ready;
  if (!loading_seen_ || !state.ready ||
      state.zone != test_.door.destination_zone ||
      state.room != test_.door.destination_room) {
    control_tick_ = 0;
    return false;
  }
  if (arrived_tick_ == 0) arrived_tick_ = tick;
  ++destination_frames_;
  if (state.control_mode == 1 && state.transition == 0 && event_idle) {
    if (control_tick_ == 0) control_tick_ = tick;
    if (tick - control_tick_ >= kObservationTicks) status_ = "passed";
  } else {
    control_tick_ = 0;
  }
  return false;
}

std::uint32_t DoorTrial::Input(std::uint64_t tick) {
  constexpr std::uint64_t kDialoguePulsePeriod = 30;
  constexpr std::uint64_t kDialogueHoldTicks = 2;
  const bool held = !Finished() && dialogue_waiting_ &&
                    tick % kDialoguePulsePeriod < kDialogueHoldTicks;
  if (held && !dialogue_held_) ++dialogue_pulses_;
  dialogue_held_ = held;
  return held ? VS::Input::Circle : 0;
}

void DoorTrial::Requested(const Game::EncounterDoorRequest& request,
                          std::uint64_t tick) {
  if (request.status == Game::EncounterDoorRequestStatus::kAccepted) {
    requested_ = true;
    requested_tick_ = tick;
    reason_.clear();
  } else if (request.status != Game::EncounterDoorRequestStatus::kBusy) {
    status_ = request.status == Game::EncounterDoorRequestStatus::kBlocked
                  ? "blocked"
                  : "failed";
    reason_ = request.reason;
  } else {
    reason_ = request.reason;
  }
}

void DoorTrial::Timeout() {
  if (Finished()) return;
  status_ = requested_ && arrived_tick_ == 0 ? "failed" : "blocked";
  reason_ = !requested_
                ? "source entry event did not restore control before budget"
            : arrived_tick_ == 0
                ? "requested destination did not become ready"
                : "destination entry event did not settle before budget";
}

void DoorTrial::Fail(std::string reason) {
  status_ = "failed";
  reason_ = std::move(reason);
}

nlohmann::json DoorTrial::Result() const {
  auto result = EncodeDoorTest(test_);
  result["status"] = status_;
  result["reason"] = reason_;
  result["requested_tick"] = requested_tick_;
  result["arrived_tick"] = arrived_tick_;
  result["control_tick"] = control_tick_;
  result["destination_frames"] = destination_frames_;
  result["last_zone"] = last_zone_;
  result["last_room"] = last_room_;
  result["last_control"] = last_control_;
  result["last_ready"] = last_ready_;
  result["dialogue_confirm_pulses"] = dialogue_pulses_;
  return result;
}

}  // namespace VS::combat_lab
