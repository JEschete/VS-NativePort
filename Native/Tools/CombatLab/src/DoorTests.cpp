#include "CombatLab/DoorTests.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
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

std::string ResultIdentity(const nlohmann::json& result) {
  if (!result.is_object() || !result.contains("door") ||
      !result.at("door").is_object() || !result.contains("name") ||
      !result.at("name").is_string() || !result.at("door").contains("id") ||
      result.at("door").at("id") != result.at("name")) {
    throw std::runtime_error("door checkpoint has an invalid case identity");
  }
  const auto name = result.at("name").get<std::string>();
  if (name.empty()) throw std::runtime_error("door checkpoint case is unnamed");
  return name;
}

void RequirePassedResult(const nlohmann::json& result) {
  if (result.value("status", "") != "passed" ||
      result.at("door").value("status", "") != "passed" ||
      !result.value("initialized", false) ||
      !result.value("teardown_complete", false) ||
      !result.contains("engine_errors") || result.at("engine_errors") != 0) {
    throw std::runtime_error(
        "door checkpoint pass lacks successful initialization/teardown");
  }
  const auto& control = result.at("door").at("control_tick");
  const auto& ticks = result.at("ticks");
  if (!control.is_number_integer() || control <= 0 ||
      !ticks.is_number_integer() || ticks < control ||
      ticks.get<std::uint64_t>() - control.get<std::uint64_t>() <
          DoorTrial::kObservationTicks) {
    throw std::runtime_error(
        "door checkpoint pass lacks the required observation interval");
  }
}

bool SameDoorContract(const DoorTestCase& test, const nlohmann::json& result) {
  const auto expected = EncodeDoorTest(test);
  const auto& actual = result.at("door");
  for (const auto& [key, value] : expected.items()) {
    if (!actual.contains(key)) {
      throw std::runtime_error("door checkpoint pass lacks field: " + key);
    }
    if (actual.at(key) != value) return false;
  }
  return true;
}

nlohmann::json ReadCheckpointJson(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("cannot open door checkpoint: " + path.string());
  }
  return nlohmann::json::parse(input);
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

nlohmann::json DecodeDoorCheckpoint(const nlohmann::json& summary,
                                    const nlohmann::json& provenance) {
  if (!summary.is_object() || summary.value("schema", 0) != 1 ||
      summary.value("coverage", "") != "loader_entry_event" ||
      !summary.contains("scripts") || !summary.at("scripts").is_array() ||
      !provenance.is_object()) {
    throw std::runtime_error(
        "resume requires a schema 1 door result checkpoint");
  }
  auto passed = nlohmann::json::array();
  std::set<std::string> seen;
  const auto add = [&passed, &seen](const nlohmann::json& entry) {
    const auto& result = entry.at("result");
    const auto id = ResultIdentity(result);
    RequirePassedResult(result);
    if (!entry.at("provenance").is_object() || !seen.insert(id).second) {
      throw std::runtime_error(
          "door checkpoint has duplicate/invalid prior passes");
    }
    passed.push_back(entry);
  };
  if (summary.contains("retained_passed")) {
    if (!summary.at("retained_passed").is_array()) {
      throw std::runtime_error(
          "door checkpoint retained passes must be an array");
    }
    for (const auto& entry : summary.at("retained_passed")) add(entry);
  }
  for (const auto& result : summary.at("scripts")) {
    const auto id = ResultIdentity(result);
    const auto status = result.at("status").get<std::string>();
    if (status == "passed") {
      add({{"result", result}, {"provenance", provenance}});
    } else {
      if (status != "blocked" && status != "failed" && status != "running" &&
          status != "not_run" && status != "cancelled") {
        throw std::runtime_error("door checkpoint has unknown status: " +
                                 status);
      }
      if (!seen.insert(id).second) {
        throw std::runtime_error("door checkpoint has duplicate case: " + id);
      }
    }
  }
  return passed;
}

nlohmann::json ReadDoorCheckpoint(const std::filesystem::path& path) {
  const auto absolute = std::filesystem::absolute(path).lexically_normal();
  const auto summary = ReadCheckpointJson(absolute);
  const auto manifest =
      ReadCheckpointJson(absolute.parent_path() / "manifest.json");
  const auto provenance =
      nlohmann::json{{"checkpoint", absolute.string()},
                     {"revision", manifest.at("revision")},
                     {"worktree", manifest.at("worktree")},
                     {"source_status", manifest.at("source_status")},
                     {"data_root", manifest.at("data_root")}};
  return DecodeDoorCheckpoint(summary, provenance);
}

DoorResumePlan PlanDoorResume(const std::vector<DoorTestCase>& tests,
                              const DoorSelection& selection,
                              const nlohmann::json& passed) {
  if (tests.empty() || !passed.is_array() ||
      (!selection.first.empty() && !selection.only.empty())) {
    throw std::runtime_error("invalid door resume selection");
  }
  std::map<std::string, const DoorTestCase*> indexed;
  for (const auto& test : tests) {
    if (!indexed.emplace(test.id, &test).second) {
      throw std::runtime_error("duplicate current door identity: " + test.id);
    }
  }
  const auto& selector =
      selection.only.empty() ? selection.first : selection.only;
  if (!selector.empty() && !indexed.contains(selector)) {
    throw std::runtime_error("door selector has no inventory match");
  }
  DoorResumePlan plan;
  std::set<std::string> retained;
  std::set<std::string> seen_passes;
  for (const auto& entry : passed) {
    const auto& result = entry.at("result");
    const auto id = ResultIdentity(result);
    RequirePassedResult(result);
    if (!seen_passes.insert(id).second) {
      throw std::runtime_error("duplicate resumed door pass: " + id);
    }
    const auto found = indexed.find(id);
    if (found != indexed.end() && SameDoorContract(*found->second, result)) {
      plan.retained_passed.push_back(entry);
      retained.insert(id);
    } else {
      plan.invalidated.push_back(id);
    }
  }
  bool started = selector.empty();
  for (const auto& test : tests) {
    started |= test.id == selector;
    if (!started || (!selection.only.empty() && test.id != selection.only) ||
        retained.contains(test.id)) {
      continue;
    }
    if (selection.limit != 0 && plan.pending.size() >= selection.limit) break;
    plan.pending.push_back(test);
  }
  return plan;
}

nlohmann::json EncodeDoorCheckpoint(const nlohmann::json& results,
                                    const nlohmann::json& retained_passed,
                                    std::string_view status) {
  nlohmann::json counts = nlohmann::json::object();
  for (const auto& result : results) {
    const auto outcome = result.at("status").get<std::string>();
    counts[outcome] = counts.value(outcome, 0) + 1;
  }
  return {{"schema", 1},
          {"status", status},
          {"coverage", "loader_entry_event"},
          {"selected_cases", results.size()},
          {"scripts", results},
          {"counts", std::move(counts)},
          {"retained_passed_count", retained_passed.size()},
          {"retained_passed", retained_passed}};
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
