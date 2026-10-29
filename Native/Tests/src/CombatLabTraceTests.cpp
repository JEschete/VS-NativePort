#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "CombatLab/Trace.hpp"
#include "Diagnostics/PlayerObservation.hpp"
#include "Diagnostics/SceneTrace.h"
#include "Legacy/Psx.h"
#include "Platform/App.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Damage.h"
#include "Battle/EncounterHooks.h"
#include "Boot/GameLoop.h"
#include "Core/BiosRand.h"
#include "Event/ScriptVm.h"
#include "Field/Collision.h"
#include "Field/GameMode.h"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Input/Controller.h"
#include "Items/Loot.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Framework/MenuRows.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Magic.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "State/Flags.h"
#include "State/Timers.h"
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
                            const VS::Game::EncounterState& state,
                            bool player_observations = false) {
  VS::combat_lab::Trace trace(scope.parent, "combat-lab", {},
                              player_observations);
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
void CheckSceneCommand(const nlohmann::json& record, std::uint64_t command) {
  const auto& captured = record.at("data");
  CHECK(record.at("tick") == 73);
  CHECK(captured.at("command") == command);
  CHECK(captured.at("event") == 80);
  CHECK(captured.at("source") == 2);
  CHECK(captured.at("thread") == 3);
  CHECK(captured.at("script_offset") == 16);
  CHECK(captured.at("encoded_size") == 10);
  CHECK(captured.at("opcode") == 128);
  CHECK(captured.at("operands") ==
        nlohmann::json::array({129, 130, 131, 132, 133, 134, 135, 136, 137}));
}

void CheckSceneRecords(const TraceScope& scope, std::uint64_t command,
                       std::int64_t branch_address) {
  std::ifstream stream(scope.events);
  std::string line;
  std::vector<nlohmann::json> records;
  while (std::getline(stream, line)) {
    const auto record = nlohmann::json::parse(line);
    if (record.at("category") == "event.command" ||
        record.at("category") == "event.result" ||
        record.at("category") == "state.flag_write") {
      records.push_back(record);
    }
  }
  REQUIRE(records.size() == 4);
  CheckSceneCommand(records[0], command);
  CHECK(records[1].at("data") ==
        nlohmann::json(
            {{"flag", 180}, {"before", 7}, {"after", 9}, {"changed", true}}));
  CHECK(records[2].at("data").at("changed") == false);
  CHECK(records[3].at("data").at("command") == command);
  CHECK(records[3].at("data").at("result") == branch_address);
  CHECK(records[3].at("data").at("flow") == "branch");
  std::ifstream text_stream(scope.log);
  const std::string text((std::istreambuf_iterator<char>(text_stream)), {});
  CHECK(text.find("event-command") != std::string::npos);
  CHECK(text.find("129,130,131,132,133,134,135,136,137") != std::string::npos);
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

std::vector<nlohmann::json> ReadTraceRecords(
    const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("trace records are missing");
  std::vector<nlohmann::json> records;
  std::string line;
  while (std::getline(input, line)) {
    records.push_back(nlohmann::json::parse(line));
  }
  return records;
}

std::size_t CountCategory(const std::vector<nlohmann::json>& records,
                          std::string_view category) {
  return static_cast<std::size_t>(
      std::ranges::count_if(records, [category](const nlohmann::json& record) {
        return record.at("category").get_ref<const std::string&>() == category;
      }));
}

void CheckVerificationScriptRecords(const std::filesystem::path& run,
                                    const std::filesystem::path& script_path) {
  const auto root = ReadTraceRecords(run);
  const auto script = ReadTraceRecords(script_path / "events.jsonl");
  CHECK(CountCategory(root, "frame") == 0);
  CHECK(CountCategory(script, "frame") == 1);
  CHECK(CountCategory(script, "event.command") == 1);
  CHECK(CountCategory(script, "event.result") == 1);
  CHECK(CountCategory(script, "state.flag_write") == 1);
  CHECK(CountCategory(script, "engine") == 1);
  CHECK(CompleteScriptLog(script_path / "events.jsonl", 1));
  const auto command = std::ranges::find_if(script, [](const auto& record) {
    return record.at("category") == "event.command";
  });
  REQUIRE(command != script.end());
  CHECK(command->at("data").at("operands") == nlohmann::json::array({1, 2, 3}));
}
}  // namespace

TEST_CASE(
    "Verification logs retain source evidence without duplicate run frames",
    "[diagnostics][trace][verification-log]") {
  TraceScope scope;
  std::filesystem::path script_path;
  {
    VS::Game::GameplayTrace trace(scope.parent, "combat-lab", {}, false, true);
    scope.run = trace.Directory();
    scope.events = scope.run / "events.jsonl";
    scope.log = scope.run / "session.log";
    script_path = trace.BeginScript(1, 10);
    scope.directories.push_back(script_path);
    scope.files.push_back(script_path / "events.jsonl");
    const std::array<std::uint8_t, 4> instruction{128, 1, 2, 3};
    const auto command = Diagnostics_RecordSceneCommand(
        80, 0, 0, instruction.data(), instruction.data(), instruction.size(),
        0);
    Diagnostics_RecordSceneResult(command, 2);
    Diagnostics_RecordScriptFlagWrite(180, 0, 1);
    VS_LOG_TRACE("rng", "unneeded per-draw trace");
    VS_LOG_DEBUG("event-step", "duplicate instruction text");
    VS_LOG_DEBUG("content", "disc read: repeated source evidence");
    trace.Frame(12, {}, SampleState());
    trace.EndScript(13, {{"status", "test"}});
    CHECK_FALSE(trace.Failed());
  }
  CheckVerificationScriptRecords(scope.events, script_path);
}

namespace {
void CheckPlayerFramePayload(const nlohmann::json& frame, bool shared) {
  if (shared) {
    CHECK(frame.at("data").at("player_schema") == 2);
    CHECK(frame.at("data").contains("navigation"));
    CHECK_FALSE(frame.contains("shared_record_sequence"));
  } else {
    CHECK_FALSE(frame.at("data").contains("player_schema"));
    CHECK_FALSE(frame.at("data").contains("navigation"));
    CHECK(frame.at("shared_record_sequence") == frame.at("sequence"));
  }
}

void CheckPlayerFrameStorage(const std::filesystem::path& path, bool shared) {
  const auto records = ReadTraceRecords(path);
  CHECK(CountCategory(records, "frame") == 1);
  CHECK(CountCategory(records, "engine") == 2);
  const auto frame = std::ranges::find_if(records, [](const auto& record) {
    return record.at("category") == "frame";
  });
  REQUIRE(frame != records.end());
  CHECK(frame->at("data").at("actors").empty());
  CHECK(frame->at("data").at("scene").at("proof") == "source");
  CheckPlayerFramePayload(*frame, shared);
}
}  // namespace

TEST_CASE("Verification logs preserve player root frames and all error gates",
          "[player][diagnostics][trace][verification-log]") {
  TraceScope scope;
  std::filesystem::path script_path;
  {
    VS::Game::GameplayTrace trace(scope.parent, "combat-lab", {}, true, true);
    scope.run = trace.Directory();
    scope.events = scope.run / "events.jsonl";
    scope.log = scope.run / "session.log";
    script_path = trace.BeginScript(1, 10);
    scope.directories.push_back(script_path);
    scope.files.push_back(script_path / "events.jsonl");
    VS_LOG_WARN("audio", "retained verification warning");
    VS_LOG_ERROR("audio", "retained verification error");
    trace.Frame(12, {}, {}, {{"available", true}, {"proof", "source"}});
    trace.EndScript(13, {{"status", "test"}});
    CHECK(trace.Errors() == 1);
    CHECK_FALSE(trace.Failed());
  }
  CheckPlayerFrameStorage(scope.events, true);
  CheckPlayerFrameStorage(script_path / "events.jsonl", false);
}

TEST_CASE("Verification logs retain native boot transition receipts",
          "[diagnostics][trace][verification-log][boot-transition]") {
  TraceScope scope;
  std::filesystem::path script_path;
  {
    VS::Game::GameplayTrace trace(scope.parent, "combat-lab", {}, false, true);
    scope.run = trace.Directory();
    scope.events = scope.run / "events.jsonl";
    scope.log = scope.run / "session.log";
    script_path = trace.BeginScript(1, 10);
    scope.directories.push_back(script_path);
    scope.files.push_back(script_path / "events.jsonl");
    VS_LOG_DEBUG("boot", "native session transition: title");
    trace.EndScript(13, {{"status", "test"}});
  }
  const auto records = ReadTraceRecords(script_path / "events.jsonl");
  const auto transition = std::ranges::find_if(records, [](const auto& record) {
    return record.at("category") == "engine" &&
           record.at("data").at("channel") == "boot";
  });
  REQUIRE(transition != records.end());
  CHECK(transition->at("data").at("message") ==
        "native session transition: title");
  CHECK(CountCategory(ReadTraceRecords(scope.events), "engine") == 0);
}

TEST_CASE(
    "Shared scene diagnostics snapshot complete operands and native results",
    "[scenes][diagnostics][trace]") {
  for (const auto* host : {"combat-lab", "desktop"}) {
    TraceScope scope;
    std::array<std::uint8_t, 26> script{};
    for (std::size_t index = 0; index < 10; ++index) {
      script[16 + index] = static_cast<std::uint8_t>(index + 128);
    }
    std::uint64_t command = 0;
    constexpr std::int64_t kBranchAddress = 0x7fff12345678LL;
    {
      VS::Game::GameplayTrace trace(scope.parent, host);
      scope.run = trace.Directory();
      scope.events = scope.run / "events.jsonl";
      scope.log = scope.run / "session.log";
      VS::Core::SetLogTick(73);
      command = Diagnostics_RecordSceneCommand(80, 2, 3, script.data() + 16,
                                               script.data(), 10, 0);
      CHECK(command != 0);
      script.fill(0);
      Diagnostics_RecordScriptFlagWrite(180, 7, 9);
      Diagnostics_RecordScriptFlagWrite(180, 9, 9);
      Diagnostics_RecordSceneResult(command, kBranchAddress);
      trace.Frame(73, {}, SampleState());
      CHECK(trace.Errors() == 0);
      CHECK_FALSE(trace.Failed());
      VS::Core::SetLogTick(0);
    }
    CheckSceneRecords(scope, command, kBranchAddress);
  }
}

TEST_CASE(
    "Disabled scene diagnostics neither inspect operands nor alter native "
    "flags",
    "[scenes][diagnostics][trace]") {
  TraceScope scope;
  const auto saved_flag = State_ScriptFlag(180);
  {
    VS::Game::GameplayTrace trace(scope.parent);
    scope.run = trace.Directory();
    scope.events = scope.run / "events.jsonl";
    scope.log = scope.run / "session.log";
    VS::Core::SetLogLevel(VS::Core::LogLevel::Info);
    CHECK(Diagnostics_RecordSceneCommand(80, 2, 0, nullptr, nullptr, 999, 0) ==
          0);
    Diagnostics_RecordSceneResult(0, 3);
    State_SetScriptFlag(180, 17);
    CHECK(State_ScriptFlag(180) == 17);
    State_SetScriptFlag(180, saved_flag);
    CHECK(trace.Errors() == 0);
    CHECK_FALSE(trace.Failed());
  }
  std::ifstream stream(scope.events);
  std::string line;
  while (std::getline(stream, line)) {
    const auto category = nlohmann::json::parse(line).at("category");
    CHECK(category != "event.command");
    CHECK(category != "event.result");
    CHECK(category != "state.flag_write");
  }
}

TEST_CASE("Invalid scene diagnostic records report explicit failures",
          "[scenes][diagnostics][trace]") {
  TraceScope scope;
  VS::Game::GameplayTrace trace(scope.parent);
  scope.run = trace.Directory();
  scope.events = scope.run / "events.jsonl";
  scope.log = scope.run / "session.log";
  std::uint8_t opcode = 3;
  CHECK(Diagnostics_RecordSceneCommand(80, 2, 0, nullptr, nullptr, 1, 0) == 0);
  CHECK(Diagnostics_RecordSceneCommand(80, 2, 0, &opcode, &opcode, 257, 0) ==
        0);
  CHECK(trace.Errors() == 2);
  VS::Core::LogWrite(VS::Core::LogLevel::Debug, "event-command", "not-json");
  CHECK(trace.Failed());
}

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
  decltype(Field_roomData.doors) saved_doors = Field_roomData.doors;
  decltype(Field_roomData.header.doorsLength) saved_door_length =
      Field_roomData.header.doorsLength;
  decltype(Field_roomData) saved_room = Field_roomData;
  decltype(Menu_Framework_menuItems) saved_menu_items =
      Menu_Framework_menuItems;
  decltype(Menu_Framework_itemsText) saved_text = Menu_Framework_itemsText;
  decltype(Menu_Framework_menuState) saved_menu = Menu_Framework_menuState;
  decltype(Event_isMenuTransitionActive) saved_event_menu =
      Event_isMenuTransitionActive;
  decltype(Audio_Seq_unusedStateFlags.menuFSubmenu) saved_results_kind =
      Audio_Seq_unusedStateFlags.menuFSubmenu;
  std::array<char, 16> saved_submenus =
      std::to_array(Menu_Framework_submenuStates);
  int saved_results_state = Menu_Results_screenState;
  int saved_results_timer = Menu_Results_screenTimer;
  u_int saved_results_animation = Menu_Results_elementAnimationState;
  int saved_results_reel = Menu_Results_buffReelIndex;
  u_int saved_results_selection = Menu_Results_buffReelSelection;
  unsigned saved_random = Core_Random_State();
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
    Field_roomData.doors = nullptr;
    Field_roomData.header.doorsLength = 0;
    Field_roomData.collisionMap = nullptr;
    Field_roomData.tileCells = nullptr;
    Menu_Framework_menuItems = nullptr;
    Menu_Framework_itemsText = nullptr;
    Menu_Framework_menuState.currentMenu = Menu_Framework_MenuState_NoneClosed;
    Event_isMenuTransitionActive = 0;
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
    Field_roomData.doors = saved_doors;
    Field_roomData.header.doorsLength = saved_door_length;
    Field_roomData = saved_room;
    Menu_Framework_menuItems = saved_menu_items;
    Menu_Framework_itemsText = saved_text;
    Menu_Framework_menuState = saved_menu;
    Event_isMenuTransitionActive = saved_event_menu;
    Audio_Seq_unusedStateFlags.menuFSubmenu = saved_results_kind;
    std::copy(saved_submenus.begin(), saved_submenus.end(),
              Menu_Framework_submenuStates);
    Menu_Results_screenState = saved_results_state;
    Menu_Results_screenTimer = saved_results_timer;
    Menu_Results_elementAnimationState = saved_results_animation;
    Menu_Results_buffReelIndex = saved_results_reel;
    Menu_Results_buffReelSelection = saved_results_selection;
    Core_Random_Seed(saved_random);
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    std::copy(saved_models.begin(), saved_models.end(), Actors_actorRecords);
    std::copy(saved_waits.begin(), saved_waits.end(),
              Event_isTextBoxAwaitingClose);
  }
};

void CheckRouletteSample(const VS::Game::EncounterRouletteState& reel) {
  CHECK(reel.entry_phase == 2);
  CHECK(reel.screen_state == 1);
  CHECK(reel.timer == 490);
  CHECK(reel.animation == 35);
  CHECK(reel.reel == 2);
  CHECK(reel.selection == 248);
  CHECK(reel.bios_state == 0xFEEDC0DEu);
}

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

TEST_CASE("Shared trace records optional raw roulette state without truncation",
          "[diagnostics][trace][roulette-trace]") {
  TraceScope absent;
  CHECK_FALSE(
      FlushedFrame(absent, SampleState()).at("data").contains("roulette"));
  auto state = SampleState();
  state.roulette = VS::Game::EncounterRouletteState{.entry_phase = 2,
                                                    .screen_state = 1,
                                                    .timer = 490,
                                                    .animation = 35,
                                                    .reel = 2,
                                                    .selection = 248,
                                                    .bios_state = 0xFEEDC0DEu};
  TraceScope present;
  const auto data = FlushedFrame(present, state).at("data").at("roulette");
  CHECK(data == nlohmann::json{{"entry_phase", 2},
                               {"screen_state", 1},
                               {"timer", 490},
                               {"animation", 35},
                               {"reel", 2},
                               {"selection", 248},
                               {"bios_state", 0xFEEDC0DEu}});
}

TEST_CASE(
    "Gameplay roulette inspection reads the original state without changes",
    "[diagnostics][inspection][roulette-trace]") {
  GameplayInspectionFixture fixture;
  Menu_Framework_menuState.currentMenu = 15;
  Audio_Seq_unusedStateFlags.menuFSubmenu = 0;
  Menu_Framework_submenuStates[15] = 2;
  Menu_Results_screenState = 1;
  Menu_Results_screenTimer = 490;
  Menu_Results_elementAnimationState = 35;
  Menu_Results_buffReelIndex = 2;
  Menu_Results_buffReelSelection = 248;
  Core_Random_Seed(0xFEEDC0DEu);
  Boot_EncounterPollStop();
  const auto state = VS::Game::InspectGameplay();
  if (state.roulette) {
    CheckRouletteSample(*state.roulette);
  } else {
    FAIL("Active game-clear roulette was not captured");
  }
  CHECK(Core_Random_State() == 0xFEEDC0DEu);
  CHECK(Menu_Results_screenTimer == 490);
  CHECK(Menu_Results_buffReelSelection == 248);
}

TEST_CASE(
    "Roulette inspection excludes loading other submenus and closed menus",
    "[diagnostics][inspection][roulette-trace]") {
  GameplayInspectionFixture fixture;
  Menu_Framework_menuState.currentMenu = 15;
  Audio_Seq_unusedStateFlags.menuFSubmenu = 0;
  CHECK_FALSE(VS::Game::InspectGameplay().roulette);
  Boot_EncounterPollStop();
  REQUIRE(VS::Game::InspectGameplay().roulette.has_value());
  Audio_Seq_unusedStateFlags.menuFSubmenu = 1;
  CHECK_FALSE(VS::Game::InspectGameplay().roulette);
  Audio_Seq_unusedStateFlags.menuFSubmenu = 0;
  Menu_Framework_menuState.currentMenu = Menu_Framework_MenuState_NoneClosed;
  CHECK_FALSE(VS::Game::InspectGameplay().roulette);
  Menu_Framework_menuState.currentMenu = 15;
  Boot_ResetGameplayInspection();
  CHECK_FALSE(VS::Game::InspectGameplay().roulette);
}

namespace {
struct ReactionInspectionFixture : GameplayInspectionFixture {
  decltype(Input_settings) saved_settings = Input_settings;
  decltype(Battle_characterState) saved_character = Battle_characterState;
  decltype(Battle_actionFlowMode) saved_phase = Battle_actionFlowMode;
  int saved_tick_speed = State_tickSpeed;
  decltype(std::to_array(Battle_Ai_actorAiStates)) saved_ai =
      std::to_array(Battle_Ai_actorAiStates);
  decltype(std::to_array(Battle_actionDefinitions)) saved_definitions =
      std::to_array(Battle_actionDefinitions);
  std::unique_ptr<Battle_ActionState> action =
      std::make_unique<Battle_ActionState>();
  Battle_Actor player{};
  Battle_Actor enemy{};
  Battle_ActorStats player_stats{};
  Battle_ActorStats enemy_stats{};
  Actors_ActorRecord player_model{};
  Battle_Ai_ActorAiState enemy_ai{};
  decltype(std::to_array(Menu_Magic_shamanSpellIds)) saved_shaman =
      std::to_array(Menu_Magic_shamanSpellIds);

  ReactionInspectionFixture() {
    Field_controlMode = 2;
    Battle_actionFlowMode = 7;
    State_tickSpeed = 2;
    Battle_actionState = action.get();
    player.stats = &player_stats;
    enemy.stats = &enemy_stats;
    player_stats.currentHp = player_stats.maxHp = 250;
    player_stats.currentMp = player_stats.maxMp = 50;
    player_stats.risk = 12;
    player_stats.currentRange = 4;
    player_stats.currentRangeY = 6;
    player_stats.currentRangeZ = 10;
    player_stats.currentAttackShape = 1;
    enemy_stats.currentHp = enemy_stats.maxHp = 73;
    Battle_actors[0] = Battle_characterState = &player;
    Battle_actors[2] = &enemy;
    player_model.modelId = 0;
    Actors_actorRecords[0] = &player_model;
    std::fill_n(Battle_Ai_actorAiStates, saved_ai.size(), nullptr);
    enemy_ai.relations[0].classification = 3;
    enemy_ai.relations[0].active = 1;
    enemy_ai.relations[0].visible = 1;
    Battle_Ai_actorAiStates[2] = &enemy_ai;
    action->abilityTimer = 42;
    action->abilityWindow = 40;
    action->previousChainInput = 1;
    action->queue[0].primaryTarget.target.targetActor = 0;
    action->queue[0].targetCount = 1;
    action->queue[0].targets[0].target.targetActor = 2;
    for (std::size_t slot = 0;
         slot < VS::Game::EncounterReactionTimingState::kSlots; ++slot) {
      Input_settings.mappedChainAbilities[slot] = static_cast<char>(22 + slot);
      Input_settings.mappedDefenseAbilities[slot] =
          static_cast<char>(40 + slot);
      for (const auto id : {22 + slot, 40 + slot}) {
        Battle_actionDefinitions[id].type = Battle_ActionType_Ability;
        Battle_actionDefinitions[id].cost = 5;
      }
    }
    Boot_EncounterPollStop();
  }

  ~ReactionInspectionFixture() {
    Input_settings = saved_settings;
    Battle_characterState = saved_character;
    Battle_actionFlowMode = saved_phase;
    State_tickSpeed = saved_tick_speed;
    std::copy(saved_ai.begin(), saved_ai.end(), Battle_Ai_actorAiStates);
    std::copy(saved_definitions.begin(), saved_definitions.end(),
              Battle_actionDefinitions);
    std::copy(saved_shaman.begin(), saved_shaman.end(),
              Menu_Magic_shamanSpellIds);
  }
};

void CheckPlayerGeometry(const VS::Game::PlayerActorObservation& player) {
  CHECK(player.model == 0);
  CHECK(player.weapon_range.components_world ==
        std::array<int, 3>{128, 192, 320});
  CHECK(player.weapon_range.shape == 1);
  CHECK(player.weapon_range.angle_steps == 0);
  CHECK(player.weapon_range.native_extent_world == 320);
}

void CheckReactionActor(const VS::Game::PlayerActorObservation& enemy) {
  CHECK(enemy.model == 58);
  CHECK(enemy.hostile_to_player);
  CHECK(enemy.relation_to_player == 3);
  CHECK(enemy.relation_active);
  CHECK(enemy.player_visible);
}

void CheckNativeTiming(const VS::Game::EncounterReactionTimingState& timing) {
  CHECK(timing.active);
  CHECK(timing.eligibility_status == "evaluated");
  CHECK(timing.source_actor == 0);
  CHECK(timing.target_actor == 2);
  CHECK(timing.timer == 42);
  CHECK(timing.window == 40);
  CHECK(timing.tick_speed == 2);
  CHECK(timing.chain_actions == std::array<int, 3>{22, 23, 24});
  CHECK(timing.defense_actions == std::array<int, 3>{40, 41, 42});
  CHECK(timing.eligible == std::array<bool, 3>{false, true, true});
}

void CheckResourcesUnchanged(const ReactionInspectionFixture& fixture) {
  CHECK(fixture.player_stats.currentHp == 250);
  CHECK(fixture.player_stats.currentMp == 50);
  CHECK(fixture.player_stats.risk == 12);
  CHECK(fixture.action->abilityTimer == 42);
  CHECK(fixture.action->abilityInputAttempted == 0);
}

void CheckDefenseAcknowledgment(
    const VS::Game::EncounterReactionTimingState& timing) {
  CHECK_FALSE(timing.active);
  CHECK(timing.attempted);
  CHECK(timing.succeeded == 2);
  CHECK(timing.previous_slot == 3);
  CHECK(timing.eligible == std::array<bool, 3>{false, false, false});
}

void CheckCachedTargetForecasts(const nlohmann::json& data) {
  REQUIRE(data.at("target_forecasts").size() == 2);
  CHECK(data.at("target_forecasts").at(0).at("amount") == 3);
  CHECK(data.at("target_forecasts").at(0).at("hit_threshold") == 25);
  CHECK(data.at("target_forecasts").at(1).at("limb") == 3);
  CHECK(data.at("target_forecasts").at(1).at("amount") == 18);
  CHECK(data.at("target_forecasts").at(1).at("hit_threshold") == 90);
}
}  // namespace

TEST_CASE("Player observations retain native weapon geometry and hostility",
          "[player][diagnostics][inspection]") {
  ReactionInspectionFixture fixture;
  fixture.player_stats.weapon.currentPp = 37;
  const auto state =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay());
  REQUIRE(state.actors.size() == 2);
  CheckPlayerGeometry(state.player_actors[0]);
  CheckReactionActor(state.player_actors[1]);
  TraceScope scope;
  const auto data = FlushedFrame(scope, state, true).at("data");
  CHECK(data.at("player_schema") == VS::Game::kPlayerObservationSchema);
  CHECK(state.player_actors[0].weapon_pp == 37);
  CHECK(data.at("actors").at(0).at("weapon_pp") == 37);
  CHECK(data.at("actors").at(0).at("weapon_range").at("native_extent_world") ==
        320);
  CHECK(data.at("actors").at(1).at("hostile_to_player") == true);
}

TEST_CASE("Player timing observations use production eligibility without costs",
          "[player][diagnostics][inspection]") {
  ReactionInspectionFixture fixture;
  const auto random = Core_Random_State();
  const auto state =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay());
  CheckNativeTiming(state.reaction_timing);
  CheckResourcesUnchanged(fixture);
  CHECK(Core_Random_State() == random);
}

TEST_CASE("Opt-in player target forecasts mirror native cached values",
          "[player][diagnostics][target-forecast]") {
  ReactionInspectionFixture fixture;
  Battle_actionFlowMode = 1;
  fixture.action->previewCount = 2;
  auto& head = fixture.action->previews[0];
  head.actorId = 2;
  head.limb = 0;
  head.effect = 58;
  head.previewAmount = 3;
  head.hitChance = 25;
  auto& limb = fixture.action->previews[1];
  limb.actorId = 2;
  limb.limb = 3;
  limb.effect = 58;
  limb.previewAmount = 18;
  limb.hitChance = 90;
  const auto random = Core_Random_State();
  const auto state = VS::Game::InspectGameplay();
  TraceScope scope;
  const auto data = FlushedFrame(scope, state, true).at("data");
  CheckCachedTargetForecasts(data);
  CHECK(Core_Random_State() == random);
  CHECK(head.previewAmount == 3);
  CHECK(limb.previewAmount == 18);
}

TEST_CASE("Unknown native target effects do not expose stale amounts",
          "[player][diagnostics][target-forecast]") {
  ReactionInspectionFixture fixture;
  Battle_actionFlowMode = 1;
  fixture.action->previewCount = 1;
  auto& preview = fixture.action->previews[0];
  preview.actorId = 2;
  preview.limb = 0;
  preview.effect = 0;
  preview.previewAmount = 999;
  preview.hitChance = 255;
  TraceScope scope;
  const auto data =
      FlushedFrame(scope, VS::Game::InspectGameplay(), true).at("data");
  REQUIRE(data.at("target_forecasts").size() == 1);
  CHECK(data.at("target_forecasts").at(0).at("amount").is_null());
  CHECK(data.at("target_forecasts").at(0).at("hit_threshold") == 255);
}

TEST_CASE("Ordinary traces do not collect player target forecasts",
          "[player][diagnostics][target-forecast]") {
  ReactionInspectionFixture fixture;
  TraceScope scope;
  const auto data = FlushedFrame(scope, VS::Game::InspectGameplay()).at("data");
  CHECK_FALSE(data.contains("target_forecasts"));
}

TEST_CASE("Player timing keeps acknowledgments after the active input phase",
          "[player][diagnostics][inspection]") {
  ReactionInspectionFixture fixture;
  fixture.action->queue[0].primaryTarget.target.targetActor = 2;
  fixture.action->queue[0].targets[0].target.targetActor = 0;
  fixture.action->queue[0].actionId = 54;
  fixture.action->previousChainInput = 2;
  const auto defense =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay())
          .reaction_timing;
  CHECK(defense.active);
  CHECK(defense.source_actor == 2);
  CHECK(defense.target_actor == 0);
  CHECK(defense.eligible == std::array<bool, 3>{true, false, true});
  fixture.action->abilityInputAttempted = 1;
  fixture.action->abilityInputSucceeded = 2;
  fixture.action->previousChainInput = 3;
  Battle_actionFlowMode = 8;
  const auto acknowledged =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay())
          .reaction_timing;
  CheckDefenseAcknowledgment(acknowledged);
  Boot_ResetGameplayInspection();
  const auto released =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay());
  CHECK_FALSE(released.ready);
  CHECK_FALSE(released.reaction_timing.active);
}

TEST_CASE("Player reads actual learned spell data without granting an action",
          "[player][diagnostics][inspection]") {
  ReactionInspectionFixture fixture;
  constexpr unsigned kHeal = 96;
  std::fill_n(Menu_Magic_shamanSpellIds, std::size(Menu_Magic_shamanSpellIds),
              0);
  Menu_Magic_shamanSpellIds[0] = kHeal;
  auto& heal = Battle_actionDefinitions[kHeal];
  heal = {};
  heal.type = Battle_ActionType_Spell;
  heal.cost = 5;
  const std::array<char, 5> name{0x11, 0x28, 0x24, 0x2F,
                                 static_cast<char>(0xE7)};
  std::copy(name.begin(), name.end(), heal.name);
  const auto absent =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay());
  CHECK(std::ranges::none_of(absent.action_options, [](const auto& option) {
    return option.id == kHeal;
  }));
  heal.unlocked = 1;
  const auto state =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay());
  const auto found = std::ranges::find(state.action_options, kHeal,
                                       &VS::Game::EncounterActionOption::id);
  REQUIRE(found != state.action_options.end());
  CHECK(found->name == "Heal");
  CHECK(found->cost == 5);
  CHECK(found->family == "shaman");
  CHECK(heal.unlocked == 1);
}

TEST_CASE("Incoming effect observations distinguish damage and recovery",
          "[player][diagnostics][inspection]") {
  ReactionInspectionFixture fixture;
  auto& incoming = fixture.action->queue[0];
  fixture.action->selection.actionId = 1;
  Battle_actionDefinitions[1].type = Battle_ActionType_BaseAttack;
  incoming.primaryTarget.target.targetActor = 2;
  incoming.targetCount = 2;
  incoming.targets[0].target.targetActor = 0;
  incoming.targets[0].hpEffect = 1;
  incoming.targets[0].target.hp = 16;
  incoming.targets[1].target.targetActor = 0;
  incoming.targets[1].hpEffect = 2;
  incoming.targets[1].target.hp = 80;
  const auto timing =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay())
          .reaction_timing;
  CHECK(timing.incoming_type == Battle_ActionType_BaseAttack);
  CHECK(timing.incoming_hp_damage == 16);
  CHECK(timing.incoming_mp_damage == 0);
  CHECK(incoming.targets[1].target.hp == 80);
}

TEST_CASE("Observed door contracts preserve the current authored flags",
          "[player][diagnostics][inspection]") {
  GameplayInspectionFixture fixture;
  Field_MpdDoorRecord door{};
  door.destinationZoneId = 9;
  door.destinationRoomId = 1;
  door.tileLocation.tileX = 3;
  door.tileLocation.tileZ = 14;
  Field_roomData.doors = &door;
  Field_roomData.header.doorsLength = sizeof(door);
  Boot_EncounterPollStop();
  const auto state =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay());
  REQUIRE(state.doors.size() == 1);
  CHECK(state.doors[0].destination_room == 1);
  CHECK(state.doors[0].tile_z == 14);
  CHECK(state.door_flag_values.size() == 1);
}

namespace {
void CheckNavigationGeometry(const VS::Game::PlayerObservationState& state) {
  REQUIRE(state.navigation.available);
  CHECK(state.navigation.width == 6);
  CHECK(state.navigation.depth == 6);
  REQUIRE(state.navigation.samples.size() == 36);
  CHECK(state.navigation.samples[0] == std::array<int, 3>{0, 30736, 1});
  CHECK(state.navigation.samples[2][2] == 0);
  CHECK(state.navigation.samples[4][2] == 3);
  CHECK(state.navigation.samples[7][2] == 5);
  CHECK(state.navigation.collision_height == 128);
}

void CheckOrdinaryTraceSkipsPlayerInspection() {
  TraceScope ordinary;
  const auto data =
      FlushedFrame(ordinary, VS::Game::InspectGameplay()).at("data");
  CHECK_FALSE(data.contains("player_schema"));
  CHECK_FALSE(data.contains("navigation"));
  CHECK_FALSE(data.at("actors").at(0).contains("weapon_range"));
}
}  // namespace

namespace {
struct NavigationInspectionFixture : ReactionInspectionFixture {
  decltype(std::to_array(Field_gimmickInstances)) saved_gimmicks =
      std::to_array(Field_gimmickInstances);
  struct Samples {
    Field_MpdCollisionHeader header{3, 3, 0, 9};
    std::array<std::uint16_t, 18> descriptors{};
    std::array<unsigned char, 32> patterns{};
  } samples;
  std::array<Field_MpdTileCell, 9> cells{};
  NavigationInspectionFixture() {
    std::fill_n(Field_gimmickInstances, saved_gimmicks.size(), nullptr);
    std::fill(samples.descriptors.begin() + 9, samples.descriptors.end(), 1);
    std::fill(samples.patterns.begin() + 16, samples.patterns.end(), 127);
    cells[1].doorPassability = 5;
    cells[2].isPit = 1;
    Field_roomData.collisionMap = &samples.header;
    Field_roomData.tileCells = cells.data();
    Field_roomData.header.collisionMapLength = sizeof(samples);
    Field_roomData.header.tileCellsLength = sizeof(cells);
    player_model.collisionHeight = 128;
    player_model.collisionRadius = 48;
  }
  ~NavigationInspectionFixture() {
    std::copy(saved_gimmicks.begin(), saved_gimmicks.end(),
              Field_gimmickInstances);
  }
};
}  // namespace

TEST_CASE("Navigation observes bounded runtime terrain without changing it",
          "[player][diagnostics][inspection]") {
  NavigationInspectionFixture fixture;
  const auto random = Core_Random_State();
  const auto probe = Field_collisionProbePosition;
  const auto state =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay());
  CheckNavigationGeometry(state);
  CHECK(Core_Random_State() == random);
  CHECK(Field_collisionProbePosition.vx == probe.vx);
  CHECK(Field_collisionProbePosition.vz == probe.vz);
  TraceScope scope;
  const auto data =
      FlushedFrame(scope, state, true).at("data").at("navigation");
  CHECK(data.at("samples").size() == 36);
  CHECK(data.at("collision_radius") == 48);
}

TEST_CASE("Ordinary traces never collect opt-in player geometry",
          "[player][diagnostics][inspection]") {
  NavigationInspectionFixture fixture;
  Field_roomData.header.tileCellsLength = 0;
  CheckOrdinaryTraceSkipsPlayerInspection();
  CHECK_THROWS(VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay()));
  Field_roomData.collisionMap = nullptr;
  CHECK_FALSE(VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay())
                  .navigation.available);
}

TEST_CASE("Navigation observes stacked support without mutating contact state",
          "[player][diagnostics][inspection]") {
  NavigationInspectionFixture fixture;
  Field_GimmickInstance lower{};
  Field_GimmickInstance upper{};
  lower.tileX = upper.tileX = lower.tileZ = upper.tileZ = 1;
  upper.positionY = -128;
  Field_gimmickInstances[0] = &lower;
  Field_gimmickInstances[1] = &upper;
  const auto contact = Actors_supportingGimmickIndex;
  const auto state =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay());
  CHECK(state.navigation.samples[21] == std::array<int, 3>{32512, 30736, 5});
  CHECK(Actors_supportingGimmickIndex == contact);
  upper.motionState = 1;
  const auto moving =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay());
  CHECK(moving.navigation.samples[21][0] == 32640);
}

TEST_CASE("Earned ability menus are observed without changing combat mode",
          "[player][diagnostics][inspection]") {
  ReactionInspectionFixture fixture;
  std::array<Menu_Framework_MenuRow, 10> rows{};
  Menu_Framework_menuItems = rows.data();
  Menu_Framework_menuState.currentMenu = 3;
  Battle_actionFlowMode = 14;
  const auto reward =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay());
  CHECK(reward.menu.available);
  CHECK(reward.menu.selected_menu == 3);
  CHECK(Field_controlMode == 2);
  CHECK(Battle_actionFlowMode == 14);
  Battle_actionFlowMode = 7;
  CHECK_FALSE(VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay())
                  .menu.available);
}

namespace {
struct ItemInspectionFixture : ReactionInspectionFixture {
  decltype(Menu_Loot_controlsEnabled) saved_loot_controls =
      Menu_Loot_controlsEnabled;
  decltype(Menu_Loot_lootListCount) saved_loot_count = Menu_Loot_lootListCount;
  decltype(std::to_array(Audio_Seq_unusedInventory.misc)) saved_items =
      std::to_array(Audio_Seq_unusedInventory.misc);
  decltype(std::to_array(Audio_Seq_unusedInventoryIndices.misc)) saved_indices =
      std::to_array(Audio_Seq_unusedInventoryIndices.misc);
  decltype(Menu_Framework_shortcutInvoked) saved_shortcut =
      Menu_Framework_shortcutInvoked;
  decltype(Menu_Framework_itemsListRow) saved_list =
      Menu_Framework_itemsListRow;
  decltype(Menu_Framework_subMenuRowCount) saved_rows =
      Menu_Framework_subMenuRowCount;

  ItemInspectionFixture() {
    std::fill_n(Audio_Seq_unusedInventory.misc, saved_items.size(),
                Items_InventoryMisc{});
    Audio_Seq_unusedInventory.misc[0].id = 323;
    Audio_Seq_unusedInventory.misc[0].count = 2;
    auto& definition = Battle_actionDefinitions[224];
    definition.type = Battle_ActionType_Item;
    definition.cost = 0;
    definition.unlocked = 0;
    definition.hits[0].effectId = 62;
    definition.hits[0].statCalculatorId = 15;
    definition.hits[0].statFactor = 10;
  }
  ~ItemInspectionFixture() {
    std::copy(saved_items.begin(), saved_items.end(),
              Audio_Seq_unusedInventory.misc);
    std::copy(saved_indices.begin(), saved_indices.end(),
              Audio_Seq_unusedInventoryIndices.misc);
    Menu_Framework_shortcutInvoked = saved_shortcut;
    Menu_Framework_itemsListRow = saved_list;
    Menu_Framework_subMenuRowCount = saved_rows;
    Menu_Loot_controlsEnabled = saved_loot_controls;
    Menu_Loot_lootListCount = saved_loot_count;
  }
};

void CheckRecoveryItemOption(const VS::Game::EncounterItemOption& item) {
  CHECK(item.id == 323);
  CHECK(item.action_id == 224);
  CHECK(item.count == 2);
  CHECK(item.effect_id == 62);
  CHECK(item.calculator_id == 15);
  CHECK(item.stat_factor == 10);
  CHECK((item.restrictions & 4) != 0);
}
}  // namespace

TEST_CASE("Consumable observations do not consume stock or grant a spell",
          "[player][diagnostics][inspection]") {
  ItemInspectionFixture fixture;
  const auto random = Core_Random_State();
  const auto state =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay());
  REQUIRE(state.item_options.size() == 1);
  CheckRecoveryItemOption(state.item_options.front());
  CHECK(Audio_Seq_unusedInventory.misc[0].count == 2);
  CHECK(Battle_actionDefinitions[224].unlocked == 0);
  CHECK(Core_Random_State() == random);
  CheckResourcesUnchanged(fixture);
}

TEST_CASE("Consumable menu rows retain native inventory identity and bounds",
          "[player][diagnostics][inspection]") {
  ItemInspectionFixture fixture;
  std::array<Menu_Framework_MenuRow, 10> rows{};
  std::array<Menu_Framework_ItemText, 1> text{};
  Field_controlMode = 5;
  Menu_Framework_menuItems = rows.data();
  Menu_Framework_itemsText = reinterpret_cast<std::uint16_t*>(text.data());
  Menu_Framework_menuState.currentMenu = 4;
  Menu_Framework_shortcutInvoked = 5;
  Menu_Framework_itemsListRow = 17;
  Menu_Framework_subMenuRowCount = 1;
  Audio_Seq_unusedInventoryIndices.misc[0] = 1;
  const auto state =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay());
  REQUIRE(state.menu.rows.size() == 1);
  CHECK(state.menu.shortcut == 5);
  CHECK(state.menu.rows[0].item_id == 323);
  CHECK(state.menu.rows[0].item_count == 2);
  Audio_Seq_unusedInventoryIndices.misc[0] = 0;
  CHECK_THROWS(VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay()));
}

TEST_CASE("Loot input readiness is observed without claiming the drop",
          "[player][diagnostics][inspection]") {
  ItemInspectionFixture fixture;
  std::array<Menu_Framework_MenuRow, 10> rows{};
  Menu_Framework_menuItems = rows.data();
  Menu_Framework_menuState.currentMenu = 12;
  Battle_actionFlowMode = 12;
  Menu_Loot_controlsEnabled = 1;
  Menu_Loot_lootListCount = 1;
  const auto state =
      VS::Game::InspectPlayerObservation(VS::Game::InspectGameplay());
  CHECK(state.menu.available);
  CHECK(state.menu.loot_input_ready);
  CHECK(state.menu.loot_count == 1);
  CHECK(Audio_Seq_unusedInventory.misc[0].count == 2);
}

TEST_CASE(
    "Event-owned loot is observed while native cutscene control remains active",
    "[player][diagnostics][event-reward]") {
  ItemInspectionFixture fixture;
  std::array<Menu_Framework_MenuRow, 10> rows{};
  Menu_Framework_menuItems = rows.data();
  Menu_Framework_menuState.currentMenu = 12;
  Field_controlMode = 3;
  Battle_actionFlowMode = 4;
  Event_isMenuTransitionActive = 1;
  Menu_Loot_controlsEnabled = 1;
  Menu_Loot_lootListCount = 2;
  const auto base = VS::Game::InspectGameplay();
  TraceScope scope;
  const auto data = FlushedFrame(scope, base, true).at("data");
  CHECK(data.at("menu").at("available") == true);
  CHECK(data.at("menu").at("event_owned") == true);
  CHECK(data.at("menu").at("loot_input_ready") == true);
  CHECK(Field_controlMode == 3);
  CHECK(Battle_actionFlowMode == 4);
  CHECK(Menu_Loot_lootListCount == 2);
}
