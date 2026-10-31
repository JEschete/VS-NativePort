#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <charconv>
#include <climits>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Audio/AudioDevice.hpp"
#include "Boot/GameApp.hpp"
#include "Content/DataStore.hpp"
#include "Content/Disc.hpp"
#include "Core/FrameClock.hpp"
#include "Core/Log.hpp"
#include "Diagnostics/GameplayTrace.hpp"
#include "Input/Pad.hpp"
#include "Movie/MoviePlayer.h"
#include "Platform/App.hpp"
#include "Platform/CrashHandler.hpp"
#include "Platform/Paths.hpp"
#include "Render/Device.hpp"
#include "Render/PageSpace.hpp"

namespace fs = std::filesystem;
using VS::i32;
using VS::u32;
using VS::u64;
using VS::usize;
namespace Audio = VS::Audio;
namespace Content = VS::Content;
namespace Core = VS::Core;
namespace Game = VS::Game;
namespace Input = VS::Input;
namespace Platform = VS::Platform;
namespace Render = VS::Render;

namespace {

#ifdef VS_RELEASE_BUILD
constexpr bool kEnableDetailedGameLogging = false;
#else
constexpr bool kEnableDetailedGameLogging = true;
#endif

bool ConfigureLogging(int argc, char** argv, Game::GameplayTrace* trace) {
  const auto reject = [trace](const std::string& message) {
    std::fprintf(stderr, "%s\n", message.c_str());
    if (trace != nullptr) {
      trace->Event("startup.error", {{"message", message}});
    }
    return false;
  };
  for (int i = 1; i < argc; ++i) {
    const std::string_view option = argv[i];
    if (option != "--log-level" && option != "--log-channels") {
      continue;
    }
    if (i + 1 == argc || std::string_view(argv[i + 1]).starts_with("--")) {
      return reject(std::format("{} requires a value", argv[i]));
    }
    const std::string_view value = argv[++i];
    if (option == "--log-level") {
      const auto level = Core::ParseLogLevel(value);
      if (!level) {
        return reject(
            "--log-level expects trace, debug, info, warn, error, or off");
      }
      Core::SetLogLevel(*level);
    } else if (!Core::SetLogChannels(value)) {
      return reject(
          "--log-channels expects comma-separated names (no spaces), or *");
    }
  }
  if constexpr (!kEnableDetailedGameLogging) {
    Core::SetLogLevel(Core::LogLevel::Off);
    Core::SetConsoleLogLevel(Core::LogLevel::Off);
    return true;
  }
  if (trace == nullptr &&
      !Core::SetLogFile(
          (Platform::ExecutableDir() / "vs-native.log").string().c_str())) {
    return reject("cannot open vs-native.log");
  }
  return true;
}

std::optional<std::string> Option(int argc, char** argv,
                                  std::string_view name) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == name) {
      return std::string(argv[i + 1]);
    }
  }
  return std::nullopt;
}

// Walks up from the executable for `relative` (so a dev build finds the
// repository copy).
std::optional<fs::path> FindUp(const fs::path& relative) {
  std::error_code ec;
  for (fs::path dir = Platform::ExecutableDir(); !dir.empty();
       dir = dir.parent_path()) {
    if (fs::exists(dir / relative, ec)) {
      return dir / relative;
    }
    if (dir == dir.parent_path()) {
      break;
    }
  }
  return std::nullopt;
}

// --disc <path>, else the first of: VS_DISC env var, disks\SLUS-01040.cue or
// SLUS-01040\ found walking up from the executable (so a dev build finds the
// repo copy).
std::optional<fs::path> LocateDisc(int argc, char** argv) {
  if (auto opt = Option(argc, argv, "--disc")) {
    return Platform::Utf8Path(*opt);
  }
  if (const char* env = SDL_getenv("VS_DISC")) {
    return Platform::Utf8Path(env);
  }
  if (auto p = FindUp(fs::path("disks") / "SLUS-01040.cue")) {
    return p;
  }
  return FindUp("SLUS-01040");
}

// --data <dir>, else Native\data found walking up from the executable.
std::optional<fs::path> LocateData(int argc, char** argv) {
  if (auto opt = Option(argc, argv, "--data")) {
    return Platform::Utf8Path(*opt);
  }
  return FindUp(fs::path("Native") / "data" / Content::DataStore::ManifestName)
      .transform([](const fs::path& p) { return p.parent_path(); });
}

// --capture <dir> --at <tick,first:last,...>: writes the presented frame at
// selected ticks (for reference comparisons).
std::optional<u64> ParseTick(std::string_view text) {
  u64 value = 0;
  const char* end = text.data() + text.size();
  // from_chars is bounded by end and never expects a null terminator.
  // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
  const auto result = std::from_chars(text.data(), end, value);
  if (result.ec != std::errc() || result.ptr != end) return std::nullopt;
  return value;
}

struct Capture {
  fs::path dir;
  std::vector<u64> ticks;
  std::vector<Render::EffectPathTraceRange> ranges;
  bool pages = false;

  bool Selects(u64 tick) const {
    return std::ranges::binary_search(ticks, tick) ||
           std::ranges::any_of(ranges, [tick](const auto& range) {
             return tick >= range.first && tick <= range.last;
           });
  }

  void WriteClear(u64 tick) const {
    if (dir.empty() || !Selects(tick)) return;
    constexpr i32 kClearWidth = 320;
    constexpr i32 kClearHeight = 240;
    constexpr u32 kOpaqueBlack = 0xFF000000u;
    const std::vector<u32> black(static_cast<usize>(kClearWidth) * kClearHeight,
                                 kOpaqueBlack);
    Write(tick, black, kClearWidth, kClearHeight);
  }

  void Write(u64 tick, const std::vector<u32>& rgba, i32 w, i32 h) const {
    if (dir.empty() || !Selects(tick)) {
      return;
    }
    fs::create_directories(dir);
    const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
        SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888,
                              const_cast<u32*>(rgba.data()),
                              w * static_cast<i32>(sizeof(u32))),
        SDL_DestroySurface);
    const fs::path file = dir / ("frame_" + std::to_string(tick) + ".bmp");
    if (!surface || !SDL_SaveBMP(surface.get(), file.string().c_str())) {
      VS_LOG_ERROR("capture", "{}: {}", file.string(), SDL_GetError());
    } else {
      VS_LOG_INFO("capture", "{} ({}x{})", file.string(), w, h);
    }
    if (pages) {
      constexpr u32 kChannelBits = 5;
      constexpr u32 kChannelMaximum = (1u << kChannelBits) - 1;
      constexpr u32 kByteMaximum = (1u << CHAR_BIT) - 1;
      constexpr u32 kOpaqueAlpha = 0xFF000000u;
      const auto source = Render::GamePages().Pixels();
      std::ofstream raw(dir / ("pages_" + std::to_string(tick) + ".bin"),
                        std::ios::binary);
      raw.write(reinterpret_cast<const char*>(source.data()),
                static_cast<std::streamsize>(source.size_bytes()));
      if (!raw) {
        VS_LOG_ERROR("capture", "Failed to write raw pages at tick {}", tick);
      }
      std::vector<u32> colors(source.size());
      for (usize index = 0; index < source.size(); ++index) {
        const u32 pixel = source[index];
        const u32 red =
            (pixel & kChannelMaximum) * kByteMaximum / kChannelMaximum;
        const u32 green = ((pixel >> kChannelBits) & kChannelMaximum) *
                          kByteMaximum / kChannelMaximum;
        const u32 blue = ((pixel >> (2 * kChannelBits)) & kChannelMaximum) *
                         kByteMaximum / kChannelMaximum;
        colors[index] =
            kOpaqueAlpha | red | (green << CHAR_BIT) | (blue << (2 * CHAR_BIT));
      }
      const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>
          page_surface(SDL_CreateSurfaceFrom(
                           Render::PageSpace::Width, Render::PageSpace::Height,
                           SDL_PIXELFORMAT_ABGR8888, colors.data(),
                           Render::PageSpace::Width * sizeof(u32)),
                       SDL_DestroySurface);
      const fs::path output = dir / ("pages_" + std::to_string(tick) + ".bmp");
      if (!page_surface ||
          !SDL_SaveBMP(page_surface.get(), output.string().c_str())) {
        VS_LOG_ERROR("capture", "{}: {}", output.string(), SDL_GetError());
      }
    }
  }
};

Capture ParseCapture(int argc, char** argv) {
  Capture c;
  c.pages = std::any_of(argv + 1, argv + argc, [](const char* argument) {
    return std::string_view(argument) == "--capture-pages";
  });
  if (auto dir = Option(argc, argv, "--capture")) {
    c.dir = *dir;
    const std::string at = Option(argc, argv, "--at").value_or("");
    std::string_view list = at;
    while (!list.empty()) {
      const usize comma = list.find(',');
      const std::string_view item = list.substr(0, comma);
      if (item.find(':') != std::string_view::npos) {
        const auto range = Render::EffectPathTraceRange::Parse(item);
        if (!range) {
          throw std::runtime_error("--at: " + range.error());
        }
        c.ranges.push_back(*range);
      } else {
        const auto tick = ParseTick(item);
        if (!tick) throw std::runtime_error("--at contains an invalid tick");
        c.ticks.push_back(*tick);
      }
      list = comma == std::string_view::npos ? std::string_view{}
                                             : list.substr(comma + 1);
    }
  }
  std::ranges::sort(c.ticks);
  return c;
}

// --press <tick:Buttons,...> (e.g. "3300:Right,3400:Cross"): scripted presses,
// each held for a few ticks, so a capture run can drive the menus.
struct InputScript {
  static constexpr u64 kHoldTicks = 3;
  std::vector<std::pair<u64, u32>> presses;

  u32 MaskAt(u64 tick) const {
    u32 mask = 0;
    for (const auto& [at, buttons] : presses) {
      if (tick >= at && tick - at < kHoldTicks) {
        mask |= buttons;
      }
    }
    return mask;
  }
};

InputScript ParseInputScript(int argc, char** argv) {
  InputScript s;
  const std::string text = Option(argc, argv, "--press").value_or("");
  std::string_view list = text;
  while (!list.empty()) {
    const usize comma = list.find(',');
    const std::string_view item = list.substr(0, comma);
    const usize colon = item.find(':');
    const auto tick = ParseTick(item.substr(0, colon));
    if (colon == std::string_view::npos || !tick) {
      throw std::runtime_error("--press expects tick:Buttons entries");
    }
    s.presses.emplace_back(*tick, Input::ParseButtons(item.substr(colon + 1)));
    list = comma == std::string_view::npos ? std::string_view{}
                                           : list.substr(comma + 1);
  }
  return s;
}

struct RunSettings {
  u64 max_ticks = 0;
  u32 speed = 1;
  bool devices = true;
  Capture capture;
  InputScript script;
  std::optional<fs::path> save_directory;
  std::optional<Render::EffectPathTraceRange> effect_paths;
};

RunSettings ParseRunSettings(int argc, char** argv) {
  RunSettings settings;
  if (auto s = Option(argc, argv, "--speed")) {
    std::from_chars(s->data(), s->data() + s->size(), settings.speed);
    settings.speed = std::max(settings.speed, 1u);
  }
  if (auto frames = Option(argc, argv, "--frames")) {
    std::from_chars(frames->data(), frames->data() + frames->size(),
                    settings.max_ticks);
  }
  settings.capture = ParseCapture(argc, argv);
  settings.script = ParseInputScript(argc, argv);
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--trace-effect-paths") {
      if (i + 1 == argc) {
        throw std::runtime_error("--trace-effect-paths requires first:last");
      }
      const auto range = Render::EffectPathTraceRange::Parse(argv[++i]);
      if (!range) throw std::runtime_error(range.error());
      settings.effect_paths = *range;
      continue;
    }
    if (std::string_view(argv[i]) != "--save-dir") continue;
    if (i + 1 == argc || std::string_view(argv[i + 1]).empty() ||
        std::string_view(argv[i + 1]).starts_with("--")) {
      throw std::runtime_error("--save-dir requires a directory");
    }
    const std::string_view path = argv[++i];
    settings.save_directory = fs::path(std::u8string(path.begin(), path.end()));
  }
  settings.devices =
      std::none_of(argv + 1, argv + argc, [](const char* argument) {
        return std::string_view(argument) == "--scripted";
      });
  if (settings.effect_paths &&
      ParseTick(Option(argc, argv, "--speed").value_or("1")) != 1) {
    throw std::runtime_error("--trace-effect-paths requires normal --speed 1");
  }
  return settings;
}

void ConfigureEffectPathTrace(const RunSettings& settings,
                              Game::GameplayTrace* trace) {
  if (settings.effect_paths &&
      !Core::IsLogEnabled(Core::LogLevel::Trace, "effect-path")) {
    throw std::runtime_error(
        "effect-path tracing requires trace logging and the effect-path "
        "channel");
  }
  Render::GamePages().ConfigureEffectPathTrace(settings.effect_paths);
  if (settings.effect_paths && trace != nullptr) {
    trace->Event("effect.paths.settings",
                 {{"first", settings.effect_paths->first},
                  {"last", settings.effect_paths->last},
                  {"speed", 1}});
  }
}

void BeginLogicalTick(u64 tick) {
  Core::SetLogTick(tick);
  Render::GamePages().SetEffectPathTraceTick(tick);
  VS_LOG_TRACE("frame", "begin tick");
}

void ReportStartupFailure(const char* title, const std::string& message,
                          SDL_Window* window) {
  VS_LOG_ERROR("boot", "{}", message);
  if constexpr (!kEnableDetailedGameLogging) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message.c_str(),
                             window);
  }
}

struct PresentedFrame {
  std::vector<u32> pixels;
  i32 width = 0;
  i32 height = 0;
};

// Advances one logical tick; returns whether it produced an image.
bool RunLogicalTick(Game::GameApp& game, Input::InputSystem& input,
                    Platform::ProgressWatchdog& watchdog,
                    const RunSettings& settings, Game::GameplayTrace* trace,
                    PresentedFrame& frame) {
  watchdog.Pulse(game.Ticks() + 1);
  BeginLogicalTick(game.Ticks() + 1);
  input.Inject(settings.script.MaskAt(game.Ticks() + 1));
  const auto& pad = input.Update();
  if (pad.pressed) {
    VS_LOG_INFO("input", "pressed {}", Input::DescribeButtons(pad.pressed));
  }
  game.Tick(pad, true);
  if (trace != nullptr) {
    const auto state = Game::InspectGameplay();
    trace->Frame(game.Ticks(), pad, state, Game::InspectGameplayScene(state));
    if (trace->Failed()) {
      throw std::runtime_error("detailed gameplay trace failed");
    }
  }
  const bool shown =
      Render::GamePages().Present(frame.pixels, frame.width, frame.height);
  VS_LOG_TRACE("frame", "end tick; presented={}, image={}x{}", shown,
               frame.width, frame.height);
  if (shown) {
    settings.capture.Write(game.Ticks(), frame.pixels, frame.width,
                           frame.height);
  } else {
    settings.capture.WriteClear(game.Ticks());
  }
  return shown;
}

int RunGame(const Content::Disc* disc, const RunSettings& settings,
            Game::GameplayTrace* trace) {
  ConfigureEffectPathTrace(settings, trace);
  VS_LOG_INFO("boot", "run: tick limit={}, speed={}, scripted presses={}",
              settings.max_ticks, settings.speed,
              settings.script.presses.size());

  Platform::App app;
  if (auto r = app.Init({}); !r) {
    ReportStartupFailure("Window initialization failed", r.error(), nullptr);
    return 1;
  }
  Render::Device gpu;
  if (auto r = gpu.Init(app.Window(), true); !r) {
    ReportStartupFailure("Graphics initialization failed", r.error(),
                         app.Window());
    return 1;
  }
  Audio::AudioDevice audio(settings.speed == 1);
  if (auto r = audio.Open(); !r) {
    VS_LOG_WARN("boot", "{} (continuing without audio)", r.error());
  }
  struct MovieScope {
    ~MovieScope() { Movie_PlayerClose(); }
  } movie_scope;

  Input::InputSystem input;
  if (!settings.devices) {
    input.SetDevicesEnabled(false);
    VS_LOG_INFO("input", "--scripted: keyboard and gamepad ignored");
  }
  Game::GameApp game({disc, settings.save_directory});
  Platform::ProgressWatchdog watchdog(settings.max_ticks != 0);
  Core::FrameClock clock;
  double last = app.NowSeconds();
  PresentedFrame frame;

  const auto handle_event = [&input](const SDL_Event& event) {
    input.HandleEvent(event);
  };
  bool running = true;
  while (running && app.PumpEvents(handle_event)) {
    const double now = app.NowSeconds();
    const u32 ticks = clock.Advance(now - last) * settings.speed;
    last = now;
    bool shown = false;
    for (u32 index = 0; index < ticks && (settings.max_ticks == 0 ||
                                          game.Ticks() < settings.max_ticks);
         ++index) {
      // The PS1 samples the pad every vertical blank. Catch-up and accelerated
      // batches pump events before each tick so a short press or release is
      // sampled on the next tick rather than at the end of the whole batch.
      if (index != 0 && !app.PumpEvents(handle_event)) {
        running = false;
        break;
      }
      shown = RunLogicalTick(game, input, watchdog, settings, trace, frame);
    }
    if (ticks == 0) {
      shown =
          Render::GamePages().Present(frame.pixels, frame.width, frame.height);
    }
    if (shown) {
      gpu.PresentImage(frame.pixels.data(), frame.width, frame.height);
    } else {
      gpu.PresentClear({0.0f, 0.0f, 0.0f, 1.0f});
    }
    if (settings.max_ticks != 0 && game.Ticks() >= settings.max_ticks) {
      VS_LOG_INFO("boot", "stopping after {} ticks (--frames)", game.Ticks());
      break;
    }
  }
  if (trace != nullptr) {
    trace->Event("host.exit",
                 {{"ticks", game.Ticks()}, {"game_finished", game.Finished()}},
                 game.Ticks());
  }
  return 0;
}

int RunDesktop(int argc, char** argv, Game::GameplayTrace* trace) {
  Platform::InstallCrashHandler();
  std::unique_ptr<Content::Disc> disc;
  if (auto disc_path = LocateDisc(argc, argv)) {
    if (auto opened = Content::Disc::Open(*disc_path)) {
      disc = std::move(*opened);
      VS_LOG_INFO("boot", "disc '{}' ({} entries) from {}", disc->Label(),
                  disc->Files().size(), Platform::PathText(*disc_path));
    } else {
      VS_LOG_WARN("boot", "{}", opened.error());
    }
  }
  const auto data_root = LocateData(argc, argv);
  if (!data_root) {
    VS_LOG_ERROR("boot",
                 "no extracted data; run VS.Extract extract <disc> "
                 "Native\\data, or pass --data <dir>");
    if constexpr (!kEnableDetailedGameLogging) {
      SDL_ShowSimpleMessageBox(
          SDL_MESSAGEBOX_ERROR, "Game data not prepared",
          "Open VS.Launcher.exe and press Play to prepare your own game disc.",
          nullptr);
    }
    return 1;
  }
  if (auto result = Content::DataStore::Open(*data_root); !result) {
    VS_LOG_ERROR("boot", "{}", result.error());
    if constexpr (!kEnableDetailedGameLogging) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game data unavailable",
                               result.error().c_str(), nullptr);
    }
    return 1;
  }
  if (trace != nullptr) {
    trace->Event("content.data",
                 {{"data_root", fs::absolute(*data_root).string()}});
  }
  return RunGame(disc.get(), ParseRunSettings(argc, argv), trace);
}

}  // namespace

int main(int argc, char** argv) try {
  if constexpr (!kEnableDetailedGameLogging) {
    Core::SetLogLevel(Core::LogLevel::Off);
    Core::SetConsoleLogLevel(Core::LogLevel::Off);
  }
  std::unique_ptr<Game::GameplayTrace> trace;
  const bool player_observations =
      std::any_of(argv + 1, argv + argc, [](const char* argument) {
        return std::string_view(argument) == "--player-observations";
      });
  const bool verification_log =
      std::any_of(argv + 1, argv + argc, [](const char* argument) {
        return std::string_view(argument) == "--verification-log";
      });
  if ((player_observations || verification_log) &&
      !kEnableDetailedGameLogging) {
    throw std::runtime_error(
        "player observations/verification log require detailed logging");
  }
  if (kEnableDetailedGameLogging) {
    const auto directory = Platform::ExecutableDir();
    trace = std::make_unique<Game::GameplayTrace>(
        directory / "game-logs", "desktop", directory / "vs-native.log",
        player_observations, verification_log);
    const auto source = Game::TraceSourceProvenance(directory);
    Game::WriteTraceManifest(
        trace->Directory(),
        {{"schema", 1},
         {"host", "desktop"},
         {"detailed_logging", true},
         {"verification_log", verification_log},
         {"build", __DATE__ " " __TIME__},
         {"revision", source.at("revision")},
         {"worktree", source.at("worktree")},
         {"source_status", source.at("status")},
         {"arguments", std::vector<std::string>(argv, argv + argc)},
         {"compatibility_log", (directory / "vs-native.log").string()}});
  }
  int result = 1;
  try {
    if (ConfigureLogging(argc, argv, trace.get())) {
      if (trace != nullptr) {
        VS_LOG_INFO("diagnostics", "detailed logs: {}",
                    trace->Directory().string());
        trace->Event("host.start", {{"detailed_logging", true}});
      }
      result = RunDesktop(argc, argv, trace.get());
    }
  } catch (const std::exception& error) {
    VS_LOG_ERROR("boot", "{}", error.what());
    if constexpr (!kEnableDetailedGameLogging) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game startup failed",
                               error.what(), nullptr);
    }
    if (trace != nullptr) trace->Event("fatal", {{"message", error.what()}});
  }
  if (trace != nullptr) {
    trace->Event("host.result",
                 {{"exit_code", result}, {"engine_errors", trace->Errors()}});
    if (trace->Failed()) return 1;
  }
  return result;
} catch (const std::exception& error) {
  std::fprintf(stderr, "Cannot initialize game diagnostics: %s\n",
               error.what());
  return 1;
}
