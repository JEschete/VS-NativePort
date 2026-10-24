#include "Platform/App.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Core/Log.hpp"

namespace VS::Platform {

App::~App() { Shutdown(); }

std::expected<void, std::string> App::Init(const WindowDesc& desc) {
  const SDL_InitFlags devices =
      desc.initialize_devices ? SDL_INIT_AUDIO | SDL_INIT_GAMEPAD : 0;
  if (!SDL_Init(SDL_INIT_VIDEO | devices)) {
    return std::unexpected(std::string("SDL_Init failed: ") + SDL_GetError());
  }
  sdl_initialized_ = true;
  SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (desc.resizable) {
    flags |= SDL_WINDOW_RESIZABLE;
  }
  window_ =
      SDL_CreateWindow(desc.title.c_str(), desc.width, desc.height, flags);
  if (!window_) {
    return std::unexpected(std::string("SDL_CreateWindow failed: ") +
                           SDL_GetError());
  }
  VS_LOG_INFO("platform", "SDL {}.{}.{} window {}x{}", SDL_MAJOR_VERSION,
              SDL_MINOR_VERSION, SDL_MICRO_VERSION, desc.width, desc.height);
  return {};
}

std::expected<void, std::string> App::InitDevices(bool gamepads) {
  if (!sdl_initialized_) {
    return std::unexpected("initialize the application before its devices");
  }
  const SDL_InitFlags flags =
      SDL_INIT_AUDIO | (gamepads ? SDL_INIT_GAMEPAD : 0);
  if (!SDL_InitSubSystem(flags)) {
    return std::unexpected(std::string("SDL device initialization failed: ") +
                           SDL_GetError());
  }
  return {};
}

void App::Shutdown() {
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  if (sdl_initialized_) {
    SDL_Quit();
    sdl_initialized_ = false;
  }
}

bool App::PumpEvents(const std::function<void(const SDL_Event&)>& handler) {
  bool running = true;
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    if (ev.type == SDL_EVENT_QUIT ||
        ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
      running = false;
    }
    if (handler) {
      handler(ev);
    }
  }
  return running;
}

double App::NowSeconds() const {
  return static_cast<double>(SDL_GetPerformanceCounter()) /
         static_cast<double>(SDL_GetPerformanceFrequency());
}

std::filesystem::path ExecutableDir() {
  const char* base = SDL_GetBasePath();
  return base ? std::filesystem::path(base) : std::filesystem::current_path();
}

std::expected<std::filesystem::path, std::string> UserDataDir() {
  const std::unique_ptr<char, decltype(&SDL_free)> pref(
      SDL_GetPrefPath("VS-NativePort", "VagrantStory"), SDL_free);
  if (!pref) {
    return std::unexpected(std::string("User data directory is unavailable: ") +
                           SDL_GetError());
  }
  const std::string_view path(pref.get());
  return std::filesystem::path(std::u8string(path.begin(), path.end()));
}

std::expected<int, std::string> RunProcess(
    std::span<const std::string> arguments) {
  if (arguments.empty() || arguments.front().empty()) {
    return std::unexpected("process executable is missing");
  }
  std::vector<const char*> pointers;
  pointers.reserve(arguments.size() + 1);
  for (const auto& argument : arguments) {
    if (argument.find('\0') != std::string::npos) {
      return std::unexpected("process argument contains a null character");
    }
    pointers.push_back(argument.c_str());
  }
  pointers.push_back(nullptr);
  const std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process(
      SDL_CreateProcess(pointers.data(), false), SDL_DestroyProcess);
  if (!process) return std::unexpected(std::string(SDL_GetError()));
  int exit_code = 0;
  if (!SDL_WaitProcess(process.get(), true, &exit_code)) {
    return std::unexpected(std::string(SDL_GetError()));
  }
  return exit_code;
}

}  // namespace VS::Platform
