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

nlohmann::json ExpectedEnding() {
  auto value = ExpectedScene();
  value.erase("return_zone");
  value.erase("return_room");
  value["terminal"] = "ending_to_title";
  return value;
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

TEST_CASE("Scene endings accept only an explicit terminal contract",
          "[scenes][playback][ending-goal]") {
  auto value = ExpectedEnding();
  SECTION("valid") {
    const auto expected = VS::combat_lab::DecodeSceneExpectation(value);
    CHECK(expected.ending_to_title);
  }
  SECTION("invalid") {
    SECTION("unknown terminal") { value["terminal"] = "any_exit"; }
    SECTION("boolean terminal") { value["terminal"] = true; }
    SECTION("mixed room goal") { value["return_room"] = 0; }
    SECTION("mixed zone goal") { value["return_zone"] = 0; }
    CHECK_THROWS_AS(VS::combat_lab::DecodeSceneExpectation(value),
                    std::runtime_error);
  }
}

TEST_CASE("Ending goals never substitute a field-control interval",
          "[scenes][playback][ending-goal]") {
  SceneTrial trial(VS::combat_lab::DecodeSceneExpectation(ExpectedEnding()));
  PlayExpectedEvents(trial);
  trial.Observe(ReadyState(), Event(81, 0), 4);
  trial.Observe(ReadyState(), Event(81, 0), 124);
  CHECK_FALSE(trial.Finished());
  CHECK(trial.Result().at("control_tick").is_null());
  trial.ObserveEndingReturn(true, false, true, 140);
  CHECK(trial.Result().at("status") == "passed");
  CHECK(trial.Result().at("coverage") == "scene_playback_ending_to_title");
  CHECK(trial.Result().at("passed_tick") == 140);
  CHECK_FALSE(trial.Result().contains("return_zone"));
}

TEST_CASE("Ending acceptance requires completed unskipped native credits",
          "[scenes][playback][ending-goal]") {
  SceneTrial trial(VS::combat_lab::DecodeSceneExpectation(ExpectedEnding()));
  PlayExpectedEvents(trial);
  bool credits_finished = true;
  bool credits_skipped = false;
  bool clear_save_pending = true;
  SECTION("credits not finished") { credits_finished = false; }
  SECTION("credits skipped") { credits_skipped = true; }
  SECTION("no clear-save handoff") { clear_save_pending = false; }
  trial.ObserveEndingReturn(credits_finished, credits_skipped,
                            clear_save_pending, 140);
  CHECK(trial.Result().at("status") == "failed");
  CHECK_FALSE(trial.Result().at("reason").get<std::string>().empty());
  CHECK(trial.Result().at("passed_tick") == 0);
}

TEST_CASE("Ending flags cannot replace the expected source events",
          "[scenes][playback][ending-goal]") {
  SceneTrial trial(VS::combat_lab::DecodeSceneExpectation(ExpectedEnding()));
  trial.Observe(ReadyState(), Event(80), 1);
  trial.ObserveEndingReturn(true, false, true, 140);
  CHECK(trial.Result().at("status") == "failed");
  CHECK(trial.Result().at("matched_events") == 1);
}

TEST_CASE("Failed or timed-out endings cannot be upgraded by later flags",
          "[scenes][playback][ending-goal]") {
  SceneTrial trial(VS::combat_lab::DecodeSceneExpectation(ExpectedEnding()));
  PlayExpectedEvents(trial);
  SECTION("timeout") { trial.Timeout(); }
  SECTION("cancelled") { trial.Fail("cancelled"); }
  SECTION("skipped") {
    auto skipped = Event(81);
    skipped["event"]["forced_skip"] = true;
    trial.Observe(ReadyState(), skipped, 4);
  }
  const auto status = trial.Result().at("status");
  trial.ObserveEndingReturn(true, false, true, 140);
  CHECK(trial.Result().at("status") == status);
  CHECK(trial.Result().at("passed_tick") == 0);
}

TEST_CASE("Ordinary scene goals ignore terminal ending flags",
          "[scenes][playback][ending-goal]") {
  auto trial = Trial();
  PlayExpectedEvents(trial);
  trial.ObserveEndingReturn(true, false, true, 140);
  CHECK_FALSE(trial.Finished());
  CHECK(trial.Result().at("coverage") == "scene_playback_control");
  CHECK_FALSE(trial.Result().contains("terminal"));
}

namespace {
nlohmann::json ExpectedAttract() {
  auto value = ExpectedEnding();
  value["terminal"] = "attract_to_title";
  value["confirm_dialogue"] = false;
  return value;
}

nlohmann::json AttractSetup() {
  return {{"version", 1},
          {"zone", 20},
          {"room", 0},
          {"seed", 1},
          {"player", {{"x", 4}, {"z", 2}, {"elevation", 2}, {"direction", 0}}},
          {"enemies", nlohmann::json::array()},
          {"room_events", "production"},
          {"attract_demo", 3}};
}
}  // namespace

TEST_CASE("Attract demos accept only the original title-timeout start",
          "[scenes][playback][attract-goal]") {
  auto setup = AttractSetup();
  SECTION("valid cutscene 1") {
    const auto decoded = VS::combat_lab::DecodeSetup(setup);
    CHECK(decoded.attract_demo == 3);
    CHECK(VS::combat_lab::EncodeSetup(decoded).at("attract_demo") == 3);
  }
  SECTION("valid cutscene 2") {
    setup["attract_demo"] = 4;
    setup["zone"] = 2;
    setup["room"] = 1;
    CHECK(VS::combat_lab::DecodeSetup(setup).attract_demo == 4);
  }
  SECTION("invalid") {
    SECTION("unknown cause") { setup["attract_demo"] = 0; }
    SECTION("boolean cause") { setup["attract_demo"] = true; }
    SECTION("wrong door") { setup["zone"] = 21; }
    SECTION("other cause door") { setup["attract_demo"] = 4; }
    SECTION("moved entry") { setup["player"]["x"] = 0; }
    SECTION("isolated room") { setup["room_events"] = "isolated"; }
    SECTION("granted kit") { setup["ashley_profile"] = "post-prologue"; }
    SECTION("stat assist") { setup["ashley_stats"] = {{"max_hp", 999}}; }
    SECTION("direct event") { setup["external_event"] = 20; }
    SECTION("story progress") { setup["story_progress"] = 1; }
    CHECK_THROWS_AS(VS::combat_lab::DecodeSetup(setup), std::runtime_error);
  }
}

TEST_CASE("Attract terminals reject dialogue confirmation",
          "[scenes][playback][attract-goal]") {
  auto value = ExpectedAttract();
  CHECK(VS::combat_lab::DecodeSceneExpectation(value).attract_to_title);
  value["confirm_dialogue"] = true;
  CHECK_THROWS_AS(VS::combat_lab::DecodeSceneExpectation(value),
                  std::runtime_error);
}

TEST_CASE("Attract demos pass only on an unpressed native title return",
          "[scenes][playback][attract-goal]") {
  SceneTrial trial(VS::combat_lab::DecodeSceneExpectation(ExpectedAttract()));
  PlayExpectedEvents(trial);
  trial.Observe(ReadyState(), Event(81, 0), 124);
  CHECK_FALSE(trial.Finished());
  CHECK(trial.Input(120) == 0);
  SECTION("passes") {
    trial.ObserveAttractReturn(3, false, false, 140);
    CHECK(trial.Result().at("status") == "passed");
    CHECK(trial.Result().at("coverage") == "scene_playback_attract_to_title");
    CHECK(trial.Result().at("terminal").at("entry_mode") == 3);
    CHECK(trial.Result().at("passed_tick") == 140);
  }
  SECTION("ending handoff") {
    trial.ObserveAttractReturn(3, true, true, 140);
    CHECK(trial.Result().at("status") == "failed");
  }
  SECTION("ordinary entry mode") {
    trial.ObserveAttractReturn(0, false, false, 140);
    CHECK(trial.Result().at("status") == "failed");
  }
  SECTION("button interrupt") {
    auto pressed = ReadyState();
    pressed.native_pressed = 0x20;
    trial.Observe(pressed, Event(81, 0), 130);
    CHECK(trial.Result().at("status") == "failed");
    trial.ObserveAttractReturn(3, false, false, 140);
    CHECK(trial.Result().at("status") == "failed");
  }
  SECTION("ending observer ignored") {
    trial.ObserveEndingReturn(true, false, true, 140);
    CHECK_FALSE(trial.Finished());
  }
}

TEST_CASE("Attract returns cannot replace the expected source events",
          "[scenes][playback][attract-goal]") {
  SceneTrial trial(VS::combat_lab::DecodeSceneExpectation(ExpectedAttract()));
  trial.Observe(ReadyState(), Event(80), 1);
  trial.ObserveAttractReturn(3, false, false, 140);
  CHECK(trial.Result().at("status") == "failed");
  CHECK(trial.Result().at("matched_events") == 1);
}

TEST_CASE("Field-control goals ignore attract returns",
          "[scenes][playback][attract-goal]") {
  auto trial = Trial();
  PlayExpectedEvents(trial);
  trial.ObserveAttractReturn(3, false, false, 140);
  CHECK_FALSE(trial.Finished());
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
