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

nlohmann::json EncodeStats(const Game::EncounterStatOverrides& stats) {
  if (!stats.Valid()) {
    throw std::runtime_error("Ashley stat overrides must be in 1..999");
  }
  auto json = nlohmann::json::object();
  if (stats.max_hp) json["max_hp"] = *stats.max_hp;
  if (stats.max_mp) json["max_mp"] = *stats.max_mp;
  if (stats.strength) json["strength"] = *stats.strength;
  if (stats.intelligence) json["intelligence"] = *stats.intelligence;
  if (stats.agility) json["agility"] = *stats.agility;
  return json;
}

Game::EncounterStatOverrides DecodeStats(const nlohmann::json& json) {
  if (!json.is_object()) {
    throw std::runtime_error("ashley_stats must be an object");
  }
  Game::EncounterStatOverrides stats;
  for (const auto& [key, value] : json.items()) {
    if (!value.is_number_integer() ||
        value < Game::EncounterLimits::kMinPlayerStat ||
        value > Game::EncounterLimits::kMaxPlayerStat) {
      throw std::runtime_error(
          std::format("Ashley {} must be an integer in 1..999", key));
    }
    const int amount = value.get<int>();
    if (key == "max_hp") {
      stats.max_hp = amount;
    } else if (key == "max_mp") {
      stats.max_mp = amount;
    } else if (key == "strength") {
      stats.strength = amount;
    } else if (key == "intelligence") {
      stats.intelligence = amount;
    } else if (key == "agility") {
      stats.agility = amount;
    } else {
      throw std::runtime_error(std::format("unknown Ashley stat: {}", key));
    }
  }
  return stats;
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

nlohmann::json EncodeActor(const Game::EncounterActorState& actor) {
  return {{"slot", actor.slot},
          {"hp", actor.hp},
          {"mp", actor.mp},
          {"max_hp", actor.max_hp},
          {"max_mp", actor.max_mp},
          {"risk", actor.risk},
          {"strength", actor.strength},
          {"intelligence", actor.intelligence},
          {"agility", actor.agility},
          {"total_strength", actor.total_strength},
          {"total_intelligence", actor.total_intelligence},
          {"total_agility", actor.total_agility},
          {"walk_speed", actor.walk_speed},
          {"run_speed", actor.run_speed},
          {"statuses", actor.statuses},
          {"status_timers", actor.status_timers},
          {"command", actor.command},
          {"command_state", actor.command_state},
          {"weapon_drawn", actor.weapon_drawn},
          {"weapon_category", actor.weapon_category},
          {"weapon_blade", actor.weapon_blade},
          {"weapon_grip", actor.weapon_grip},
          {"weapon_model", actor.weapon_model},
          {"shield_model", actor.shield_model},
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
          {"position", {actor.x, actor.y, actor.z}}};
}

void Trace::Frame(std::uint64_t tick, const Input::PadState& input,
                  const Game::EncounterState& state) {
  auto actors = nlohmann::json::array();
  for (const auto& actor : state.actors) {
    actors.push_back(EncodeActor(actor));
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
  auto floor_triggers = nlohmann::json::array();
  for (const auto& trigger : state.floor_triggers) {
    floor_triggers.push_back({{"kind", trigger.kind},
                              {"tile_x", trigger.tile_x},
                              {"tile_z", trigger.tile_z},
                              {"activation", trigger.activation},
                              {"marker_fade", trigger.marker_fade}});
  }
  auto target_previews = nlohmann::json::array();
  for (const auto& target : state.target_previews) {
    target_previews.push_back({{"actor", target.actor},
                               {"limb", target.limb},
                               {"position", target.position}});
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
         {"menu_highlighted_row", state.menu_highlighted_row},
         {"frame_buffer", state.frame_buffer},
         {"transition", state.transition},
         {"native_buttons", state.native_buttons},
         {"native_pressed", state.native_pressed},
         {"action_phase", state.action_phase},
         {"action", state.action},
         {"source_actor", state.source_actor},
         {"target_count", state.target_count},
         {"area_center", state.area_center},
         {"target_previews", target_previews},
         {"rewards_available", state.rewards_available},
         {"pending_loot", pending_loot},
         {"items", items},
         {"learned_actions", state.learned_actions},
         {"floor_triggers", floor_triggers},
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
          {"ashley_stats", EncodeStats(setup.stat_overrides)},
          {"ashley_weapon", Game::EncounterWeaponId(setup.weapon)},
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
  } else if (profile == "all-items-a") {
    setup.profile = Game::EncounterProfile::kAllItemsA;
  } else if (profile == "all-items-b") {
    setup.profile = Game::EncounterProfile::kAllItemsB;
  } else if (profile == "all-spells") {
    setup.profile = Game::EncounterProfile::kAllSpells;
  } else if (profile == "all-abilities") {
    setup.profile = Game::EncounterProfile::kAllAbilities;
  } else if (profile == "all-break-arts") {
    setup.profile = Game::EncounterProfile::kAllBreakArts;
  } else {
    throw std::runtime_error("unknown encounter Ashley profile");
  }
  setup.player = DecodePosition(json.at("player"));
  const auto weapon = json.value("ashley_weapon", "default");
  setup.weapon = Game::EncounterWeapon::kCount;
  for (int index = 0; index < static_cast<int>(Game::EncounterWeapon::kCount);
       ++index) {
    const auto choice = static_cast<Game::EncounterWeapon>(index);
    if (weapon == Game::EncounterWeaponId(choice)) setup.weapon = choice;
  }
  if (setup.weapon == Game::EncounterWeapon::kCount) {
    throw std::runtime_error("unknown encounter Ashley weapon");
  }
  if (json.contains("ashley_stats")) {
    setup.stat_overrides = DecodeStats(json.at("ashley_stats"));
  }
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
