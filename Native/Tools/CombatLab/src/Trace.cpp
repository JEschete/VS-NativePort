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
          {"room_events",
           setup.room_policy == Game::EncounterRoomPolicy::kOriginalRoom
               ? "production"
               : "isolated"},
          {"position_units", "tiles; elevation steps of 64 native units"}};
}

Game::EncounterSetup DecodeSetup(const nlohmann::json& json) {
  if (json.at("version").get<int>() != 1) {
    throw std::runtime_error("unsupported encounter preset version or policy");
  }
  Game::EncounterSetup setup;
  const auto policy = json.value("room_events", "isolated");
  if (policy == "production") {
    setup.room_policy = Game::EncounterRoomPolicy::kOriginalRoom;
  } else if (policy != "isolated") {
    throw std::runtime_error("unknown encounter room event policy");
  }
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
