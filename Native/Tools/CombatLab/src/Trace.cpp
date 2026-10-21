#include "CombatLab/Trace.hpp"

#include <chrono>
#include <cstdio>
#include <format>
#include <stdexcept>
#include <string>
#include <utility>

namespace VS::combat_lab {
namespace {
nlohmann::json EncodePosition(const Game::EncounterPosition& position) {
  return {{"x", position.x},
          {"z", position.z},
          {"elevation", position.elevation},
          {"direction", position.direction}};
}

Game::EncounterPosition DecodePosition(const nlohmann::json& value) {
  return {value.at("x").get<int>(), value.at("z").get<int>(),
          value.value("elevation", 0), value.value("direction", 0)};
}
}  // namespace

Trace::Trace(const std::filesystem::path& parent) {
  const auto timestamp =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  std::filesystem::create_directories(parent);
  for (int suffix = 0;; ++suffix) {
    directory_ = parent / std::format("{}-{}", timestamp, suffix);
    if (std::filesystem::create_directory(directory_)) break;
  }
  stream_.open(directory_ / "events.jsonl");
  if (!stream_) throw std::runtime_error("cannot create structured trace");
  if (!Core::SetLogFile((directory_ / "session.log").string().c_str())) {
    throw std::runtime_error("cannot create engine session log");
  }
  Core::SetLogLevel(Core::LogLevel::Trace);
  Core::SetLogChannels("*");
  previous_console_level_ = Core::SetConsoleLogLevel(Core::LogLevel::Warn);
  previous_flush_policy_ = Core::SetLogFlushEachMessage(false);
  previous_observer_ =
      Core::SetLogObserver([this](const Core::LogRecord& record) noexcept {
        try {
          Event("engine",
                {{"channel", record.channel},
                 {"level", static_cast<int>(record.level)},
                 {"message", record.message}},
                record.tick);
        } catch (...) {
          failed_ = true;
        }
      });
  Event("session.begin",
        {{"schema", 1}, {"trace", "full"}, {"build", __DATE__ " " __TIME__}});
}

Trace::~Trace() try {
  Core::SetLogObserver(std::move(previous_observer_));
  Event("session.end", {{"trace_failed", Failed()}});
  Core::SetLogFlushEachMessage(previous_flush_policy_);
  Core::SetConsoleLogLevel(previous_console_level_);
  Core::SetLogFile("");
} catch (...) {
  std::fputs("Combat Lab trace finalization failed\n", stderr);
}

void Trace::Event(std::string_view category, const nlohmann::json& details,
                  std::uint64_t tick) noexcept {
  try {
    std::lock_guard lock(mutex_);
    stream_ << nlohmann::json({{"sequence", ++sequence_},
                               {"tick", tick},
                               {"category", category},
                               {"data", details}})
                   .dump()
            << '\n';
    const bool boundary =
        category != "engine" || details.at("level").get<int>() >=
                                    static_cast<int>(Core::LogLevel::Error);
    if (boundary) {
      stream_.flush();
      if (!Core::FlushLogFile()) failed_ = true;
    }
    if (!stream_) failed_ = true;
  } catch (...) {
    failed_ = true;
  }
}

void Trace::Frame(std::uint64_t tick, const Input::PadState& input,
                  const Game::EncounterState& state) {
  auto actors = nlohmann::json::array();
  for (const auto& actor : state.actors) {
    actors.push_back(
        {{"slot", actor.slot},
         {"hp", actor.hp},
         {"mp", actor.mp},
         {"risk", actor.risk},
         {"statuses", actor.statuses},
         {"command", actor.command},
         {"command_state", actor.command_state},
         {"weapon_drawn", actor.weapon_drawn},
         {"actor_flags", actor.actor_flags},
         {"recovery", actor.recovery},
         {"spawn_index", actor.spawn_index},
         {"defeated", actor.defeated},
         {"awareness_marker", actor.awareness_marker},
         {"ai",
          {{"present", actor.ai_present},
           {"target", actor.ai_target},
           {"candidates", actor.ai_candidates},
           {"engagement_frames", actor.ai_engagement_frames},
           {"movement_mode", actor.ai_movement_mode},
           {"destination", {actor.ai_destination_x, actor.ai_destination_z}}}},
         {"position", {actor.x, actor.y, actor.z}}});
  }
  auto pending_loot = nlohmann::json::array();
  for (const auto& loot : state.pending_loot) {
    auto dropped_items = nlohmann::json::array();
    for (const auto& item : loot.items) {
      dropped_items.push_back(
          {{"slot", item.slot}, {"id", item.id}, {"count", item.count}});
    }
    pending_loot.push_back(
        {{"source_actor", loot.source_actor}, {"items", dropped_items}});
  }
  auto items = nlohmann::json::array();
  for (const auto& item : state.items) {
    items.push_back(
        {{"slot", item.slot}, {"id", item.id}, {"count", item.count}});
  }
  Event("frame",
        {{"input",
          {{"held", input.held},
           {"pressed", input.pressed},
           {"released", input.released},
           {"repeat", input.repeat},
           {"analog", input.analog},
           {"sticks",
            {input.sticks.lx, input.sticks.ly, input.sticks.rx,
             input.sticks.ry}}}},
         {"ready", state.ready},
         {"zone", state.zone},
         {"room", state.room},
         {"control_mode", state.control_mode},
         {"game_over", state.game_over},
         {"field_rendering_enabled", state.field_rendering_enabled},
         {"menu_state", state.menu_state},
         {"frame_buffer", state.frame_buffer},
         {"transition", state.transition},
         {"native_buttons", state.native_buttons},
         {"native_pressed", state.native_pressed},
         {"action_phase", state.action_phase},
         {"action", state.action},
         {"source_actor", state.source_actor},
         {"target_count", state.target_count},
         {"rewards_available", state.rewards_available},
         {"pending_loot", pending_loot},
         {"items", items},
         {"actors", actors}},
        tick);
}

nlohmann::json EncodeSetup(const Game::EncounterSetup& setup) {
  auto enemies = nlohmann::json::array();
  for (const auto& enemy : setup.enemies) {
    enemies.push_back({{"definition", enemy.enemy},
                       {"position", EncodePosition(enemy.position)}});
  }
  return {{"version", 1},
          {"zone", setup.zone},
          {"room", setup.room},
          {"seed", setup.seed},
          {"ashley_profile", Game::EncounterProfileId(setup.profile)},
          {"player", EncodePosition(setup.player)},
          {"enemies", enemies},
          {"room_events", "isolated"},
          {"position_units", "tiles; elevation steps of 64 native units"}};
}

Game::EncounterSetup DecodeSetup(const nlohmann::json& json) {
  if (json.at("version").get<int>() != 1 ||
      json.value("room_events", "isolated") != "isolated") {
    throw std::runtime_error("unsupported encounter preset version or policy");
  }
  Game::EncounterSetup setup;
  setup.zone = json.at("zone").get<int>();
  setup.room = json.at("room").get<int>();
  const auto seed = json.at("seed").get<std::int64_t>();
  if (seed < 0 || seed > UINT32_MAX) {
    throw std::runtime_error("seed is outside uint32 range");
  }
  setup.seed = static_cast<std::uint32_t>(seed);
  const auto profile = json.value("ashley_profile", "prologue");
  if (profile == "prologue") {
    setup.profile = Game::EncounterProfile::kPrologue;
  } else if (profile == "post-prologue") {
    setup.profile = Game::EncounterProfile::kPostPrologue;
  } else if (profile == "post-minotaur") {
    setup.profile = Game::EncounterProfile::kPostMinotaur;
  } else {
    throw std::runtime_error("unknown encounter Ashley profile");
  }
  setup.player = DecodePosition(json.at("player"));
  const auto& enemies = json.at("enemies");
  if (!enemies.is_array() || enemies.size() > Game::EncounterLimits::kEnemies) {
    throw std::runtime_error("preset exceeds the primary actor budget");
  }
  for (const auto& enemy : enemies) {
    setup.enemies.push_back({enemy.at("definition").get<int>(),
                             DecodePosition(enemy.at("position"))});
  }
  return setup;
}

}  // namespace VS::combat_lab
