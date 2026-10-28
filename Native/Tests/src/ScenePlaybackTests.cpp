#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "CombatLab/Trace.hpp"

namespace {
using VS::combat_lab::SceneExpectation;
using VS::combat_lab::SceneTrial;

nlohmann::json ExpectedScene() {
  return {{"events", {80, 82, 81}},
          {"return_zone", 12},
          {"return_room", 0},
          {"preconditions",
           "Source-backed component; boss victory not established"},
          {"confirm_dialogue", true}};
}

VS::Game::EncounterState ReadyState() {
  VS::Game::EncounterState state;
  state.ready = true;
  state.zone = 12;
  state.room = 0;
  state.control_mode = 1;
  return state;
}

nlohmann::json Event(int id, int phase = 4) {
  return {{"available", true},
          {"event",
           {{"id", id},
            {"source", 0},
            {"phase", phase},
            {"skip_state", 0},
            {"forced_skip", false},
            {"dialogue_waits", {false}}}}};
}

SceneTrial Trial() {
  return SceneTrial(VS::combat_lab::DecodeSceneExpectation(ExpectedScene()));
}

void PlayExpectedEvents(SceneTrial& trial) {
  const auto state = ReadyState();
  trial.Observe(state, Event(80), 1);
  trial.Observe(state, Event(82), 2);
  trial.Observe(state, Event(81), 3);
}
}  // namespace

TEST_CASE(
    "Scene expectations require explicit source conditions and native IDs",
    "[scenes][playback]") {
  auto value = ExpectedScene();
  SECTION("valid") {
    const auto decoded = VS::combat_lab::DecodeSceneExpectation(value);
    CHECK(decoded.events == std::vector<int>{80, 82, 81});
    CHECK(decoded.confirm_dialogue);
  }
  SECTION("invalid") {
    SECTION("empty events") { value["events"] = nlohmann::json::array(); }
    SECTION("unknown option") { value["force_flags"] = true; }
    SECTION("boolean ID") { value["events"] = {true}; }
    SECTION("out of range ID") { value["events"] = {32768}; }
    SECTION("invalid source") { value["event_source"] = 3; }
    SECTION("negative external ID") { value["events"] = {-1}; }
    SECTION("overflowing room") { value["return_room"] = 2147483648LL; }
    SECTION("negative room") { value["return_room"] = -1; }
    SECTION("boolean zone") { value["return_zone"] = true; }
    SECTION("undocumented conditions") { value["preconditions"] = ""; }
    CHECK_THROWS_AS(VS::combat_lab::DecodeSceneExpectation(value),
                    std::runtime_error);
  }
}

TEST_CASE(
    "Scene acceptance requires ordered events and the full control interval",
    "[scenes][playback]") {
  auto trial = Trial();
  PlayExpectedEvents(trial);
  trial.Observe(ReadyState(), Event(81, 0), 4);
  trial.Observe(ReadyState(), Event(81, 0), 123);
  CHECK_FALSE(trial.Finished());
  trial.Observe(ReadyState(), Event(81, 0), 124);
  CHECK(trial.Result().at("status") == "passed");
  CHECK(trial.Result().at("observed_events") == nlohmann::json({80, 82, 81}));
  CHECK(trial.Result().at("control_tick") == 4);
  CHECK(trial.Result().at("passed_tick") == 124);
  CHECK(trial.Result().at("campaign_progression") == "not_established");
}

TEST_CASE(
    "Frame budgets and missing handoffs never constitute scene completion",
    "[scenes][playback]") {
  auto trial = Trial();
  SECTION("no scene started") { trial.Observe(ReadyState(), Event(80, 0), 1); }
  SECTION("wrong order") {
    trial.Observe(ReadyState(), Event(82), 1);
    trial.Observe(ReadyState(), Event(80), 2);
    trial.Observe(ReadyState(), Event(81), 3);
  }
  SECTION("scene still active") { PlayExpectedEvents(trial); }
  trial.Timeout();
  CHECK(trial.Result().at("status") == "blocked");
  CHECK_FALSE(trial.Result().at("reason").get<std::string>().empty());
  CHECK(trial.Result().at("passed_tick") == 0);
}

TEST_CASE(
    "Interrupted scene control observation resets the acceptance interval",
    "[scenes][playback]") {
  auto trial = Trial();
  PlayExpectedEvents(trial);
  auto state = ReadyState();
  auto scene = Event(81, 0);
  trial.Observe(state, scene, 4);
  SECTION("loading") { state.ready = false; }
  SECTION("transition") { state.transition = 1; }
  SECTION("control") { state.control_mode = 2; }
  SECTION("wrong room") { state.room = 1; }
  SECTION("wrong zone") { state.zone = 19; }
  SECTION("event") { scene["event"]["phase"] = 4; }
  SECTION("dialogue") { scene["event"]["dialogue_waits"] = {true}; }
  trial.Observe(state, scene, 123);
  CHECK(trial.Result().at("control_tick").is_null());
  trial.Observe(ReadyState(), Event(81, 0), 124);
  trial.Observe(ReadyState(), Event(81, 0), 243);
  CHECK_FALSE(trial.Finished());
  trial.Observe(ReadyState(), Event(81, 0), 244);
  CHECK(trial.Result().at("status") == "passed");
}

TEST_CASE("Repeated event IDs require an actual end and reentry",
          "[scenes][playback]") {
  auto expected = VS::combat_lab::DecodeSceneExpectation(ExpectedScene());
  expected.events = {80, 80};
  SceneTrial trial(expected);
  trial.Observe(ReadyState(), Event(80), 1);
  trial.Observe(ReadyState(), Event(80), 2);
  trial.Observe(ReadyState(), {{"available", false}}, 2);
  trial.Observe(ReadyState(), Event(80), 2);
  CHECK(trial.Result().at("matched_events") == 1);
  trial.Observe(ReadyState(), Event(80, 0), 3);
  trial.Observe(ReadyState(), Event(80), 4);
  CHECK(trial.Result().at("matched_events") == 2);
}

TEST_CASE(
    "Scene start requires executing the declared source not preparation or "
    "stale IDs",
    "[scenes][playback]") {
  auto trial = Trial();
  for (int phase = 1; phase <= 3; ++phase) {
    trial.Observe(ReadyState(), Event(80, phase), phase);
  }
  auto handoff = Event(80);
  handoff["event"]["source"] = 1;
  trial.Observe(ReadyState(), handoff, 4);
  CHECK(trial.Result().at("matched_events") == 0);
  trial.Observe(ReadyState(), Event(80), 5);
  CHECK(trial.Result().at("matched_events") == 1);
}

TEST_CASE("Skipped scenes and defeated fixtures cannot pass playback",
          "[scenes][playback]") {
  auto trial = Trial();
  auto state = ReadyState();
  auto scene = Event(80);
  SECTION("ordinary Start") { state.native_pressed = VS::Input::Start; }
  SECTION("forced skip") { scene["event"]["forced_skip"] = true; }
  SECTION("active skip") { scene["event"]["skip_state"] = 2; }
  SECTION("defeat") { state.game_over = true; }
  trial.Observe(state, scene, 1);
  CHECK(trial.Result().at("status") == "failed");
  CHECK(trial.Input(30) == 0);
}

TEST_CASE(
    "Scene dialogue assistance uses normal visible-wait Circle pulses only",
    "[scenes][playback]") {
  auto trial = Trial();
  auto scene = Event(80);
  CHECK(trial.Input(30) == 0);
  scene["event"]["dialogue_waits"] = {true};
  trial.Observe(ReadyState(), scene, 29);
  CHECK(trial.Input(30) == VS::Input::Circle);
  CHECK(trial.Input(31) == VS::Input::Circle);
  CHECK(trial.Input(32) == 0);
  CHECK(trial.Result().at("dialogue_confirm_pulses") == 1);
  scene["event"]["dialogue_waits"] = {false};
  trial.Observe(ReadyState(), scene, 59);
  CHECK(trial.Input(60) == 0);
  auto interactive = VS::combat_lab::DecodeSceneExpectation(ExpectedScene());
  interactive.confirm_dialogue = false;
  SceneTrial manual(interactive);
  scene["event"]["dialogue_waits"] = {true};
  manual.Observe(ReadyState(), scene, 29);
  CHECK(manual.Input(30) == 0);
}
