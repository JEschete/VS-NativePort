#include "vs/render/Device.hpp"

#include "vs/core/Log.hpp"

#include <SDL3/SDL.h>

namespace VS::Render {

Device::~Device() { Shutdown(); }

std::expected<void, std::string> Device::Init(SDL_Window* window, bool debug)
{
    m_window = window;
    m_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, debug, nullptr);
    if (!m_device) {
        return std::unexpected(std::string("SDL_CreateGPUDevice failed: ") + SDL_GetError());
    }
    if (!SDL_ClaimWindowForGPUDevice(m_device, m_window)) {
        return std::unexpected(std::string("SDL_ClaimWindowForGPUDevice failed: ") + SDL_GetError());
    }
    VS_LOG_INFO("render", "GPU backend: {}", BackendName());
    return {};
}

void Device::Shutdown()
{
    if (m_device) {
        if (m_window) {
            SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
        }
        SDL_DestroyGPUDevice(m_device);
        m_device = nullptr;
    }
}

bool Device::PresentClear(const ClearColor& color)
{
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_device);
    if (!cmd) {
        return false;
    }
    SDL_GPUTexture* swapchain = nullptr;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, m_window, &swapchain, nullptr, nullptr) || !swapchain) {
        SDL_SubmitGPUCommandBuffer(cmd);
        return false;
    }
    SDL_GPUColorTargetInfo target{};
    target.texture = swapchain;
    target.clear_color = SDL_FColor{color.r, color.g, color.b, color.a};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
    SDL_EndGPURenderPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);
    return true;
}

const char* Device::BackendName() const
{
    return m_device ? SDL_GetGPUDeviceDriver(m_device) : "none";
}

} // namespace VS::Render
