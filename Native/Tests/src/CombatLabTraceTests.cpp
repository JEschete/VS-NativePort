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
#include "Platform/App.hpp"

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
