#include "Render/Device.hpp"

#include "Core/Log.hpp"

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
        if (m_frame) {
            SDL_ReleaseGPUTexture(m_device, m_frame);
            m_frame = nullptr;
        }
        if (m_upload) {
            SDL_ReleaseGPUTransferBuffer(m_device, m_upload);
            m_upload = nullptr;
        }
        if (m_window) {
            SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
        }
        SDL_DestroyGPUDevice(m_device);
        m_device = nullptr;
    }
}

bool Device::EnsureFrameTexture(i32 width, i32 height)
{
    if (m_frame && m_frameW == width && m_frameH == height) {
        return true;
    }
    if (m_frame) {
        SDL_ReleaseGPUTexture(m_device, m_frame);
        SDL_ReleaseGPUTransferBuffer(m_device, m_upload);
        m_frame = nullptr;
        m_upload = nullptr;
    }
    SDL_GPUTextureCreateInfo ti{};
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width = static_cast<Uint32>(width);
    ti.height = static_cast<Uint32>(height);
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    m_frame = SDL_CreateGPUTexture(m_device, &ti);
    SDL_GPUTransferBufferCreateInfo bi{};
    bi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    bi.size = static_cast<Uint32>(width * height * 4);
    m_upload = SDL_CreateGPUTransferBuffer(m_device, &bi);
    if (!m_frame || !m_upload) {
        VS_LOG_ERROR("render", "frame texture {}x{}: {}", width, height, SDL_GetError());
        return false;
    }
    m_frameW = width;
    m_frameH = height;
    return true;
}

bool Device::PresentImage(const u32* rgba, i32 width, i32 height, float aspect)
{
    if (!EnsureFrameTexture(width, height)) {
        return false;
    }
    void* map = SDL_MapGPUTransferBuffer(m_device, m_upload, true);
    if (!map) {
        return false;
    }
    SDL_memcpy(map, rgba, static_cast<size_t>(width) * height * 4);
    SDL_UnmapGPUTransferBuffer(m_device, m_upload);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_device);
    if (!cmd) {
        return false;
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo src{};
    src.transfer_buffer = m_upload;
    SDL_GPUTextureRegion dst{};
    dst.texture = m_frame;
    dst.w = static_cast<Uint32>(width);
    dst.h = static_cast<Uint32>(height);
    dst.d = 1;
    SDL_UploadToGPUTexture(copy, &src, &dst, true);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUTexture* swapchain = nullptr;
    Uint32 sw = 0, sh = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, m_window, &swapchain, &sw, &sh) || !swapchain) {
        SDL_SubmitGPUCommandBuffer(cmd);
        return false;
    }
    // letterbox: largest rectangle of the display aspect that fits the window
    float w = static_cast<float>(sw), h = static_cast<float>(sh);
    if (w / h > aspect) {
        w = h * aspect;
    } else {
        h = w / aspect;
    }
    SDL_GPUBlitInfo blit{};
    blit.source.texture = m_frame;
    blit.source.w = static_cast<Uint32>(width);
    blit.source.h = static_cast<Uint32>(height);
    blit.destination.texture = swapchain;
    blit.destination.x = static_cast<Uint32>((static_cast<float>(sw) - w) / 2);
    blit.destination.y = static_cast<Uint32>((static_cast<float>(sh) - h) / 2);
    blit.destination.w = static_cast<Uint32>(w);
    blit.destination.h = static_cast<Uint32>(h);
    blit.load_op = SDL_GPU_LOADOP_CLEAR;
    blit.clear_color = SDL_FColor{0, 0, 0, 1};
    blit.filter = SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(cmd, &blit);
    SDL_SubmitGPUCommandBuffer(cmd);
    return true;
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
