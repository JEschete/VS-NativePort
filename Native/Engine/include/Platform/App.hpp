#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <span>
#include <string>

#include "Core/Types.hpp"

struct SDL_Window;
union SDL_Event;

namespace VS::Platform {

struct WindowDesc {
  static constexpr i32 kDefaultWidth = 1280;
  static constexpr i32 kDefaultHeight = 960;
  std::string title = "Vagrant Story";
  i32 width = kDefaultWidth;  // 4:3 by default (the original is 320x240)
  i32 height = kDefaultHeight;
  bool resizable = true;
  // Hosts can present loading feedback before device discovery.
  bool initialize_devices = true;
};

// Owns SDL initialization and the main window. Every other system receives the
// native window handle from here; nothing else calls SDL_Init.
class App {
 public:
  App() = default;
  ~App();
  App(const App&) = delete;
  App& operator=(const App&) = delete;

  std::expected<void, std::string> Init(const WindowDesc& desc);
  std::expected<void, std::string> InitDevices(bool gamepads = true);
  void Shutdown();

  // Pumps OS events and forwards each to `handler`. Returns false once quit was
  // requested.
  bool PumpEvents(const std::function<void(const SDL_Event&)>& handler);

  SDL_Window* Window() const { return window_; }
  double NowSeconds() const;

 private:
  SDL_Window* window_ = nullptr;
  bool sdl_initialized_ = false;
};

// Directory containing the running executable.
std::filesystem::path ExecutableDir();

// Per-user writable directory (saves, config). Created on first call.
std::filesystem::path UserDataDir();

std::expected<int, std::string> RunProcess(
    std::span<const std::string> arguments);

}  // namespace VS::Platform
