#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <charconv>
#include <filesystem>
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

namespace VS::combat_lab {
namespace {
namespace fs = std::filesystem;

struct Press {
  std::uint64_t tick;
  std::uint32_t buttons;
};

struct Options {
  fs::path data;
  fs::path scenario;
  fs::path capture;
  std::uint64_t frames = 0;
  int speed = 1;
  bool start = false;
  bool catalog_only = false;
  bool devices = true;
  std::vector<Press> presses;
};

std::uint64_t Number(std::string_view value) {
  std::uint64_t result = 0;
  const auto parsed =
      std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size()) {
    throw std::runtime_error("invalid nonnegative integer option");
  }
  return result;
}

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

Options Parse(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "--start") {
      options.start = true;
      continue;
    }
    if (argument == "--catalog-only") {
      options.catalog_only = true;
      continue;
    }
    if (argument == "--scripted") {
      options.devices = false;
      continue;
    }
    if (++index == argc) throw std::runtime_error("option is missing a value");
    const std::string_view value = argv[index];
    if (argument == "--data") {
      options.data = value;
    } else if (argument == "--scenario") {
      options.scenario = value;
    } else if (argument == "--capture") {
      options.capture = value;
    } else if (argument == "--frames") {
      options.frames = Number(value);
    } else if (argument == "--speed") {
      const auto speed = Number(value);
      constexpr std::uint64_t kMaximumSpeed = 64;
      if (speed < 1 || speed > kMaximumSpeed) {
        throw std::runtime_error("speed must be 1-64");
      }
      options.speed = static_cast<int>(speed);
    } else if (argument == "--press") {
      auto remaining = value;
      while (!remaining.empty()) {
        const auto comma = remaining.find(',');
        const auto item = remaining.substr(0, comma);
        const auto colon = item.find(':');
        if (colon == std::string_view::npos) {
          throw std::runtime_error("press expects tick:Button entries");
        }
        const auto mask = Input::ParseButtons(item.substr(colon + 1));
        if (mask == 0) throw std::runtime_error("unknown scripted button");
        options.presses.push_back({Number(item.substr(0, colon)), mask});
        if (comma == std::string_view::npos) break;
        remaining.remove_prefix(comma + 1);
      }
    } else {
      throw std::runtime_error("unknown option: " + std::string(argument));
    }
  }
  if (options.data.empty()) options.data = FindData();
  return options;
}

std::string GitOutput(const char* command, const char* argument) {
  const auto directory = Platform::ExecutableDir().string();
  const char* argv[] = {"git",   "-C",     directory.c_str(),
                        command, argument, nullptr};
  const std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process(
      SDL_CreateProcess(argv, true), SDL_DestroyProcess);
  if (!process) return "unavailable";
  std::size_t size = 0;
  int exit_code = 0;
  const std::unique_ptr<void, decltype(&SDL_free)> output(
      SDL_ReadProcess(process.get(), &size, &exit_code), SDL_free);
  if (!output || exit_code != 0) return "unavailable";
  return std::string(static_cast<const char*>(output.get()), size);
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

std::vector<Game::EncounterZone> LoadCatalog(const Options& options,
                                             Trace& trace) {
  if (const auto opened = Content::DataStore::Open(options.data); !opened) {
    throw std::runtime_error(opened.error());
  }
  if (Content_LoadModuleData() != 0) {
    throw std::runtime_error("required production module data failed to load");
  }
  auto catalog = Game::ReadEncounterCatalog();
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

class LiveSession {
 public:
  LiveSession(const Options& options, Trace& trace,
              std::vector<Game::EncounterZone> catalog)
      : options_(options),
        trace_(trace),
        canvas_(),
        editor_(std::move(catalog)),
        input_(),
        watchdog_(options.frames != 0),
        fiber_(),
        ticks_(0),
        encounter_number_(0),
        start_(options.start),
        confirm_(false),
        paused_(false),
        step_(false),
        rejected_(false) {
    input_.SetDevicesEnabled(options_.devices);
    if (!options_.scenario.empty()) {
      std::ifstream file(options_.scenario);
      if (!file) throw std::runtime_error("cannot open encounter preset");
      editor_.Load(DecodeSetup(nlohmann::json::parse(file)));
    }
  }

  int Run(Platform::App& app, Render::Device& gpu) {
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
    Capture(options_.capture, pixels_, width_, height_);
    trace_.Event(
        "host.exit",
        {{"ticks", ticks_}, {"encounter_interrupted", fiber_ != nullptr}},
        ticks_);
    fiber_.reset();
    Movie_PlayerClose();
    return rejected_ ? 2 : 0;
  }

 private:
  const Options& options_;
  Trace& trace_;
  Canvas canvas_;
  Editor editor_;
  Input::InputSystem input_;
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
  double previous_ = 0;

  void Handle(const SDL_Event& event) {
    input_.HandleEvent(event);
    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) return;
    if (!fiber_ && event.key.key == SDLK_RETURN) confirm_ = true;
    if (fiber_ && event.key.key == SDLK_F10) {
      trace_.Event("control.stop", {}, ticks_);
      Game::RequestEncounterStop();
      paused_ = false;
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

  void UpdateEditor(const Input::PadState& pad) {
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
        rejected_ = options_.start;
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
    std::uint32_t injected = 0;
    for (const auto& press : options_.presses) {
      if (ticks_ >= press.tick && ticks_ - press.tick < 2) {
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
      const bool running = fiber_->Resume();
      const auto state = Game::InspectEncounter();
      trace_.Frame(ticks_, pad, state);
      if (!running) {
        fiber_.reset();
        trace_.Event("encounter.end", {{"number", encounter_number_}}, ticks_);
        editor_.Message("ENCOUNTER ENDED - READY TO CONFIGURE OR RESTART");
      }
    }
    if (trace_.Failed()) {
      throw std::runtime_error(
          "trace write failed; stopping without silent loss");
    }
  }

  void Present(Render::Device& gpu) {
    if (fiber_) {
      if (Render::GamePages().Present(pixels_, width_, height_)) {
        gpu.PresentImage(pixels_.data(), width_, height_);
      } else {
        gpu.PresentClear({0.0f, 0.0f, 0.0f, 1.0f});
      }
    } else {
      editor_.Draw(canvas_);
      width_ = Canvas::kWidth;
      height_ = Canvas::kHeight;
      pixels_.assign(
          canvas_.Pixels(),
          canvas_.Pixels() + std::ptrdiff_t{Canvas::kWidth} * Canvas::kHeight);
      gpu.PresentImage(pixels_.data(), width_, height_);
    }
  }

  bool Finished() const {
    return rejected_ || (options_.frames != 0 && ticks_ >= options_.frames);
  }
};

int Run(int argc, char** argv) {
  Trace trace(Platform::ExecutableDir() / "combat-lab" / "runs");
  try {
    Platform::InstallCrashHandler();
    VS_LOG_INFO("combat-lab", "log files: {}", trace.Directory().string());
    const auto options = Parse(argc, argv);
    auto catalog = LoadCatalog(options, trace);
    if (options.catalog_only) return trace.Failed() ? 1 : 0;

    Platform::App app;
    Platform::WindowDesc window;
    window.title = "Vagrant Story - Combat Lab";
    if (const auto result = app.Init(window); !result) {
      throw std::runtime_error(result.error());
    }
    WriteJson(trace.Directory() / "manifest.json",
              {{"schema", 1},
               {"revision", GitOutput("rev-parse", "HEAD")},
               {"worktree", GitOutput("status", "--porcelain")},
               {"data_root", fs::absolute(options.data).string()},
               {"configuration", "standalone isolated encounter"},
               {"frames", options.frames},
               {"speed", options.speed},
               {"devices", options.devices}});
    Render::Device gpu;
    if (const auto result = gpu.Init(app.Window(), true); !result) {
      throw std::runtime_error(result.error());
    }
    Audio::AudioDevice audio;
    if (const auto result = audio.Open(); !result) {
      VS_LOG_WARN("audio", "{}", result.error());
    }
    LiveSession session(options, trace, std::move(catalog));
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
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Combat Lab", error.what(),
                           nullptr);
  return 1;
}
