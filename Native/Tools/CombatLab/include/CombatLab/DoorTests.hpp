#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Battle/Encounter.hpp"
#include "nlohmann/json.hpp"

namespace VS::combat_lab {

struct DoorTestCase {
  std::string id;
  Game::EncounterSetup setup;
  Game::EncounterDoor door;
  std::string unavailable;
};

std::vector<DoorTestCase> EnumerateDoorTests(
    const std::vector<Game::EncounterZone>& catalog);
nlohmann::json EncodeDoorTest(const DoorTestCase& test);

class DoorTrial {
 public:
  static constexpr std::uint64_t kObservationTicks = 120;
  explicit DoorTrial(DoorTestCase test);
  bool Observe(const Game::EncounterState& state, bool event_idle,
               std::uint64_t tick, bool dialogue_waiting = false);
  std::uint32_t Input(std::uint64_t tick);
  void Requested(const Game::EncounterDoorRequest& request, std::uint64_t tick);
  void Timeout();
  void Fail(std::string reason);
  bool Finished() const { return status_ != "running"; }
  nlohmann::json Result() const;

 private:
  DoorTestCase test_;
  std::string status_ = "running";
  std::string reason_;
  bool requested_ = false;
  bool loading_seen_ = false;
  std::uint64_t requested_tick_ = 0;
  std::uint64_t arrived_tick_ = 0;
  std::uint64_t control_tick_ = 0;
  std::uint64_t destination_frames_ = 0;
  int last_zone_ = -1;
  int last_room_ = -1;
  std::uint32_t last_control_ = 0;
  bool last_ready_ = false;
  bool dialogue_waiting_ = false;
  bool dialogue_held_ = false;
  std::uint64_t dialogue_pulses_ = 0;
};

}  // namespace VS::combat_lab
