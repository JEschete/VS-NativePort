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
#include "Input/Pad.hpp"
#include "Movie/MoviePlayer.h"
#include "Platform/App.hpp"
#include "Platform/CrashHandler.hpp"
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

bool ConfigureLogging(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    const std::string_view option = argv[i];
    if (option != "--log-level" && option != "--log-channels") {
      continue;
    }
    if (i + 1 == argc || std::string_view(argv[i + 1]).starts_with("--")) {
      std::fprintf(stderr, "%s requires a value\n", argv[i]);
      return false;
    }
    const std::string_view value = argv[++i];
    if (option == "--log-level") {
      const auto level = Core::ParseLogLevel(value);
      if (!level) {
        std::fprintf(
            stderr,
            "--log-level expects trace, debug, info, warn, error, or off\n");
        return false;
      }
      Core::SetLogLevel(*level);
    } else if (!Core::SetLogChannels(value)) {
      std::fprintf(
          stderr,
          "--log-channels expects comma-separated names (no spaces), or *\n");
      return false;
    }
  }
  if (!Core::SetLogFile(
          (Platform::ExecutableDir() / "vs-native.log").string().c_str())) {
    std::fprintf(stderr, "cannot open vs-native.log\n");
    return false;
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
    return fs::path(*opt);
  }
  if (const char* env = SDL_getenv("VS_DISC")) {
    return fs::path(env);
  }
  if (auto p = FindUp(fs::path("disks") / "SLUS-01040.cue")) {
    return p;
  }
  return FindUp("SLUS-01040");
}

// --data <dir>, else Native\data found walking up from the executable.
std::optional<fs::path> LocateData(int argc, char** argv) {
  if (auto opt = Option(argc, argv, "--data")) {
    return fs::path(*opt);
  }
  return FindUp(fs::path("Native") / "data" / Content::DataStore::ManifestName)
      .transform([](const fs::path& p) { return p.parent_path(); });
}

// --capture <dir> --at <tick,tick,...>: writes the presented frame at those
// ticks (for reference comparisons).
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
  bool pages = false;

  void Write(u64 tick, const std::vector<u32>& rgba, i32 w, i32 h) const {
    if (dir.empty() || !std::ranges::binary_search(ticks, tick)) {
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
      const auto tick = ParseTick(item);
      if (!tick) throw std::runtime_error("--at contains an invalid tick");
      c.ticks.push_back(*tick);
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
  settings.devices =
      std::none_of(argv + 1, argv + argc, [](const char* argument) {
        return std::string_view(argument) == "--scripted";
      });
  return settings;
}

int RunGame(const Content::Disc* disc, const RunSettings& settings) {
  VS_LOG_INFO("boot", "run: tick limit={}, speed={}, scripted presses={}",
              settings.max_ticks, settings.speed,
              settings.script.presses.size());

  Platform::App app;
  if (auto r = app.Init({}); !r) {
    VS_LOG_ERROR("boot", "{}", r.error());
    return 1;
  }
  Render::Device gpu;
  if (auto r = gpu.Init(app.Window(), true); !r) {
    VS_LOG_ERROR("boot", "{}", r.error());
    return 1;
  }
  Audio::AudioDevice audio(settings.speed == 1);
  if (auto r = audio.Open(); !r) {
    VS_LOG_WARN("boot", "{} (continuing without audio)", r.error());
  }

  Input::InputSystem input;
  if (!settings.devices) {
    input.SetDevicesEnabled(false);
    VS_LOG_INFO("input", "--scripted: keyboard and gamepad ignored");
  }
  Game::GameApp game({disc});
  Platform::ProgressWatchdog watchdog(settings.max_ticks != 0);
  Core::FrameClock clock;
  double last = app.NowSeconds();
  std::vector<u32> frame;
  i32 frame_w = 0, frame_h = 0;

  const auto handle_event = [&input](const SDL_Event& event) {
    input.HandleEvent(event);
  };
  while (app.PumpEvents(handle_event)) {
    const double now = app.NowSeconds();
    const u32 ticks = clock.Advance(now - last) * settings.speed;
    last = now;
    bool shown = false;
    for (u32 index = 0; index < ticks && (settings.max_ticks == 0 ||
                                          game.Ticks() < settings.max_ticks);
         ++index) {
      watchdog.Pulse(game.Ticks() + 1);
      Core::SetLogTick(game.Ticks() + 1);
      VS_LOG_TRACE("frame", "begin tick");
      input.Inject(settings.script.MaskAt(game.Ticks() + 1));
      const auto& pad = input.Update();
      if (pad.pressed) {
        VS_LOG_INFO("input", "pressed {}", Input::DescribeButtons(pad.pressed));
      }
      game.Tick(pad, true);
      shown = Render::GamePages().Present(frame, frame_w, frame_h);
      VS_LOG_TRACE("frame", "end tick; presented={}, image={}x{}", shown,
                   frame_w, frame_h);
      if (shown) {
        settings.capture.Write(game.Ticks(), frame, frame_w, frame_h);
      }
    }
    if (ticks == 0) {
      shown = Render::GamePages().Present(frame, frame_w, frame_h);
    }
    if (shown) {
      gpu.PresentImage(frame.data(), frame_w, frame_h);
    } else {
      gpu.PresentClear({0.0f, 0.0f, 0.0f, 1.0f});
    }
    if (settings.max_ticks != 0 && game.Ticks() >= settings.max_ticks) {
      VS_LOG_INFO("boot", "stopping after {} ticks (--frames)", game.Ticks());
      break;
    }
  }
  Movie_PlayerClose();  // its soundtrack source must go before the audio device
                        // and SDL
  return 0;
}

}  // namespace

int main(int argc, char** argv) try {
  if (!ConfigureLogging(argc, argv)) return 1;
  Platform::InstallCrashHandler();
  std::unique_ptr<Content::Disc> disc;
  if (auto disc_path = LocateDisc(argc, argv)) {
    if (auto opened = Content::Disc::Open(*disc_path)) {
      disc = std::move(*opened);
      VS_LOG_INFO("boot", "disc '{}' ({} entries) from {}", disc->Label(),
                  disc->Files().size(), disc_path->string());
    } else {
      VS_LOG_WARN("boot", "{}", opened.error());
    }
  }
  const auto data_root = LocateData(argc, argv);
  if (!data_root) {
    VS_LOG_ERROR("boot",
                 "no extracted data; run VS.Extract extract <disc> "
                 "Native\\data, or pass --data <dir>");
    return 1;
  }
  if (auto result = Content::DataStore::Open(*data_root); !result) {
    VS_LOG_ERROR("boot", "{}", result.error());
    return 1;
  }
  return RunGame(disc.get(), ParseRunSettings(argc, argv));
} catch (const std::exception& error) {
  VS_LOG_ERROR("boot", "{}", error.what());
  return 1;
}
