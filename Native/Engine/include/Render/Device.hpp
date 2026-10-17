#pragma once

#include "Core/Types.hpp"

#include <expected>
#include <string>

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUTexture;
struct SDL_GPUTransferBuffer;

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

    // Shows an RGBA8 image (0xAABBGGRR) letterboxed at the given display aspect (4:3 for the PS1 TV picture).
    bool PresentImage(const u32* rgba, i32 width, i32 height, float aspect = 4.0f / 3.0f);

    const char* BackendName() const;

private:
    bool EnsureFrameTexture(i32 width, i32 height);

    SDL_Window* m_window = nullptr;
    SDL_GPUDevice* m_device = nullptr;
    SDL_GPUTexture* m_frame = nullptr;
    SDL_GPUTransferBuffer* m_upload = nullptr;
    i32 m_frameW = 0, m_frameH = 0;
};

} // namespace VS::Render
