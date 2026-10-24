#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "CombatLab/Trace.hpp"
#include "Legacy/Psx.h"
#include "Platform/App.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/EncounterHooks.h"
#include "Boot/GameLoop.h"
#include "Event/ScriptVm.h"
#include "Field/GameMode.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Items/Loot.h"
#include "Menu/Framework/Framework.h"
}

namespace {
struct TraceScope {
  std::filesystem::path parent =
      std::filesystem::temp_directory_path() /
      ("vs-combat-trace-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::path run;
  std::filesystem::path events;
  std::filesystem::path log;
  std::vector<std::filesystem::path> files;
  std::vector<std::filesystem::path> directories;

  std::filesystem::path Write(std::string_view name,
                              const nlohmann::json& value) {
    std::filesystem::create_directories(parent);
    const auto path = parent / name;
    files.push_back(path);
    std::ofstream stream(path);
    stream << value.dump();
    stream.flush();
    if (!stream) throw std::runtime_error("cannot write test sequence fixture");
    return path;
  }

  ~TraceScope() {
    VS::Core::SetLogLevel(VS::Core::LogLevel::Info);
    VS::Core::SetLogChannels("");
    std::error_code error;
    for (const auto& path : files) std::filesystem::remove(path, error);
    for (const auto& path : directories) std::filesystem::remove(path, error);
    if (!events.empty()) std::filesystem::remove(events, error);
    if (!log.empty()) std::filesystem::remove(log, error);
    if (!run.empty()) std::filesystem::remove(run, error);
    std::filesystem::remove(parent, error);
  }
};

VS::Game::EncounterState SampleState() {
  VS::Game::EncounterState state;
  state.ready = true;
  state.rewards_available = false;
  VS::Game::EncounterActorState actor;
  actor.slot = 2;
  actor.spawn_index = 3;
  actor.hp = 0;
  actor.defeated = true;
  actor.ai_present = true;
  actor.ai_target = 0;
  actor.ai_candidates = 2;
  actor.ai_engagement_frames = 32;
  actor.ai_movement_mode = 1;
  actor.ai_destination_x = 4;
  actor.ai_destination_z = 9;
  state.actors.push_back(actor);
  state.pending_loot.push_back({2, {{0, 323, 2}, {2, 324, 1}}});
  return state;
}

nlohmann::json FlushedFrame(TraceScope& scope,
                            const VS::Game::EncounterState& state) {
  VS::combat_lab::Trace trace(scope.parent);
  scope.run = trace.Directory();
  scope.events = scope.run / "events.jsonl";
  scope.log = scope.run / "session.log";
  VS_LOG_TRACE("combat.test", "retained buffered diagnostic");
  trace.Frame(42, {}, state);
  REQUIRE_FALSE(trace.Failed());
  std::ifstream stream(scope.events);
  std::string line;
  nlohmann::json frame;
  while (std::getline(stream, line)) {
    const auto record = nlohmann::json::parse(line);
    if (record.at("category") == "frame") frame = record;
  }
  REQUIRE_FALSE(frame.is_null());
  std::ifstream text_log(scope.log);
  const std::string text((std::istreambuf_iterator<char>(text_log)),
                         std::istreambuf_iterator<char>());
  CHECK(text.find("retained buffered diagnostic") != std::string::npos);
  return frame;
}
void CheckItemAttributes(const nlohmann::json& snapshot) {
  CHECK(snapshot.at("strength") == 90);
  CHECK(snapshot.at("total_strength") == 100);
  CHECK(snapshot.at("intelligence") == 105);
  CHECK(snapshot.at("total_intelligence") == 105);
  CHECK(snapshot.at("agility") == 99);
  CHECK(snapshot.at("total_agility") == 99);
  CHECK(snapshot.at("walk_speed") == 4);
  CHECK(snapshot.at("run_speed") == 6);
}

nlohmann::json DescribeScript(const VS::combat_lab::EncounterScript& script) {
  return {{"name", script.name},
          {"scenario", script.scenario.filename().string()},
          {"setup", VS::combat_lab::EncodeSetup(script.setup)},
          {"frames", script.frames},
          {"press_count", script.presses.size()}};
}

bool CompleteScriptLog(const std::filesystem::path& path, std::size_t number) {
  std::ifstream stream(path);
  if (!stream) throw std::runtime_error("script trace is missing");
  std::string line;
  bool frame = false;
  bool ended = false;
  bool first = true;
  std::int64_t previous_elapsed = 0;
  while (std::getline(stream, line)) {
    const auto record = nlohmann::json::parse(line);
    const auto elapsed = record.at("elapsed_us").get<std::int64_t>();
    if (record.at("script") != number || elapsed < previous_elapsed ||
        (first && record.at("category") != "session.begin")) {
      return false;
    }
    first = false;
    previous_elapsed = elapsed;
    if (record.at("category") == "frame") {
      if (record.at("script_tick") != 2) return false;
      frame = true;
    }
    ended = record.at("category") == "session.end";
  }
  return frame && ended;
}
}  // namespace

TEST_CASE("Combat Lab trace flushes read-only targeting state",
          "[combat][encounter][trace]") {
  TraceScope scope;
  const auto state = SampleState();
  const auto frame = FlushedFrame(scope, state);
  CHECK(frame.at("tick") == 42);
  const auto& data = frame.at("data");
  CHECK(data.at("rewards_available") == false);
  const auto& snapshot = data.at("actors").at(0);
  CHECK(snapshot.at("spawn_index") == 3);
  CHECK(snapshot.at("defeated") == true);
  CHECK(snapshot.at("ai").at("target") == 0);
  CHECK(snapshot.at("ai").at("candidates") == 2);
  CHECK(snapshot.at("ai").at("engagement_frames") == 32);
  CHECK(snapshot.at("ai").at("destination") == nlohmann::json::array({4, 9}));
  CHECK(state.actors.front().ai_target == 0);
}

TEST_CASE(
    "Combat Lab trace distinguishes pending drops from inventory delivery",
    "[combat][encounter][trace]") {
  TraceScope scope;
  const auto state = SampleState();
  const auto frame = FlushedFrame(scope, state);
  const auto& data = frame.at("data");
  CHECK(data.at("rewards_available") == false);
  const auto& loot = data.at("pending_loot").at(0);
  CHECK(loot.at("source_actor") == 2);
  CHECK(loot.at("items").at(0).at("id") == 323);
  CHECK(loot.at("items").at(0).at("count") == 2);
  CHECK(loot.at("items").at(1).at("slot") == 2);
  CHECK(data.at("items").empty());
  CHECK(state.pending_loot.front().items.front().count == 2);
}

TEST_CASE("Combat Lab trace preserves read-only game-over and render state",
          "[combat][encounter][trace]") {
  TraceScope scope;
  auto state = SampleState();
  state.game_over = true;
  state.field_rendering_enabled = false;
  state.menu_state = 7;
  state.menu_highlighted_row = 3;
  state.frame_buffer = 1;
  const auto frame = FlushedFrame(scope, state);
  const auto& data = frame.at("data");
  CHECK(data.at("game_over") == true);
  CHECK(data.at("field_rendering_enabled") == false);
  CHECK(data.at("menu_state") == 7);
  CHECK(data.at("menu_highlighted_row") == 3);
  CHECK(data.at("frame_buffer") == 1);
  CHECK(state.game_over);
  CHECK(state.menu_state == 7);
}

TEST_CASE("Combat Lab trace records floor trigger visibility",
          "[combat][encounter][trace][items]") {
  TraceScope scope;
  auto state = SampleState();
  state.floor_triggers.push_back({3, 5, 8, 1, 128});
  const auto data = FlushedFrame(scope, state).at("data");
  const auto trigger = data.at("floor_triggers").at(0);
  CHECK(trigger.at("kind") == 3);
  CHECK(trigger.at("tile_x") == 5);
  CHECK(trigger.at("tile_z") == 8);
  CHECK(trigger.at("activation") == 1);
  CHECK(trigger.at("marker_fade") == 128);
  CHECK(state.floor_triggers.front().marker_fade == 128);
}

TEST_CASE("Combat Lab trace preserves area targeting coordinates",
          "[combat][encounter][trace][items]") {
  TraceScope scope;
  auto state = SampleState();
  state.action_phase = 2;
  state.area_center = {512, -128, 768};
  state.target_previews.push_back({2, -1, {640, -144, 800}});
  const auto data = FlushedFrame(scope, state).at("data");
  CHECK(data.at("area_center") == nlohmann::json::array({512, -128, 768}));
  const auto target = data.at("target_previews").at(0);
  CHECK(target.at("actor") == 2);
  CHECK(target.at("limb") == -1);
  CHECK(target.at("position") == nlohmann::json::array({640, -144, 800}));
  CHECK(state.target_previews.front().position[1] == -144);
}

TEST_CASE("Combat Lab trace records item effect observables",
          "[combat][encounter][trace][items]") {
  TraceScope scope;
  auto state = SampleState();
  auto& actor = state.actors.front();
  actor.max_hp = 251;
  actor.max_mp = 101;
  actor.strength = 90;
  actor.total_strength = 100;
  actor.intelligence = actor.total_intelligence = 105;
  actor.agility = actor.total_agility = 99;
  actor.walk_speed = 4;
  actor.run_speed = 6;
  actor.statuses = 0x4000;
  actor.status_timers[5] = 30;
  state.learned_actions = {54, 97};
  const auto data = FlushedFrame(scope, state).at("data");
  const auto snapshot = data.at("actors").at(0);
  CHECK(snapshot.at("max_hp") == 251);
  CHECK(snapshot.at("max_mp") == 101);
  CheckItemAttributes(snapshot);
  CHECK(snapshot.at("statuses") == 0x4000);
  CHECK(snapshot.at("status_timers").at(5) == 30);
  CHECK(data.at("learned_actions") == nlohmann::json::array({54, 97}));
}

TEST_CASE("Combat Lab scripts retain relative controller inputs",
          "[combat][encounter][sequence]") {
  const auto presses = VS::combat_lab::DecodePresses("2:Circle,7:Up+Cross");
  REQUIRE(presses.size() == 2);
  CHECK(presses[0].tick == 2);
  CHECK(presses[0].buttons == VS::Input::Circle);
  CHECK(presses[1].tick == 7);
  CHECK(presses[1].buttons == (VS::Input::Up | VS::Input::Cross));
  CHECK(VS::combat_lab::DecodePresses("").empty());
  for (const auto* invalid :
       {"-1:Circle", "1:Unknown", "1:Circle+Unknown", "1:Circle+", "1:Circle,",
        ":Circle", "3", "18446744073709551616:Cross"}) {
    CHECK_THROWS(VS::combat_lab::DecodePresses(invalid));
  }
}

TEST_CASE("Combat Lab sequences resolve presets beside their manifest",
          "[combat][encounter][sequence]") {
  TraceScope scope;
  VS::Game::EncounterSetup setup;
  setup.zone = 9;
  setup.room = 2;
  setup.seed = 17;
  const auto first =
      scope.Write("first.json", VS::combat_lab::EncodeSetup(setup));
  scope.Write("second.json", VS::combat_lab::EncodeSetup(setup));
  const auto path = scope.Write(
      "sequence.json", {{"version", 1},
                        {"scripts",
                         {{{"name", "first pass"},
                           {"scenario", "first.json"},
                           {"frames", 60},
                           {"press", "2:Circle,7:Up+Cross"}},
                          {{"scenario", "second.json"}, {"frames", 1}}}}});
  const auto scripts = VS::combat_lab::ReadScriptSequence(path);
  REQUIRE(scripts.size() == 2);
  CHECK(scripts[0].scenario == first);
  CHECK((DescribeScript(scripts[0]) ==
         nlohmann::json{{"name", "first pass"},
                        {"scenario", "first.json"},
                        {"setup", VS::combat_lab::EncodeSetup(setup)},
                        {"frames", 60},
                        {"press_count", 2}}));
  CHECK((DescribeScript(scripts[1]) ==
         nlohmann::json{{"name", "second"},
                        {"scenario", "second.json"},
                        {"setup", VS::combat_lab::EncodeSetup(setup)},
                        {"frames", 1},
                        {"press_count", 0}}));
}

TEST_CASE("Combat Lab sequences reject malformed or unfinished script inputs",
          "[combat][encounter][sequence]") {
  TraceScope scope;
  scope.Write("room.json", VS::combat_lab::EncodeSetup({}));
  const nlohmann::json good{{"scenario", "room.json"}, {"frames", 60}};
  auto invalid_entries = nlohmann::json::array();
  for (const auto& frames : nlohmann::json::array({0, -1, 0.5, true, "60"})) {
    auto entry = good;
    entry["frames"] = frames;
    invalid_entries.push_back(entry);
  }
  for (const auto* press : {"60:Circle", "1:Unknown", "1:Circle+Unknown"}) {
    auto entry = good;
    entry["press"] = press;
    invalid_entries.push_back(entry);
  }
  invalid_entries.push_back({{"scenario", "missing.json"}, {"frames", 60}});
  invalid_entries.push_back(
      {{"scenario", "room.json"}, {"frames", 60}, {"typo", true}});
  for (const auto& entry : invalid_entries) {
    const auto path =
        scope.Write("invalid.json", {{"version", 1}, {"scripts", {entry}}});
    CHECK_THROWS(VS::combat_lab::ReadScriptSequence(path));
  }
  for (const auto& document : nlohmann::json::array(
           {{{"version", 2}, {"scripts", {good}}},
            {{"version", 1}, {"scripts", nlohmann::json::array()}},
            {{"scripts", {good}}}})) {
    const auto path = scope.Write("invalid.json", document);
    CHECK_THROWS(VS::combat_lab::ReadScriptSequence(path));
  }
}

TEST_CASE(
    "Combat Lab sequence traces separate scripts and preserve error counts",
    "[combat][encounter][sequence][trace]") {
  TraceScope scope;
  {
    VS::combat_lab::Trace trace(scope.parent);
    scope.run = trace.Directory();
    scope.events = scope.run / "events.jsonl";
    scope.log = scope.run / "session.log";
    for (std::size_t number = 1; number <= 2; ++number) {
      const auto path = trace.BeginScript(number, number * 10);
      scope.directories.push_back(path);
      scope.files.push_back(path / "events.jsonl");
      trace.Frame(number * 10 + 2, {}, SampleState());
      if (number == 2) VS_LOG_ERROR("combat.test", "intentional script error");
      trace.EndScript(number * 10 + 3, {{"status", "test"}});
      CHECK(CompleteScriptLog(path / "events.jsonl", number));
    }
    CHECK(trace.Errors() == 1);
    CHECK_FALSE(trace.Failed());
  }
}

TEST_CASE(
    "Deferred device initialization preserves ordinary application defaults",
    "[platform][startup][combat-lab]") {
  CHECK(VS::Platform::WindowDesc{}.initialize_devices);
  VS::Platform::App app;
  const auto initialized = app.InitDevices();
  REQUIRE_FALSE(initialized);
  CHECK(initialized.error() == "initialize the application before its devices");
}

namespace {
void CheckMirroredLog(const std::filesystem::path& primary_path,
                      const std::filesystem::path& mirror_path) {
  std::ifstream primary(primary_path);
  std::ifstream compatibility(mirror_path);
  const std::string text((std::istreambuf_iterator<char>(primary)), {});
  const std::string copy((std::istreambuf_iterator<char>(compatibility)), {});
  CHECK(text == copy);
  CHECK(text.find("diagnostic opcode test") != std::string::npos);
}

void CheckDesktopFrame(const std::filesystem::path& path) {
  std::ifstream stream(path);
  std::string line;
  bool frame = false;
  while (std::getline(stream, line)) {
    const auto record = nlohmann::json::parse(line);
    if (record.at("category") != "frame") continue;
    frame = true;
    CHECK(record.at("data").at("actors").at(0).at("hp") == 0);
    CHECK(record.at("data").at("pending_loot").size() == 1);
    CHECK(record.at("data").at("scene").at("event").at("id") == 7);
  }
  CHECK(frame);
}
}  // namespace

TEST_CASE("Desktop diagnostics retain Lab frame data and a live text mirror",
          "[diagnostics][trace]") {
  TraceScope scope;
  const auto mirror = scope.parent / "vs-native.log";
  scope.files.push_back(mirror);
  {
    VS::Game::GameplayTrace trace(scope.parent, "desktop", mirror);
    scope.run = trace.Directory();
    scope.events = scope.run / "events.jsonl";
    scope.log = scope.run / "session.log";
    scope.files.push_back(scope.run / "manifest.json");
    VS::Game::WriteTraceManifest(scope.run, {{"detailed_logging", true}});
    VS_LOG_DEBUG("event-step", "diagnostic opcode test");
    trace.Frame(42, {}, SampleState(),
                {{"available", true}, {"event", {{"id", 7}}}});
    REQUIRE_FALSE(trace.Failed());
    CheckMirroredLog(scope.log, mirror);
  }
  CheckDesktopFrame(scope.events);
}

TEST_CASE("Warning provenance is durable before any later trace boundary",
          "[diagnostics][trace][warning-persistence]") {
  TraceScope scope;
  const auto mirror = scope.parent / "vs-native.log";
  scope.files.push_back(mirror);
  {
    VS::Game::GameplayTrace trace(scope.parent, "desktop", mirror);
    scope.run = trace.Directory();
    scope.events = scope.run / "events.jsonl";
    scope.log = scope.run / "session.log";
    const auto script = trace.BeginScript(1, 0);
    scope.directories.push_back(script);
    scope.files.push_back(script / "events.jsonl");
    VS_LOG_WARN("effects.texture", "warning must survive an immediate crash");
    for (const auto& path :
         {scope.events, script / "events.jsonl", scope.log, mirror}) {
      CAPTURE(path.string());
      std::ifstream input(path);
      const std::string contents((std::istreambuf_iterator<char>(input)), {});
      CHECK(contents.find("warning must survive an immediate crash") !=
            std::string::npos);
    }
    CHECK(trace.Errors() == 0);
    CHECK_FALSE(trace.Failed());
  }
}

TEST_CASE("Detailed runs preserve previous evidence and report invalid output",
          "[diagnostics][trace]") {
  TraceScope scope;
  std::filesystem::path first;
  {
    VS::Game::GameplayTrace trace(scope.parent, "desktop");
    first = trace.Directory();
    scope.directories.push_back(first);
    scope.files.push_back(first / "events.jsonl");
    scope.files.push_back(first / "session.log");
    trace.Event("test.first", {{"retained", true}});
  }
  {
    VS::Game::GameplayTrace trace(scope.parent, "desktop");
    scope.run = trace.Directory();
    scope.events = scope.run / "events.jsonl";
    scope.log = scope.run / "session.log";
    CHECK(scope.run != first);
    trace.Event("invalid", {{"text", std::string(1, static_cast<char>(0xFF))}});
    CHECK(trace.Failed());
    CHECK_THROWS(VS::Game::WriteTraceManifest(scope.run / "missing-directory",
                                              {{"test", true}}));
  }
  std::ifstream saved(first / "events.jsonl");
  const std::string retained((std::istreambuf_iterator<char>(saved)), {});
  CHECK(retained.find("test.first") != std::string::npos);
}

namespace {
struct GameplayInspectionFixture {
  int saved_gameplay = Boot_isGameplayActive;
  decltype(Field_controlMode) saved_control = Field_controlMode;
  decltype(Battle_actionState) saved_action = Battle_actionState;
  decltype(Items_lootListHead) saved_loot = Items_lootListHead;
  decltype(Field_roomData.floorTriggers) saved_triggers =
      Field_roomData.floorTriggers;
  decltype(Menu_Framework_itemsText) saved_text = Menu_Framework_itemsText;
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Actors_actorRecords)) saved_models =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Event_isTextBoxAwaitingClose)) saved_waits =
      std::to_array(Event_isTextBoxAwaitingClose);
  Actors_ActorRecord model{};

  GameplayInspectionFixture() {
    Boot_ResetGameplayInspection();
    Boot_isGameplayActive = 1;
    Field_controlMode = 1;
    Battle_actionState = nullptr;
    Items_lootListHead = nullptr;
    Field_roomData.floorTriggers = nullptr;
    Menu_Framework_itemsText = nullptr;
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(Actors_actorRecords, saved_models.size(), nullptr);
    model.modelId = 58;
    model.pose.frame = 7;
    model.animationLength = 91;
    Actors_actorRecords[2] = &model;
    std::fill_n(Event_isTextBoxAwaitingClose, saved_waits.size(), 1);
    for (std::size_t box = 0; box < 8; ++box) {
      Event_isTextBoxAwaitingClose[box] = static_cast<char>(box % 2);
    }
  }

  ~GameplayInspectionFixture() {
    Boot_ResetGameplayInspection();
    Boot_isGameplayActive = saved_gameplay;
    Field_controlMode = saved_control;
    Battle_actionState = saved_action;
    Items_lootListHead = saved_loot;
    Field_roomData.floorTriggers = saved_triggers;
    Menu_Framework_itemsText = saved_text;
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    std::copy(saved_models.begin(), saved_models.end(), Actors_actorRecords);
    std::copy(saved_waits.begin(), saved_waits.end(),
              Event_isTextBoxAwaitingClose);
  }
};

void CheckEventScene(const nlohmann::json& event) {
  const auto& waits = event.at("dialogue_waits");
  REQUIRE(waits.size() == 8);
  for (std::size_t box = 0; box < waits.size(); ++box) {
    CHECK(waits.at(box) == (box % 2 != 0));
  }
  const auto& threads = event.at("threads");
  REQUIRE(threads.size() == 4);
  for (std::size_t index = 0; index < threads.size(); ++index) {
    CHECK(threads.at(index).at("thread") == index);
    CHECK(threads.at(index).contains("wait_frames"));
    CHECK(threads.at(index).contains("dialogue_wait"));
  }
}

void CheckReadyScene(const VS::Game::EncounterState& state,
                     const Actors_ActorRecord& model) {
  REQUIRE(state.ready);
  CHECK_FALSE(VS::Game::InspectEncounter().ready);
  const auto scene = VS::Game::InspectGameplayScene(state);
  REQUIRE(scene.at("models").size() == 1);
  CHECK(scene.at("models").at(0).at("model") == 58);
  CHECK(scene.at("models").at(0).at("frame") == 7);
  const auto& camera = scene.at("camera");
  CHECK(camera.at("view_rotation").size() == 3);
  CHECK(camera.at("view_rotation").at(0).size() == 3);
  CHECK(camera.at("view_translation").size() == 3);
  CheckEventScene(scene.at("event"));
  CHECK(model.pose.frame == 7);
}
}  // namespace

TEST_CASE("Normal-game inspection observes scene models only in a ready field",
          "[diagnostics][inspection]") {
  GameplayInspectionFixture fixture;
  CHECK_FALSE(VS::Game::InspectGameplay().ready);
  Boot_EncounterPollStop();
  CheckReadyScene(VS::Game::InspectGameplay(), fixture.model);
  Boot_ResetGameplayInspection();
  const auto loading = VS::Game::InspectGameplay();
  CHECK_FALSE(loading.ready);
  CHECK_FALSE(VS::Game::InspectGameplayScene(loading).contains("models"));
  Boot_isGameplayActive = 0;
  Boot_EncounterPollStop();
  CHECK_FALSE(VS::Game::InspectGameplay().ready);
}

TEST_CASE("Detailed log initialization rejects an unwritable mirror path",
          "[diagnostics][trace]") {
  TraceScope scope;
  const auto blocked = scope.Write("regular-file", {{"not", "a directory"}});
  CHECK_THROWS(VS::Game::GameplayTrace(scope.parent, "desktop",
                                       blocked / "vs-native.log"));
  for (const auto& entry : std::filesystem::directory_iterator(scope.parent)) {
    if (!entry.is_directory()) continue;
    scope.run = entry.path();
    scope.events = scope.run / "events.jsonl";
    scope.log = scope.run / "session.log";
  }
  CHECK_FALSE(scope.run.empty());
}

TEST_CASE("A cached ready snapshot cannot inspect a released gameplay scene",
          "[diagnostics][inspection]") {
  GameplayInspectionFixture fixture;
  Boot_EncounterPollStop();
  const auto before = VS::Game::InspectGameplay();
  REQUIRE(before.ready);
  Boot_ResetGameplayInspection();
  CHECK_FALSE(
      VS::Game::InspectGameplayScene(before).at("available").get<bool>());
  CHECK_FALSE(VS::Game::InspectGameplayScene(before).contains("models"));
}
