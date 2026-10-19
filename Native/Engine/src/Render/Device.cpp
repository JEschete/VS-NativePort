#include "Render/Device.hpp"

#include <SDL3/SDL.h>

#include <limits>
#include <string>

#include "Core/Log.hpp"

namespace VS::Render {

Device::~Device() { Shutdown(); }

std::expected<void, std::string> Device::Init(SDL_Window* window, bool debug) {
  m_window_ = window;
  m_device_ = SDL_CreateGPUDevice(
      SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, debug, nullptr);
  if (!m_device_) {
    return std::unexpected(std::string("SDL_CreateGPUDevice failed: ") +
                           SDL_GetError());
  }
  if (!SDL_ClaimWindowForGPUDevice(m_device_, m_window_)) {
    return std::unexpected(std::string("SDL_ClaimWindowForGPUDevice failed: ") +
                           SDL_GetError());
  }
  VS_LOG_INFO("render", "GPU backend: {}", BackendName());
  return {};
}

void Device::Shutdown() {
  if (m_device_) {
    if (m_frame_) {
      SDL_ReleaseGPUTexture(m_device_, m_frame_);
      m_frame_ = nullptr;
    }
    if (m_upload_) {
      SDL_ReleaseGPUTransferBuffer(m_device_, m_upload_);
      m_upload_ = nullptr;
    }
    if (m_window_) {
      SDL_ReleaseWindowFromGPUDevice(m_device_, m_window_);
    }
    SDL_DestroyGPUDevice(m_device_);
    m_device_ = nullptr;
  }
}

std::optional<u32> Device::FrameByteCount(i32 width, i32 height) {
  if (width <= 0 || height <= 0) return std::nullopt;
  const u64 bytes =
      static_cast<u64>(width) * static_cast<u64>(height) * sizeof(u32);
  if (bytes > std::numeric_limits<u32>::max()) return std::nullopt;
  return static_cast<u32>(bytes);
}

bool Device::EnsureFrameTexture(i32 width, i32 height) {
  const auto bytes = FrameByteCount(width, height);
  if (!bytes || !m_device_) return false;
  if (m_frame_ && m_upload_ && m_frame_w_ == width && m_frame_h_ == height) {
    return true;
  }
  SDL_GPUTextureCreateInfo ti{};
  ti.type = SDL_GPU_TEXTURETYPE_2D;
  ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  ti.width = static_cast<Uint32>(width);
  ti.height = static_cast<Uint32>(height);
  ti.layer_count_or_depth = 1;
  ti.num_levels = 1;
  SDL_GPUTexture* frame = SDL_CreateGPUTexture(m_device_, &ti);
  if (!frame) {
    VS_LOG_ERROR("render", "frame texture {}x{}: {}", width, height,
                 SDL_GetError());
    return false;
  }
  SDL_GPUTransferBufferCreateInfo bi{};
  bi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  bi.size = *bytes;
  SDL_GPUTransferBuffer* upload = SDL_CreateGPUTransferBuffer(m_device_, &bi);
  if (!upload) {
    SDL_ReleaseGPUTexture(m_device_, frame);
    VS_LOG_ERROR("render", "frame upload {}x{}: {}", width, height,
                 SDL_GetError());
    return false;
  }
  if (m_frame_) SDL_ReleaseGPUTexture(m_device_, m_frame_);
  if (m_upload_) SDL_ReleaseGPUTransferBuffer(m_device_, m_upload_);
  m_frame_ = frame;
  m_upload_ = upload;
  m_frame_w_ = width;
  m_frame_h_ = height;
  return true;
}

bool Device::PresentImage(const u32* rgba, i32 width, i32 height,
                          float aspect) {
  const auto bytes = FrameByteCount(width, height);
  if (rgba == nullptr || !bytes || !EnsureFrameTexture(width, height)) {
    return false;
  }
  void* map = SDL_MapGPUTransferBuffer(m_device_, m_upload_, true);
  if (!map) {
    return false;
  }
  SDL_memcpy(map, rgba, *bytes);
  SDL_UnmapGPUTransferBuffer(m_device_, m_upload_);

  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_device_);
  if (!cmd) {
    return false;
  }
  SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
  SDL_GPUTextureTransferInfo src{};
  src.transfer_buffer = m_upload_;
  SDL_GPUTextureRegion dst{};
  dst.texture = m_frame_;
  dst.w = static_cast<Uint32>(width);
  dst.h = static_cast<Uint32>(height);
  dst.d = 1;
  SDL_UploadToGPUTexture(copy, &src, &dst, true);
  SDL_EndGPUCopyPass(copy);

  SDL_GPUTexture* swapchain = nullptr;
  Uint32 sw = 0, sh = 0;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, m_window_, &swapchain, &sw,
                                             &sh) ||
      !swapchain) {
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
  blit.source.texture = m_frame_;
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

bool Device::PresentClear(const ClearColor& color) {
  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_device_);
  if (!cmd) {
    return false;
  }
  SDL_GPUTexture* swapchain = nullptr;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, m_window_, &swapchain,
                                             nullptr, nullptr) ||
      !swapchain) {
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

const char* Device::BackendName() const {
  return m_device_ ? SDL_GetGPUDeviceDriver(m_device_) : "none";
}

}  // namespace VS::Render
