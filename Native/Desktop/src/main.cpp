#include "Audio/AudioDevice.hpp"
#include "Content/DataStore.hpp"
#include "Content/Disc.hpp"
#include "Core/FrameClock.hpp"
#include "Core/Log.hpp"
#include "Boot/GameApp.hpp"
#include "Input/Pad.hpp"
#include "Platform/App.hpp"
#include "Platform/CrashHandler.hpp"
#include "Render/Device.hpp"
#include "Render/PageSpace.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <charconv>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace VS;

namespace {

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
    }
};

Capture ParseCapture(int argc, char** argv)
{
    Capture c;
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

} // namespace

int main(int argc, char** argv)
{
    Core::SetLogFile((Platform::ExecutableDir() / "vs-native.log").string().c_str());
    Platform::InstallCrashHandler();
    auto discPath = LocateDisc(argc, argv);
    if (!discPath) {
        VS_LOG_ERROR("boot", "no disc found; pass --disc <SLUS-01040.cue|folder> or set VS_DISC");
        return 1;
    }
    auto disc = Content::Disc::Open(*discPath);
    if (!disc) {
        VS_LOG_ERROR("boot", "{}", disc.error());
        return 1;
    }
    VS_LOG_INFO("boot", "disc '{}' ({} entries) from {}", (*disc)->Label(), (*disc)->Files().size(),
                discPath->string());
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
    if (auto frames = Option(argc, argv, "--frames")) {
        std::from_chars(frames->data(), frames->data() + frames->size(), maxTicks);
    }
    const Capture capture = ParseCapture(argc, argv);

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
    Game::GameApp game({disc->get()});
    Core::FrameClock clock;
    double last = app.NowSeconds();
    std::vector<u32> frame;
    i32 frameW = 0, frameH = 0;

    while (app.PumpEvents([&](const SDL_Event& ev) { input.HandleEvent(ev); })) {
        const double now = app.NowSeconds();
        const u32 ticks = clock.Advance(now - last);
        last = now;
        bool shown = false;
        for (u32 i = 0; i < ticks; ++i) {
            const auto& pad = input.Update();
            if (pad.pressed) {
                VS_LOG_INFO("input", "pressed {}", Input::DescribeButtons(pad.pressed));
            }
            game.Tick(pad, true);
            shown = Render::GamePages().Present(frame, frameW, frameH);
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
    return 0;
}
