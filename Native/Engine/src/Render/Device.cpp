#include "Render/Device.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Core/Log.hpp"

namespace VS::Render {

namespace {

#ifdef _DEBUG
constexpr bool kGpuValidationByDefault = true;
#else
constexpr bool kGpuValidationByDefault = false;
#endif

// Collects SDL's GPU-category messages while the device is created: SDL logs
// whether it found the validation layers only there. Other categories pass
// through to the previous output function.
class SdlGpuReportCapture {
 public:
  explicit SdlGpuReportCapture(std::vector<std::string>* reports)
      : reports_(reports) {
    if (reports_ == nullptr) return;
    SDL_GetLogOutputFunction(&previous_, &previous_data_);
    priority_ = SDL_GetLogPriority(SDL_LOG_CATEGORY_GPU);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_INFO);
    SDL_SetLogOutputFunction(&Output, this);
  }
  ~SdlGpuReportCapture() {
    if (reports_ == nullptr) return;
    SDL_SetLogOutputFunction(previous_, previous_data_);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, priority_);
  }
  SdlGpuReportCapture(const SdlGpuReportCapture&) = delete;
  SdlGpuReportCapture& operator=(const SdlGpuReportCapture&) = delete;

  bool Lost() const { return lost_; }

 private:
  static void SDLCALL Output(void* userdata, int category,
                             SDL_LogPriority priority, const char* message) {
    auto& self = *static_cast<SdlGpuReportCapture*>(userdata);
    if (category == SDL_LOG_CATEGORY_GPU && message != nullptr) {
      // SDL is C: an exception must not unwind through it.
      try {
        self.reports_->emplace_back(message);
      } catch (const std::exception&) {
        self.lost_ = true;
      }
      return;
    }
    if (self.previous_ != nullptr) {
      self.previous_(self.previous_data_, category, priority, message);
    }
  }

  std::vector<std::string>* reports_;
  SDL_LogOutputFunction previous_ = nullptr;
  void* previous_data_ = nullptr;
  SDL_LogPriority priority_ = SDL_LOG_PRIORITY_INVALID;
  bool lost_ = false;
};

}  // namespace

bool GpuValidationByDefault() { return kGpuValidationByDefault; }

bool ChooseGpuValidation(int argc, const char* const* argv) {
  bool validation = kGpuValidationByDefault;
  if (argv == nullptr || argc <= 0) return validation;
  for (const char* argument : std::span(argv, static_cast<std::size_t>(argc))) {
    const std::string_view text = argument == nullptr ? "" : argument;
    if (text == "--gpu-validation") {
      validation = true;
    } else if (text == "--no-gpu-validation") {
      validation = false;
    }
  }
  return validation;
}

GpuValidation ClassifyGpuValidation(bool requested,
                                    std::span<const std::string> sdl_reports) {
  if (!requested) return GpuValidation::kOff;
  const auto reported = [sdl_reports](std::string_view text) {
    return std::ranges::any_of(sdl_reports, [text](const std::string& report) {
      return report.find(text) != std::string::npos;
    });
  };
  // SDL 3.4 D3D12 and Vulkan report the layers with these messages.
  if (reported("Validation layers enabled")) return GpuValidation::kOn;
  if (reported("Validation layers partially enabled")) {
    return GpuValidation::kPartial;
  }
  if (reported("Validation layers not found")) {
    return GpuValidation::kUnavailable;
  }
  return GpuValidation::kUnreported;
}

std::string_view GpuValidationName(GpuValidation validation) {
  switch (validation) {
    case GpuValidation::kOff:
      return "off";
    case GpuValidation::kOn:
      return "on";
    case GpuValidation::kPartial:
      return "partial";
    case GpuValidation::kUnavailable:
      return "unavailable";
    case GpuValidation::kUnreported:
      return "unreported";
  }
  return "unreported";
}

Device::~Device() { Shutdown(); }

std::expected<void, std::string> Device::Init(SDL_Window* window,
                                              bool validation) {
  m_window_ = window;
  std::vector<std::string> reports;
  bool lost_reports = false;
  {
    const SdlGpuReportCapture capture(validation ? &reports : nullptr);
    m_device_ = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, validation,
        nullptr);
    lost_reports = capture.Lost();
  }
  for (const auto& report : reports) {
    VS::Core::LogInfo("render", "SDL GPU: {}", report);
  }
  if (lost_reports) {
    VS::Core::LogWarn("render",
                      "SDL GPU reports were lost while recording them");
  }
  if (!m_device_) {
    return std::unexpected(std::string("SDL_CreateGPUDevice failed: ") +
                           SDL_GetError());
  }
  if (!SDL_ClaimWindowForGPUDevice(m_device_, m_window_)) {
    return std::unexpected(std::string("SDL_ClaimWindowForGPUDevice failed: ") +
                           SDL_GetError());
  }
  m_validation_ = ClassifyGpuValidation(validation, reports);
  if (validation && m_validation_ != GpuValidation::kOn) {
    VS::Core::LogWarn("render", "GPU backend: {}; validation requested but {}",
                      BackendName(), GpuValidationName(m_validation_));
  } else {
    VS::Core::LogInfo("render", "GPU backend: {}; validation {}", BackendName(),
                      GpuValidationName(m_validation_));
  }
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
    m_validation_ = GpuValidation::kOff;
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
    VS::Core::LogError("render", "frame texture {}x{}: {}", width, height,
                       SDL_GetError());
    return false;
  }
  SDL_GPUTransferBufferCreateInfo bi{};
  bi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  bi.size = *bytes;
  SDL_GPUTransferBuffer* upload = SDL_CreateGPUTransferBuffer(m_device_, &bi);
  if (!upload) {
    SDL_ReleaseGPUTexture(m_device_, frame);
    VS::Core::LogError("render", "frame upload {}x{}: {}", width, height,
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

bool Device::PresentClear(const SwapchainClearColor& color) {
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
