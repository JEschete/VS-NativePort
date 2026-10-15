#pragma once

#include "vs/core/Types.hpp"

#include <expected>
#include <filesystem>
#include <functional>
#include <string>

struct SDL_Window;
union SDL_Event;

namespace VS::Platform {

struct WindowDesc {
    std::string title = "Vagrant Story";
    i32 width = 1280;   // 4:3 by default (the original is 320x240)
    i32 height = 960;
    bool resizable = true;
};

// Owns SDL initialization and the main window. Every other system receives the native
// window handle from here; nothing else calls SDL_Init.
class App {
public:
    App() = default;
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    std::expected<void, std::string> Init(const WindowDesc& desc);
    void Shutdown();

    // Pumps OS events and forwards each to `handler`. Returns false once quit was requested.
    bool PumpEvents(const std::function<void(const SDL_Event&)>& handler);

    SDL_Window* Window() const { return m_window; }
    double NowSeconds() const;

private:
    SDL_Window* m_window = nullptr;
    bool m_sdlInit = false;
};

// Directory containing the running executable.
std::filesystem::path ExecutableDir();

// Per-user writable directory (saves, config). Created on first call.
std::filesystem::path UserDataDir();

} // namespace VS::Platform
