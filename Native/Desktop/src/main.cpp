#include "vs/audio/AudioDevice.hpp"
#include "vs/content/Disc.hpp"
#include "vs/core/FrameClock.hpp"
#include "vs/core/Log.hpp"
#include "vs/game/boot/GameApp.hpp"
#include "vs/input/Pad.hpp"
#include "vs/platform/App.hpp"
#include "vs/render/Device.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;
using namespace VS;

namespace {

// --disc <path>, else the first of: VS_DISC env var, disks\SLUS-01040.cue or SLUS-01040\ found
// walking up from the executable (so a dev build finds the repo copy).
std::optional<fs::path> LocateDisc(int argc, char** argv)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--disc") {
            return fs::path(argv[i + 1]);
        }
    }
    if (const char* env = SDL_getenv("VS_DISC")) {
        return fs::path(env);
    }
    std::error_code ec;
    for (fs::path dir = Platform::ExecutableDir(); !dir.empty(); dir = dir.parent_path()) {
        for (const auto& candidate : {dir / "disks" / "SLUS-01040.cue", dir / "SLUS-01040"}) {
            if (fs::exists(candidate, ec)) {
                return candidate;
            }
        }
        if (dir == dir.parent_path()) {
            break;
        }
    }
    return std::nullopt;
}

} // namespace

int main(int argc, char** argv)
{
    Core::SetLogFile((Platform::ExecutableDir() / "vs-native.log").string().c_str());
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

    while (app.PumpEvents([&](const SDL_Event& ev) { input.HandleEvent(ev); })) {
        const double now = app.NowSeconds();
        const u32 ticks = clock.Advance(now - last);
        last = now;
        for (u32 i = 0; i < ticks; ++i) {
            const auto& pad = input.Update();
            if (pad.pressed) {
                VS_LOG_INFO("input", "pressed {}", Input::DescribeButtons(pad.pressed));
            }
            game.Tick(pad);
        }
        gpu.PresentClear({0.02f, 0.02f, 0.05f, 1.0f});
    }
    return 0;
}
