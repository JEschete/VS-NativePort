#include "CombatLab/Trace.hpp"

#include <charconv>
#include <chrono>
#include <cstdio>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
  if (script_stream_.is_open()) {
    EndScript(0, {{"status", "interrupted"}, {"reason", "host terminated"}});
  }
  Event("session.end", {{"trace_failed", Failed()}});
  Core::SetLogFlushEachMessage(previous_flush_policy_);
  Core::SetConsoleLogLevel(previous_console_level_);
  Core::SetLogFile("");
} catch (...) {
  std::fputs("Combat Lab trace finalization failed\n", stderr);
}

std::filesystem::path Trace::BeginScript(std::size_t number,
                                         std::uint64_t tick) {
  const auto path = directory_ / std::format("script-{:04}", number);
  {
    std::lock_guard lock(mutex_);
    if (script_stream_.is_open()) {
      throw std::runtime_error("previous script trace is still open");
    }
    if (!std::filesystem::create_directory(path)) {
      throw std::runtime_error("script trace directory already exists");
    }
    script_stream_.open(path / "events.jsonl");
    if (!script_stream_) throw std::runtime_error("cannot create script trace");
    script_number_ = number;
    script_start_tick_ = tick;
    WriteRecord("session.begin", {{"schema", 1}, {"trace", "full"}}, tick);
  }
  return path;
}

void Trace::EndScript(std::uint64_t tick, const nlohmann::json& result) {
  std::lock_guard lock(mutex_);
  WriteRecord("session.end", {{"trace_failed", Failed()}, {"result", result}},
              tick);
  script_stream_.flush();
  if (!script_stream_) failed_ = true;
  script_stream_.close();
  script_number_ = 0;
}

void Trace::Event(std::string_view category, const nlohmann::json& details,
                  std::uint64_t tick) noexcept {
  try {
    std::lock_guard lock(mutex_);
    WriteRecord(category, details, tick);
  } catch (...) {
    failed_ = true;
  }
}

void Trace::WriteRecord(std::string_view category,
                        const nlohmann::json& details, std::uint64_t tick) {
  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::steady_clock::now() - started_)
                           .count();
  nlohmann::json record{{"sequence", ++sequence_},
                        {"tick", tick},
                        {"elapsed_us", elapsed},
                        {"category", category},
                        {"data", details}};
  if (script_number_ != 0) {
    record["script"] = script_number_;
    record["script_tick"] =
        tick >= script_start_tick_ ? tick - script_start_tick_ : 0;
  }
  const auto line = record.dump();
  stream_ << line << '\n';
  if (script_stream_.is_open()) script_stream_ << line << '\n';
  const bool boundary =
      category != "engine" ||
      details.at("level").get<int>() >= static_cast<int>(Core::LogLevel::Error);
  if (category == "engine" && details.at("level").get<int>() >=
                                  static_cast<int>(Core::LogLevel::Error)) {
    ++errors_;
  }
  if (boundary) {
    stream_.flush();
    if (script_stream_.is_open()) script_stream_.flush();
    if (!Core::FlushLogFile()) failed_ = true;
  }
  if (!stream_ || (script_stream_.is_open() && !script_stream_)) failed_ = true;
}

std::uint64_t ParseTick(std::string_view value) {
  std::uint64_t result = 0;
  const auto parsed =
      std::from_chars(value.data(), value.data() + value.size(), result);
  if (value.empty() || parsed.ec != std::errc() ||
      parsed.ptr != value.data() + value.size()) {
    throw std::runtime_error("invalid nonnegative integer: " +
                             std::string(value));
  }
  return result;
}

std::vector<ScriptPress> DecodePresses(std::string_view value) {
  std::vector<ScriptPress> result;
  while (!value.empty()) {
    const auto comma = value.find(',');
    const auto item = value.substr(0, comma);
    const auto colon = item.find(':');
    if (colon == std::string_view::npos) {
      throw std::runtime_error("press expects tick:Button entries");
    }
    auto names = item.substr(colon + 1);
    std::uint32_t buttons = 0;
    do {
      const auto plus = names.find('+');
      const auto button = Input::ParseButtons(names.substr(0, plus));
      if (button == 0) throw std::runtime_error("unknown scripted button");
      buttons |= button;
      if (plus == std::string_view::npos) break;
      names.remove_prefix(plus + 1);
    } while (true);
    result.push_back({ParseTick(item.substr(0, colon)), buttons});
    if (comma == std::string_view::npos) break;
    value.remove_prefix(comma + 1);
    if (value.empty()) throw std::runtime_error("empty press entry");
  }
  return result;
}

std::vector<EncounterScript> ReadScriptSequence(
    const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file) throw std::runtime_error("cannot open sequence: " + path.string());
  const auto document = nlohmann::json::parse(file);
  if (!document.is_object() || !document.contains("version") ||
      !document.at("version").is_number_integer() ||
      document.at("version") != 1 || !document.contains("scripts") ||
      !document.at("scripts").is_array() || document.at("scripts").empty()) {
    throw std::runtime_error(
        "sequence requires version 1 and nonempty scripts");
  }
  for (const auto& [key, unused] : document.items()) {
    (void)unused;
    if (key != "version" && key != "scripts") {
      throw std::runtime_error("unknown sequence field: " + key);
    }
  }
  std::vector<EncounterScript> scripts;
  for (const auto& entry : document.at("scripts")) {
    if (!entry.is_object() || !entry.contains("frames") ||
        !entry.at("frames").is_number_integer() || entry.at("frames") <= 0 ||
        entry.at("frames") >
            std::numeric_limits<std::uint64_t>::max() - kScriptStopGraceTicks) {
      throw std::runtime_error(
          "each script requires a positive integer frames");
    }
    for (const auto& [key, unused] : entry.items()) {
      (void)unused;
      if (key != "name" && key != "scenario" && key != "frames" &&
          key != "press") {
        throw std::runtime_error("unknown script field: " + key);
      }
    }
    EncounterScript script;
    script.scenario = (std::filesystem::absolute(path).parent_path() /
                       entry.at("scenario").get<std::string>())
                          .lexically_normal();
    script.name = entry.value("name", script.scenario.stem().string());
    if (script.name.empty()) throw std::runtime_error("script name is empty");
    script.frames = entry.at("frames").get<std::uint64_t>();
    script.presses = DecodePresses(entry.value("press", std::string{}));
    for (const auto& press : script.presses) {
      if (press.tick >= script.frames) {
        throw std::runtime_error("script press lies outside its frame budget");
      }
    }
    std::ifstream preset(script.scenario);
    if (!preset) {
      throw std::runtime_error("cannot open script scenario: " +
                               script.scenario.string());
    }
    script.setup = DecodeSetup(nlohmann::json::parse(preset));
    scripts.push_back(std::move(script));
  }
  return scripts;
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
         {"target_page",
          {{"total", state.target_page.total},
           {"page", state.target_page.page},
           {"pages", state.target_page.pages},
           {"first", state.target_page.first},
           {"visible", state.target_page.visible},
           {"selected", state.target_page.selected}}},
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
