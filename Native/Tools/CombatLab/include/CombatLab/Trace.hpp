#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Battle/Encounter.hpp"
#include "CombatLab/DoorTests.hpp"
#include "Diagnostics/GameplayTrace.hpp"
#include "nlohmann/json.hpp"

namespace VS::combat_lab {

inline constexpr std::uint64_t kScriptStopGraceTicks = 600;

using Trace = Game::GameplayTrace;
using Game::EncodeActor;

nlohmann::json EncodeSetup(const Game::EncounterSetup& setup);
Game::EncounterSetup DecodeSetup(const nlohmann::json& json);

struct ScriptPress {
  std::uint64_t tick = 0;
  std::uint32_t buttons = 0;
};

struct SceneExpectation {
  std::vector<int> events;
  int event_source = 0;
  int return_zone = 0;
  int return_room = 0;
  std::string preconditions;
  bool confirm_dialogue = false;
};

SceneExpectation DecodeSceneExpectation(const nlohmann::json& value);

class SceneTrial {
 public:
  explicit SceneTrial(SceneExpectation expected);
  void Observe(const Game::EncounterState& state, const nlohmann::json& scene,
               std::uint64_t tick);
  std::uint32_t Input(std::uint64_t tick);
  void Timeout();
  void Fail(std::string reason);
  bool Finished() const { return status_ != "running"; }
  nlohmann::json Result() const;

 private:
  SceneExpectation expected_;
  std::size_t next_event_ = 0;
  std::vector<int> observed_;
  std::string status_ = "running";
  std::string reason_;
  std::optional<std::uint64_t> control_tick_;
  std::uint64_t passed_tick_ = 0;
  bool dialogue_waiting_ = false;
  bool dialogue_held_ = false;
  bool event_active_ = false;
  std::uint64_t dialogue_pulses_ = 0;
  nlohmann::json last_;
};

struct EncounterScript {
  std::string name;
  std::filesystem::path scenario;
  Game::EncounterSetup setup;
  std::uint64_t frames = 0;
  std::vector<ScriptPress> presses;
  std::optional<DoorTestCase> door;
  std::optional<SceneExpectation> scene;
};

std::uint64_t ParseTick(std::string_view value);
std::vector<ScriptPress> DecodePresses(std::string_view value);
std::vector<EncounterScript> ReadScriptSequence(
    const std::filesystem::path& path);

}  // namespace VS::combat_lab
