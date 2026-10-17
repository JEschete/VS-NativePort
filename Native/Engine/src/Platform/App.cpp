#include "Platform/App.hpp"

#include "Core/Log.hpp"

#include <SDL3/SDL.h>

namespace VS::Platform {

App::~App() { Shutdown(); }

std::expected<void, std::string> App::Init(const WindowDesc& desc)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        return std::unexpected(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    m_sdlInit = true;
    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (desc.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    m_window = SDL_CreateWindow(desc.title.c_str(), desc.width, desc.height, flags);
    if (!m_window) {
        return std::unexpected(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }
    VS_LOG_INFO("platform", "SDL {}.{}.{} window {}x{}", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION,
                desc.width, desc.height);
    return {};
}

void App::Shutdown()
{
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_sdlInit) {
        SDL_Quit();
        m_sdlInit = false;
    }
}

bool App::PumpEvents(const std::function<void(const SDL_Event&)>& handler)
{
    bool running = true;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            running = false;
        }
        if (handler) {
            handler(ev);
        }
    }
    return running;
}

double App::NowSeconds() const
{
    return static_cast<double>(SDL_GetPerformanceCounter()) / static_cast<double>(SDL_GetPerformanceFrequency());
}

std::filesystem::path ExecutableDir()
{
    const char* base = SDL_GetBasePath();
    return base ? std::filesystem::path(base) : std::filesystem::current_path();
}

std::filesystem::path UserDataDir()
{
    char* pref = SDL_GetPrefPath("VS-NativePort", "VagrantStory");
    if (!pref) {
        return std::filesystem::current_path() / "userdata";
    }
    std::filesystem::path p(pref);
    SDL_free(pref);
    return p;
}

} // namespace VS::Platform
