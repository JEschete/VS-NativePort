#pragma once

#include "vs/core/Types.hpp"

#include <expected>
#include <string>

struct SDL_Window;
struct SDL_GPUDevice;

namespace VS::Render {

struct ClearColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

// SDL3 GPU device bound to the main window (plan D: SDL3 GPU now, optional D3D12 later).
// Phase 1a only clears the swapchain; the PS1-semantics renderer (RenderQueue, VRAM model,
// semi-transparency modes) is built on top of this in 1b.
class Device {
public:
    Device() = default;
    ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    std::expected<void, std::string> Init(SDL_Window* window, bool debug);
    void Shutdown();

    // Acquires the swapchain, clears it and presents. Returns false if the frame was skipped
    // (e.g. minimized window).
    bool PresentClear(const ClearColor& color);

    const char* BackendName() const;

private:
    SDL_Window* m_window = nullptr;
    SDL_GPUDevice* m_device = nullptr;
};

} // namespace VS::Render
