#include "Audio/AudioDevice.hpp"
#include "Content/DataStore.hpp"
#include "Content/Disc.hpp"
#include "Core/FrameClock.hpp"
#include "Core/Log.hpp"
#include "Boot/GameApp.hpp"
#include "Input/Pad.hpp"
#include "Movie/MoviePlayer.h"
#include "Platform/App.hpp"
#include "Platform/CrashHandler.hpp"
#include "Render/Device.hpp"
#include "Render/PageSpace.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace VS;

namespace {

bool ConfigureLogging(int argc, char** argv)
{
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
                std::fprintf(stderr, "--log-level expects trace, debug, info, warn, error, or off\n");
                return false;
            }
            Core::SetLogLevel(*level);
        } else if (!Core::SetLogChannels(value)) {
            std::fprintf(stderr, "--log-channels expects comma-separated names (no spaces), or *\n");
            return false;
        }
    }
    if (!Core::SetLogFile((Platform::ExecutableDir() / "vs-native.log").string().c_str())) {
        std::fprintf(stderr, "cannot open vs-native.log\n");
        return false;
    }
    return true;
}

std::optional<std::string> Option(int argc, char** argv, std::string_view name)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            return std::string(argv[i + 1]);
        }
    }
    return std::nullopt;
}

// Walks up from the executable for `relative` (so a dev build finds the repository copy).
std::optional<fs::path> FindUp(const fs::path& relative)
{
    std::error_code ec;
    for (fs::path dir = Platform::ExecutableDir(); !dir.empty(); dir = dir.parent_path()) {
        if (fs::exists(dir / relative, ec)) {
            return dir / relative;
        }
        if (dir == dir.parent_path()) {
            break;
        }
    }
    return std::nullopt;
}

// --disc <path>, else the first of: VS_DISC env var, disks\SLUS-01040.cue or SLUS-01040\ found
// walking up from the executable (so a dev build finds the repo copy).
std::optional<fs::path> LocateDisc(int argc, char** argv)
{
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
std::optional<fs::path> LocateData(int argc, char** argv)
{
    if (auto opt = Option(argc, argv, "--data")) {
        return fs::path(*opt);
    }
    return FindUp(fs::path("Native") / "data" / Content::DataStore::ManifestName).transform([](const fs::path& p) {
        return p.parent_path();
    });
}

// --capture <dir> --at <tick,tick,...>: writes the presented frame at those ticks (for reference comparisons).
struct Capture {
    fs::path dir;
    std::set<u64> ticks;
    bool pages = false;

    void Write(u64 tick, const std::vector<u32>& rgba, i32 w, i32 h) const
    {
        if (dir.empty() || !ticks.contains(tick)) {
            return;
        }
        fs::create_directories(dir);
        SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, const_cast<u32*>(rgba.data()), w * 4);
        const fs::path file = dir / ("frame_" + std::to_string(tick) + ".bmp");
        if (s == nullptr || !SDL_SaveBMP(s, file.string().c_str())) {
            VS_LOG_ERROR("capture", "{}: {}", file.string(), SDL_GetError());
        } else {
            VS_LOG_INFO("capture", "{} ({}x{})", file.string(), w, h);
        }
        SDL_DestroySurface(s);
        if (pages) {
            const auto source = Render::GamePages().Pixels();
            std::ofstream raw(dir / ("pages_" + std::to_string(tick) + ".bin"), std::ios::binary);
            raw.write(reinterpret_cast<const char*>(source.data()), static_cast<std::streamsize>(source.size_bytes()));
            if (!raw) VS_LOG_ERROR("capture", "Failed to write raw pages at tick {}", tick);
            std::vector<u32> colors(source.size());
            for (usize index = 0; index < source.size(); ++index) {
                const u32 pixel = source[index];
                const u32 red = (pixel & 31) * 255 / 31;
                const u32 green = ((pixel >> 5) & 31) * 255 / 31;
                const u32 blue = ((pixel >> 10) & 31) * 255 / 31;
                colors[index] = 0xFF000000u | red | (green << 8) | (blue << 16);
            }
            SDL_Surface* surface = SDL_CreateSurfaceFrom(Render::PageSpace::Width, Render::PageSpace::Height,
                SDL_PIXELFORMAT_ABGR8888, colors.data(), Render::PageSpace::Width * 4);
            const fs::path output = dir / ("pages_" + std::to_string(tick) + ".bmp");
            if (surface == nullptr || !SDL_SaveBMP(surface, output.string().c_str())) {
                VS_LOG_ERROR("capture", "{}: {}", output.string(), SDL_GetError());
            }
            SDL_DestroySurface(surface);
        }
    }
};

Capture ParseCapture(int argc, char** argv)
{
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
            u64 v = 0;
            if (std::from_chars(item.data(), item.data() + item.size(), v).ec == std::errc{}) {
                c.ticks.insert(v);
            }
            list = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);
        }
    }
    return c;
}

// --press <tick:Buttons,...> (e.g. "3300:Right,3400:Cross"): scripted presses, each held for a few ticks, so a
// capture run can drive the menus.
struct InputScript {
    static constexpr u64 HoldTicks = 3;
    std::vector<std::pair<u64, u32>> presses;

    u32 MaskAt(u64 tick) const
    {
        u32 mask = 0;
        for (const auto& [at, buttons] : presses) {
            if (tick >= at && tick < at + HoldTicks) {
                mask |= buttons;
            }
        }
        return mask;
    }
};

InputScript ParseInputScript(int argc, char** argv)
{
    InputScript s;
    const std::string text = Option(argc, argv, "--press").value_or("");
    std::string_view list = text;
    while (!list.empty()) {
        const usize comma = list.find(',');
        const std::string_view item = list.substr(0, comma);
        const usize colon = item.find(':');
        u64 tick = 0;
        if (colon != std::string_view::npos &&
            std::from_chars(item.data(), item.data() + colon, tick).ec == std::errc{}) {
            s.presses.emplace_back(tick, Input::ParseButtons(item.substr(colon + 1)));
        }
        list = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);
    }
    return s;
}

} // namespace

int main(int argc, char** argv)
try
{
    if (!ConfigureLogging(argc, argv)) {
        return 1;
    }
    Platform::InstallCrashHandler();
    // The game reads only the extracted data (plan D20); the disc, if found, is only identified in the log.
    std::unique_ptr<Content::Disc> disc;
    if (auto discPath = LocateDisc(argc, argv)) {
        if (auto opened = Content::Disc::Open(*discPath)) {
            disc = std::move(*opened);
            VS_LOG_INFO("boot", "disc '{}' ({} entries) from {}", disc->Label(), disc->Files().size(),
                        discPath->string());
        } else {
            VS_LOG_WARN("boot", "{}", opened.error());
        }
    }
    const auto dataRoot = LocateData(argc, argv);
    if (!dataRoot) {
        VS_LOG_ERROR("boot", "no extracted data; run VS.Extract extract <disc> Native\\data, or pass --data <dir>");
        return 1;
    }
    if (auto r = Content::DataStore::Open(*dataRoot); !r) {
        VS_LOG_ERROR("boot", "{}", r.error());
        return 1;
    }
    u64 maxTicks = 0;
    u32 speed = 1;   // --speed N: N logical ticks per real tick (test runs; audio still plays in real time)
    if (auto s = Option(argc, argv, "--speed")) {
        std::from_chars(s->data(), s->data() + s->size(), speed);
        speed = std::max(speed, 1u);
    }
    if (auto frames = Option(argc, argv, "--frames")) {
        std::from_chars(frames->data(), frames->data() + frames->size(), maxTicks);
    }
    const Capture capture = ParseCapture(argc, argv);
    const InputScript script = ParseInputScript(argc, argv);
    VS_LOG_INFO("boot", "run: tick limit={}, speed={}, scripted presses={}", maxTicks, speed, script.presses.size());

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
    Audio::AudioDevice audio;
    if (auto r = audio.Open(); !r) {
        VS_LOG_WARN("boot", "{} (continuing without audio)", r.error());
    }

    Input::InputSystem input;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--scripted") {
            input.SetDevicesEnabled(false);
            VS_LOG_INFO("input", "--scripted: keyboard and gamepad ignored");
        }
    }
    Game::GameApp game({disc.get()});
    Platform::ProgressWatchdog watchdog(maxTicks != 0);
    Core::FrameClock clock;
    double last = app.NowSeconds();
    std::vector<u32> frame;
    i32 frameW = 0, frameH = 0;

    while (app.PumpEvents([&](const SDL_Event& ev) { input.HandleEvent(ev); })) {
        const double now = app.NowSeconds();
        const u32 ticks = clock.Advance(now - last) * speed;
        last = now;
        bool shown = false;
        for (u32 i = 0; i < ticks && (maxTicks == 0 || game.Ticks() < maxTicks); ++i) {
            watchdog.Pulse(game.Ticks() + 1);
            Core::SetLogTick(game.Ticks() + 1);
            VS_LOG_TRACE("frame", "begin tick");
            input.Inject(script.MaskAt(game.Ticks() + 1));
            const auto& pad = input.Update();
            if (pad.pressed) {
                VS_LOG_INFO("input", "pressed {}", Input::DescribeButtons(pad.pressed));
            }
            game.Tick(pad, true);
            shown = Render::GamePages().Present(frame, frameW, frameH);
            VS_LOG_TRACE("frame", "end tick; presented={}, image={}x{}", shown, frameW, frameH);
            if (shown) {
                capture.Write(game.Ticks(), frame, frameW, frameH);
            }
        }
        if (ticks == 0) {
            shown = Render::GamePages().Present(frame, frameW, frameH);
        }
        if (shown) {
            gpu.PresentImage(frame.data(), frameW, frameH);
        } else {
            gpu.PresentClear({0.0f, 0.0f, 0.0f, 1.0f});
        }
        if (maxTicks != 0 && game.Ticks() >= maxTicks) {
            VS_LOG_INFO("boot", "stopping after {} ticks (--frames)", game.Ticks());
            break;
        }
    }
    Movie_PlayerClose();  // its soundtrack source must go before the audio device and SDL
    return 0;
}
catch (const std::exception& error)
{
    VS_LOG_ERROR("boot", "{}", error.what());
    return 1;
}
