#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "CombatLab/Trace.hpp"

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

  ~TraceScope() {
    VS::Core::SetLogLevel(VS::Core::LogLevel::Info);
    VS::Core::SetLogChannels("");
    std::error_code error;
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
