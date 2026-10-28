#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Audio/AudioDevice.hpp"
#include "Battle/Encounter.hpp"
#include "CombatLab/Canvas.hpp"
#include "CombatLab/Editor.hpp"
#include "CombatLab/SceneCatalog.hpp"
#include "CombatLab/Trace.hpp"
#include "Content/DataAccess.h"
#include "Content/DataStore.hpp"
#include "Core/FrameClock.hpp"
#include "Core/Log.hpp"
#include "Input/LegacyPad.hpp"
#include "Input/Pad.hpp"
#include "Movie/MoviePlayer.h"
#include "Platform/App.hpp"
#include "Platform/CrashHandler.hpp"
#include "Platform/GameFiber.hpp"
#include "Render/Device.hpp"
#include "Render/PageSpace.hpp"
#include "Save/NativeMenu.hpp"

namespace VS::combat_lab {
namespace {
namespace fs = std::filesystem;

constexpr std::array<Game::EncounterAdjustment, 9> kTestConditions = {
    Game::EncounterAdjustment::kPoison,
    Game::EncounterAdjustment::kParalysis,
    Game::EncounterAdjustment::kNumbness,
    Game::EncounterAdjustment::kCurse,
    Game::EncounterAdjustment::kSilence,
    Game::EncounterAdjustment::kStrengthDown,
    Game::EncounterAdjustment::kIntelligenceDown,
    Game::EncounterAdjustment::kAgilityDown,
    Game::EncounterAdjustment::kEquipmentDown,
};

struct Options {
  fs::path data;
  fs::path scenario;
  fs::path sequence;
  fs::path capture;
  fs::path save_directory;
  std::uint64_t frames = 0;
  int speed = 1;
  bool start = false;
  bool catalog_only = false;
  bool devices = true;
  bool player_observations = false;
  bool door_transitions = false;
  bool scene_catalog = false;
  std::string door_case;
  std::string door_from;
  fs::path door_resume;
  std::uint64_t door_limit = 0;
  std::vector<ScriptPress> presses;
  std::optional<Render::EffectPathTraceRange> effect_paths;
};

struct PhaseTiming {
  std::uint64_t count = 0;
  std::uint64_t total_us = 0;
  std::uint64_t maximum_us = 0;
  std::uint64_t over_tick_budget = 0;
  std::uint64_t over_frame_budget = 0;

  void Record(std::uint64_t start) {
    constexpr std::uint64_t kNsPerMicrosecond = 1000;
    constexpr std::uint64_t kTickBudgetUs = 16667;
    constexpr std::uint64_t kFrameBudgetUs = 33333;
    const auto elapsed = (SDL_GetTicksNS() - start) / kNsPerMicrosecond;
    ++count;
    total_us += elapsed;
    maximum_us = std::max(maximum_us, elapsed);
    over_tick_budget += elapsed > kTickBudgetUs;
    over_frame_budget += elapsed > kFrameBudgetUs;
  }

  nlohmann::json Summary() const {
    constexpr double kUsPerMillisecond = 1000;
    return {{"samples", count},
            {"mean_ms", count == 0 ? 0.0
                                   : static_cast<double>(total_us) /
                                         static_cast<double>(count) /
                                         kUsPerMillisecond},
            {"max_ms", static_cast<double>(maximum_us) / kUsPerMillisecond},
            {"over_16_667_ms", over_tick_budget},
            {"over_33_333_ms", over_frame_budget}};
  }
};

fs::path FindData() {
  for (auto path = Platform::ExecutableDir(); !path.empty();) {
    const auto candidate = path / "Native" / "data";
    if (fs::exists(candidate / Content::DataStore::ManifestName)) {
      return candidate;
    }
    const auto parent = path.parent_path();
    if (parent == path) break;
    path = parent;
  }
  throw std::runtime_error("extracted data not found; pass --data <directory>");
}

void ParseValue(Options& options, std::string_view argument,
                std::string_view value) {
  if (argument == "--data") {
    options.data = value;
  } else if (argument == "--scenario") {
    options.scenario = value;
  } else if (argument == "--sequence") {
    options.sequence = value;
  } else if (argument == "--door-case") {
    options.door_case = value;
  } else if (argument == "--door-from") {
    options.door_from = value;
  } else if (argument == "--door-resume") {
    if (value.empty() || value.starts_with("--")) {
      throw std::runtime_error("--door-resume requires a checkpoint file");
    }
    options.door_resume = value;
  } else if (argument == "--capture") {
    options.capture = value;
  } else if (argument == "--save-dir") {
    if (value.empty() || value.starts_with("--")) {
      throw std::runtime_error("--save-dir requires a directory");
    }
    options.save_directory =
        fs::path(std::u8string(value.begin(), value.end()));
  } else if (argument == "--frames") {
    options.frames = ParseTick(value);
  } else if (argument == "--door-limit") {
    options.door_limit = ParseTick(value);
    if (options.door_limit == 0) {
      throw std::runtime_error("--door-limit must be positive");
    }
  } else if (argument == "--speed") {
    const auto speed = ParseTick(value);
    constexpr std::uint64_t kMaximumSpeed = 64;
    if (speed < 1 || speed > kMaximumSpeed) {
      throw std::runtime_error("speed must be 1-64");
    }
    options.speed = static_cast<int>(speed);
  } else if (argument == "--press") {
    const auto presses = DecodePresses(value);
    options.presses.insert(options.presses.end(), presses.begin(),
                           presses.end());
  } else if (argument == "--trace-effect-paths") {
    const auto range = Render::EffectPathTraceRange::Parse(value);
    if (!range) throw std::runtime_error(range.error());
    options.effect_paths = *range;
  } else {
    throw std::runtime_error("unknown option: " + std::string(argument));
  }
}

void ValidateOptions(Options& options) {
  if (options.scene_catalog && !options.catalog_only) {
    throw std::runtime_error("--scene-catalog requires --catalog-only");
  }
  if (options.effect_paths && options.speed != 1) {
    throw std::runtime_error("--trace-effect-paths requires normal --speed 1");
  }
  if (!options.sequence.empty() &&
      (!options.scenario.empty() || options.start || options.catalog_only ||
       options.frames != 0 || !options.presses.empty())) {
    throw std::runtime_error(
        "--sequence cannot combine with "
        "scenario/start/frames/press/catalog-only");
  }
  if (!options.door_transitions) {
    if (!options.door_case.empty() || !options.door_from.empty() ||
        !options.door_resume.empty() || options.door_limit) {
      throw std::runtime_error("door selectors require --door-transitions");
    }
    return;
  }
  if (!options.scenario.empty() || !options.sequence.empty() ||
      !options.presses.empty() || options.start ||
      (!options.door_case.empty() && !options.door_from.empty())) {
    throw std::runtime_error(
        "door tests cannot combine with scenario/sequence/start/press or both "
        "door selectors");
  }
  constexpr std::uint64_t kDefaultDoorFrameBudget = 3600;
  options.devices = false;
  if (options.frames == 0) options.frames = kDefaultDoorFrameBudget;
  if (options.frames > UINT64_MAX - kScriptStopGraceTicks) {
    throw std::runtime_error("door frame budget exceeds supported tick range");
  }
}

Options Parse(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "--start") {
      options.start = true;
    } else if (argument == "--catalog-only") {
      options.catalog_only = true;
    } else if (argument == "--scripted") {
      options.devices = false;
    } else if (argument == "--player-observations") {
      options.player_observations = true;
    } else if (argument == "--door-transitions") {
      options.door_transitions = true;
    } else if (argument == "--scene-catalog") {
      options.scene_catalog = true;
    } else {
      if (++index == argc) {
        throw std::runtime_error("option is missing a value");
      }
      ParseValue(options, argument, argv[index]);
    }
  }
  ValidateOptions(options);
  if (options.data.empty()) options.data = FindData();
  return options;
}

void WriteJson(const fs::path& path, const nlohmann::json& value) {
  std::ofstream stream(path);
  stream << value.dump(2) << '\n';
  stream.flush();
  if (!stream) throw std::runtime_error("cannot write " + path.string());
}

void Capture(const fs::path& path, const std::vector<std::uint32_t>& pixels,
             int width, int height) {
  if (path.empty() || pixels.empty()) return;
  if (!path.parent_path().empty()) fs::create_directories(path.parent_path());
  const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
      SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_ABGR8888,
                            const_cast<std::uint32_t*>(pixels.data()),
                            width * static_cast<int>(sizeof(std::uint32_t))),
      SDL_DestroySurface);
  if (!surface || !SDL_SaveBMP(surface.get(), path.string().c_str())) {
    throw std::runtime_error(std::string("capture failed: ") + SDL_GetError());
  }
}

void LoadData(const Options& options) {
  if (const auto opened = Content::DataStore::Open(options.data); !opened) {
    throw std::runtime_error(opened.error());
  }
  if (Content_LoadModuleData() != 0) {
    throw std::runtime_error("required production module data failed to load");
  }
}

std::vector<Game::EncounterZone> LoadCatalog(const Options& options,
                                             Trace& trace) {
  auto catalog = Game::ReadEncounterCatalog(options.door_transitions ||
                                            options.scene_catalog);
  if (!catalog) throw std::runtime_error(catalog.error());
  auto catalog_json = nlohmann::json::array();
  for (const auto& zone : *catalog) {
    auto enemies = nlohmann::json::array();
    for (const auto& enemy : zone.enemies) {
      enemies.push_back({{"index", enemy.index},
                         {"name", enemy.name},
                         {"hp", enemy.hp},
                         {"mp", enemy.mp},
                         {"spawn_asset", enemy.spawn_asset},
                         {"spawn_index", enemy.spawn_index},
                         {"unavailable", enemy.unavailable_reason}});
    }
    auto rooms = nlohmann::json::array();
    for (const auto& room : zone.rooms) {
      rooms.push_back({{"index", room.index},
                       {"asset", room.asset},
                       {"width", room.width},
                       {"depth", room.depth},
                       {"unavailable", room.unavailable_reason}});
    }
    catalog_json.push_back({{"zone", zone.id},
                            {"asset", zone.asset},
                            {"rooms", rooms},
                            {"enemies", enemies}});
  }
  WriteJson(trace.Directory() / "catalog.json", catalog_json);
  trace.Event("catalog", {{"zones", catalog->size()},
                          {"data_root", fs::absolute(options.data).string()}});
  return std::move(*catalog);
}

bool ShowLoading(Platform::App& app, Render::Device& gpu, Canvas& canvas,
                 Input::InputSystem& input, Trace& trace,
                 std::string_view message, bool first = false) {
  if (!app.PumpEvents(
          [&input](const SDL_Event& event) { input.HandleEvent(event); })) {
    trace.Event("startup.cancelled", {{"reason", "window_closed"}});
    return false;
  }
  canvas.Clear();
  constexpr int kLeft = 32, kHeadingTop = 48, kMessageTop = 92;
  canvas.Text(kLeft, kHeadingTop, "COMBAT LAB");
  canvas.Text(kLeft, kMessageTop, message);
  if ((SDL_GetWindowFlags(app.Window()) &
       (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) != 0) {
    return true;
  }
  if (!gpu.PresentImage(canvas.Pixels(), Canvas::kWidth, Canvas::kHeight)) {
    throw std::runtime_error("cannot present the Combat Lab loading screen");
  }
  if (first) {
    trace.Event("startup.first_present", {{"content", "readable_loading"}});
  }
  return true;
}

void InitializeWindow(Platform::App& app, Render::Device& gpu, Trace& trace) {
  Platform::WindowDesc window;
  window.title = "Vagrant Story - Combat Lab";
  window.initialize_devices = false;
  trace.Event("startup.stage", {{"stage", "window.begin"}});
  if (const auto initialized = app.Init(window); !initialized) {
    throw std::runtime_error(initialized.error());
  }
  trace.Event("startup.stage", {{"stage", "window.end"}});
  trace.Event("host.window", {{"id", SDL_GetWindowID(app.Window())}});
  trace.Event("startup.stage", {{"stage", "gpu.begin"}});
  if (const auto initialized = gpu.Init(app.Window(), true); !initialized) {
    throw std::runtime_error(initialized.error());
  }
  trace.Event("startup.stage", {{"stage", "gpu.end"}});
}

void WriteManifest(const Options& options, Trace& trace, std::size_t scripts) {
  const auto source = Game::TraceSourceProvenance(Platform::ExecutableDir());
  WriteJson(trace.Directory() / "manifest.json",
            {{"schema", 1},
             {"revision", source.at("revision")},
             {"worktree", source.at("worktree")},
             {"source_status", source.at("status")},
             {"data_root", fs::absolute(options.data).string()},
             {"configuration",
              options.scene_catalog      ? "static scene inventory"
              : options.door_transitions ? "single-window door checks"
              : scripts == 0             ? "standalone production-gameplay lab"
                                         : "single-window script sequence"},
             {"scene_catalog", options.scene_catalog},
             {"frames", options.frames},
             {"sequence", options.sequence.string()},
             {"door_resume", options.door_resume.empty()
                                 ? std::string{}
                                 : fs::absolute(options.door_resume).string()},
             {"scripts", scripts},
             {"speed", options.speed},
             {"effect_path_trace",
              options.effect_paths
                  ? nlohmann::json{{"first", options.effect_paths->first},
                                   {"last", options.effect_paths->last}}
                  : nlohmann::json(nullptr)},
             {"devices", options.devices}});
}

void CompleteRetainedDoors(const Options& options, Trace& trace,
                           const nlohmann::json& retained_passed) {
  WriteManifest(options, trace, 0);
  WriteJson(trace.Directory() / "sequence-results.json",
            EncodeDoorCheckpoint(nlohmann::json::array(), retained_passed,
                                 "completed"));
  trace.Event("door.resume.complete",
              {{"executed_cases", 0},
               {"retained_passed_count", retained_passed.size()}});
}

void WriteSceneInventory(const Options& options, Trace& trace,
                         const std::vector<Game::EncounterZone>& catalog) {
  const auto inventory = ReadSceneCatalog(catalog);
  WriteManifest(options, trace, 0);
  WriteJson(trace.Directory() / "scene-inventory.json", inventory);
  trace.Event("scene.catalog", {{"assets", inventory.at("assets").size()},
                                {"errors", inventory.at("errors")}});
  if (inventory.at("errors") != 0) {
    throw std::runtime_error(
        "scene inventory contains invalid definitions; see "
        "scene-inventory.json");
  }
}

void WriteDoorInventory(Trace& trace,
                        const std::vector<Game::EncounterZone>& catalog,
                        const std::vector<DoorTestCase>& tests) {
  auto cases = nlohmann::json::array();
  for (const auto& test : tests) cases.push_back(EncodeDoorTest(test));
  auto unavailable = nlohmann::json::array();
  std::size_t rooms = 0;
  for (const auto& zone : catalog) {
    rooms += zone.rooms.size();
    for (const auto& room : zone.rooms) {
      if (!room.unavailable_reason.empty()) {
        unavailable.push_back({{"zone", zone.id},
                               {"room", room.index},
                               {"reason", room.unavailable_reason}});
      }
    }
  }
  WriteJson(trace.Directory() / "door-inventory.json",
            {{"schema", 1},
             {"coverage", "loader_entry_event"},
             {"zones", catalog.size()},
             {"rooms", rooms},
             {"directed_doors", tests.size()},
             {"unavailable_rooms", unavailable},
             {"cases", cases}});
}

std::vector<EncounterScript> BuildDoorScripts(
    const Options& options, Trace& trace,
    const std::vector<Game::EncounterZone>& catalog,
    nlohmann::json& retained_passed) {
  auto tests = EnumerateDoorTests(catalog);
  WriteDoorInventory(trace, catalog, tests);
  auto prior = options.door_resume.empty()
                   ? nlohmann::json::array()
                   : ReadDoorCheckpoint(options.door_resume);
  for (const auto& entry : prior) {
    const auto root = entry.at("provenance").at("data_root").get<std::string>();
    if (!fs::equivalent(root, options.data)) {
      throw std::runtime_error("door checkpoint uses a different data root");
    }
  }
  auto plan = PlanDoorResume(
      tests, {options.door_from, options.door_case, options.door_limit}, prior);
  retained_passed = std::move(plan.retained_passed);
  auto pending_ids = nlohmann::json::array();
  for (const auto& test : plan.pending) pending_ids.push_back(test.id);
  const nlohmann::json selection = {
      {"checkpoint", options.door_resume.string()},
      {"selected_cases", plan.pending.size()},
      {"pending", pending_ids},
      {"retained_passed_count", retained_passed.size()},
      {"invalidated_passes", plan.invalidated}};
  WriteJson(trace.Directory() / "door-plan.json", selection);
  trace.Event("door.resume.plan", selection);
  VS_LOG_INFO("combat-lab", "door plan: {} to run, {} prior passes retained",
              plan.pending.size(), retained_passed.size());
  std::vector<EncounterScript> scripts;
  for (const auto& test : plan.pending) {
    EncounterScript script;
    script.name = test.id;
    script.scenario = trace.Directory() / "door-inventory.json";
    script.setup = test.setup;
    script.frames = options.frames;
    script.door = test;
    scripts.push_back(std::move(script));
  }
  return scripts;
}

int RunCatalogOnly(const Options& options, Trace& trace) {
  const auto catalog = LoadCatalog(options, trace);
  nlohmann::json retained = nlohmann::json::array();
  if (options.door_transitions) {
    BuildDoorScripts(options, trace, catalog, retained);
  }
  if (options.scene_catalog) WriteSceneInventory(options, trace, catalog);
  return trace.Failed() ? 1 : 0;
}

void ValidateScripts(const std::vector<EncounterScript>& scripts,
                     const std::vector<Game::EncounterZone>& catalog) {
  for (const auto& script : scripts) {
    if (script.door && !script.door->unavailable.empty()) continue;
    const auto zone =
        std::ranges::find(catalog, script.setup.zone, &Game::EncounterZone::id);
    if (zone == catalog.end()) {
      throw std::runtime_error("script " + script.name + ": zone is missing");
    }
    const auto errors = Game::ValidateEncounter(script.setup, *zone);
    if (!errors.empty()) {
      throw std::runtime_error("script " + script.name + ": " + errors.front());
    }
  }
}

class LiveSession {
 public:
  LiveSession(const Options& options, Trace& trace,
              std::vector<Game::EncounterZone> catalog,
              std::vector<EncounterScript> scripts, Canvas canvas,
              Input::InputSystem& input, nlohmann::json retained_passed)
      : options_(options),
        trace_(trace),
        canvas_(std::move(canvas)),
        editor_(std::move(catalog)),
        input_(input),
        watchdog_(options.frames != 0 || !scripts.empty()),
        fiber_(),
        ticks_(0),
        encounter_number_(0),
        start_(options.start),
        confirm_(false),
        paused_(false),
        step_(false),
        rejected_(false),
        scripts_(std::move(scripts)),
        retained_passed_(std::move(retained_passed)) {
    input_.SetDevicesEnabled(options_.devices);
    if (!options_.scenario.empty()) {
      std::ifstream file(options_.scenario);
      if (!file) throw std::runtime_error("cannot open encounter preset");
      editor_.Load(DecodeSetup(nlohmann::json::parse(file)));
    }
    if (!scripts_.empty()) {
      for (const auto& script : scripts_) {
        results_.push_back({{"name", script.name},
                            {"scenario", script.scenario.string()},
                            {"frames", script.frames},
                            {"status", "not_run"}});
        if (script.door) {
          results_.back()["door"] = EncodeDoorTest(*script.door);
        }
      }
      LoadScript();
    }
  }

  int Run(Platform::App& app, Render::Device& gpu) {
    window_ = app.Window();
    UpdateWindowTitle();
    Core::FrameClock clock;
    previous_ = app.NowSeconds();
    const auto handle_event = [this](const SDL_Event& event) { Handle(event); };
    while (app.PumpEvents(handle_event)) {
      const double now = app.NowSeconds();
      const auto count = clock.Advance(now - previous_) * options_.speed;
      previous_ = now;
      for (unsigned index = 0; index < count; ++index) {
        if (Finished()) break;
        if (paused_ && !step_) {
          watchdog_.Pulse(ticks_);
          break;
        }
        Tick();
      }
      Present(gpu);
      if (Finished()) break;
    }
    if (!scripts_.empty() && !sequence_done_) {
      CancelSequence("window_closed");
      while (fiber_) Tick();
    }
    Capture(options_.capture, pixels_, width_, height_);
    trace_.Event("performance.summary",
                 {{"simulation", simulation_timing_.Summary()},
                  {"inspection", inspection_timing_.Summary()},
                  {"trace_frame", trace_timing_.Summary()},
                  {"frame_interval", frame_interval_timing_.Summary()},
                  {"presentation", presentation_timing_.Summary()}},
                 ticks_);
    trace_.Event(
        "host.exit",
        {{"ticks", ticks_}, {"encounter_interrupted", fiber_ != nullptr}},
        ticks_);
    fiber_.reset();
    Movie_PlayerClose();
    return rejected_ || sequence_failed_ || sequence_cancelled_ ? 2 : 0;
  }

 private:
  const Options& options_;
  Trace& trace_;
  Canvas canvas_;
  Editor editor_;
  Input::InputSystem& input_;
  Platform::ProgressWatchdog watchdog_;
  std::unique_ptr<Platform::GameFiber> fiber_;
  std::vector<std::uint32_t> pixels_;
  int width_ = 0;
  int height_ = 0;
  std::uint64_t ticks_;
  std::uint64_t encounter_number_;
  bool start_;
  bool confirm_;
  bool paused_;
  bool step_;
  bool rejected_;
  std::vector<EncounterScript> scripts_;
  std::size_t script_index_ = 0;
  fs::path script_directory_;
  nlohmann::json results_ = nlohmann::json::array();
  nlohmann::json retained_passed_ = nlohmann::json::array();
  std::uint64_t script_start_tick_ = 0;
  std::uint64_t script_stop_tick_ = 0;
  std::uint64_t script_start_errors_ = 0;
  bool script_initialized_ = false;
  bool script_stopping_ = false;
  bool sequence_done_ = false;
  bool sequence_failed_ = false;
  bool sequence_cancelled_ = false;
  std::string script_stop_reason_;
  std::optional<DoorTrial> door_trial_;
  SDL_Window* window_ = nullptr;
  bool help_ = false;
  bool paused_before_help_ = false;
  std::size_t condition_ = 0;
  std::string adjustment_message_ = "PRECONDITIONS ONLY - USE ITEMS NORMALLY";
  double previous_ = 0;
  PhaseTiming simulation_timing_;
  PhaseTiming inspection_timing_;
  PhaseTiming trace_timing_;
  PhaseTiming presentation_timing_;
  PhaseTiming frame_interval_timing_;
  std::uint64_t previous_presentation_ = 0;

  void UpdateWindowTitle() {
    if (!scripts_.empty() && !sequence_done_ && window_ != nullptr) {
      const auto title =
          std::format("Combat Lab - {}/{}: {}", script_index_ + 1,
                      scripts_.size(), scripts_.at(script_index_).name);
      if (!SDL_SetWindowTitle(window_, title.c_str())) {
        throw std::runtime_error(SDL_GetError());
      }
    }
  }

  void SaveSequence() {
    nlohmann::json summary = {{"schema", 1},
                              {"status", sequence_cancelled_ ? "cancelled"
                                         : sequence_failed_  ? "failed"
                                         : sequence_done_    ? "completed"
                                                             : "running"},
                              {"scripts", results_}};
    if (options_.door_transitions) {
      summary = EncodeDoorCheckpoint(results_, retained_passed_,
                                     summary.at("status").get<std::string>());
    }
    WriteJson(trace_.Directory() / "sequence-results.json", summary);
  }

  void LoadScript() {
    const auto& script = scripts_.at(script_index_);
    door_trial_.reset();
    if (script.door) door_trial_.emplace(*script.door);
    if (!door_trial_ || !door_trial_->Finished()) editor_.Load(script.setup);
    script_start_tick_ = ticks_;
    script_start_errors_ = trace_.Errors();
    script_stop_tick_ = 0;
    script_initialized_ = script_stopping_ = false;
    script_stop_reason_.clear();
    input_.Inject(0);
    input_.Step(0, 1);
    script_directory_ = trace_.BeginScript(script_index_ + 1, ticks_);
    WriteJson(script_directory_ / "encounter.json", EncodeSetup(script.setup));
    std::ifstream manifest(trace_.Directory() / "manifest.json");
    if (!manifest) throw std::runtime_error("run manifest is missing");
    auto metadata = nlohmann::json::parse(manifest);
    metadata["frames"] = script.frames;
    metadata["script"] = script_index_ + 1;
    metadata["scenario"] = script.scenario.string();
    WriteJson(script_directory_ / "manifest.json", metadata);
    results_[script_index_]["status"] = "running";
    results_[script_index_]["trace"] = script_directory_.filename().string();
    SaveSequence();
    trace_.Event("sequence.script.begin", results_[script_index_], ticks_);
    start_ = true;
    UpdateWindowTitle();
  }

  void RequestScriptStop(std::string reason) {
    if (script_stopping_) return;
    script_stopping_ = true;
    script_stop_tick_ = ticks_;
    script_stop_reason_ = std::move(reason);
    paused_ = help_ = false;
    Game::RequestEncounterStop();
    trace_.Event("control.stop", {{"reason", script_stop_reason_}}, ticks_);
  }

  void FinishScript() {
    const auto errors = trace_.Errors() - script_start_errors_;
    if (door_trial_ && (!door_trial_->Finished() || errors != 0)) {
      door_trial_->Fail(script_stop_reason_.empty()
                            ? "encounter ended before transition acceptance"
                            : script_stop_reason_);
    }
    const bool door_case = scripts_.at(script_index_).door.has_value();
    const auto door_result =
        door_trial_ ? door_trial_->Result() : nlohmann::json{};
    const bool complete = script_initialized_ && errors == 0 &&
                          (door_trial_ ? door_result.at("status") == "passed"
                                       : script_stop_reason_ == "frame_budget");
    auto& result = results_[script_index_];
    result["status"] = sequence_cancelled_ ? "cancelled"
                       : door_trial_
                           ? door_result.at("status").get<std::string>()
                       : complete ? "completed"
                                  : "failed";
    if (door_trial_) result["door"] = door_result;
    result["initialized"] = script_initialized_;
    result["engine_errors"] = errors;
    result["ticks"] = ticks_ - script_start_tick_;
    result["end_reason"] = script_stop_reason_.empty() ? "encounter_ended_early"
                                                       : script_stop_reason_;
    result["teardown_complete"] = fiber_ == nullptr;
    WriteJson(script_directory_ / "result.json", result);
    trace_.Event("sequence.script.end", result, ticks_);
    trace_.EndScript(ticks_, result);
    sequence_failed_ |= !complete && !sequence_cancelled_;
    ++script_index_;
    sequence_done_ = (!door_case && sequence_failed_) || sequence_cancelled_ ||
                     script_index_ == scripts_.size();
    SaveSequence();
    if (!sequence_done_) LoadScript();
  }

  void CancelSequence(std::string reason) {
    sequence_cancelled_ = true;
    if (fiber_) {
      if (script_stopping_) script_stop_reason_ = reason;
      RequestScriptStop(std::move(reason));
    } else {
      script_stop_reason_ = std::move(reason);
      FinishScript();
    }
  }

  void UpdateDoorTrial(DoorTrial& trial, const DoorTestCase& test,
                       const Game::EncounterState& state,
                       const nlohmann::json& scene) {
    const auto& script = scripts_.at(script_index_);
    const bool idle =
        scene.value("available", false) && scene.at("event").at("phase") == 0;
    const bool dialogue =
        scene.value("available", false) &&
        std::ranges::any_of(
            scene.at("event").at("dialogue_waits"),
            [](const auto& wait) { return wait.template get<bool>(); });
    if (trial.Observe(state, idle, ticks_ - script_start_tick_, dialogue)) {
      const auto request =
          Game::RequestEncounterDoorTransition(test.door.index);
      trial.Requested(request, ticks_ - script_start_tick_);
      if (request.status != Game::EncounterDoorRequestStatus::kBusy) {
        trace_.Event(
            "door.request",
            {{"id", script.name},
             {"accepted",
              request.status == Game::EncounterDoorRequestStatus::kAccepted},
             {"reason", request.reason}},
            ticks_);
      }
    }
    if (ticks_ - script_start_tick_ >= script.frames) trial.Timeout();
    if (trial.Finished()) {
      const auto status = trial.Result().at("status").get<std::string>();
      RequestScriptStop("door_" + status);
    }
  }

  void UpdateScript(const Game::EncounterState& state,
                    const nlohmann::json& scene) {
    if (scripts_.empty()) return;
    const auto& script = scripts_.at(script_index_);
    script_initialized_ |= state.ready && state.zone == script.setup.zone &&
                           state.room == script.setup.room;
    if (trace_.Errors() != script_start_errors_) {
      RequestScriptStop("engine_error");
    }
    if (door_trial_ && !script_stopping_) {
      if (!script.door) {
        throw std::logic_error("door trial has no inventory case");
      }
      UpdateDoorTrial(*door_trial_, *script.door, state, scene);
    } else if (!door_trial_ && ticks_ - script_start_tick_ >= script.frames) {
      RequestScriptStop("frame_budget");
    }
    if (script_stopping_ &&
        ticks_ - script_stop_tick_ > kScriptStopGraceTicks) {
      throw std::runtime_error("script encounter teardown exceeded 600 ticks");
    }
  }

  void Handle(const SDL_Event& event) {
    input_.HandleEvent(event);
    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) return;
    if (!scripts_.empty() && !sequence_done_ && event.key.key == SDLK_F10) {
      CancelSequence("F10");
      return;
    }
    if (!fiber_ && event.key.key == SDLK_RETURN) confirm_ = true;
    if (fiber_ && HandleLabKey(event.key.key)) return;
    if (fiber_ && event.key.key == SDLK_F10) {
      trace_.Event("control.stop", {}, ticks_);
      Game::RequestEncounterStop();
      paused_ = false;
      help_ = false;
    }
    if (fiber_ && event.key.key == SDLK_F6) {
      paused_ = !paused_;
      trace_.Event("control.pause", {{"paused", paused_}}, ticks_);
    }
    if (fiber_ && event.key.key == SDLK_F7) {
      paused_ = true;
      step_ = true;
      trace_.Event("control.step", {}, ticks_);
    }
  }

  void Adjust(Game::EncounterAdjustment adjustment) {
    const auto result = Game::AdjustEncounterPlayer(adjustment);
    nlohmann::json details = {
        {"adjustment", Game::EncounterAdjustmentName(adjustment)},
        {"applied", result.has_value()}};
    if (result) {
      details["before"] = EncodeActor(result->before);
      details["after"] = EncodeActor(result->after);
      adjustment_message_ =
          std::format("APPLIED: {}", Game::EncounterAdjustmentName(adjustment));
    } else {
      details["reason"] = result.error();
      adjustment_message_ = "REJECTED: " + result.error();
    }
    trace_.Event("control.adjust", details, ticks_);
  }

  bool HandleLabKey(SDL_Keycode key) {
    if (options_.door_transitions &&
        (key == SDLK_F1 || key == SDLK_F2 || key == SDLK_F3 || key == SDLK_F4 ||
         key == SDLK_F5 || key == SDLK_F8)) {
      trace_.Event(
          "control.rejected",
          {{"reason",
            "automated door fixtures prohibit live stat/status adjustments"}},
          ticks_);
      return true;
    }
    if (key == SDLK_F11) {
      help_ = !help_;
      if (help_) {
        paused_before_help_ = paused_;
        paused_ = true;
        step_ = false;
      } else {
        paused_ = paused_before_help_;
      }
      trace_.Event("control.help", {{"visible", help_}, {"paused", paused_}},
                   ticks_);
      return true;
    }
    if (help_ && (key == SDLK_F6 || key == SDLK_F7)) {
      adjustment_message_ = "CLOSE HELP WITH F11 BEFORE PAUSING OR STEPPING";
      trace_.Event("control.rejected", {{"reason", adjustment_message_}},
                   ticks_);
      return true;
    }
    switch (key) {
      case SDLK_F1:
        Adjust(Game::EncounterAdjustment::kLowerHp);
        return true;
      case SDLK_F2:
        Adjust(Game::EncounterAdjustment::kLowerMp);
        return true;
      case SDLK_F3:
        Adjust(Game::EncounterAdjustment::kRaiseRisk);
        return true;
      case SDLK_F4:
        condition_ = (condition_ + 1) % kTestConditions.size();
        adjustment_message_ = std::format(
            "SELECTED: {}",
            Game::EncounterAdjustmentName(kTestConditions[condition_]));
        trace_.Event("control.condition", {{"selected", adjustment_message_}},
                     ticks_);
        return true;
      case SDLK_F5:
        Adjust(kTestConditions[condition_]);
        return true;
      case SDLK_F8:
        Adjust(Game::EncounterAdjustment::kRestoreMp);
        return true;
      default:
        return false;
    }
  }

  void UpdateEditor(const Input::PadState& pad) {
    if (door_trial_ && door_trial_->Finished()) {
      script_stop_reason_ = "door_blocked";
      FinishScript();
      return;
    }
    const auto action = editor_.Update(pad, std::exchange(confirm_, false));
    if (pad.pressed != 0) {
      trace_.Event("editor.input", {{"buttons", pad.pressed}}, ticks_);
    }
    if (action == Editor::Action::kSave) {
      const auto path =
          Platform::ExecutableDir() / "combat-lab" / "last-scenario.json";
      WriteJson(path, EncodeSetup(editor_.Setup()));
      editor_.Message("SAVED COMBAT-LAB/LAST-SCENARIO.JSON");
      trace_.Event("preset.saved", {{"path", path.string()}}, ticks_);
    }
    if (start_ || action == Editor::Action::kStart) {
      start_ = false;
      const auto setup = editor_.Setup();
      if (const auto configured =
              Game::ConfigureEncounter(setup, editor_.Zone());
          !configured) {
        editor_.Message(configured.error());
        trace_.Event("encounter.rejected", {{"reason", configured.error()}},
                     ticks_);
        rejected_ = options_.start || !scripts_.empty();
        if (!scripts_.empty()) {
          script_stop_reason_ = "encounter_rejected: " + configured.error();
          sequence_failed_ = true;
          FinishScript();
        }
      } else {
        ++encounter_number_;
        WriteJson(
            trace_.Directory() /
                ("encounter-" + std::to_string(encounter_number_) + ".json"),
            EncodeSetup(setup));
        trace_.Event("encounter.begin", EncodeSetup(setup), ticks_);
        fiber_ = std::make_unique<Platform::GameFiber>(&Game::RunEncounter);
      }
    }
  }

  void Tick() {
    if (Finished()) return;
    step_ = false;
    ++ticks_;
    watchdog_.Pulse(ticks_);
    Core::SetLogTick(ticks_);
    Render::GamePages().SetEffectPathTraceTick(ticks_);
    std::uint32_t injected = 0;
    const auto& presses = scripts_.empty() ? options_.presses
                                           : scripts_.at(script_index_).presses;
    const auto input_tick =
        scripts_.empty() ? ticks_ : ticks_ - script_start_tick_;
    if (door_trial_ && !script_stopping_) {
      injected |= door_trial_->Input(input_tick);
    }
    for (const auto& press : presses) {
      if (!script_stopping_ && input_tick >= press.tick &&
          input_tick - press.tick < 2) {
        injected |= press.buttons;
      }
    }
    input_.Inject(injected);
    const auto& pad = input_.Update();
    if (!fiber_) {
      UpdateEditor(pad);
    }
    if (fiber_) {
      Legacy::FeedPad(0, pad, true);
      const auto simulation_start = SDL_GetTicksNS();
      const bool running = fiber_->Resume();
      simulation_timing_.Record(simulation_start);
      const auto inspection_start = SDL_GetTicksNS();
      const auto state = Game::InspectEncounter();
      inspection_timing_.Record(inspection_start);
      const auto trace_start = SDL_GetTicksNS();
      const auto scene = Game::InspectGameplayScene(state);
      trace_.Frame(ticks_, pad, state, scene);
      trace_timing_.Record(trace_start);
      UpdateScript(state, scene);
      if (!running) {
        fiber_.reset();
        help_ = paused_ = false;
        trace_.Event("encounter.end", {{"number", encounter_number_}}, ticks_);
        editor_.Message("ENCOUNTER ENDED - READY TO CONFIGURE OR RESTART");
        if (!scripts_.empty()) FinishScript();
      }
    }
    if (trace_.Failed()) {
      throw std::runtime_error(
          "trace write failed; stopping without silent loss");
    }
  }

  void DrawHelp() {
    canvas_.Clear();
    constexpr int kLeft = 24, kTop = 28, kLine = 28;
    const auto lines = std::to_array<std::string>(
        {"COMBAT LAB - ITEM TEST CONTROLS",
         "F11 CLOSE HELP (RESTORES PREVIOUS PAUSE STATE)",
         "F1  HP -50, NEVER BELOW 1", "F2  MP -25, NEVER BELOW 0",
         "F3  RISK +25, NEVER ABOVE 100",
         std::format(
             "F4  SELECT CONDITION: {}",
             Game::EncounterAdjustmentName(kTestConditions[condition_])),
         "F5  APPLY SELECTED CONDITION", "F8  RESTORE MP TO CURRENT MAXIMUM",
         "F6  PAUSE / RESUME    F7  SINGLE TICK (CLOSE HELP FIRST)",
         scripts_.empty() ? "F10 RETURN TO ENCOUNTER BUILDER"
                          : "F10 CANCEL SCRIPT SEQUENCE",
         "ADJUSTMENTS REQUIRE LIVING ASHLEY IN IDLE FIELD.",
         "MENUS / ACTIONS: CLOSE OR FINISH THEM FIRST.",
         "ITEM SELECTION AND EFFECTS USE PRODUCTION GAMEPLAY.",
         adjustment_message_,
         "ALL ADJUSTMENTS AND REJECTIONS ARE LOGGED TO FILE."});
    for (std::size_t index = 0; index < lines.size(); ++index) {
      canvas_.Text(kLeft, kTop + static_cast<int>(index) * kLine, lines[index]);
    }
  }

  void Present(Render::Device& gpu) {
    const auto presentation_start = SDL_GetTicksNS();
    if (fiber_ && !help_) {
      if (Render::GamePages().Present(pixels_, width_, height_)) {
        gpu.PresentImage(pixels_.data(), width_, height_);
      } else {
        gpu.PresentClear({0.0f, 0.0f, 0.0f, 1.0f});
      }
    } else {
      if (help_) {
        DrawHelp();
      } else {
        editor_.Draw(canvas_);
      }
      width_ = Canvas::kWidth;
      height_ = Canvas::kHeight;
      pixels_.assign(
          canvas_.Pixels(),
          canvas_.Pixels() + std::ptrdiff_t{Canvas::kWidth} * Canvas::kHeight);
      gpu.PresentImage(pixels_.data(), width_, height_);
    }
    if (presentation_timing_.count == 0) {
      trace_.Event("startup.interactive_present",
                   {{"content", "editor_or_encounter"}});
    }
    presentation_timing_.Record(presentation_start);
    if (previous_presentation_ != 0) {
      frame_interval_timing_.Record(previous_presentation_);
    }
    previous_presentation_ = SDL_GetTicksNS();
  }

  bool Finished() const {
    return (rejected_ && !options_.door_transitions) || sequence_done_ ||
           (scripts_.empty() && options_.frames != 0 &&
            ticks_ >= options_.frames);
  }
};

int Run(int argc, char** argv) {
  const bool player_observations =
      std::any_of(argv + 1, argv + argc, [](const char* argument) {
        return std::string_view(argument) == "--player-observations";
      });
  Trace trace(Platform::ExecutableDir() / "combat-lab" / "runs", "combat-lab",
              {}, player_observations);
  try {
    Platform::InstallCrashHandler();
    VS_LOG_INFO("combat-lab", "log files: {}", trace.Directory().string());
    const auto options = Parse(argc, argv);
    if (options.effect_paths &&
        !Core::IsLogEnabled(Core::LogLevel::Trace, "effect-path")) {
      throw std::runtime_error(
          "effect-path tracing requires trace logging and the effect-path "
          "channel");
    }
    Render::GamePages().ConfigureEffectPathTrace(options.effect_paths);
    auto scripts = options.sequence.empty()
                       ? std::vector<EncounterScript>{}
                       : ReadScriptSequence(options.sequence);
    trace.Event("startup.stage", {{"stage", "data.begin"}});
    LoadData(options);
    trace.Event("startup.stage", {{"stage", "data.end"}});
    if (options.catalog_only) {
      return RunCatalogOnly(options, trace);
    }

    Platform::App app;
    Render::Device gpu;
    InitializeWindow(app, gpu, trace);
    if (!options.save_directory.empty()) {
      Game::Save::ConfigureDirectory(options.save_directory);
    } else {
      const auto root = Platform::UserDataDir();
      if (!root) throw std::runtime_error(root.error());
      Game::Save::ConfigureDirectory(*root / "combat-lab-saves");
    }
    Input::InputSystem input;
    input.SetDevicesEnabled(options.devices);
    Canvas canvas;
    if (!ShowLoading(app, gpu, canvas, input, trace,
                     "READING ENCOUNTER CATALOG", true)) {
      return scripts.empty() ? 0 : 2;
    }
    trace.Event("startup.stage", {{"stage", "catalog.begin"}});
    auto catalog = LoadCatalog(options, trace);
    auto retained_passed = nlohmann::json::array();
    if (options.door_transitions) {
      scripts = BuildDoorScripts(options, trace, catalog, retained_passed);
    }
    ValidateScripts(scripts, catalog);
    trace.Event("startup.stage", {{"stage", "catalog.end"}});
    if (options.door_transitions && scripts.empty()) {
      CompleteRetainedDoors(options, trace, retained_passed);
      return trace.Failed() ? 1 : 0;
    }
    if (!ShowLoading(app, gpu, canvas, input, trace,
                     "INITIALIZING AUDIO AND CONTROLLERS")) {
      return scripts.empty() ? 0 : 2;
    }
    trace.Event("startup.stage", {{"stage", "devices.begin"}});
    if (const auto devices = app.InitDevices(options.devices); !devices) {
      throw std::runtime_error(devices.error());
    }
    trace.Event("startup.stage", {{"stage", "devices.end"}});
    if (!ShowLoading(app, gpu, canvas, input, trace,
                     "RECORDING RUN CONFIGURATION")) {
      return scripts.empty() ? 0 : 2;
    }
    WriteManifest(options, trace, scripts.size());
    trace.Event("startup.stage", {{"stage", "audio.begin"}});
    Audio::AudioDevice audio(options.speed == 1);
    VS_LOG_INFO("audio", "Combat Lab native mix timing: {}",
                audio.IsRealtime() ? "device-demand" : "sequencer-driven");
    if (const auto result = audio.Open(); !result) {
      VS_LOG_WARN("audio", "{}", result.error());
    }
    trace.Event("startup.stage", {{"stage", "audio.end"}});
    LiveSession session(options, trace, std::move(catalog), std::move(scripts),
                        std::move(canvas), input, std::move(retained_passed));
    struct FileMenuScope {
      ~FileMenuScope() { Game::Save::ShutdownFileMenus(); }
    } file_menu_scope;
    trace.Event("startup.stage", {{"stage", "session.ready"}});
    return session.Run(app, gpu);
  } catch (const std::exception& error) {
    trace.Event("fatal", {{"message", error.what()}});
    VS_LOG_ERROR("combat-lab", "{}", error.what());
    throw;
  }
}
}  // namespace
}  // namespace VS::combat_lab

int main(int argc, char** argv) try {
  return VS::combat_lab::Run(argc, argv);
} catch (const std::exception& error) {
  VS_LOG_ERROR("combat-lab", "{}", error.what());
  const bool noninteractive =
      std::any_of(argv + 1, argv + argc, [](std::string_view argument) {
        return argument == "--sequence" || argument == "--scripted" ||
               argument == "--catalog-only" || argument == "--door-transitions";
      });
  if (!noninteractive) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Combat Lab", error.what(),
                             nullptr);
  }
  return 1;
}
