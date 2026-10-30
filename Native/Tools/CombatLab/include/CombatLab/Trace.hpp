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
  // Optional room that must be current while each expected event executes;
  // room-local IDs (source 2) repeat across rooms along a door route.
  int event_zone = -1;
  int event_room = -1;
  std::string preconditions;
  bool confirm_dialogue = false;
  bool ending_to_title = false;
  // Title idle-timeout demo: the original game loop's entry-mode 3/4 reset
  // (vs_battle_exec, BATTLE 146C.c:7558) is the authored terminal.
  bool attract_to_title = false;
  // Ashley's defeat requests external event 0 (BATTLE 146C.c:7421); its
  // script opens the MENU7 game-over screen, whose Title choice resets
  // through vs_overlay_jumpToTitle (MENU7 260.c:3603).
  bool game_over_to_title = false;
};

SceneExpectation DecodeSceneExpectation(const nlohmann::json& value);

class SceneTrial {
 public:
  explicit SceneTrial(SceneExpectation expected);
  void Observe(const Game::EncounterState& state, const nlohmann::json& scene,
               std::uint64_t tick);
  // Only a natural native fiber return, never a stop request, supplies this.
  void ObserveEndingReturn(bool credits_finished, bool credits_skipped,
                           bool clear_save_pending, std::uint64_t tick);
  void ObserveAttractReturn(int entry_mode, bool credits_finished,
                            bool clear_save_pending, std::uint64_t tick);
  void ObserveGameOverReturn(bool credits_finished, bool clear_save_pending,
                             std::uint64_t tick);
  bool ExpectsEndingReturn() const { return expected_.ending_to_title; }
  bool ExpectsAttractReturn() const { return expected_.attract_to_title; }
  bool ExpectsGameOverReturn() const { return expected_.game_over_to_title; }
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
  bool dialogue_pulse_ = false;
  bool dialogue_held_ = false;
  bool event_active_ = false;
  std::uint64_t dialogue_pulses_ = 0;
  std::optional<std::uint64_t> game_over_menu_tick_;
  bool title_choice_pressed_ = false;
  nlohmann::json last_;
  nlohmann::json terminal_;
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
