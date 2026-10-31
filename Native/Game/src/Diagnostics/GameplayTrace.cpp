#include "Diagnostics/GameplayTrace.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Diagnostics/PlayerObservation.hpp"

namespace VS::Game {
namespace {
std::string GitOutput(const std::filesystem::path& directory,
                      const char* command, const char* argument) {
  const auto working = directory.string();
  const char* arguments[] = {"git",   "-C",     working.c_str(),
                             command, argument, nullptr};
  const std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process(
      SDL_CreateProcess(arguments, true), SDL_DestroyProcess);
  if (!process) return "unavailable";
  std::size_t size = 0;
  int exit_code = 0;
  const std::unique_ptr<void, decltype(&SDL_free)> output(
      SDL_ReadProcess(process.get(), &size, &exit_code), SDL_free);
  if (!output || exit_code != 0) return "unavailable";
  return {static_cast<const char*>(output.get()), size};
}
}  // namespace

GameplayTrace::GameplayTrace(const std::filesystem::path& parent,
                             std::string_view host,
                             const std::filesystem::path& compatibility_log,
                             bool player_observations, bool verification_log)
    : player_observations_(player_observations),
      verification_log_(verification_log) {
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
  if (!Core::SetLogMirrorFile(compatibility_log.string().c_str())) {
    Core::SetLogFile("");
    throw std::runtime_error("cannot create compatibility engine log");
  }
  Core::SetLogLevel(verification_log_ ? Core::LogLevel::Debug
                                      : Core::LogLevel::Trace);
  Core::SetLogChannels("*");
  previous_console_level_ = Core::SetConsoleLogLevel(Core::LogLevel::Warn);
  previous_flush_policy_ = Core::SetLogFlushEachMessage(false);
  previous_observer_ =
      Core::SetLogObserver([this](const Core::LogRecord& record) noexcept {
        try {
          if (record.channel == "event-command" ||
              record.channel == "event-result" ||
              record.channel == "state-flag") {
            const auto category =
                record.channel == "event-command"  ? "event.command"
                : record.channel == "event-result" ? "event.result"
                                                   : "state.flag_write";
            Event(category, nlohmann::json::parse(record.message), record.tick);
            if (verification_log_) return;
          }
          if (verification_log_ && record.level < Core::LogLevel::Info &&
              record.channel != "content" && record.channel != "boot") {
            return;
          }
          Event("engine",
                {{"channel", record.channel},
                 {"level", static_cast<int>(record.level)},
                 {"message", record.message}},
                record.tick);
        } catch (...) {
          Fail();
        }
      });
  Event("session.begin",
        {{"schema", 1},
         {"trace", verification_log_ ? "verification" : "full"},
         {"scoped_records", verification_log_ && !player_observations_
                                ? "per-script"
                                : "run-and-script"},
         {"scoped_player_frames",
          verification_log_ && player_observations_
              ? "base-state-with-shared-rich-record-reference"
              : "full"},
         {"host", host},
         {"build", __DATE__ " " __TIME__}});
}

GameplayTrace::~GameplayTrace() {
  try {
    Core::SetLogObserver(std::move(previous_observer_));
    if (script_stream_.is_open()) {
      EndScript(0, {{"status", "interrupted"}, {"reason", "host terminated"}});
    }
    Event("session.end", {{"trace_failed", Failed()}});
    Core::SetLogFlushEachMessage(previous_flush_policy_);
    Core::SetConsoleLogLevel(previous_console_level_);
    Core::SetLogFile("");
    Core::SetLogMirrorFile("");
  } catch (...) {
    Fail();
    std::fputs("Gameplay trace finalization failed\n", stderr);
  }
}

void GameplayTrace::Fail() noexcept {
  if (!failed_.exchange(true)) {
    std::fputs(
        "Gameplay trace write/flush failed; diagnostics are incomplete\n",
        stderr);
  }
}

std::filesystem::path GameplayTrace::BeginScript(std::size_t number,
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
    WriteRecord(
        "session.begin",
        {{"schema", 1}, {"trace", verification_log_ ? "verification" : "full"}},
        tick);
  }
  return path;
}

void GameplayTrace::EndScript(std::uint64_t tick,
                              const nlohmann::json& result) {
  std::lock_guard lock(mutex_);
  WriteRecord("session.end", {{"trace_failed", Failed()}, {"result", result}},
              tick);
  script_stream_.flush();
  if (!script_stream_) Fail();
  script_stream_.close();
  script_number_ = 0;
}

void GameplayTrace::Event(std::string_view category,
                          const nlohmann::json& details, std::uint64_t tick,
                          const nlohmann::json* scoped_details) noexcept {
  try {
    std::lock_guard lock(mutex_);
    WriteRecord(category, details, tick, scoped_details);
  } catch (...) {
    Fail();
  }
}

void GameplayTrace::WriteRecord(std::string_view category,
                                const nlohmann::json& details,
                                std::uint64_t tick,
                                const nlohmann::json* scoped_details) {
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
  const bool scoped_only =
      verification_log_ && !player_observations_ && script_stream_.is_open() &&
      (category == "frame" ||
       (category == "engine" && details.at("level").get<int>() <
                                    static_cast<int>(Core::LogLevel::Warn)) ||
       category == "event.command" || category == "event.result" ||
       category == "state.flag_write");
  if (!scoped_only) stream_ << line << '\n';
  if (script_stream_.is_open()) {
    if (scoped_details != nullptr) {
      record["data"] = *scoped_details;
      record["shared_record_sequence"] = sequence_;
      script_stream_ << record.dump() << '\n';
    } else {
      script_stream_ << line << '\n';
    }
  }
  const bool instruction = category == "event.command" ||
                           category == "event.result" ||
                           category == "state.flag_write";
  const bool boundary =
      !instruction &&
      (category != "engine" || details.at("level").get<int>() >=
                                   static_cast<int>(Core::LogLevel::Warn));
  if (category == "engine" && details.at("level").get<int>() >=
                                  static_cast<int>(Core::LogLevel::Error)) {
    ++errors_;
  }
  if (boundary) {
    stream_.flush();
    if (script_stream_.is_open()) script_stream_.flush();
    if (!Core::FlushLogFile()) Fail();
  }
  if (!stream_ || (script_stream_.is_open() && !script_stream_)) Fail();
}

nlohmann::json EncodeActor(const EncounterActorState& actor) {
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

namespace {
nlohmann::json EncodeInventory(const std::vector<EncounterItemState>& items) {
  auto result = nlohmann::json::array();
  for (const auto& item : items) {
    result.push_back(
        {{"slot", item.slot}, {"id", item.id}, {"count", item.count}});
  }
  return result;
}

nlohmann::json EncodeActors(const EncounterState& state) {
  auto actors = nlohmann::json::array();
  for (const auto& actor : state.actors) {
    actors.push_back(EncodeActor(actor));
  }
  return actors;
}

nlohmann::json EncodePendingLoot(const EncounterState& state) {
  auto pending_loot = nlohmann::json::array();
  for (const auto& loot : state.pending_loot) {
    pending_loot.push_back({{"source_actor", loot.source_actor},
                            {"items", EncodeInventory(loot.items)}});
  }
  return pending_loot;
}

nlohmann::json EncodeFloorTriggers(const EncounterState& state) {
  auto floor_triggers = nlohmann::json::array();
  for (const auto& trigger : state.floor_triggers) {
    floor_triggers.push_back({{"kind", trigger.kind},
                              {"tile_x", trigger.tile_x},
                              {"tile_z", trigger.tile_z},
                              {"activation", trigger.activation},
                              {"marker_fade", trigger.marker_fade}});
  }
  return floor_triggers;
}

nlohmann::json EncodeTargetPreviews(const EncounterState& state) {
  auto target_previews = nlohmann::json::array();
  for (const auto& target : state.target_previews) {
    target_previews.push_back({{"actor", target.actor},
                               {"limb", target.limb},
                               {"position", target.position}});
  }
  return target_previews;
}

nlohmann::json EncodeAreaVolumes(const EncounterState& state) {
  auto area_volumes = nlohmann::json::array();
  for (const auto& volume : state.area_volumes) {
    area_volumes.push_back({{"kind", volume.kind},
                            {"half_x", volume.half_x},
                            {"half_z", volume.half_z},
                            {"angle", volume.angle},
                            {"position", volume.position}});
  }
  return area_volumes;
}

nlohmann::json EncodeInput(const Input::PadState& input) {
  return {
      {"held", input.held},
      {"pressed", input.pressed},
      {"released", input.released},
      {"repeat", input.repeat},
      {"analog", input.analog},
      {"sticks",
       {input.sticks.lx, input.sticks.ly, input.sticks.rx, input.sticks.ry}}};
}
}  // namespace

void GameplayTrace::Frame(std::uint64_t tick, const Input::PadState& input,
                          const EncounterState& state,
                          const nlohmann::json& scene) {
  nlohmann::json data{
      {"input", EncodeInput(input)},
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
      {"area_positioning", state.area_positioning},
      {"area_volumes", EncodeAreaVolumes(state)},
      {"target_previews", EncodeTargetPreviews(state)},
      {"target_page",
       {{"total", state.target_page.total},
        {"page", state.target_page.page},
        {"pages", state.target_page.pages},
        {"first", state.target_page.first},
        {"visible", state.target_page.visible},
        {"selected", state.target_page.selected}}},
      {"rewards_available", state.rewards_available},
      {"pending_loot", EncodePendingLoot(state)},
      {"items", EncodeInventory(state.items)},
      {"learned_actions", state.learned_actions},
      {"floor_triggers", EncodeFloorTriggers(state)},
      {"actors", EncodeActors(state)}};
  if (state.roulette) {
    const auto& reel = *state.roulette;
    data["roulette"] = {{"entry_phase", reel.entry_phase},
                        {"screen_state", reel.screen_state},
                        {"timer", reel.timer},
                        {"animation", reel.animation},
                        {"reel", reel.reel},
                        {"selection", reel.selection},
                        {"bios_state", reel.bios_state}};
  }
  nlohmann::json scoped_frame;
  const bool shared_player_frame =
      verification_log_ && player_observations_ && script_stream_.is_open();
  if (shared_player_frame) scoped_frame = data;
  if (player_observations_) AppendPlayerObservations(data, state);
  if (!scene.is_null()) data["scene"] = scene;
  if (shared_player_frame) {
    if (!scene.is_null()) scoped_frame["scene"] = scene;
    Event("frame", data, tick, &scoped_frame);
  } else {
    Event("frame", data, tick);
  }
}

void WriteTraceManifest(const std::filesystem::path& directory,
                        const nlohmann::json& metadata) {
  std::ofstream stream(directory / "manifest.json");
  stream << metadata.dump(2) << '\n';
  stream.flush();
  if (!stream) throw std::runtime_error("cannot write diagnostic manifest");
}

nlohmann::json TraceSourceProvenance(const std::filesystem::path& directory) {
  const auto revision = GitOutput(directory, "rev-parse", "HEAD");
  const auto worktree = GitOutput(directory, "status", "--porcelain");
  return {{"revision", revision},
          {"worktree", worktree},
          {"status", revision == "unavailable" || worktree == "unavailable"
                         ? "unavailable"
                         : "available"}};
}

}  // namespace VS::Game
