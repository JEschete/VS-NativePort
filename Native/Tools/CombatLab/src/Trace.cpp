#include "CombatLab/Trace.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <format>
#include <limits>
#include <ranges>
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

SceneExpectation DecodeSceneExpectation(const nlohmann::json& value) {
  if (!value.is_object()) throw std::runtime_error("scene must be an object");
  for (const auto& [key, unused] : value.items()) {
    (void)unused;
    if (key != "events" && key != "event_source" && key != "return_zone" &&
        key != "return_room" && key != "preconditions" &&
        key != "confirm_dialogue" && key != "terminal") {
      throw std::runtime_error("unknown scene field: " + key);
    }
  }
  const auto& events = value.at("events");
  if (!events.is_array() || events.empty()) {
    throw std::runtime_error("scene requires a nonempty ordered event list");
  }
  SceneExpectation expected;
  for (const auto& event : events) {
    if (!event.is_number_integer() ||
        event < std::numeric_limits<std::int16_t>::min() ||
        event > std::numeric_limits<std::int16_t>::max()) {
      throw std::runtime_error(
          "scene event ID must fit the native signed short");
    }
    expected.events.push_back(event.get<int>());
  }
  if (value.contains("terminal")) {
    const auto& terminal = value.at("terminal");
    if (!terminal.is_string() ||
        (terminal != "ending_to_title" && terminal != "attract_to_title") ||
        value.contains("return_zone") || value.contains("return_room")) {
      throw std::runtime_error(
          "scene terminal must be ending_to_title or attract_to_title without "
          "a room-return goal");
    }
    expected.ending_to_title = terminal == "ending_to_title";
    expected.attract_to_title = terminal == "attract_to_title";
  } else {
    for (const auto* key : {"return_zone", "return_room"}) {
      if (!value.at(key).is_number_integer() || value.at(key) < 0 ||
          value.at(key) > std::numeric_limits<int>::max()) {
        throw std::runtime_error(
            "scene return zone/room must be nonnegative integers");
      }
    }
    expected.return_zone = value.at("return_zone").get<int>();
    expected.return_room = value.at("return_room").get<int>();
  }
  const auto source = value.value("event_source", nlohmann::json(0));
  if (!source.is_number_integer() || source < 0 || source > 2) {
    throw std::runtime_error("scene event_source must be a native source 0..2");
  }
  expected.event_source = source.get<int>();
  if (expected.event_source == 0 &&
      std::ranges::any_of(expected.events, [](int id) {
        return id < 0 || id > Game::EncounterLimits::kLastExternalEvent;
      })) {
    throw std::runtime_error(
        "external scene ID is outside the native asset range");
  }
  expected.preconditions = value.at("preconditions").get<std::string>();
  expected.confirm_dialogue = value.value("confirm_dialogue", false);
  if (expected.attract_to_title && expected.confirm_dialogue) {
    // Any button ends the original demo; it must not be confirmed.
    throw std::runtime_error("attract_to_title scenes cannot confirm dialogue");
  }
  if (expected.preconditions.empty()) {
    throw std::runtime_error(
        "scene requires documented source/save preconditions");
  }
  return expected;
}

SceneTrial::SceneTrial(SceneExpectation expected)
    : expected_(std::move(expected)) {
  if (expected_.events.empty() || expected_.preconditions.empty() ||
      expected_.return_zone < 0 || expected_.return_room < 0 ||
      expected_.event_source < 0 || expected_.event_source > 2) {
    throw std::runtime_error(
        "scene trial requires events and documented preconditions");
  }
}

void SceneTrial::Observe(const Game::EncounterState& state,
                         const nlohmann::json& scene, std::uint64_t tick) {
  if (Finished()) return;
  const bool available = scene.value("available", false);
  const auto event = available ? scene.at("event") : nlohmann::json{};
  last_ = {{"ready", state.ready},
           {"zone", state.zone},
           {"room", state.room},
           {"control", state.control_mode},
           {"transition", state.transition},
           {"event", event},
           {"tick", tick}};
  dialogue_waiting_ =
      available &&
      std::ranges::any_of(event.at("dialogue_waits"), [](const auto& wait) {
        return wait.template get<bool>();
      });
  if (state.game_over || (state.native_pressed & VS::Input::Start) != 0 ||
      (available &&
       (event.at("skip_state") == 2 || event.at("forced_skip").get<bool>()))) {
    Fail(state.game_over ? "fixture defeated" : "target scene was skipped");
    return;
  }
  if (expected_.attract_to_title && state.native_pressed != 0) {
    Fail("attract demo received button input");
    return;
  }
  // Event_UpdateEventPlayback: phases1..3 prepare/load; only4 executes.
  // Source1 may retain an earlier external ID while running a room handoff.
  constexpr int kExecutingPhase = 4;
  if (available && event.at("phase") == kExecutingPhase &&
      event.at("source") == expected_.event_source) {
    const int id = event.at("id").get<int>();
    if (!event_active_ || observed_.empty() || observed_.back() != id) {
      observed_.push_back(id);
      if (next_event_ < expected_.events.size() &&
          id == expected_.events[next_event_]) {
        ++next_event_;
      }
    }
    event_active_ = true;
  } else if (available) {
    event_active_ = false;
  }
  if (expected_.ending_to_title || expected_.attract_to_title) return;
  const bool returned =
      next_event_ == expected_.events.size() && state.ready && available &&
      event.at("phase") == 0 && state.zone == expected_.return_zone &&
      state.room == expected_.return_room && state.control_mode == 1 &&
      state.transition == 0 && !dialogue_waiting_;
  if (!returned) {
    control_tick_.reset();
  } else {
    if (!control_tick_) control_tick_ = tick;
    if (tick - *control_tick_ >= DoorTrial::kObservationTicks) {
      status_ = "passed";
      passed_tick_ = tick;
    }
  }
}

void SceneTrial::ObserveEndingReturn(bool credits_finished,
                                     bool credits_skipped,
                                     bool clear_save_pending,
                                     std::uint64_t tick) {
  if (Finished() || !expected_.ending_to_title) return;
  terminal_ = {{"kind", "ending_to_title"},
               {"native_fiber_returned", true},
               {"credits_finished", credits_finished},
               {"credits_skipped", credits_skipped},
               {"clear_save_pending", clear_save_pending},
               {"title_host", "lab_encounter_finish_callback"},
               {"tick", tick}};
  if (credits_skipped) {
    Fail("credits were skipped");
  } else if (!credits_finished || !clear_save_pending) {
    Fail("encounter ended without completed credits and clear-save handoff");
  } else if (next_event_ != expected_.events.size()) {
    Fail("ending returned without the expected source events");
  } else {
    status_ = "passed";
    passed_tick_ = tick;
  }
}

void SceneTrial::ObserveAttractReturn(int entry_mode, bool credits_finished,
                                      bool clear_save_pending,
                                      std::uint64_t tick) {
  if (Finished() || !expected_.attract_to_title) return;
  terminal_ = {{"kind", "attract_to_title"},
               {"native_fiber_returned", true},
               {"entry_mode", entry_mode},
               {"credits_finished", credits_finished},
               {"clear_save_pending", clear_save_pending},
               {"title_host", "lab_encounter_finish_callback"},
               {"tick", tick}};
  if (entry_mode != Game::kAttractDemoCutscene1 &&
      entry_mode != Game::kAttractDemoCutscene2) {
    Fail("title return without an attract-demo entry mode");
  } else if (credits_finished || clear_save_pending) {
    Fail("attract demo returned through the ending handoff");
  } else if (next_event_ != expected_.events.size()) {
    Fail("attract demo returned without the expected source events");
  } else {
    status_ = "passed";
    passed_tick_ = tick;
  }
}

std::uint32_t SceneTrial::Input(std::uint64_t tick) {
  constexpr std::uint64_t kDialoguePulsePeriod = 30;
  constexpr std::uint64_t kDialogueHoldTicks = 2;
  const bool held = !Finished() && expected_.confirm_dialogue &&
                    dialogue_waiting_ &&
                    tick % kDialoguePulsePeriod < kDialogueHoldTicks;
  if (held && !dialogue_held_) ++dialogue_pulses_;
  dialogue_held_ = held;
  return held ? VS::Input::Circle : 0;
}

void SceneTrial::Timeout() {
  if (Finished()) return;
  status_ = "blocked";
  reason_ = next_event_ == 0 ? "expected starting event was not observed"
            : next_event_ < expected_.events.size()
                ? "expected event handoff was not observed"
            : expected_.ending_to_title
                ? "ending did not return to title before budget"
            : expected_.attract_to_title
                ? "attract demo did not return to title before budget"
                : "scene did not restore stable player control before budget";
}

void SceneTrial::Fail(std::string reason) {
  status_ = "failed";
  reason_ = std::move(reason);
}

nlohmann::json SceneTrial::Result() const {
  auto result = nlohmann::json{
      {"status", status_},
      {"reason", reason_},
      {"coverage", "scene_playback_control"},
      {"expected_events", expected_.events},
      {"event_source", expected_.event_source},
      {"observed_events", observed_},
      {"matched_events", next_event_},
      {"return_zone", expected_.return_zone},
      {"return_room", expected_.return_room},
      {"preconditions", expected_.preconditions},
      {"campaign_progression", "not_established"},
      {"control_tick",
       control_tick_ ? nlohmann::json(*control_tick_) : nlohmann::json{}},
      {"passed_tick", passed_tick_},
      {"last", last_},
      {"dialogue_confirm_pulses", dialogue_pulses_}};
  if (expected_.ending_to_title || expected_.attract_to_title) {
    result["coverage"] = expected_.ending_to_title
                             ? "scene_playback_ending_to_title"
                             : "scene_playback_attract_to_title";
    result["terminal"] = terminal_;
    result.erase("return_zone");
    result.erase("return_room");
  }
  return result;
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
          key != "press" && key != "scene") {
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
    if (entry.contains("scene")) {
      script.scene = DecodeSceneExpectation(entry.at("scene"));
      if ((!script.setup.external_event &&
           script.setup.room_policy !=
               Game::EncounterRoomPolicy::kOriginalRoom) ||
          (script.setup.external_event &&
           script.scene->events.front() != *script.setup.external_event) ||
          std::ranges::any_of(script.presses, [](const auto& press) {
            return (press.buttons & VS::Input::Start) != 0;
          })) {
        throw std::runtime_error(
            "scene assertions require aligned production events and no Start "
            "skips");
      }
    }
    const bool attract_scene = script.scene && script.scene->attract_to_title;
    if (script.setup.attract_demo.has_value() != attract_scene ||
        (attract_scene && !script.presses.empty())) {
      throw std::runtime_error(
          "attract_demo setups require an attract_to_title scene without "
          "scripted presses");
    }
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
  auto result = nlohmann::json{
      {"version", 1},
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
  if (setup.external_event) result["external_event"] = *setup.external_event;
  if (setup.story_progress) result["story_progress"] = *setup.story_progress;
  if (setup.attract_demo) result["attract_demo"] = *setup.attract_demo;
  return result;
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
  if (json.contains("external_event")) {
    const auto& event = json.at("external_event");
    if (!event.is_number_integer() || event < 0 ||
        event > Game::EncounterLimits::kLastExternalEvent) {
      throw std::runtime_error(
          "external_event must be an integer asset number in 0..9999");
    }
    setup.external_event = event.get<int>();
  }
  if (json.contains("story_progress")) {
    const auto& progress = json.at("story_progress");
    if (!progress.is_number_integer() || progress < 0 || progress > 0xFF) {
      throw std::runtime_error("story_progress must be an integer in 0..255");
    }
    setup.story_progress = progress.get<int>();
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
  if (json.contains("attract_demo")) {
    const auto& cause = json.at("attract_demo");
    if (!cause.is_number_integer() || (cause != Game::kAttractDemoCutscene1 &&
                                       cause != Game::kAttractDemoCutscene2)) {
      throw std::runtime_error(
          "attract_demo must be title timeout cause 3 or 4");
    }
    setup.attract_demo = cause.get<int>();
    // Original title-timeout state only: Title_InitializeNewGameData's entry
    // tile/New Game kit and _setTitleExitFlags' door (TITLE 22C.c:4741).
    const Game::EncounterStatOverrides no_stat_overrides;
    const bool first = setup.attract_demo == Game::kAttractDemoCutscene1;
    if (setup.zone != (first ? 20 : 2) || setup.room != (first ? 0 : 1) ||
        setup.player.x != 4 || setup.player.z != 2 ||
        setup.player.elevation != 2 || setup.player.direction != 0 ||
        setup.room_policy != Game::EncounterRoomPolicy::kOriginalRoom ||
        setup.profile != Game::EncounterProfile::kPrologue ||
        setup.weapon != Game::EncounterWeapon::kDefault ||
        setup.stat_overrides != no_stat_overrides || !setup.enemies.empty() ||
        setup.external_event || setup.story_progress) {
      throw std::runtime_error(
          "attract_demo requires the original title-timeout door, entry "
          "tile and unmodified New Game production room");
    }
  }
  return setup;
}

}  // namespace VS::combat_lab
